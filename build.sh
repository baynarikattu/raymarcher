#!/bin/sh

set -xe

CC="${CC:-cc}"
COPT="-O2 -ffast-math"
CFLAGS="-g -std=c99 -Wall -Wextra -pedantic -pedantic-errors -Werror"

$CC $COPT $CFLAGS -o raymarcher_c raymarcher.c -lm

odin build . -out:raymarcher_odin -o:speed
