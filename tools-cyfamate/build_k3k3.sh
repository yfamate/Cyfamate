#!/bin/sh
# k3k3-compatible build (standard YaneuraOu SFNN_halfka2_1024_7_64_k3k3 nets), for measurements. usage: build_k3k3.sh <out.exe>
set -e
CXX=${CXX:-clang++}
OUT=$1; shift
python tools-cyfamate/amalgamate.py source build/cyfamate_k3k3.cpp "$CXX -E -std=c++20 -march=native -DCYF_ARCH_K3K3 -Isource/cyfamate $*" > /dev/null
$CXX -std=c++20 -O3 -march=native -static -DCYF_ARCH_K3K3 -Wno-deprecated-enum-enum-conversion -Wl,--stack,134217728 -Isource/cyfamate "$@" build/cyfamate_k3k3.cpp -o "$OUT" -lpthread
