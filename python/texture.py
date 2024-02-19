# Convert RGB color image to 2D C array of RGB565 colors.
# Run with: python texture.py imagefile | clang-format > texture.h
# where imagefile is typically PNG or JPEG, then edit the texture,
# TEXTURE_WIDTH and TEXTURE_HEIGHT names in the output file to match
# the C code's expectations. Textures can be arbitrary dimensions,
# no need for squares, 2:1 rects or powers of two.

import sys
from PIL import Image

f    = Image.open(sys.argv[1])
size = f.size
tex  = f.load()
f.close()

print("#define TEXTURE_WIDTH", size[0])
print("#define TEXTURE_HEIGHT", size[1])
print("const uint16_t texture[TEXTURE_HEIGHT][TEXTURE_WIDTH] = {")
for y in range(size[1]):
    for x in range(size[0]):
        rgb = tex[x, y]
        c = ((rgb[0] & 0xF8) << 8) | ((rgb[1] & 0xFC) << 3) | (rgb[2] >> 3)
        # If target is a color LCD rather than PicoDVI,
        # enable this next line to swap bytes on output:
        # c = ((c & 0xFF) << 8) | (c >> 8)
        print("0x{:04x},".format(c), end="")
print("};")
