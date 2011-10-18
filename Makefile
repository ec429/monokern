# Makefile for monokern
CC := gcc
CFLAGS := -Wall -Wextra -Werror -pedantic --std=gnu99 -g

all: mk_kern mk_scores kern.o

mk_scores: mk_scores.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDFLAGS) -o $@

%: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDFLAGS) -o $@ `sdl-config --cflags --libs` -lSDL_image

%.o: %.c %.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@ `sdl-config --libs`
