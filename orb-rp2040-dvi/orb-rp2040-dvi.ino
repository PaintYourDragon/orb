// SPDX-FileCopyrightText: 2024 Phillip Burgess
//
// SPDX-License-Identifier: MIT

/*
Quick textured sphere with 3-axis rotation. (C) 2024 Phillip Burgess.
Written here for RP2040 using PicoDVI-Adafruit Fork library (for HDMI out),
but should be easily adaptable to most 32-bit MCUs & display types.

MIT license, all text here must be included in any redistribution.

The challenge here was quickly finding texture coordinates on a 3D rotated
sphere. Unlike prior efforts, it's not a 2D displacement trick. Actual 3D.
This involved first considering the problem from a generalized ray tracing
approach, then applying incremental constraints -- fixed size, no lighting,
no perspective -- what remains isn't ray tracing, but a near trivial task.
The gimmick comes down to not actually rotating the sphere (it stays fixed
at the origin), instead pivoting the image plane (also centered at origin).
Advancing through each pixel in the plane is simple linear interpolation,
and the perpendicular 'height' of the sphere at each pixel comes from a
table (averting a costly per-pixel square root), yielding (X,Y,Z).

The inner pixel loop involves scaling and adding a set of fixed-point vectors.
All integer math; multiply, shift, add, some table lookups. No floating-point,
division, or square roots. The essential scaling operation on each vector is:

    result = input * scale / 65536;

where input is a 16-bit value, signed (-32K to +32K maps to -1.0 to +1.0)
or unsigned (0 to 64K maps to 0.0 to +1.0), and scale is unsigned (0 to 64K
maps to 0.0 to +1.0); this scales down, not up. The interim result is 32-bit,
which the divide then brings back to the original 16-bit coordinate space.
It is VERY IMPORTANT to keep that divide intact and NOT be L33T with a right-
shift operation! The compiler, even at a vanilla no-optimization setting,
will handle this with an arithmetic shift right instruction that preserves
the sign of the input, often just 1 cycle.
*/

#include <PicoDVI.h> // Display & graphics library

// Using Pimoroni Pimoroni Pico DV Demo Base, see PicoDVI lib "16bit_hello"
// example for other boards (e.g. Adafruit Feather DVI). 16-bit (RGB565)
// color, single-buffered -- not enough RAM on RP2040 for double buffering,
// may experience some "tearing" -- or modify texture loop for 8-bit color).
DVIGFX16 display(DVI_RES_320x240p60, pimoroni_demo_hdmi_cfg);

// RADIUS must be #defined before #including various headers. It can NOT be
// changed willy-nilly and still expect to compile. Tables in most of the
// header files must be re-generated to suit (see 'python' folder).
#define RADIUS   120
#define DIAMETER (RADIUS * 2) // Sphere width/height on screen

// Not every example requires every header, but it's less explanation and
// de-commenting just to include them all. Won't bloat the executable;
// compiler's good about not allocating unused variables (including arrays).
#include "vecscale.h"        // Image plane X/Y vector scaling table
#include "height.h"          // Sphere height field (Z) table
#include "arctan.h"          // For rectangular to polar conversion
#include "arcsin.h"          // "
#include "texture-polar.h"   // Texture map for polar mapping example
#include "texture-stencil.h" // Texture map for stencil mapping example

// RP2040-SPECIFIC OPTIMIZATION: certain const tables in flash are copied
// to RAM on startup. Off-chip flash access is a bottleneck, but collectively
// these tables are too large for RAM (especially after the DVI framebuffer
// stakes its claim). These are prioritized by use and available space.
// Likely no benefit on chips w/onboard flash, just access tables directly.
uint16_t vecscale_ram[DIAMETER];
uint16_t height_ram[RADIUS][RADIUS];
uint16_t arcsin_ram[1 << ARCSIN_BITS];

GFXcanvas1 canvas(48, 7);  // For FPS indicator
uint32_t   last   = -1000; // "
uint32_t   frames = 0;     // "
uint32_t   fps    = 0;     // "

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  if (!display.begin()) { // Fast blink LED if insufficient RAM
    for (;;) digitalWrite(LED_BUILTIN, (millis() / 200) & 1);
  }

  // See note above re: RP2040 optimization
  memcpy(vecscale_ram, vecscale, sizeof vecscale);
  memcpy(height_ram, height, sizeof height);
  memcpy(arcsin_ram, arcsin, sizeof arcsin);
}

