# Makefile for monokern
CC := gcc
CFLAGS := -Wall -Wextra -Werror -pedantic --std=gnu99 -g
PREFIX := /usr/local

all: mk_kern mk_scores kernify kern.o termk

install: $(PREFIX)/bin/termk

$(PREFIX)/bin/termk: termk
	install -D termk $(PREFIX)/bin/termk

termk: termk.c kern.h kern.o
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDFLAGS) kern.o -o $@ `sdl-config --cflags --libs` -lSDL_image

mk_scores: mk_scores.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDFLAGS) -o $@

kernify: kernify.c kern.h kern.o
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDFLAGS) kern.o -o $@ `sdl-config --cflags --libs` -lSDL_image

%: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDFLAGS) -o $@ `sdl-config --cflags --libs` -lSDL_image

%.o: %.c %.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@ `sdl-config --libs`
