# orb
Fast sphere renderer…for certain values of sphere.

https://github.com/PaintYourDragon/orb/assets/887611/1b244efa-a6fc-4bed-bc4e-61fcb36eca43
240 px diameter sphere running on one core of RP2040 overclocked to 252 MHz
(because other core is generating DVI video).

**TL;DR not ray tracing.** The math started there, but by eliminating most
generalizations and reducing to a single special case, the result is visually
and numerically comparable, with an inner loop that's 100% integer math.

Likely portable to other 32-bit MCUs (ESP32, SAMD51), sans DVI video.
