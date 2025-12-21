/* See LICENSE file for copyright and license details. */
#include "common.h"

USAGE("[-f font-family] [-s [font-size][/[[min]-[max]]]] [-B | -S] [-bi]");


#define DEFAULT_LISTING_TYPE "script"
#define LIST_LISTINGS(X, D)\
	X(BY_SCRIPT, "By _script", DEFAULT_LISTING_TYPE, by_script_selected, populate_scripts, GDK_s) D\
	X(BY_BLOCK, "By Unicode _block", "block", by_block_selected, populate_blocks, GDK_b)

enum listing {
#define X(ENUM, TITLE, TYPE, SELFUN, POPFUN, ACCEL) ENUM
	LIST_LISTINGS(X, COMMA)
#undef X
};

#define NLISTINGS_X(...) (size_t)1
#define NLISTINGS (LIST_LISTINGS(NLISTINGS_X, +))


static const struct libcmap_block all_block = {.name = "All", .range = LIBCMAP_UNIVERSE_RANGE};

static unsigned int min_font_size = 4;
static unsigned int default_font_size = 22;
static unsigned int max_font_size = 500;
static unsigned int small_font_size_increment = 1;
static unsigned int big_font_size_increment = 8;

static enum listing listing = (enum listing)0;
static int use_bold = 0;
static int use_italic = 0;

static size_t term_width;
static size_t term_height;
static size_t left_pane_width;
static int automatic_left_pane_width = 1;

static volatile sig_atomic_t term_resized = 0;
static struct termios term_attributes_stdin;
static int term_attributes_stdin_saved = 0;
static int term_entered_submode_stdout = 0;

static int interrupt_deferred = 0;
static int exiting = 0;


static void
enter_subterminal(void)
{
	printf("\033[?1049h"   /* enter subterminal */
	       "\033[?25l"     /* hide cursor */
	       "\033[H\033[2J" /* clear terminal */
	       );
	fflush(stdout);
	term_entered_submode_stdout = 1;
}


static void
exit_subterminal(void)
{
	if (term_entered_submode_stdout) {
		term_entered_submode_stdout = 0;
		printf("\033[H\033[2J" /* clear terminal */
		       "\033[?25h"     /* show cursor */
		       "\033[?1049l"   /* exit subterminal */
		       );
		fflush(stdout);
	}
}


static void
set_term_attributes(void)
{
	struct termios new_term_attributes_stdin;

	if (tcgetattr(STDIN_FILENO, &term_attributes_stdin))
		eprintf("tcgetattr <stdin>:");
	term_attributes_stdin_saved = 1;

	memcpy(&new_term_attributes_stdin, &term_attributes_stdin, sizeof(term_attributes_stdin));
	new_term_attributes_stdin.c_iflag &= (tcflag_t)~(IXON | IXANY | IXOFF);
	new_term_attributes_stdin.c_lflag &= (tcflag_t)~(ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHONL);

	if (tcsetattr(STDIN_FILENO, TCSANOW, &new_term_attributes_stdin))
		weprintf("tcsetattr <stdin> TCSANOW:");
}


static void
restore_term_attributes(void)
{
	if (term_attributes_stdin_saved) {
		term_attributes_stdin_saved = 0;
		if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &term_attributes_stdin))
			weprintf("tcsetattr <stdin> TCSAFLUSH:");
	}
}


static void
configure_terminal(void)
{
	set_term_attributes();
	enter_subterminal();
}


static void
restore_terminal(void)
{
	restore_term_attributes();
	exit_subterminal();
}


static void
cleanup(void)
{
	restore_terminal();
}


static void
term_resized_callback(int signo)
{
	(void) signo;
	term_resized = 1;
}


static void
subscribe_term_resize(void)
{
	struct sigaction sa;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = &term_resized_callback;

	if (sigaction(SIGWINCH, &sa, NULL))
		eprintf("sigaction SIGWINCH:");
}


static void
update_term_size(void)
{
	struct winsize ws;

	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws))
		eprintf("ioclt <stdout> TIOCGWINSZ:");

	term_width = ws.ws_col;
	term_height = ws.ws_row;
}


static const char *
maybe_sat_parse_int_as_uint(const char *s, unsigned int *out, int *setp)
{
	unsigned int digit;
	if (!isdigit(*s))
		return s;
	*out = 0;
	*setp = 1;
	for (; isdigit(*s); s++) {
		digit = (unsigned int)(*s & 15);
		if (*out > ((unsigned int)INT_MAX - digit) / 10U)
			*out = (unsigned int)INT_MAX;
		else
			*out = *out * 10U + digit;
	}
	return s;
}


static void
parse_fontsize(const char *s, int *default_font_size_setp, int *min_font_size_setp, int *max_font_size_setp)
{
	s = maybe_sat_parse_int_as_uint(s, &default_font_size, default_font_size_setp);

	if (!*s)
		return;
	if (*s++ != '/')
		usage();
	if (!*s)
		return;

	s = maybe_sat_parse_int_as_uint(s, &min_font_size, min_font_size_setp);

	if (*s++ != '-')
		usage();
	if (!*s)
		return;

	s = maybe_sat_parse_int_as_uint(s, &max_font_size, max_font_size_setp);

	if (*s)
		usage();

	if (!default_font_size)
		eprintf("default font size cannot be zero");
	if (!min_font_size)
		eprintf("minimum font size cannot be zero");
	if (!max_font_size)
		eprintf("maximum font size cannot be zero");
}


