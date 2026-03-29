#!/bin/bash
# Phase 8: Integration tests for VertexOS kernel
# Tests boot, build quality, and binary correctness via QEMU

set -e

QEMU=qemu-system-i386
IMAGE=build/os-image.bin
KERNEL=build/kernel.elf
BOOT_TIMEOUT=8

echo "=== VertexOS Phase 8 Integration Tests ==="
echo

if [ ! -f "$IMAGE" ]; then
    echo "ERROR: $IMAGE not found. Run 'make' first."
    exit 1
fi

TESTS_RUN=0
TESTS_PASSED=0
TESTS_FAILED=0

pass() {
    TESTS_RUN=$((TESTS_RUN + 1))
    TESTS_PASSED=$((TESTS_PASSED + 1))
    echo "  PASS: $1"
}

fail() {
    TESTS_RUN=$((TESTS_RUN + 1))
    TESTS_FAILED=$((TESTS_FAILED + 1))
    echo "  FAIL: $1"
    [ -n "$2" ] && echo "        $2"
}

# -------------------------------------------------------
echo "--- 1. Build Artifact Checks ---"

# Check image exists and is reasonable size
img_size=$(stat -c%s "$IMAGE" 2>/dev/null || echo 0)
if [ "$img_size" -gt 0 ]; then
    pass "OS image exists ($(du -h "$IMAGE" | cut -f1))"
else
    fail "OS image missing or empty"
fi

# Check kernel ELF exists
if [ -f "$KERNEL" ]; then
    kern_size=$(stat -c%s "$KERNEL")
    if [ "$kern_size" -gt 1024 ] && [ "$kern_size" -lt 200000 ]; then
        pass "Kernel ELF size reasonable ($kern_size bytes)"
    else
        fail "Kernel ELF size suspicious ($kern_size bytes)"
    fi
else
    fail "Kernel ELF not found"
fi

# Check boot sector magic number (last 2 bytes = 0x55AA)
boot_magic=$(xxd -s 510 -l 2 -p "$IMAGE" 2>/dev/null)
if [ "$boot_magic" = "55aa" ]; then
    pass "Boot sector magic (0x55AA) present"
else
    fail "Boot sector magic missing" "Got: $boot_magic"
fi

# -------------------------------------------------------
echo "--- 2. Kernel Binary Checks ---"

# Check that essential strings are compiled into the kernel
check_string() {
    if strings "$KERNEL" | grep -qi "$1" > /dev/null 2>&1; then
        pass "Kernel contains '$1'"
    else
        fail "Kernel missing string '$1'"
    fi
}

check_string "VertexOS"
check_string "help"
check_string "Type 'help'"
check_string "ls"
check_string "echo"
check_string "edit"

# -------------------------------------------------------
echo "--- 3. Boot Test (QEMU headless) ---"

# Capture serial output to file (VGA text mirrors to serial via display_put_char)
SERIAL_LOG=/tmp/vos_test_serial.log
rm -f "$SERIAL_LOG"

timeout "$BOOT_TIMEOUT" $QEMU \
    -drive file="$IMAGE",format=raw \
    -display none \
    -serial file:"$SERIAL_LOG" \
    -no-reboot \
    2>/dev/null || true

boot_output=""
[ -f "$SERIAL_LOG" ] && boot_output=$(cat "$SERIAL_LOG")

if echo "$boot_output" | grep -q "VertexOS"; then
    pass "Kernel boots to welcome banner"
else
    fail "No 'VertexOS' banner in boot output"
fi

if echo "$boot_output" | grep -q "help"; then
    pass "Help prompt visible at boot"
else
    fail "Help prompt not visible at boot"
fi

# Check for panic messages (should NOT appear)
if echo "$boot_output" | grep -qi "panic\|fault"; then
    fail "Panic/fault detected during boot" "$(echo "$boot_output" | grep -i 'panic\|fault' | head -1)"
else
    pass "No panics or faults during boot"
fi

rm -f "$SERIAL_LOG"

# -------------------------------------------------------
echo "--- 4. Compiler Warning Check ---"

warn_count=$(make -s clean > /dev/null 2>&1; make 2>&1 | grep -c "warning:" || true)
if [ "$warn_count" -le 3 ]; then
    pass "Acceptable warning count ($warn_count, threshold: 3)"
else
    fail "Too many compiler warnings ($warn_count)"
fi

# Rebuild if clean wiped it
make -s build/os-image.bin > /dev/null 2>&1

# -------------------------------------------------------
echo
echo "=== Results ==="
echo "Passed: $TESTS_PASSED / $TESTS_RUN"

if [ "$TESTS_FAILED" -eq 0 ]; then
    echo "Status: ALL PASSED"
    exit 0
else
    echo "Failed: $TESTS_FAILED"
    echo "Status: SOME FAILED"
    exit 1
fi
