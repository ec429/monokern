# Makefile for monokern
PREFIX := /usr/local
TERMINFO := /usr/share/terminfo
CC := gcc
CFLAGS := -Wall -Wextra -Werror -pedantic --std=gnu99 -g -DPREFIX=\"$(PREFIX)\"

all: kern.o edkern termk as.termkf 18.termkf

clean:
	-rm *.termkf *.o edkern termk

install: $(PREFIX)/bin/termk $(PREFIX)/share/sounds/bell.wav $(PREFIX)/share/fonts/as.termkf $(PREFIX)/share/fonts/18.termkf $(TERMINFO)/t/termk52

$(PREFIX)/bin/termk: termk
	install -D $< $@

$(TERMINFO)/t/termk52: termk52.ti
	sudo tic termk52.ti

$(PREFIX)/share/sounds/bell.wav: bell.wav
	install -D $< $@

$(PREFIX)/share/fonts/%.termkf: %.termkf
	install -D -m644 $< $@

%.termkf: fontify %/*
	./fontify $@

termk: termk.c kern.h kern.o bits.h bits.o kfa.h kfa.o pbm.h pbm.o
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDFLAGS) kern.o bits.o kfa.o pbm.o -o $@ `sdl-config --cflags --libs`

edkern: edkern.c kern.h kern_hack.h kern.o bits.h bits.o pbm.h pbm.o
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDFLAGS) kern.o bits.o pbm.o -o $@ `sdl-config --cflags --libs`

fontify: fontify.c bits.h bits.o kfa.h kfa.o
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDFLAGS) bits.o kfa.o -o $@

kern.o: kern.c kern.h kern_hack.h

%: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDFLAGS) -o $@ `sdl-config --cflags --libs`

%.o: %.c %.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@ `sdl-config --cflags`
