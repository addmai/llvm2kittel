#!/bin/bash

# Test of conversion from .c into .ll, .bc and .t2
# This takes a C program as $1 argument.

# strict mode
set -euo pipefail
IFS=$'\n\t'

SCRIPT_DIR=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)

# Prepare the directory to store resultant files
SCRIPT_OUTPUT_DIR="$SCRIPT_DIR/outputs"
mkdir -p $SCRIPT_OUTPUT_DIR

# The filename for output files
output_filename="$SCRIPT_OUTPUT_DIR/$(basename $1)"
output_filestem=${output_filename%.*}

# Convert .c into .bc(binary) and .ll(assembly, just for visualizasion)
clang -Wall -Wextra -c -emit-llvm -O0 $1 -o "$output_filestem.bc"
clang -Wall -Wextra -c -S -emit-llvm -O0 $1 -o "$output_filestem.ll"

# Apply -tailcallelim
opt -tailcallelim "$output_filestem.bc" -o "${output_filestem}_opt.bc"
opt -tailcallelim "$output_filestem.bc" -S -o "${output_filestem}_opt.ll"

# Copy .bc file as the input for llvm2kittel
cp "${output_filestem}_opt.bc" "${output_filestem}_opt_tmp.bc"

# Convert .bc into .t2 (this also outputs two .ll files as intermediate results)
# llvm2kittel --dump-ll --no-slicing --eager-inline --t2 "${output_filestem}.bc" > "$output_filestem.t2" # test without tailcallToLoop conversion
llvm2kittel --dump-ll --no-slicing --eager-inline --t2 "${output_filestem}_opt_tmp.bc" > "${output_filestem}_opt.t2"