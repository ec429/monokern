# Makefile for monokern
CC := gcc
CFLAGS := -Wall -Wextra -Werror -pedantic --std=gnu99 -g

all: mk_kern

%: %.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $< $(LDFLAGS) -o $@ `sdl-config --cflags --libs` -lSDL_image

%.o: %.c %.h
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@ `sdl-config --libs`
