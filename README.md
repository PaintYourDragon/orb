# orb
Fast sphere renderer…for certain values of sphere.

https://github.com/PaintYourDragon/orb/assets/887611/1b244efa-a6fc-4bed-bc4e-61fcb36eca43

_240 px diameter sphere running on one core of RP2040 overclocked to 252 MHz (while other core is generating DVI video)._

**TL;DR not ray tracing.** The math started there, but by eliminating most generalizations and reducing to a single special case, the result is visually and numerically comparable, with an inner loop that’s 100% integer math. **There’s some pre-compiled, drag-and-drop .UF2 files for a few boards in the uf2 folder.**

Likely portable to other 32-bit MCUs (ESP32, SAMD51, etc.), sans DVI video.

## Pondering Orbs

Every musical act with a breakout hit is cursed to perform that song at every concert forever, else fans will riot. Devo’s _Whip It_, Jean-Michel Jarre’s _Oxygene IV._

My personal _Whip It_ is animated eyeballs. It’s been an evolutionary series of projects…first in the early Arduino Uno days, then through Adafruit’s HalloWing and Monster M4SK dev boards, and Raspberry Pi. I’d been tasked with another iteration, this time for the Adafruit Feather RP2040 DVI, but encountered turbulence: while prior projects relied on progressively larger tables in both flash and RAM, RP2040’s off-chip flash presents a bottleneck, and then the PicoDVI library demands a majority of on-chip resources. On a good day with a tailwind there’s perhaps 100K of RAM left over.

<table>
<TR><TD WIDTH="35%" ALIGN="center"><IMG SRC="https://github.com/PaintYourDragon/orb/assets/887611/2d2aab14-f100-438a-b320-56bd75d8a0ec"/></TD>
<TD>Prior eyeballs on Adafruit Monster M4SK (SAMD51 MCU).
<BR><BR>
<i>Image credit: Adafruit</i></TD></TR>
</table>

Excepting Raspberry Pi (using OpenGL), all the prior microcontroller-based iterations really just created the _illusion_ of a rotating eyeball using 2D blitting or bitmap displacement tricks. This limited the range of motion, but revolving in 3D was surely beyond these chips’ capacity. Additionally, each new chip, each new and higher-resolution screen involved a _different_ approach, new tables, new incompatible code. The peculiar mix of strengths and constraints of the RP2040 had me re-considering what’s possible. “Solving the sphere problem” has been on my code bucket list for a while, and I was in the mood for a demoscene-style flex.

<table>
<TR><TD WIDTH="35%" ALIGN="center"><IMG SRC="https://github.com/PaintYourDragon/orb/assets/887611/2676c5e0-83bb-4ab9-b3dd-2028dbe5865a"/></TD>
<TD>The pixel-perfect gold standard would be <i>ray tracing.</i> But ray tracing is mathematically <i>expensive…</i> too much for current microcontrollers, even those with floating-point hardware <i>(e.g. on ARM Cortex-M4F, a square root operation takes 14 cycles, vs. 1 cycle for most integer operations).</i>
<BR><BR>
<i>Image credit: Henrik, Wikimedia Commons</i></TD></TR>
</table>

What makes this project work is that it doesn’t actually _require_ a fully generalized ray tracer. It’s just a single object, pivoting in place but otherwise remaining in the same location on screen. A series of assumptions can be made, each stripping away elements of the ray tracing equation, until eventually there’s no ray tracing left at all.

## Assumptions

There’s a whole genre of engineering jokes with the punchline, “First, we assume each (cow, racehorse, etc.) is a perfect sphere in a vacuum…”

**First, we assume eyes are perfect spheres.**

<table>
<TR><TD WIDTH="35%" ALIGN="center"><IMG SRC="https://github.com/PaintYourDragon/orb/assets/887611/5e4133cf-e8e3-4a52-ba13-a16d09d6ca3c"/></TD>
<TD>Eyes aren’t actually spheres. They’re <I>oblate spheroids</I> with a <I>corneal bulge.</I> The iris is a whole weird internal thing. But often spheres are <I>good enough…</I>this is for frivolous Halloween stuff, that sort of thing. Spheres can also be reasonable approximations of planets or sportsballs.</TD></TR>
</table>

