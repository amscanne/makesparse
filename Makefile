#!/usr/bin/make -f

CC := gcc
RM := rm

makesparse: makesparse.c
	@$(CC) -O3 -o $@ $^

clean:
	@$(RM) makesparse
