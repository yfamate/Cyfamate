#!/bin/sh
# usage: build.sh <out.exe> [extra flags]   (run from the repo root; CXX = clang++ 18+ / llvm-mingw)
set -e
CXX=${CXX:-clang++}
OUT=$1; shift
python tools-cyfamate/amalgamate.py source build/cyfamate.cpp "$CXX -E -std=c++20 -march=native -DCYF_FT_WIDTH=1536 -DCYF_H1=15 -DCYF_THREAT -Isource/cyfamate $*" > /dev/null
$CXX -std=c++20 -O3 -march=native -static -DCYF_FT_WIDTH=1536 -DCYF_H1=15 -DCYF_THREAT -Wno-deprecated-enum-enum-conversion -Wl,--stack,134217728 -Isource/cyfamate "$@" build/cyfamate.cpp -o "$OUT" -lpthread
