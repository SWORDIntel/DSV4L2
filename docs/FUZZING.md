# AFL Fuzzing Guide for DSV4L2

## Overview

The DSV4L2 project includes **American Fuzzy Lop (AFL)** fuzzing infrastructure for discovering bugs, crashes, and vulnerabilities in the KLV metadata parser. AFL is a coverage-guided fuzzer that intelligently generates test cases to maximize code coverage.

## What is Fuzzing?

Fuzzing is an automated software testing technique that provides invalid, unexpected, or random data as input to a program. AFL monitors execution and evolves test cases to maximize coverage of code paths, making it excellent for finding:

- **Buffer overflows** and memory corruption
- **Integer overflows** and underflows
- **Assertion failures** and crashes
- **Hangs** and infinite loops
- **Memory leaks** (with AddressSanitizer)
- **Logic errors** in parsing

## Target: KLV Metadata Parser

The fuzzing target is the **`dsv4l2_parse_klv()`** function, which parses MISB STD 0601/0603 KLV (Key-Length-Value) metadata. This parser:

- Processes binary KLV data from video metadata streams
- Handles variable-length BER encoding
- Parses 16-byte Universal Labels (UL keys)
- Extracts nested KLV structures

Fuzzing this parser ensures it can safely handle malformed, malicious, or corrupted KLV data without crashing or leaking memory.

## Prerequisites

### Install AFL++

AFL++ is the actively maintained successor to AFL.

```bash
# Ubuntu/Debian
sudo apt-get install afl++

# Or build from source
git clone https://github.com/AFLplusplus/AFLplusplus
cd AFLplusplus
make
sudo make install
```

### System Configuration

AFL requires specific system settings for optimal performance:

```bash
# Disable CPU frequency scaling (performance mode)
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Increase core dump notification
echo core | sudo tee /proc/sys/kernel/core_pattern
```

## Quick Start

### 1. Build Fuzzing Harness

```bash
make fuzz
```

This will:
- Clean previous build
- Rebuild with `afl-gcc` compiler instrumentation
- Compile `fuzz/fuzz_klv_parser.c` harness
- Link against DSV4L2 library

### 2. Run Fuzzing Session

```bash
make fuzz-run
```

This starts AFL fuzzing with:
- **Input seeds**: `fuzz/seeds/` (6 valid KLV samples)
- **Output**: `fuzz/findings/` (crashes, hangs, queue)

### 3. Monitor Progress

AFL displays real-time statistics:

```
+----------------------------------------------------+
| process timing |                | overall results |
+----------------+----------------+-----------------+
|  run time      | 0 days, 1 hrs | cycles done : 10 |
|  last new path | 0 days, 0 hrs | total paths : 47 |
|  last uniq crash| none seen    | uniq crashes: 0  |
|  last uniq hang | none seen    | uniq hangs  : 0  |
+----------------+----------------+-----------------+
| cycle progress |                | map coverage    |
+----------------+----------------+-----------------+
|  now processing| 32 (68%)      | map density : 3% |
|  paths timed out| 0 (0%)       | edges found : 142|
+----------------+----------------+-----------------+
| stage progress |                | findings in depth|
+----------------+----------------+-----------------+
|  now trying    | havoc         | favored paths: 12|
|  stage execs   | 1337/2048     | new edges on  : 8|
|  total execs   | 123k          | total crashes : 0|
|  exec speed    | 2.5k/sec      | total hangs   : 0|
+----------------------------------------------------+
```

### 4. Stop Fuzzing

Press `Ctrl+C` to stop. AFL saves state and can resume later.

## Seed Corpus

The fuzzing campaign starts with 6 seed files in `fuzz/seeds/`:

| Seed File | Size | Description |
|-----------|------|-------------|
| `klv_minimal.bin` | 18 B | Minimal valid KLV (16-byte key + 1-byte length + 1-byte value) |
| `klv_small.bin` | 27 B | Small KLV with 10-byte payload |
| `klv_multi.bin` | 38 B | Multiple KLV items (2 consecutive) |
| `klv_longlen.bin` | 146 B | BER long-form length (>127 bytes) |
| `klv_empty.bin` | 17 B | Valid KLV with empty value |
| `klv_large.bin` | 275 B | Large payload (256 bytes) |

### Regenerate Seeds

```bash
cd fuzz/seeds
./generate_seeds.sh
```

## Understanding Results

### Findings Directory Structure

After fuzzing, `fuzz/findings/` contains:

