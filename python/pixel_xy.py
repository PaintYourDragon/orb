# Generate array of 16-bit coordinates to project center point
# of each screen pixel row/column into fixed-point space.
# Run with: python pixel_xy.py | clang-format > pixel_xy.h

radius   = 120  # This and RADIUS in C code must match
diameter = radius * 2

# Coords are intentionally inset ever-so-slightly from full int16_t range
# to avoid overflows due to compounding rounding errors on 3 axes.
print("const int16_t pixel_xy[DIAMETER] = {")
for i in range(diameter):
    print((65534 * (i * 2 + 1) + diameter) // (diameter * 2) - 32767, ",", end="")
print("};")
