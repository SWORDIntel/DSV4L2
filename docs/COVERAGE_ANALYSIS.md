# Code Coverage Analysis Guide

## Overview

The DSV4L2 project includes comprehensive code coverage analysis using **gcov** and **lcov** to measure test effectiveness and identify untested code paths.

## Features

- **Line coverage**: Tracks which lines of code are executed
- **Branch coverage**: Tracks which conditional branches are taken
- **Function coverage**: Tracks which functions are called
- **HTML reports**: Interactive coverage reports with source code highlighting
- **Automated testing**: Runs all test suites and collects coverage data

## Prerequisites

### Install lcov

```bash
# Ubuntu/Debian
sudo apt-get install lcov

# Fedora/RHEL
sudo dnf install lcov

# Arch Linux
sudo pacman -S lcov
```

### Install gcc (if not already installed)

Coverage analysis requires gcc with gcov support (included by default).

## Quick Start

### Generate Coverage Report

```bash
# Clean, build with coverage, run tests, generate HTML report
make coverage-report
```

This command will:
1. Clean all build artifacts
2. Rebuild with coverage instrumentation (`--coverage` flags)
3. Run all test suites
4. Generate HTML coverage report in `coverage_html/`

### View Report

```bash
# Open in browser
xdg-open coverage_html/index.html

# Or use your browser directly
firefox coverage_html/index.html
chromium coverage_html/index.html
```

## Manual Coverage Workflow

### 1. Build with Coverage

```bash
# Clean previous build
make clean

# Build with coverage instrumentation
make COVERAGE=1

# Build tests with coverage
make COVERAGE=1 -C tests
```

### 2. Run Tests

```bash
# Run coverage test script
./scripts/run_coverage.sh

# Or run tests manually
cd tests
export LD_LIBRARY_PATH=../lib:$LD_LIBRARY_PATH
./test_basic
./test_profiles
./test_policy
./test_metadata
./test_runtime
./test_integration
./test_tpm
```

### 3. Generate Coverage Data

```bash
# Capture coverage data
lcov --capture --directory . --output-file coverage.info --rc lcov_branch_coverage=1

# Filter out system headers
lcov --remove coverage.info '/usr/*' --output-file coverage.info

# Generate HTML report
genhtml coverage.info --output-directory coverage_html --branch-coverage
```

### 4. View Results

Open `coverage_html/index.html` in your browser.

## Coverage Targets

### make coverage

Builds with coverage, runs tests, generates `.gcda` files:

```bash
make coverage
```

**Output**: Coverage data files (`*.gcda`, `*.gcno`)

### make coverage-report

Complete workflow - builds, tests, and generates HTML report:

```bash
make coverage-report
```

**Output**: Interactive HTML report in `coverage_html/`

### make coverage-clean

Removes all coverage data files:

```bash
make coverage-clean
```

**Removes**: `*.gcda`, `*.gcno`, `*.gcov`, `coverage_html/`

## Understanding Coverage Reports

### File List View

The main page shows coverage statistics for each source file:

```
Directory                       Line Coverage    Function Coverage    Branch Coverage
src/                            87.3% (423/485)  92.1% (35/38)       78.4% (102/130)
  device.c                      91.2% (83/91)    100.0% (8/8)        82.1% (23/28)
  tempest.c                     88.5% (31/35)    100.0% (4/4)        75.0% (9/12)
  buffer.c                      85.7% (72/84)    87.5% (7/8)         76.9% (20/26)
  ...
```

### Source Code View

Click on any file to see annotated source code:

- **Green lines**: Executed (covered)
- **Red lines**: Not executed (not covered)
- **Orange branches**: Partially covered conditionals
- **Execution counts**: Number of times each line was executed

### Coverage Metrics

- **Line coverage**: Percentage of executable lines hit by tests
- **Function coverage**: Percentage of functions called by tests
- **Branch coverage**: Percentage of conditional branches taken

## Coverage Goals

### Target Metrics

| Component | Line Coverage | Branch Coverage | Function Coverage |
|-----------|--------------|----------------|-------------------|
| Core Library | >85% | >75% | >90% |
| Runtime | >80% | >70% | >85% |
| Profiles | >90% | >80% | >95% |
| Policy Layer | >90% | >85% | >95% |
| Metadata Parser | >85% | >80% | >90% |

### Current Status

Run `make coverage-report` to see current coverage metrics.

## Interpreting Results

### High Coverage (>90%)

- Code is well-tested
- Most code paths exercised
- Good confidence in correctness

### Medium Coverage (70-90%)

- Adequate testing
- Some edge cases may be missed
- Consider adding targeted tests

### Low Coverage (<70%)

- Insufficient testing
- Many code paths untested
- High risk of undetected bugs

### Uncovered Code

Common reasons for uncovered code:

1. **Error handling**: Edge cases that are hard to trigger
2. **Hardware dependencies**: Code requiring specific devices
3. **Dead code**: Unreachable or obsolete code (should be removed)
4. **Initialization**: One-time setup code
5. **Defensive checks**: Assertions that should never trigger

## Test Suite Coverage

### Current Test Suites

