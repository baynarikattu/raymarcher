#!/bin/sh

set -e

CC="${CC:-cc}"
COPT="-O2 -ffast-math"
CFLAGS="-g -std=c99 -Wall -Wextra -pedantic -pedantic-errors -Werror"

ODIN="${ODIN:-odin}"

try() {
  exec=$1

  if ! command -v "$exec" >/dev/null 2>&1; then
    echo "$exec command is not available. Skipping..."
    return 1
  fi

  return 0
}

if try "$CC"; then
  # shellcheck disable=SC2086
  (set -x; "$CC" $COPT $CFLAGS -o raymarcher_c raymarcher.c -lm)
fi

if try "$ODIN"; then
  (set -x; $ODIN build . -out:raymarcher_odin -o:speed)
fi
