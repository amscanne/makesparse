#!/usr/bin/make -f

CC := gcc
RM := rm

makesparse: makesparse.c
	@$(CC) -o $@ $^

clean:
	@$(RM) makesparse
