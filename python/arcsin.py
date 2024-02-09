# Generate an arc-sine-like table for converting fixed-point coordinates
# to polar latitude values in the range 0 to 65535.
# Run with: python arcsin.py | clang-format > arcsin.h
# Table size can be adjusted to balance size vs precision; the default 14
# bits used here (16384 entries) is adequate for the RP2040 sphere demos.
# There's a minor optimization opportunity (1 shift per pixel) if one uses
# the max 16 bit resolution here, but the table will use much more space
# (128 KB vs 32 KB at present).

from math import asin, pi

bits = 14
size = 1 << bits

print("#define ARCSIN_BITS", bits)
print("const uint16_t arcsin[1 << ARCSIN_BITS] = {")
for i in range(size):
    print(int((0.5 - asin((i + 1) / size * 2 - 1) / pi) * 65536), ",", end="")
print("};")
