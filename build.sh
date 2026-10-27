#!/bin/bash

clang -O2 -g -target bpf -Wall -Wextra -Werror  -c src/synfin.c -o synfin.o