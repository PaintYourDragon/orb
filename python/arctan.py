# Generate an arctangent-like table for converting fixed point coordinates
# to polar longitude values in the range 0 to 16383 (for the 1/4 of sphere
# generated here; this is rotated and offsets added for different quadrants).
# Run with: python arctan.py | clang-format > arctan.h
# Table size can be adjusted to balance size vs precision; the default 8
# used here (yielding a 256x256 table, or 512x512 across the whole sphere)
# is adequate for the RP2040 sphere demos. A likely optimization is to extend
# both axes to range(size * 2) and eliminate the distinct quadrant handling
# in the C code. Such table would be enormous though, like half a megabyte.

import math

bits = 8
size = 1 << bits

print("#define ARCTAN_BITS", bits)
print("const uint16_t arctan[1 << ARCTAN_BITS][1 << ARCTAN_BITS] = {")
for y in range(size):
    dy = size - y - 0.5
    for x in range(size):
        dx = x + 0.5 - size # size - x - 0.5
        print(int((math.atan2(dy, dx) / (math.pi / 2) - 1) * 16384), ",", end="")
print("};")
