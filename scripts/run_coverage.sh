#!/bin/bash
#
# DSV4L2 Coverage Test Runner
#
# Runs all test suites and collects coverage data for gcov/lcov analysis.
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo "╔════════════════════════════════════════════════════════╗"
echo "║         DSV4L2 Coverage Test Runner                   ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""

# Check if lcov is installed
if ! command -v lcov &> /dev/null; then
    echo -e "${YELLOW}Warning: lcov not installed. Install with:${NC}"
    echo "  Ubuntu/Debian: sudo apt-get install lcov"
    echo "  Fedora/RHEL:   sudo dnf install lcov"
    echo "  Arch Linux:    sudo pacman -S lcov"
    echo ""
fi

# Set library path for shared libraries
export LD_LIBRARY_PATH="$(pwd)/lib:$LD_LIBRARY_PATH"

# Change to tests directory
cd tests

# Test suite list
TESTS=(
    "test_basic"
    "test_profiles"
    "test_policy"
    "test_metadata"
    "test_runtime"
    "test_integration"
    "test_tpm"
)

total_tests=0
passed_tests=0
failed_tests=0

echo "Running test suites..."
echo ""

# Run each test
for test in "${TESTS[@]}"; do
    if [ ! -f "./$test" ]; then
        echo -e "${YELLOW}⊘ $test - NOT FOUND (skipped)${NC}"
        continue
    fi

    echo -n "Running $test... "
    total_tests=$((total_tests + 1))

    if ./"$test" > "/tmp/${test}.log" 2>&1; then
        echo -e "${GREEN}✓ PASSED${NC}"
        passed_tests=$((passed_tests + 1))
    else
        echo -e "${RED}✗ FAILED${NC}"
        echo "  Log: /tmp/${test}.log"
        failed_tests=$((failed_tests + 1))
    fi
done

echo ""
echo "╔════════════════════════════════════════════════════════╗"
echo "║              Coverage Test Summary                     ║"
echo "╚════════════════════════════════════════════════════════╝"
echo ""
echo "  Total Tests:   $total_tests"
echo -e "  ${GREEN}✓ Passed:      $passed_tests${NC}"
if [ $failed_tests -gt 0 ]; then
    echo -e "  ${RED}✗ Failed:      $failed_tests${NC}"
else
    echo "  ✗ Failed:      0"
fi
echo ""

if [ $failed_tests -eq 0 ]; then
    echo -e "  Status: ${GREEN}✓ ALL TESTS PASSED${NC}"
    echo ""
    echo "Coverage data files (.gcda) generated."
    echo "Run 'make coverage-report' to generate HTML report."
    exit 0
else
    echo -e "  Status: ${RED}✗ SOME TESTS FAILED${NC}"
    echo ""
    echo "Coverage data may be incomplete."
    exit 1
fi
