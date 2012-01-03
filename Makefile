# Makefile for monokern
PREFIX := /usr/local
CC := gcc
CFLAGS := -Wall -Wextra -Werror -pedantic --std=gnu99 -g -DPREFIX=\"$(PREFIX)\"

all: mk_kern mk_scores kernify kern.o termk as.termkf

install: $(PREFIX)/bin/termk $(PREFIX)/share/sounds/bell.wav $(PREFIX)/share/fonts/as.termkf

$(PREFIX)/bin/termk: termk
	install -D $< $@

$(PREFIX)/share/sounds/bell.wav: bell.wav
	install -D $< $@

$(PREFIX)/share/fonts/as.termkf: as.termkf
	install -D -m644 $< $@

%.termkf: fontify %/*
	./fontify $@

termk: termk.c kern.h kern.o bits.h bits.o kfa.h kfa.o pbm.h pbm.o
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDFLAGS) kern.o bits.o kfa.o pbm.o -o $@ `sdl-config --cflags --libs`

mk_scores: mk_scores.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDFLAGS) -o $@

fontify: fontify.c bits.h bits.o kfa.h kfa.o
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDFLAGS) bits.o kfa.o -o $@

kernify: kernify.c bits.h bits.o kern.h kern.o
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDFLAGS) bits.o kern.o -o $@ `sdl-config --cflags --libs` -lSDL_image

%: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDFLAGS) -o $@ `sdl-config --cflags --libs` -lSDL_image

%.o: %.c %.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@ `sdl-config --cflags`

FORCE:
