#!/bin/bash

gcc \
	-o treeex \
	-ggdb -std=c11 -pedantic \
	-Wall \
	-D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 \
	*.c -lkernel32
