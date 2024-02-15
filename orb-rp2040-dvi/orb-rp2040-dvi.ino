// SPDX-FileCopyrightText: 2024 Phillip Burgess
//
// SPDX-License-Identifier: MIT

/*
Quick textured sphere with 3-axis rotation. (C) 2024 Phillip Burgess.
Written here for RP2040 using PicoDVI-Adafruit Fork library (for HDMI out),
but should be easily adaptable to most 32-bit MCUs & display types.

MIT license, all text here must be included in any redistribution.
*/

#include <PicoDVI.h> // Display & graphics library

// Using Pimoroni Pico DV Demo Base, see PicoDVI lib "16bit_hello" example for
// other boards (e.g. Adafruit Feather DVI). 16-bit (RGB565) color, single-
// buffered -- not enough RAM on RP2040 for double buffering, may experience
// some "tearing" -- or modify texture loop for 8-bit color).
DVIGFX16 display(DVI_RES_320x240p60, pimoroni_demo_hdmi_cfg);

// RADIUS must be #defined before #including various headers. It can NOT be
// changed willy-nilly and still expect to compile. Tables in most of the
// header files must be re-generated to suit (see 'python' folder).
#define RADIUS   120
#define DIAMETER (RADIUS * 2) // Sphere width/height on screen

// Not every example requires every header, but it's less explanation and
// de-commenting just to include them all. Won't bloat the executable;
// compiler's good about not allocating unused variables (including arrays).
#include "pixel_xy.h"        // Pixel cols/rows to XY fixed-point space
#include "pixel_z.h"         // Pixel cols/rows to Z space
#include "arctan.h"          // For rectangular to polar conversion
#include "arcsin.h"          // "
#include "texture-polar.h"   // Texture map for polar mapping example
#include "texture-stencil.h" // Texture map for stencil mapping example

// RP2040-SPECIFIC OPTIMIZATION: certain const tables in flash are copied
// to RAM on startup. Off-chip flash access is a bottleneck, but collectively
// these tables are too large for RAM (especially after the DVI framebuffer
// stakes its claim). These are prioritized by use and available space.
// Likely no benefit if porting to chips w/onboard flash, just access tables
// directly. (This does botch the "unused variables" thing mentioned above,
// since all of these are referenced in setup(). If paring down the project
// size, that part of the code will need modification.)
int16_t  pixel_xy_ram[DIAMETER];
int16_t  pixel_z_ram[PIXEL_Z_TABLE_SIZE];
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
  memcpy(pixel_xy_ram, pixel_xy, sizeof pixel_xy);
  memcpy(pixel_z_ram , pixel_z , sizeof pixel_z);
  memcpy(arcsin_ram  , arcsin  , sizeof arcsin);
}

