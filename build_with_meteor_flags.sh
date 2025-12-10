#!/bin/bash
# Build DSV4L2 with Meteor Lake optimization flags and IR support
# This script builds the IR-capable V4L2 stack with applicable Meteor Lake flags

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DSMIL_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
DSV4L2_DIR="$SCRIPT_DIR/DSV4L2"

echo "=========================================="
echo "DSV4L2 Build with Meteor Lake Flags"
echo "=========================================="
echo ""

# Check if submodule is initialized
if [ ! -d "$DSV4L2_DIR/.git" ]; then
    echo "Initializing DSV4L2 submodule..."
    cd "$DSMIL_ROOT"
    git submodule update --init --recursive tools/DSV4L2
fi

# Source Meteor Lake flags
if [ -f "$DSMIL_ROOT/METEOR_TRUE_FLAGS.sh" ]; then
    echo "Loading Meteor Lake optimization flags..."
    source "$DSMIL_ROOT/METEOR_TRUE_FLAGS.sh"
else
    echo "ERROR: METEOR_TRUE_FLAGS.sh not found at $DSMIL_ROOT/METEOR_TRUE_FLAGS.sh"
    exit 1
fi

# Use OPTIMAL + SECURITY flags for defensive V4L2 sensor workloads
# OPTIMAL preserves numerical precision, SECURITY adds hardening (stack protection, CFI, etc.)
CFLAGS_BUILD="${CFLAGS_OPTIMAL} ${CFLAGS_SECURITY} -I$DSV4L2_DIR/include -I/usr/include/libv4l2 -fPIC"
LDFLAGS_BUILD="${LDFLAGS_OPTIMAL} ${LDFLAGS_SECURITY} -lpthread"

# Build with TPM2 support (if libraries available)
HAVE_TPM2=1
if pkg-config --exists tss2-esys 2>/dev/null; then
    echo "TPM2 libraries found - enabling TPM2 support"
    LDFLAGS_BUILD="$LDFLAGS_BUILD $(pkg-config --libs tss2-esys 2>/dev/null || echo '')"
else
    echo "TPM2 libraries not found - building without TPM2 support"
    HAVE_TPM2=0
fi

echo ""
echo "Build configuration:"
echo "  Compiler: gcc"
echo "  TPM2 Support: $HAVE_TPM2"
echo "  IR Support: Enabled (via V4L2 profiles)"
echo "  Optimization: Meteor Lake OPTIMAL + SECURITY (defensive workload)"
echo ""

cd "$DSV4L2_DIR"

# Clean previous build
echo "Cleaning previous build..."
make clean > /dev/null 2>&1 || true

# Build
echo "Building DSV4L2..."
make all \
    HAVE_TPM2=$HAVE_TPM2 \
    CC=gcc \
    CFLAGS="$CFLAGS_BUILD" \
    LDFLAGS="$LDFLAGS_BUILD"

echo ""
echo "=========================================="
echo "Build completed successfully!"
echo "=========================================="
echo ""
echo "Output files:"
echo "  Library: $DSV4L2_DIR/lib/libdsv4l2.so"
echo "  Runtime: $DSV4L2_DIR/lib/libdsv4l2rt.a"
echo "  CLI Tool: $DSV4L2_DIR/bin/dsv4l2"
echo ""
echo "IR sensor profile available at:"
echo "  $DSV4L2_DIR/profiles/ir_sensor.yaml"
echo ""
echo "To use the library:"
echo "  export LD_LIBRARY_PATH=$DSV4L2_DIR/lib:\$LD_LIBRARY_PATH"
echo "  $DSV4L2_DIR/bin/dsv4l2 --help"
echo ""

