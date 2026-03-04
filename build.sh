#!/bin/sh

set -xe

CC="${CC:-cc}"
COPT="-O2 -ffast-math"
CFLAGS="-g -std=c99 -Wall -Wextra -pedantic -pedantic-errors -Werror"

$CC $COPT $CFLAGS -o raymarcher raymarcher.c -lm