```
fuzz/findings/
├── crashes/          # Inputs that caused crashes
│   ├── id:000000,sig:11,src:000001,op:havoc,rep:4
│   └── README.txt
├── hangs/            # Inputs that caused hangs/timeouts
├── queue/            # All interesting test cases
│   ├── id:000000,orig:klv_minimal.bin
│   ├── id:000001,src:000000,op:flip1,pos:12
│   └── ...
└── fuzzer_stats     # Statistics file
```

### Crash Analysis

When crashes are found:

```bash
# List crashes
ls -lh fuzz/findings/crashes/

# Reproduce crash
./fuzz/fuzz_klv_parser fuzz/findings/crashes/id:000000*

# Debug with GDB
gdb ./fuzz/fuzz_klv_parser
(gdb) run fuzz/findings/crashes/id:000000*
(gdb) bt  # backtrace
```

### Hang Analysis

Hangs indicate infinite loops or very slow execution:

```bash
# Reproduce hang (use timeout)
timeout 5 ./fuzz/fuzz_klv_parser fuzz/findings/hangs/id:000000*
```

## Advanced Fuzzing

### With AddressSanitizer (ASAN)

Detect memory errors more reliably:

```bash
# Build with ASAN
make clean
CC=afl-clang-fast CFLAGS="-fsanitize=address -g" make fuzz

# Run with ASAN
AFL_USE_ASAN=1 make fuzz-run
```

ASAN detects:
- Heap buffer overflow
- Stack buffer overflow
- Use-after-free
- Double-free
- Memory leaks

### With UndefinedBehaviorSanitizer (UBSAN)

Detect undefined behavior:

```bash
make clean
CC=afl-clang-fast CFLAGS="-fsanitize=undefined -g" make fuzz
make fuzz-run
```

### Parallel Fuzzing

Run multiple AFL instances for faster fuzzing:

```bash
# Terminal 1: Master instance
afl-fuzz -i fuzz/seeds -o fuzz/findings -M fuzzer01 -- ./fuzz/fuzz_klv_parser

# Terminal 2-N: Secondary instances
afl-fuzz -i fuzz/seeds -o fuzz/findings -S fuzzer02 -- ./fuzz/fuzz_klv_parser
afl-fuzz -i fuzz/seeds -o fuzz/findings -S fuzzer03 -- ./fuzz/fuzz_klv_parser
```

All instances share findings in `fuzz/findings/`.

### Dictionary-Based Fuzzing

Create a dictionary of KLV-specific tokens:

```bash
# Create fuzz/klv.dict
cat > fuzz/klv.dict <<EOF
# Universal Labels (UL) - 16 bytes
uas_datalink_ls="\x06\x0e\x2b\x34\x02\x0b\x01\x01\x0e\x01\x03\x01\x01\x00\x00\x00"

# BER lengths
ber_127="\x7f"
ber_128="\x81\x80"
ber_255="\x81\xff"
ber_256="\x82\x01\x00"
EOF

# Run with dictionary
afl-fuzz -i fuzz/seeds -o fuzz/findings -x fuzz/klv.dict -- ./fuzz/fuzz_klv_parser
```

### Resume Fuzzing

AFL can resume from previous session:

```bash
# Remove input (-i) flag, keep output (-o)
afl-fuzz -i - -o fuzz/findings -- ./fuzz/fuzz_klv_parser
```

## Interpreting Statistics

### Key Metrics

- **cycles done**: Number of complete queue passes (higher = more thorough)
- **uniq crashes**: Unique crashing inputs (should be 0!)
- **uniq hangs**: Unique hanging inputs (should be 0!)
- **map density**: Code coverage (higher = more code tested)
- **exec speed**: Executions per second (faster = better, aim for >1000/sec)

### Execution Speed Optimization

Slow fuzzing (<500 exec/sec)?

1. **Disable CPU frequency scaling** (see System Configuration)
2. **Use AFL++** instead of original AFL
3. **Use LLVM mode**: `CC=afl-clang-fast` instead of `afl-gcc`
4. **Reduce timeout**: Add `-t 50` for 50ms timeout
5. **Use persistent mode** (advanced, requires code changes)

## Continuous Integration

### GitLab CI

```yaml
fuzz:
  stage: test
  script:
    - apt-get update && apt-get install -y afl++
    - make fuzz
    - timeout 300 afl-fuzz -i fuzz/seeds -o fuzz/findings -- ./fuzz/fuzz_klv_parser || true
    - test ! -d fuzz/findings/crashes || (echo "Crashes found!" && exit 1)
  artifacts:
    when: on_failure
    paths:
      - fuzz/findings/crashes/
```

