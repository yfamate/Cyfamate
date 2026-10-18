#!/bin/sh
set -e
CXX=${CXX:-clang++}
OUT=$1; shift
mkdir -p build
python tools-cyfamate/amalgamate.py source build/cyfamate.cpp
$CXX -std=c++20 -O3 -march=native -static -DCYF_FT_WIDTH=1536 -DCYF_H1=15 -DCYF_THREAT -Wno-deprecated-enum-enum-conversion -Wl,--stack,134217728 -Isource/cyfamate -Isource "$@" build/cyfamate.cpp -o "$OUT" -lpthread -lws2_32