// RP2040-SPECIFIC OPTIMIZATION: rendering loop runs from RAM.
void __not_in_flash_func(loop)() {
  uint32_t now = millis();
  digitalWrite(LED_BUILTIN, (now / 500) & 1); // Heartbeat

  // This next section is the only part involving floating-point math,
  // and occurs just once per frame so it's really not a huge bottleneck.

  // Rotation for example purposes is a time-based wobble on X & Y axes
  // and continuous spin on Z axis.
  float rot[3] = { (float)(sin((double)now * 0.0005) * 2.1),
                   (float)((1 - sin((double)now * 0.0008)) * 1.7),
                   (float)((double)now * 0.0011) };

  // rot[] above is the sphere's rotation. Intersection points are
  // rotated in the opposite direction to yield texture space coords.
  float c[] = { cos(-rot[0]), cos(-rot[1]), cos(-rot[2]) };
  float s[] = { sin(-rot[0]), sin(-rot[1]), sin(-rot[2]) };

  // Rotation matrix (Z,Y,X)
  // OK for these to exceed ±32K because interim result is 32-bit.
  int32_t r[3][3] = {
    { int32_t(c[2] * c[1] * 65536),
      int32_t((c[2] * s[1] * s[0] - s[2] * c[0]) * 65536),
      int32_t((c[2] * s[1] * c[0] + s[2] * s[0]) * 65536) },
    { int32_t(s[2] * c[1] * 65536),
      int32_t((s[2] * s[1] * s[0] + c[2] * c[0]) * 65536),
      int32_t((s[2] * s[1] * c[0] - c[2] * s[0]) * 65536) },
    { int32_t(-s[1] * 65536),
      int32_t(c[1] * s[0] * 65536),
      int32_t(c[1] * c[0] * 65536) } };

  for (uint16_t y=0; y<DIAMETER; y++) { // For each row...
    uint16_t *ptr = display.getBuffer() + y * display.width() +
                    (display.width() - DIAMETER) / 2; // -> column 1 in rect
    int16_t *zptr;                                    // -> to pixel_z table
    uint16_t x;                                       // Current column
    int16_t yy = -pixel_xy[y];
    // OPTIMIZATION OPPORTUNITY: some texture modes add a fixed offset to a
    // pixels X/Y/Z coordinates (e.g. to translate ±32K coords to 0-64K).
    // In certain situations those offsets could be added once here to
    // coord[0], eliminating some per-pixel math.
    int32_t yy01 = r[0][1] * yy;
    int32_t yy11 = r[1][1] * yy;
    int32_t yy21 = r[2][1] * yy;

    // Z table covers half of sphere; lower half is mirrored. If space
    // is precious, could use a 1/4 sphere table (with adjustments to the
    // table-generating code, and by adding mirroring in the X loops), at the
    // expense of some extra cycles. The height table is a sparse array, so
    // a lookup is needed to find row start. From there it just increments.
    if (y < RADIUS) {
      x    = pixel_z_x1[y];                // First column for row
      zptr = &pixel_z_ram[pixel_z_idx[y]]; // First Z for row
    } else {
      x    = pixel_z_x1[DIAMETER - 1 - y];
      zptr = &pixel_z_ram[pixel_z_idx[DIAMETER - 1 - y]];
    }
    ptr += x; // Increment to sphere's first pixel for this row
    uint16_t x2 = DIAMETER - 1 - x; // Last column

    // HERE'S WHERE THE CODE DIVERGES for different texturing methods.
    // ONE of these sections should be un-commented at any time.

#if 1
    // Polar mapping with arbitrary texture scale. Probably the most intuitive
    // of the mapping modes presented here, but also most demanding, thus
    // lowest frame rate. With additional code (not present here) and ample RAM
    // (also not present), this could allow dynamic loading of textures.
    for (; x<=x2; x++) { // For each column...
      int16_t xx = pixel_xy[x];
      int16_t zz = *zptr++;
      // Rotate XYZ on axis-aligned sphere to texture space, scale to ±32K.
      // IMPORTANT: do NOT "optimize" these divides to right-shifts. Divides
      // are necessary because result is signed. Since divisor is a power of
      // two, compiler will Do The Right Thing with a single-cycle arithmetic
      // shift rather than a costly divide. Normal >> would lose sign.
      int16_t px = (r[0][0] * xx + yy01 + r[0][2] * zz) / (65536 << (ARCTAN_BITS - 1));
      int16_t py = (r[1][0] * xx + yy11 + r[1][2] * zz) / (65536 << (ARCTAN_BITS - 1));
      int16_t pz = (r[2][0] * xx + yy21 + r[2][2] * zz) / 65536;
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
      *ptr++ = texture_polar[ty][tx];
    }
#endif

#if 0
    // Stencil mapping -- texture goes completely through the sphere, same on
    // front and back (mirrored). Interesting bit here is that math for only
    // two axes is needed, saving some cycles, yet curvature still happens.
    for (; x<=x2; x++) { // For each column...
      int16_t xx = pixel_xy[x];
      int16_t zz = *zptr++;
      // Rotate XY on axis-aligned sphere to texture space, scale to ±32K
      int16_t px = (r[0][0] * xx + yy01 + r[0][2] * zz) / 65536;
      int16_t py = (r[1][0] * xx + yy11 + r[1][2] * zz) / 65536;
      // OPTIMIZATION OPPORTUNITY: if texture dimensions are limited to
      // powers of two, tx,ty could be done with simpler shift-right ops.
      uint16_t tx = TEXTURE_STENCIL_WIDTH * (px + 32768UL) / 65536;
      uint16_t ty = TEXTURE_STENCIL_HEIGHT * (32767UL - py) / 65536;
      *ptr++ = texture_stencil[ty][tx];
    }
#endif

#if 0
    // Single-axis mapping. May or may not be useful for anything, but shows
    // how even less vector math is needed. Still curved!
    for (; x<=x2; x++) { // For each column...
      int16_t xx = pixel_xy[x];
      int16_t zz = *zptr++;
      // Rotate Z on axis-aligned sphere to texture space, scale to ±32K
      int16_t pz = (r[2][0] * xx + yy21 + r[2][2] * zz) / 65536;
      *ptr++ = pz & 0x1000 ? 0xFFE0 : 0x2965; // Z axis stripes
      // Stripes are uniformly Z-spaced. If uniform polar spacing is
      // desired, incorporate arcsin usage as in first example.
    }
#endif
  }

  frames++;
  if ((now - last) >= 1000) { // Show approximate frame rate
    fps    = (fps * 7 + frames + 3) / 8;
    frames = 0;
    last   = now;
    canvas.fillScreen(0);
    canvas.setCursor(0, 0);
    canvas.printf("%d FPS", fps);
    display.drawBitmap(0, 0, canvas.getBuffer(),
                       canvas.width(), canvas.height(), 0xFFFF, 0);
  }
}