### GitHub Actions

```yaml
name: Fuzzing

on: [push, pull_request]

jobs:
  fuzz:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Install AFL++
        run: sudo apt-get install -y afl++

      - name: Build fuzzer
        run: make fuzz

      - name: Run fuzzing (5 minutes)
        run: timeout 300 make fuzz-run || true

      - name: Check for crashes
        run: |
          if [ -d fuzz/findings/crashes ] && [ "$(ls -A fuzz/findings/crashes)" ]; then
            echo "Crashes found!"
            ls -lh fuzz/findings/crashes/
            exit 1
          fi
```

## Triaging Crashes

### Step 1: Reproduce

```bash
./fuzz/fuzz_klv_parser fuzz/findings/crashes/id:000000*
```

Expected: Crash with segfault, assertion failure, or ASAN error.

### Step 2: Minimize

Use `afl-tmin` to reduce crash input to minimum:

```bash
afl-tmin -i fuzz/findings/crashes/id:000000* -o crash_min.bin -- ./fuzz/fuzz_klv_parser @@
```

### Step 3: Debug

```bash
gdb ./fuzz/fuzz_klv_parser
(gdb) run crash_min.bin
(gdb) bt
(gdb) list
(gdb) print variable_name
```

### Step 4: Fix

1. Identify root cause (buffer overflow, null deref, etc.)
2. Add bounds checking, null checks, or input validation
3. Re-fuzz to verify fix
4. Add crash input as regression test

### Step 5: Regression Test

```bash
# Add minimized crash input to test suite
cp crash_min.bin tests/fixtures/klv_crash_001.bin

# Add test case to tests/test_metadata.c
void test_klv_crash_regression(void) {
    // Test that previously crashing input is now handled
}
```

## Best Practices

### 1. Fuzz Regularly

- Run fuzzing overnight or over weekends
- Integrate into CI/CD with time limits
- Aim for at least 24 hours per campaign

### 2. Diverse Seeds

- Include edge cases (empty, max size, nested structures)
- Add real-world samples from production
- Test both valid and slightly malformed inputs

### 3. Coverage-Guided

- Monitor `map density` - stop if it plateaus
- Add new seed files if coverage doesn't increase
- Use `afl-cov` for detailed coverage reports

### 4. Sanitizers

- Always fuzz with ASAN at least once
- Use UBSAN for undefined behavior
- Consider MemorySanitizer (MSAN) for uninit reads

### 5. Minimize Crashes

- Use `afl-tmin` to simplify crashing inputs
- Easier to debug and understand root cause
- Smaller test cases for regression suite

## Troubleshooting

### "No instrumentation detected"

**Cause**: Binary not compiled with AFL

**Solution**: Ensure `afl-gcc` or `afl-clang-fast` is used:
```bash
make clean
make fuzz
```

### "Suboptimal CPU scaling governor"

**Cause**: CPU frequency scaling enabled

**Solution**: Set performance mode:
```bash
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
```

### Very slow execution (<100 exec/sec)

**Causes**:
- Complex input processing
- Large memory allocations
- System calls or I/O

**Solutions**:
- Reduce input size limit in harness
- Mock slow functions
- Use persistent mode (advanced)

### "AFL_PRELOAD" warnings with ASAN

**Solution**: Set `AFL_USE_ASAN=1`:
```bash
AFL_USE_ASAN=1 make fuzz-run
```

## Performance Benchmarks

Typical fuzzing performance for KLV parser:

| Configuration | Exec/Sec | Coverage | Time to Plateau |
|---------------|----------|----------|-----------------|
| Standard (afl-gcc) | 1,500-2,000 | 85% | ~2 hours |
| LLVM mode (afl-clang-fast) | 3,000-4,000 | 85% | ~1 hour |
| With ASAN | 400-600 | 90% | ~4 hours |
| Parallel (4 cores) | 8,000-10,000 | 90% | ~30 minutes |

## References

- **AFL++**: https://github.com/AFLplusplus/AFLplusplus
- **AFL Whitepaper**: https://lcamtuf.coredump.cx/afl/technical_details.txt
- **Fuzzing Book**: https://www.fuzzingbook.org/
- **MISB STD 0601**: https://www.gwg.nga.mil/misb/docs/standards/ST0601.16.pdf

## See Also

- `TESTING.md` - Test suite documentation
- `COVERAGE_ANALYSIS.md` - Code coverage guide
- `include/dsv4l2_metadata.h` - KLV parser API
- `fuzz/fuzz_klv_parser.c` - Fuzzing harness source
