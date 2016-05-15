#!/bin/bash
mkdir -p build-clang-analyze/reports
cd build-clang-analyze
cmake -DCMAKE_C_COMPILER=/usr/share/clang/scan-build-3.5/ccc-analyzer $* ..
scan-build -o ./reports --keep-empty make
