#!/bin/bash

# strict mode
set -euo pipefail
IFS=$'\n\t'

SCRIPT_DIR=$(cd $(dirname "${BASH_SOURCE[0]}") && pwd)
SCRIPT_OUTPUT_DIR="$SCRIPT_DIR/outputs"

mkdir -p $SCRIPT_OUTPUT_DIR

output_filename="$SCRIPT_OUTPUT_DIR/$(basename $1)"
output_filestem=${output_filename%.*}

clang -Wall -Wextra -c -emit-llvm -O0 $1 -o "$output_filestem.bc"
clang -Wall -Wextra -c -S -emit-llvm -O0 $1 -o "$output_filestem.ll"
llvm2kittel --dump-ll --no-slicing --eager-inline --t2 "$output_filestem.bc" > "$output_filestem.t2"