static void
handle_keyboard_input(union libterminput_input *input)
{
	if (input->type == LIBTERMINPUT_KEYPRESS) {
		switch ((int)input->keypress.mods) {
		case 0:
			if (input->keypress.key == LIBTERMINPUT_TAB) {
			} else if (input->keypress.key == LIBTERMINPUT_BACKTAB) backtab: {
			}
			break;

		case LIBTERMINPUT_SHIFT:
			if (input->keypress.key == LIBTERMINPUT_TAB)
				goto backtab;
			break;

		case LIBTERMINPUT_CTRL:
		case LIBTERMINPUT_CTRL | LIBTERMINPUT_SHIFT:
			if (IS_KEY(input, 'Z')) {
				restore_terminal();
				raise(SIGTSTP);
				configure_terminal();
			redraw:
				interrupt_deferred = 1;
				term_resized = 1;
			} else if (IS_KEY(input, 'L')) {
				goto redraw;
			} else if (IS_KEY(input, 'Q') || IS_KEY(input, 'C')) {
				exiting = 1;
			}
			break;

		case LIBTERMINPUT_META:
			if (input->keypress.key == LIBTERMINPUT_LEFT) {
				automatic_left_pane_width = 0;
				if (left_pane_width) {
					left_pane_width -= 1U;
					goto redraw;
				}
			} else if (input->keypress.key == LIBTERMINPUT_RIGHT) {
				automatic_left_pane_width = 0;
				if (term_width && left_pane_width < term_width - 1U) {
					left_pane_width += 1U;
					goto redraw;
				}
			}
			break;

		case LIBTERMINPUT_META | LIBTERMINPUT_SHIFT:
		case LIBTERMINPUT_META | LIBTERMINPUT_CTRL:
		case LIBTERMINPUT_META | LIBTERMINPUT_CTRL | LIBTERMINPUT_SHIFT:
		default:
			break;
		}
	}
}


int
main(int argc, char *argv[])
{
	int default_font_size_set = 0;
	int min_font_size_set = 0;
	int max_font_size_set = 0;
	const char *font_family = "sans";
	size_t i, len;
	struct libterminput_state input_ctx;
	union libterminput_input input;

	ARGBEGIN {
	case 'B':
		listing = 1;
		break;
	case 'S':
		listing = 0;
		break;
	case 'b':
		use_bold = 1;
		break;
	case 'i':
		use_italic = 1;
		break;
	case 'f':
		font_family = ARG();
		break;
	case 's':
		parse_fontsize(ARG(), &default_font_size_set, &min_font_size_set, &max_font_size_set);
		break;
	default:
		usage();
	} ARGEND;

	if (argc)
		usage();

	if (min_font_size_set && max_font_size_set) {
		if (min_font_size > max_font_size)
			eprintf("minimum font size cannot be greater than maximum font size");
	} else if (min_font_size_set) {
		if (max_font_size < min_font_size)
			max_font_size = min_font_size;
	} else if (max_font_size_set) {
		if (min_font_size > max_font_size)
			min_font_size = max_font_size;
	}
	if (default_font_size_set) {
		if (default_font_size < min_font_size) {
			if (min_font_size_set)
				eprintf("default font size cannot be less than the minimum font size");
			min_font_size = default_font_size;
		}
		if (default_font_size > max_font_size) {
			if (max_font_size_set)
				eprintf("default font size cannot be greater than the minimum font size");
			max_font_size = default_font_size;
		}
	} else {
		if (default_font_size < min_font_size)
			default_font_size = min_font_size;
		else if (default_font_size > max_font_size)
			default_font_size = max_font_size;
	}

	if (!isatty(STDIN_FILENO) || !isatty(STDOUT_FILENO))
		eprintf("<stdin> and <stdout> are expected to be terminal devices");

	left_pane_width = strlen("No Block"); /* wider than "All" */
	for (i = 0; i < libcmap_block_list_size; i++) {
		len = strlen(libcmap_block_list[i].name);
		if (len > left_pane_width)
			left_pane_width = len;
	}
	for (i = 0; i < libcmap_script_list_size; i++) {
		len = strlen(libcmap_script_list[i].name);
		if (len > left_pane_width)
			left_pane_width = len;
	}

	atexit(&cleanup);
	libsimple_eprintf_preprint = &cleanup;

	setlocale(LC_ALL, "");

	configure_terminal();
	subscribe_term_resize();
	memset(&input_ctx, 0, sizeof(input_ctx));
	libterminput_init(&input_ctx, STDIN_FILENO);

	goto beginning;
	while (!exiting) {
		if (interrupt_deferred) {
			interrupt_deferred = 0;
			goto interrupted;
		}
		switch (libterminput_read(STDIN_FILENO, &input, &input_ctx)) {
		case 1:
			handle_keyboard_input(&input);
			break;
		case 0:
			exiting = 1;
			break;
		default:
			if (errno != EINTR)
				eprintf("libterminput <stdin>:");
		interrupted:
			if (term_resized) {
			beginning:
				term_resized = 0;
				update_term_size();
				/* TODO redraw */
			}
			break;
		}
	}

	restore_terminal();
	libterminput_destroy(&input_ctx);
	return 0;
}
