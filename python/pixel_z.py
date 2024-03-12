# Generate 2D array of 16-bit height fields for 1/2 of sphere.
# Run with: python pixel_z.py | clang-format > pixel_z.h
# Only 1/2 sphere is needed, values are mirrored for the lower half.
# Could even do 1/4 sphere, adding left/right mirroring in the C code,
# but the larger full-width table saves some inner-loop cycles there.
# Additionally, this is a sparse array. Only those points inside the
# sphere need representing. Corners contribute nothing and the C code
# skips over those pixels.

radius = 120  # This and RADIUS in C code must match
diameter = radius * 2

x1 = []
idx = []
height_entries = 0

print("const int16_t pixel_z[] = {")
for y in range(radius):
    dy = (radius - y - 0.5) / radius
    first = True
    for x in range(diameter):
        dx = (radius - x - 0.5) / radius
        d = dx * dx + dy * dy
        # Working from pixel centers; this won't exceed 32767
        if d < 1:  # In circle?
            z = int(((1 - d) ** 0.5) * 32768)
            print(z, ",", end="")
            if first:  # First inside-circle point for this line?
                x1.append(x)               # Save x coord
                idx.append(height_entries) # Save 1st height index
                first = False
            height_entries += 1
print("};")
print("\n#define PIXEL_Z_TABLE_SIZE", height_entries)
print("\nconst uint16_t pixel_z_x1[RADIUS] = ")
print(str(x1).replace("[","{").replace("]","};"))
if height_entries > 65535:
    print("\nconst uint32_t pixel_z_idx[RADIUS] = ")
else:
    print("\nconst uint16_t pixel_z_idx[RADIUS] = ")
print(str(idx).replace("[","{").replace("]","};"))
