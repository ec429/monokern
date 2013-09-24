# Makefile for monokern
PREFIX := /usr/local
TERMINFO := /usr/share/terminfo
CC := gcc
CFLAGS := -Wall -Wextra -Werror -pedantic --std=gnu99 -g -DPREFIX=\"$(PREFIX)\"

all: kern.o edkern progress termk as.termkf 18.termkf scribit.termkf

clean:
	-rm *.termkf *.o edkern progress termk

install: $(PREFIX)/bin/termk $(PREFIX)/share/fonts/as.termkf $(PREFIX)/share/fonts/18.termkf $(PREFIX)/share/fonts/scribit.termkf $(TERMINFO)/t/termk52 $(TERMINFO)/t/termk52-w

$(PREFIX)/bin/termk: termk
	install -D $< $@

$(TERMINFO)/t/termk52: termk52.ti
	sudo tic termk52.ti

$(TERMINFO)/t/termk52-w: termk52.ti termk52-w.ti
	sudo tic termk52-w.ti

$(PREFIX)/share/fonts/%.termkf: %.termkf
	install -D -m644 $< $@

%.termkf: fontify %/*
	./fontify $@

termk: termk.c kern.h kern.o bits.h bits.o kfa.h kfa.o pbm.h pbm.o
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDFLAGS) kern.o bits.o kfa.o pbm.o -o $@ `sdl-config --cflags --libs` -lncurses -lX11

edkern: edkern.c kern.h kern_hack.h kern.o bits.h bits.o pbm.h pbm.o
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDFLAGS) kern.o bits.o pbm.o -o $@ `sdl-config --cflags --libs`

progress: progress.c kern.h kern_hack.h bits.h kern.o bits.o
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDFLAGS) kern.o bits.o -o $@

fontify: fontify.c bits.h bits.o kfa.h kfa.o
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDFLAGS) bits.o kfa.o -o $@

kern.o: kern.c kern.h kern_hack.h

%: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDFLAGS) -o $@ `sdl-config --cflags --libs`

%.o: %.c %.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@ `sdl-config --cflags`
