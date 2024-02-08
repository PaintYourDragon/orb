# Generate 2D array of 16-bit height fields for 1/2 of sphere.
# Run with: python height.py | clang-format > height.h

radius = 120  # This and RADIUS in C code must match
diameter = radius * 2

x1 = []
idx = []
height_entries = 0

print("const uint16_t height[] = {")
for y in range(radius):
    dy = (radius - y - 0.5) / radius
    first = True
    for x in range(diameter):
        dx = (radius - x - 0.5) / radius
        d = dx * dx + dy * dy
        # Working from pixel centers; this won't exceed 65535
        if d < 1:  # In circle?
            z = int(((1 - d) ** 0.5) * 65536)
            print(z, ",", end="")
            if first:  # First inside-circle point for this line?
                x1.append(x)               # Save x coord
                idx.append(height_entries) # Save 1st height index
                first = False
            height_entries += 1
print("};")
print("\n#define HEIGHT_TABLE_SIZE", height_entries)
print("\nconst uint16_t x1[RADIUS] = ")
print(str(x1).replace("[","{").replace("]","};"))
print("\nconst uint16_t height_idx[RADIUS] = ")
print(str(idx).replace("[","{").replace("]","};"))