void loop() {
  uint32_t now = millis();
  digitalWrite(LED_BUILTIN, (now / 500) & 1); // Heartbeat

  // Image plane is a unit square centered on origin, plus an "out" facing
  // vector is needed. Right-handed coordinate space with fixed-point values.
  // Bounds are intentionally inset ever-so-slightly from the full range to
  // prevent small but cumulative rounding errors from "wrapping around."
  int32_t coord[][3] = {      // X,Y,Z
    { -32766,  32765,     0}, // UL corner of image plane
    {  32766,  32765,     0}, // UR corner of image plane
    { -32766, -32765,     0}, // LL corner of image plane
    {      0,      0, 32765}  // Z vector from origin
  };

  // This next section is the only part involving floating-point math,
  // and occurs just once per frame so it's really not a huge bottleneck.

  // Rotation for example purposes is a time-based wobble on X & Y axes
  // and continuous spin on Z axis.
  float rot[3] = { (float)sin((double)now * 0.0005) * 2.1,
                   (float)sin((double)now * 0.0008) * 1.7,
                   (float)((double)now * 0.0011) };

  // rot[] above is the sphere's rotation. The image plane is rotated in the
  // opposite direction while the sphere stays put.
  float c[] = { cos(-rot[0]), cos(-rot[1]), cos(-rot[2]) };
  float s[] = { sin(-rot[0]), sin(-rot[1]), sin(-rot[2]) };

  // Each point in the coord[] table (3 corners of image plane + depth
  // vector) is rotated in floating-point 3space, the final result quantized
  // back to fixed-point space and stored in the original table. While the
  // sphere itself neatly fills int16_t space, the corners of the image plane
  // when rotated will exceed this, hence the int32_t type for those.
  for (int i=0; i<4; i++) {
    float x, y, z, zz;
    // Z rot (X & Y)
    x = (float)coord[i][0] * c[2] - (float)coord[i][1] * s[2];
    y = (float)coord[i][0] * s[2] + (float)coord[i][1] * c[2];
    // Y rot (X & Z)
    z = x * s[1] + (float)coord[i][2] * c[1];
    x = x * c[1] - (float)coord[i][2] * s[1];
    // X rot (Z & Y)
    zz = z * c[0] - y * s[0];
    y  = z * s[0] + y * c[0];
    coord[i][0] = (int32_t)(floor(x + 0.5));
    coord[i][1] = (int32_t)(floor(y + 0.5));
    coord[i][2] = (int32_t)(floor(zz + 0.5));
  }

  // Distances between image plane corners then represent 3D vectors
  // for incrementing along rows (y) and columns (x) of display.
  int32_t vx[3] = { coord[1][0] - coord[0][0],
                    coord[1][1] - coord[0][1],
                    coord[1][2] - coord[0][2] };
  int32_t vy[3] = { coord[2][0] - coord[0][0],
                    coord[2][1] - coord[0][1],
                    coord[2][2] - coord[0][2] };
  int32_t vz[3] = { coord[3][0], coord[3][1], coord[3][2] };

  // Get pointer to first pixel in bounding square of sphere
  uint16_t *ptr = display.getBuffer() + (display.width() - DIAMETER) / 2;

  int32_t   rowp0[3]; // Leftmost point of image plane for current row
  uint16_t  color;    // RGB565 pixel color

  // OPTIMIZATION OPPORTUNITY: some texture modes add a fixed offset to a
  // pixels X/Y/Z coordinates (e.g. to translate ±32K coords to 0-64K).
  // In certain situations those offsets could be added once here to
  // coord[0], eliminating some per-pixel math.

  for (int y=0; y<DIAMETER; y++) { // For each row...
    // Get position of leftmost "point 0"  for this row, based on rotated
    // upper-left corner of image plane, offset by y vector scaled.
    rowp0[0] = coord[0][0] + vy[0] * vecscale_ram[y] / 65536; // See prior notes,
    rowp0[1] = coord[0][1] + vy[1] * vecscale_ram[y] / 65536; // do not "optimize"
    rowp0[2] = coord[0][2] + vy[2] * vecscale_ram[y] / 65536; // with a >>

    // Height table covers 1/4 of sphere. Get pointer for current row,
    // mirroring Y axis as needed.
   uint16_t *hrow = height_ram[(y < RADIUS) ? y : DIAMETER - 1 - y];

    // HERE'S WHERE THE CODE DIVERGES for different texturing methods.
    // ONE of these sections should be un-commented at any time.

#if 1
    // Polar mapping with arbitrary texture scale. Probably the most intuitive
    // of the mapping modes presented here, but also most demanding, thus
    // lowest frame rate. With additional code (not present here), this could
    // allow dynamic loading of textures.
    for (int x=0; x<DIAMETER; x++) { // For each column...
      color = 0;
      // OPTIMIZATION OPPORTUNITY: This operation to horizontally mirror the
      // height table could be eliminated by extending the table width to the
      // full width of the sphere (but would require 2X the space). The same
      // is NOT true of the vertical axis, a simple per-scanline operation.
      uint16_t z = hrow[(x < RADIUS) ? x : (DIAMETER - 1 - x)];
      // OPTIMIZATION OPPORTUNITY: because the set of pixels drawn does not
      // change frame to frame, this z>0 check could be eliminated with a
      // table containing the first and last pixel per row, then drawing only
      // those pixels (with changes to *ptr handling).
      if (z > 0) {
        // Get pixel's XYZ coordinates on sphere surface
        int16_t px = rowp0[0] + ((vx[0] * vecscale_ram[x]) + (vz[0] * z)) / 65536;
        int16_t py = rowp0[1] + ((vx[1] * vecscale_ram[x]) + (vz[1] * z)) / 65536;
        int16_t pz = rowp0[2] + ((vx[2] * vecscale_ram[x]) + (vz[2] * z)) / 65536;
        px /= 1 << (ARCTAN_BITS - 1);  // Scale ±32K to arctan table size
        py /= 1 << (ARCTAN_BITS - 1);  // "
        uint32_t tx; // Texture X coord
        // OPTIMIZATION OPPORTUNITY: these tests and some math could be
        // eliminated using a full-circle rather than single-quadrant arctan
        // table. Would be massive though, 512KB.
        if (py >= 0) {
          tx = (px >= 0) ? arctan[255 - px][255 - py] :         // Quadrant 1
                           arctan[255 - py][255 + px] + 16384;  // Q2
        } else {
          tx = (px < 0) ?  arctan[255 + px][255 + py] + 32768 : // Q3
                           arctan[255 + py][255 - px] + 49152;  // Q4
        }
        tx = tx * TEXTURE_POLAR_WIDTH / 65536;
        uint16_t ty = arcsin_ram[(pz + 32768UL) >> (16 - ARCSIN_BITS)] *
          TEXTURE_POLAR_HEIGHT / 65536; // Texture Y coord
        // OPTIMIZATION OPPORTUNITY (maybe): rather than arctan and arcsin
        // tables that work in 0-64K space which is then then scaled to
        // texture dimensions, those tables could be pre-scaled to pixel
        // coordinates. This would lose the flexibility of arbitrary texture
        // sizes (e.g. if implementing runtime texture loading), and in some
        // cases (if using single-quadrant table) would require the texture
        // width be a multiple of 4.
        color = texture_polar[ty][tx];
      }
      *ptr++ = color;
    }
#endif

#if 0
    // Stencil mapping -- texture goes completely through the sphere, same on
    // front and back (mirrored). Interesting bit here is that math for only
    // two axes is needed, saving some cycles, yet curvature still happens.
    for (int x=0; x<DIAMETER; x++) { // For each column...
      color = 0;
      // See notes above re: horizontal mirroring optimization.
      uint16_t z = hrow[(x < RADIUS) ? x : (DIAMETER - 1 - x)];
      if (z > 0) {
        // Get pixel's XY coordinates on sphere surface
        int16_t px = rowp0[0] + ((vx[0] * vecscale_ram[x]) + (vz[0] * z)) / 65536;
        int16_t py = rowp0[1] + ((vx[1] * vecscale_ram[x]) + (vz[1] * z)) / 65536;
        // OPTIMIZATION OPPORTUNITY: if texture dimensions are limited to
        // powers of two, tx,ty could be done with simpler shift-right ops.
        uint16_t tx = TEXTURE_STENCIL_WIDTH * (px + 32768UL) / 65536;
        uint16_t ty = TEXTURE_STENCIL_HEIGHT * (32767UL - py) / 65536;
        color = texture_stencil[ty][tx];
      }
      *ptr++ = color;
    }
#endif

#if 0
    // Single-axis mapping. May or may not be useful for anything, but shows
    // how even less vector math is needed. Still curved!
    for (int x=0; x<DIAMETER; x++) { // For each column...
      color = 0;
      // See notes above re: horizontal mirroring optimization.
      uint16_t z = hrow[(x < RADIUS) ? x : (DIAMETER - 1 - x)];
      if (z > 0) {
        // Get pixel's Z coordinate on sphere surface
        int16_t pz = rowp0[2] + ((vx[2] * vecscale_ram[x]) + (vz[2] * z)) / 65536;
        color = pz & 0x1000 ? 0xF800 : 0x001F; // Z axis stripes
        // Stripes are uniformly Z-spaced. If uniform polar spacing is
        // desired, incorporate arcsin usage as in first example.
      }
      *ptr++ = color;
    }
#endif

    ptr += (display.width() - DIAMETER); // Advance to start of next line
  }

  frames++;
  if ((now - last) >= 1000) { // Show approximate frame rate
    fps    = (fps * 7 + frames) / 8;
    frames = 0;
    last   = now;
    canvas.fillScreen(0);
    canvas.setCursor(0, 0);
    canvas.printf("%d FPS", fps);
    display.drawBitmap(0, 0, canvas.getBuffer(), canvas.width(), canvas.height(), 0xFFFF, 0);
  }
}
