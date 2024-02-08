# Generate array of 16-bit vector scaling factors used to project
# center point of each screen pixel row/column into fixed-point space.
# Run with: python vecscale.py | clang-format > vecscale.h

radius   = 120  # This and RADIUS in C code must match
diameter = radius * 2

print("const uint16_t vecscale[DIAMETER] = {")
for i in range(diameter):
    print((65536 * (i * 2 + 1) + diameter) // (diameter * 2), ",", end="")
print("};")
