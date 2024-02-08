# Generate 2D array of 16-bit height fields for 1/4 of sphere.
# Run with: python height.py | clang-format > height.h
# Notes in the C code mention a likely speed optimization using 1/2 sphere
# at the expense of a 2X larger table. That can be done here by extending
# the x loop to range(radius * 2) and the table size to [RADIUS][DIAMETER].
# Corresponding changes are then needed in the C code to remove x mirroring.

radius = 120  # This and RADIUS in C code must match

print("const uint16_t height[RADIUS][RADIUS] = {")
for y in range(radius):
    dy = (radius - y - 0.5) / radius
    for x in range(radius):
        dx = (radius - x - 0.5) / radius
        d = dx * dx + dy * dy
        # Working from pixel centers; this won't exceed 65535
        z = int(((1 - d) ** 0.5) * 65536) if d < 1 else 0
        print(z, ",", end="")
print("};")
