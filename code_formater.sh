#!/bin/bash

dirs=(
    ./source/hal/
    ./source/utl/
    ./source/port/mac/
    ./source/port/stm32/
    ./test/utl/dbg/
    ./test/hal/cpu/
    ./test/hal/uart/
)

for dir in "${dirs[@]}"; do
    echo "Formatting files in $dir"
    find "$dir" -maxdepth 1 -type f \( -name "*.c" -o -name "*.h" \) -exec clang-format --verbose -i {} +
done

