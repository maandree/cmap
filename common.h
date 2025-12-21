/* See LICENSE file for copyright and license details. */
#ifndef COMMON_H_
#define COMMON_H_

#include <sys/epoll.h>
#include <locale.h>
#include <termios.h>

#include <libsimple.h>
#include <libsimple-arg.h>
#include <libterminput.h>
#include <libcmap.h>


#define COMMA ,


#define IS_KEY(INPUT, KEY)\
        ((INPUT)->keypress.key == LIBTERMINPUT_SYMBOL &&\
         toupper((INPUT)->keypress.symbol[0]) == toupper((KEY)) &&\
         !(INPUT)->keypress.symbol[1])

#define IS_CTRL_KEY(INPUT, KEY)\
	(((INPUT)->keypress.mods & (enum libterminput_mod)~LIBTERMINPUT_SHIFT) == LIBTERMINPUT_CTRL &&\
	 IS_KEY((INPUT), (KEY)))


#endif
