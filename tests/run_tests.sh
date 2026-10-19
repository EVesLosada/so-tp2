#!/usr/bin/env bash

set -euo pipefail

cd "$(dirname "$0")/.."

mkdir -p tmp

CC="${CC:-cc}"

CFLAGS=(
    -std=c99
    -Wall
    -Wextra
    -Werror
    -IKernel/include
    -IShared/include
)

run_test() {
    local name="$1"
    shift

    echo
    echo "=== $name ==="

    "$CC" "${CFLAGS[@]}" "$@" -o "tmp/$name"
    "./tmp/$name"
}

run_test test_memory_region \
    tests/test_memory_region.c \
    Kernel/memoryRegion.c

run_test test_process_manager \
    tests/test_process_manager.c \
    Kernel/processManager.c \
    Kernel/processTable.c

run_test test_process_real_freelist \
    tests/test_process_real_mm.c \
    Kernel/processManager.c \
    Kernel/processTable.c \
    Kernel/memoryManager.c

run_test test_process_real_buddy \
    tests/test_process_real_mm.c \
    Kernel/processManager.c \
    Kernel/processTable.c \
    Kernel/allocators/buddyMemoryManager.c

run_test test_process_args_freelist \
    tests/test_process_args.c \
    Kernel/processManager.c \
    Kernel/processTable.c \
    Kernel/memoryManager.c

run_test test_process_args_buddy \
    tests/test_process_args.c \
    Kernel/processManager.c \
    Kernel/processTable.c \
    Kernel/allocators/buddyMemoryManager.c

echo
echo "OK: todos los tests auxiliares superados"