Further, there’s only ever this _single_ sphere. It’s _always centered on the origin_ (0,0,0), and it’s a _unit sphere:_ the radius is 1.0. The latter means that in the [line-sphere intersection calculation](https://en.wikipedia.org/wiki/Line–sphere_intersection), any _r_ or _r<sup>2</sup>_ can be replaced with 1, and the former can simplify or eliminate other terms.

<table>
<TR><TD WIDTH="35%" ALIGN="center"><IMG SRC="https://github.com/PaintYourDragon/orb/assets/887611/cadf311f-c8bb-4966-9111-c0fb84df1966"/></TD>
<TD>
What saves a great deal of work and makes this whole project possible is that with a fixed-size, fixed-position sphere, no matter how much it revolves, <i>the set of pixels to be drawn is unchanged frame-to-frame.</i> As long as <i>only</i> those pixels are visited, no basic intersection test is needed. What’s more, the costly square root calculation that would be part of the intersection test is <i>also</i> unchanged per pixel. All these figures can be calculated once and saved in a table (tables must be minimised on RP2040, but this is the place to splurge). Extra bonus cake: such table only needs to cover 1/4 of the sphere (or even 1/8 with some extra math), and can be reflected for the other quadrants.</TD></TR>
</table>

## Still More Assumptions

Next, perspective is dropped from the equation. This might be controversial, but in practice these little graphics projects tend toward displays less than 2 inches across, and if it were a physical thing the distortion due to perspective would be nearly imperceptible. In doing so, there’s no longer a _viewing frustum_ (a pyramid-shaped volume of viewable space), but just a rectangular tube. The camera is essentially at infinity and every ray is parallel, and the image plane can be as far back or far forward as we’d like…even centered on the origin (0,0,0), like the sphere.

![perspective1](https://github.com/PaintYourDragon/orb/assets/887611/1add6889-99de-4307-acb5-683ccb9a47c9)

In doing so, that table of square roots (distances from camera to points on sphere) can be flipped around, now it’s distances from image plane to sphere…

<table>
<TR><TD WIDTH="35%" ALIGN="center"><IMG SRC="https://github.com/PaintYourDragon/orb/assets/887611/1f7d566f-d329-4dd7-94b7-b974ca794b05"/></TD>
<TD>Height fields, basically. Each point in the table is the elevation of a hemisphere above a plane.</TD></TR>
</table>

So now, looking straight down the Z axis, the X and Y coordinate of each sphere intersection can be inferred directly from screen pixel coordinates, and Z comes from the table. **At this point, it’s no longer ray tracing really,** but a single “solved” image. In hindsight it’s a pretty obvious and minimalist solution. Ray tracing was helpful for _conceptualizing the problem,_ we are grateful but do not actually require its services. With enough constraints and simplifications, the same set of points can be computed by trivial means.

To rotate the sphere, each intersection point(X,Y,Z) could be rotated around the origin to yield a new (X',Y',Z'), from which texture coordinates are derived. A full three-axis rotation involves something like 12 multiplies per pixel, but with further shenanigans the work can be cut roughly in half…

Dropping lighting from the equation — just using texture map colors directly — might also be controversial, but is part of the Last Shenanigan than makes this work.

<table>
<TR><TD WIDTH="35%" ALIGN="center"><IMG SRC="https://github.com/PaintYourDragon/orb/assets/887611/05d1121c-93a1-4981-aec5-6e012e7805e8"/></TD>
<TD>When modeling something like eyes, it’s often a good look to put a lens over the screen. This provides some curvature and an implied sense of <i>moisture</i> to the situation…and specular highlights come free, from nature, in realtime.</TD></TR>
<TR><TD><IMG SRC="https://github.com/PaintYourDragon/orb/assets/887611/9b224fb1-81a0-409b-8d1e-566847cc69ec"/></TD>
<TD>Another reason one might drop lighting is that if this is a sphere pivoting in-place, and camera and light source are unmoving, shading would be constant frame-to-frame. Pre-rendered shading (using an alpha channel) could be applied to each frame. That’s not done in this code, but might be practically a free upgrade in some recent chips (i.MX, etc.) that have 2D imaging built in. Or it can even be done with a printed acetate over the screen.</TD></TR>
<TR><TD><IMG SRC="https://github.com/PaintYourDragon/orb/assets/887611/e0b0ef4f-84cf-4b65-8663-147b137f4f06"/></TD>
<TD>With no lighting — if just texture coordinates are needed — there’s no difference between rotating each intersection point (a lot of multiplication), and rotating the image plane, projecting the height vectors “outward.” It becomes a matter of scaling and summing three vectors, one of which only changes per scanline, reducing the per-pixel math.</TD></TR>
</table>


## (Work in progress, will continue)