1. **test_basic** - Core API functionality
2. **test_profiles** - Device profile loading
3. **test_policy** - DSMIL/THREATCON enforcement
4. **test_metadata** - KLV and IR metadata parsing
5. **test_runtime** - Event buffer and sinks
6. **test_integration** - End-to-end workflows
7. **test_tpm** - TPM2 signing (requires hardware)

### Coverage by Test Suite

To measure individual test coverage:

```bash
# Build with coverage
make clean && make COVERAGE=1 && make COVERAGE=1 -C tests

# Run single test
cd tests
export LD_LIBRARY_PATH=../lib:$LD_LIBRARY_PATH
./test_metadata

# Generate report for this test only
lcov --capture --directory .. --output-file metadata_coverage.info
genhtml metadata_coverage.info --output-directory metadata_coverage_html
```

## CI/CD Integration

### GitHub Actions

```yaml
name: Coverage

on: [push, pull_request]

jobs:
  coverage:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y lcov

      - name: Build and test with coverage
        run: make coverage-report

      - name: Upload coverage to Codecov
        uses: codecov/codecov-action@v3
        with:
          files: ./coverage_html/coverage.info
          fail_ci_if_error: true
```

### GitLab CI

```yaml
coverage:
  stage: test
  script:
    - apt-get update && apt-get install -y lcov
    - make coverage-report
  coverage: '/lines\.*: \d+\.\d+\%/'
  artifacts:
    paths:
      - coverage_html/
    expire_in: 1 week
```

## Troubleshooting

### "lcov: command not found"

**Solution**: Install lcov:
```bash
sudo apt-get install lcov  # Ubuntu/Debian
sudo dnf install lcov       # Fedora/RHEL
```

### "No coverage data found"

**Cause**: Tests didn't run or `.gcda` files not generated

**Solution**: Ensure you build with `COVERAGE=1`:
```bash
make clean
make COVERAGE=1
make COVERAGE=1 -C tests
./scripts/run_coverage.sh
```

### "Permission denied" errors

**Cause**: Coverage data files owned by different user

**Solution**: Clean and rebuild:
```bash
make coverage-clean
make coverage-report
```

### Low coverage for certain files

**Cause**: Tests don't exercise those code paths

**Solution**: Add targeted tests or mark code as tested via integration tests

### ".gcda: File format not recognized"

**Cause**: Mixing coverage data from different compiler versions

**Solution**: Clean all coverage data:
```bash
make coverage-clean
make coverage-report
```

## Advanced Usage

### Exclude Files from Coverage

Edit `.lcovrc` or use `--remove` flag:

```bash
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info \
    '/usr/*' \
    '*/tests/*' \
    '*/examples/*' \
    --output-file coverage_filtered.info
```

### Merge Coverage from Multiple Runs

```bash
# Run different test suites
./test_basic
./test_profiles
./test_policy

# Capture each
lcov --capture --directory . --output-file coverage1.info
lcov --capture --directory . --output-file coverage2.info

# Merge
lcov --add-tracefile coverage1.info \
     --add-tracefile coverage2.info \
     --output-file coverage_merged.info

# Generate report
genhtml coverage_merged.info --output-directory coverage_html
```

### Branch Coverage Only

To focus on branch coverage:

```bash
lcov --rc lcov_branch_coverage=1 \
     --capture --directory . --output-file coverage.info

genhtml --branch-coverage \
        --highlight --legend \
        coverage.info --output-directory coverage_html
```

### Diff Coverage

Compare coverage between branches:

```bash
# Generate baseline
git checkout main
make coverage-report
mv coverage_html coverage_baseline

# Generate current
git checkout feature-branch
make coverage-report

# Compare (use lcov_cobertura or similar tool)
```

## Coverage Best Practices

### 1. Run Coverage Regularly

- Run coverage after every significant change
- Track coverage trends over time
- Set coverage gates in CI/CD

### 2. Focus on Critical Paths

- Prioritize testing security-critical code
- Test error handling paths
- Test boundary conditions

### 3. Don't Chase 100%

- 100% coverage doesn't guarantee correctness
- Some code is unreachable or defensive
- Focus on meaningful tests, not coverage numbers

### 4. Review Uncovered Code

- Identify why code is uncovered
- Add tests or remove dead code
- Document intentionally untested code

### 5. Use Coverage to Find Gaps

- Coverage shows what's tested, not if tests are good
- Use coverage to identify missing test scenarios
- Combine with mutation testing for better confidence

## Performance Impact

### Build Time

- Coverage build: ~10-20% slower than regular build
- Instrumentation adds extra code

### Runtime

- Coverage tests: ~5-15% slower than regular tests
- `.gcda` file I/O during execution

### Disk Usage

- `.gcno` files (compile-time): ~1-2 MB
- `.gcda` files (runtime): ~500 KB - 2 MB
- HTML report: ~5-10 MB

## References

- **gcov**: https://gcc.gnu.org/onlinedocs/gcc/Gcov.html
- **lcov**: http://ltp.sourceforge.net/coverage/lcov.php
- **genhtml**: http://ltp.sourceforge.net/coverage/lcov/genhtml.1.php

## See Also

- `TESTING.md` - Test suite documentation
- `VALIDATION_REPORT.md` - Production validation results
- `Makefile` - Build system with coverage targets
- `scripts/run_coverage.sh` - Coverage test runner
