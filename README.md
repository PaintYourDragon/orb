# orb
Fast sphere renderer…for certain values of sphere.

https://github.com/PaintYourDragon/orb/assets/887611/1b244efa-a6fc-4bed-bc4e-61fcb36eca43

_240 px diameter sphere running on one core of RP2040 overclocked to 252 MHz
(because other core is generating DVI video)._

**TL;DR not ray tracing.** The math started there, but by eliminating most
generalizations and reducing to a single special case, the result is visually
and numerically comparable, with an inner loop that's 100% integer math.

Likely portable to other 32-bit MCUs (ESP32, SAMD51), sans DVI video.

## Pondering

Every musical act with a breakout hit is cursed to perform that song at every
concert forever, else fans will riot. Devo's _Whip It_, Jean-Michel Jarre's
_Oxygene IV._

My personal _Whip It_ is animated eyeballs. It's been an evolutionary series
of projects…first in the early Arduino Uno days, then through Adafruit's
HalloWing and Monster M4SK dev boards, and Raspberry Pi. I'd been tasked with
another iteration, this time for the Adafruit Feather RP2040 DVI, but
encountered turbulence: while prior projects relied on progressively larger
tables in both flash and RAM, RP2040's off-chip flash presents a bottleneck,
and then the PicoDVI library demands a majority of on-chip resources. On a
good day with a tailwind there's perhaps 100K of RAM left over.

Excepting Raspberry Pi (using OpenGL), all the prior microcontroller-based
iterations created the _illusion_ of a "rotating" eyeball using 2D blitting
or displacement tricks. This limited the range of motion, but revolving in 3D
was surely beyond these chips' capacity. The odd constraints of the RP2040
had me re-evaluating this assumption. "Solving the sphere problem" has been
on my code bucket list for a while, and I was in the mood for a
demoscene-style flex.
