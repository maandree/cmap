.POSIX:

CONFIGFILE = config.mk
include $(CONFIGFILE)

OBJ =\
	cmap.o

HDR = common.h

all: cmap
$(OBJ): $(HDR)

.c.o:
	$(CC) -c -o $@ $< $(CFLAGS) $(CPPFLAGS)

cmap: $(OBJ)
	$(CC) -o $@ $(OBJ) $(LDFLAGS)

install: cmap
	mkdir -p -- "$(DESTDIR)$(PREFIX)/bin"
	mkdir -p -- "$(DESTDIR)$(MANPREFIX)/man1/"
	cp -- cmap "$(DESTDIR)$(PREFIX)/bin/"
	cp -- cmap.1 "$(DESTDIR)$(MANPREFIX)/man1/"

uninstall:
	-rm -f -- "$(DESTDIR)$(PREFIX)/bin/cmap"
	-rm -f -- "$(DESTDIR)$(MANPREFIX)/man1/cmap.1"

clean:
	-rm -f -- *.o *.a *.lo *.su *.so *.so.* *.gch *.gcov *.gcno *.gcda
	-rm -f -- cmap

.SUFFIXES:
.SUFFIXES: .o .c

.PHONY: all install uninstall clean
