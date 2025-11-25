#!/bin/bash
#
# Generate AFL seed corpus for KLV fuzzing
#
# KLV format: 16-byte Universal Label + BER length + value
#

set -e

cd "$(dirname "$0")"

echo "Generating KLV seed corpus..."

# Seed 1: Minimal valid KLV (16-byte key + 1-byte length + 1-byte value)
printf '\x06\x0e\x2b\x34\x02\x0b\x01\x01\x0e\x01\x03\x01\x01\x00\x00\x00\x01\x42' > klv_minimal.bin
echo "  Created klv_minimal.bin (18 bytes)"

# Seed 2: Small KLV with 10-byte value
printf '\x06\x0e\x2b\x34\x02\x0b\x01\x01\x0e\x01\x03\x01\x01\x00\x00\x00\x0a\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a' > klv_small.bin
echo "  Created klv_small.bin (27 bytes)"

# Seed 3: Multiple KLV items
printf '\x06\x0e\x2b\x34\x02\x0b\x01\x01\x0e\x01\x03\x01\x01\x00\x00\x00\x04\xaa\xbb\xcc\xdd' > klv_multi.bin
printf '\x06\x0e\x2b\x34\x02\x0b\x01\x01\x0e\x01\x03\x01\x02\x00\x00\x00\x02\x11\x22' >> klv_multi.bin
echo "  Created klv_multi.bin (38 bytes - 2 items)"

# Seed 4: BER long form length (length > 127)
printf '\x06\x0e\x2b\x34\x02\x0b\x01\x01\x0e\x01\x03\x01\x03\x00\x00\x00\x81\x80' > klv_longlen.bin
# Add 128 bytes of data
for i in {1..128}; do
    printf '\x42' >> klv_longlen.bin
done
echo "  Created klv_longlen.bin (146 bytes - long BER length)"

# Seed 5: Empty value
printf '\x06\x0e\x2b\x34\x02\x0b\x01\x01\x0e\x01\x03\x01\x04\x00\x00\x00\x00' > klv_empty.bin
echo "  Created klv_empty.bin (17 bytes - empty value)"

# Seed 6: Larger payload
printf '\x06\x0e\x2b\x34\x02\x0b\x01\x01\x0e\x01\x03\x01\x05\x00\x00\x00\x82\x01\x00' > klv_large.bin
# Add 256 bytes of varied data
for i in {0..255}; do
    printf "\\x$(printf '%02x' $i)" >> klv_large.bin
done
echo "  Created klv_large.bin (275 bytes - 256-byte payload)"

echo ""
echo "Seed corpus generated with 6 files"
echo "Total size: $(du -sh . | cut -f1)"
