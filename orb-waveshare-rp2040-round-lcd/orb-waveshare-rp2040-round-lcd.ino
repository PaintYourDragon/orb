// SPDX-FileCopyrightText: 2024 Phillip Burgess
//
// SPDX-License-Identifier: MIT

/*
Quick textured sphere with 3-axis rotation. (C) 2024 Phillip Burgess.
Written here for Waveshare RP2040 LCD 1.28 & Adafruit_GC9A01A library,
but should be easily adaptable to most 32-bit MCUs & display types.
A lot of this is just copy-and-paste code from the PicoDVI example
(the orb code is not library-ified), comments are pared back, check
that other code for explainers. 240+ MHz overclock recommended.

MIT license, all text here must be included in any redistribution.
*/

#include "SPI.h"
#include "Adafruit_GFX.h"
#include "Adafruit_GC9A01A.h"
#include "hardware/dma.h"

// RADIUS must be #defined before #including various headers. It can NOT be
// changed willy-nilly and still expect to compile. Tables in most of the
// header files must be re-generated to suit (see 'python' folder).
#define RADIUS   120
#define DIAMETER (RADIUS * 2) // Sphere width/height on screen

#include "pixel_xy.h"        // Pixel cols/rows to XY fixed-point space
#include "pixel_z.h"         // Pixel cols/rows to Z space
#include "arctan.h"          // For rectangular to polar conversion
#include "arcsin.h"          // "
#include "texture-polar.h"   // Texture map for polar mapping example
int16_t  pixel_xy_ram[DIAMETER];
int16_t  pixel_z_ram[PIXEL_Z_TABLE_SIZE];
uint16_t arcsin_ram[1 << ARCSIN_BITS];

#define TFT_DC    8
#define TFT_CS    9
#define TFT_SCK  10
#define TFT_MOSI 11
#define TFT_RST  12
#define TFT_BL   25

Adafruit_GC9A01A tft(&SPI1, TFT_DC, TFT_CS, TFT_RST);

uint8_t  dma_channel;
uint16_t buf[2][240]; // 2 scanlines; one sends while next is rendered
uint8_t  bufidx = 0;  // Currently-rendering buffer

void setup() {
  Serial.begin(9600);
  //while(!Serial);

  // Do this before SPI.begin()
  SPI1.setTX(TFT_MOSI);
  SPI1.setSCK(TFT_SCK);
  SPI1.setCS(TFT_CS);

  tft.begin(64000000);
  tft.fillScreen(GC9A01A_BLACK);

  dma_channel = dma_claim_unused_channel(true);
  dma_channel_config c = dma_channel_get_default_config(dma_channel);
  channel_config_set_transfer_data_size(&c, DMA_SIZE_8);
  channel_config_set_dreq(&c, spi_get_dreq(spi1, true));
  dma_channel_configure(dma_channel, &c,
                        &spi_get_hw(spi1)->dr, // Dest addr
                        buf[0],                // Src addr
                        480,                   // Xfer count
                        false);                // Don't start yet

  memcpy(pixel_xy_ram, pixel_xy, sizeof pixel_xy);
  memcpy(pixel_z_ram , pixel_z , sizeof pixel_z);
  memcpy(arcsin_ram  , arcsin  , sizeof arcsin);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH); // Backlight on
}

uint32_t frames = 0;

void __not_in_flash_func(loop)() {
  uint32_t now = millis();

  float rot[3] = { (float)(sin((double)now * 0.0005) * 2.1),
                   (float)((1 - sin((double)now * 0.0008)) * 1.7),
                   (float)((double)now * 0.0011) };

  float c[] = { cos(-rot[0]), cos(-rot[1]), cos(-rot[2]) };
  float s[] = { sin(-rot[0]), sin(-rot[1]), sin(-rot[2]) };

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

  // Wait for DMA completion before setting addr window
  while(dma_channel_is_busy(dma_channel));
  tft.endWrite();
  tft.setAddrWindow(0, 0, 240, 240);
  tft.startWrite();

  for (uint16_t y=0; y<DIAMETER; y++) { // For each row...
    // Scanline buf is cleared each time. Normally might rely on round display
    // to clip garbage, but there's a slight disagreement over in-circle pixels
    // for this code vs the GC9A01A display.
    memset(buf[bufidx], 0, sizeof buf[bufidx]);
    uint16_t *ptr = buf[bufidx] +
                    (tft.width() - DIAMETER) / 2; // -> column 1 in rect
    int16_t *zptr;                                // -> to pixel_z table
    uint16_t x;                                   // Current column
    int16_t yy   = -pixel_xy[y];
    int32_t yy01 = r[0][1] * yy;
    int32_t yy11 = r[1][1] * yy;
    int32_t yy21 = r[2][1] * yy;

    if (y < RADIUS) {
      x    = pixel_z_x1[y];                // First column for row
      zptr = &pixel_z_ram[pixel_z_idx[y]]; // First Z for row
    } else {
      x    = pixel_z_x1[DIAMETER - 1 - y];
      zptr = &pixel_z_ram[pixel_z_idx[DIAMETER - 1 - y]];
    }
    ptr += x; // Increment to sphere's first pixel for this row
    uint16_t x2 = DIAMETER - 1 - x; // Last column

    // Only the polar mapping demo is done here.
    for (; x<=x2; x++) { // For each column...
      int16_t xx = pixel_xy[x];
      int16_t zz = *zptr++;
      int16_t px = (r[0][0] * xx + yy01 + r[0][2] * zz) / (65536 << (ARCTAN_BITS - 1));
      int16_t py = (r[1][0] * xx + yy11 + r[1][2] * zz) / (65536 << (ARCTAN_BITS - 1));
      int16_t pz = (r[2][0] * xx + yy21 + r[2][2] * zz) / 65536;
      uint32_t tx; // Texture X coord
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
      *ptr++ = texture_polar[ty][tx];
    }

    while(dma_channel_is_busy(dma_channel));
    // Reset DMA source address and trigger next transfer.
    // Small delay is needed else last byte out is corrupted.
    // This happened in NeoPXL8 as well!
    delayMicroseconds(10);
    dma_channel_set_read_addr(dma_channel, buf[bufidx], true); // triggers DMA
    bufidx = 1 - bufidx; // Swap scanline buffers
  }
  Serial.println(++frames * 1000 / millis());
}
