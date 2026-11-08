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
    tests/process_pipe_cleanup_stub.c \
    Kernel/processTable.c

run_test test_process_real_freelist \
    tests/test_process_real_mm.c \
    Kernel/processManager.c \
    tests/process_pipe_cleanup_stub.c \
    Kernel/processTable.c \
    Kernel/memoryManager.c

run_test test_process_real_buddy \
    tests/test_process_real_mm.c \
    Kernel/processManager.c \
    tests/process_pipe_cleanup_stub.c \
    Kernel/processTable.c \
    Kernel/allocators/buddyMemoryManager.c

run_test test_process_args_freelist \
    tests/test_process_args.c \
    Kernel/processManager.c \
    tests/process_pipe_cleanup_stub.c \
    Kernel/processTable.c \
    Kernel/memoryManager.c

run_test test_process_args_buddy \
    tests/test_process_args.c \
    Kernel/processManager.c \
    tests/process_pipe_cleanup_stub.c \
    Kernel/processTable.c \
    Kernel/allocators/buddyMemoryManager.c

run_test test_process_table \
  tests/test_process_table.c \
  Kernel/processTable.c \
  Kernel/processManager.c \
    tests/process_pipe_cleanup_stub.c \
  Kernel/memoryManager.c

run_test test_scheduler \
  tests/test_scheduler.c \
  Kernel/processTable.c \
  Kernel/processManager.c \
    tests/process_pipe_cleanup_stub.c \
  Kernel/memoryManager.c

run_test test_atomic_ops \
    tests/test_atomic_ops.c

run_test test_semaphore \
    tests/test_semaphore.c \
    Kernel/semaphore.c

echo
run_test test_semaphore_management \
    tests/test_semaphore_management.c \
    Kernel/semaphore.c

run_test test_semaphore_wait \
    tests/test_semaphore_wait.c \
    Kernel/semaphore.c \
    Kernel/semaphoreSync.c

run_test test_semaphore_post \
    tests/test_semaphore_post.c \
    Kernel/semaphore.c \
    Kernel/semaphoreSync.c

run_test test_semaphore_cancel \
    tests/test_semaphore_cancel.c \
    Kernel/semaphore.c

run_test test_io_descriptors \
    tests/test_io_descriptors.c \
    tests/keyboard_io_test_stubs.c \
    Kernel/ioDescriptor.c

run_test test_keyboard_ctrl_c \
    tests/test_keyboard_ctrl_c.c \
    Kernel/drivers/keyboard.c

echo "OK: todos los tests auxiliares superados"


run_test test_pipe \
    tests/test_pipe.c \
    tests/pipe_descriptor_stubs.c \
    Kernel/pipe.c

run_test test_pipe_descriptors \
    tests/test_pipe_descriptors.c \
    Kernel/pipe.c

run_test test_pipe_wait_queue \
    tests/test_pipe_wait_queue.c \
    tests/pipe_descriptor_stubs.c \
    Kernel/pipe.c

run_test test_pipe_wakeup \
    tests/test_pipe_wakeup.c \
    tests/pipe_descriptor_stubs.c \
    Kernel/pipe.c \
    Kernel/pipeSync.c

run_test test_pipe_read \
    tests/test_pipe_read.c \
    tests/pipe_descriptor_stubs.c \
    Kernel/pipe.c \
    Kernel/pipeSync.c \
    Kernel/pipeIO.c

run_test test_pipe_write \
    tests/test_pipe_write.c \
    tests/pipe_descriptor_stubs.c \
    Kernel/pipe.c \
    Kernel/pipeSync.c \
    Kernel/pipeIO.c

run_test test_pipe_close \
    tests/test_pipe_close.c \
    Kernel/pipe.c \
    Kernel/pipeSync.c \
    Kernel/pipeClose.c

run_test test_pipe_integration \
    tests/test_pipe_integration.c \
    tests/keyboard_io_test_stubs.c \
    Kernel/ioDescriptor.c \
    Kernel/pipe.c \
    Kernel/pipeIO.c \
    Kernel/pipeSync.c \
    Kernel/pipeClose.c

run_test test_pipe_process_lifecycle \
    tests/test_pipe_process_lifecycle.c \
    tests/keyboard_io_test_stubs.c \
    Kernel/processManager.c \
    Kernel/processTable.c \
    Kernel/ioDescriptor.c \
    Kernel/pipe.c \
    Kernel/pipeIO.c \
    Kernel/pipeSync.c \
    Kernel/pipeClose.c

run_test test_shell_parser \
    tests/test_shell_parser.c \
    Userland/SampleCodeModule/shellParser.c \
    -IUserland/SampleCodeModule/include

(
    # Usar primero los headers de Userland para este test.
    CFLAGS=(
        -std=c99
        -Wall
        -Wextra
        -Werror
        -IUserland/SampleCodeModule/include
        -IKernel/include
        -IShared/include
    )

    run_test test_shell_programs \
        tests/test_shell_programs.c \
        Userland/SampleCodeModule/shellPrograms.c \
        Userland/SampleCodeModule/wc.c \
        Userland/SampleCodeModule/filter.c
)
