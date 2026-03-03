#!/bin/sh

set -xe

CC="${CC:-cc}"
CFLAGS="-g -O3 -std=c99 -Wall -Wextra -pedantic -pedantic-errors -Werror"

$CC $CFLAGS -o raymarcher raymarcher.c -lm
