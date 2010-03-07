Unreal Tournament
=================

This is a continuation of the original Unreal Tournament engine. It is based
on several parts from different sources:

 * The partial source code release to version 432
   http://unreal.epicgames.com/ (site offline)
 * The OpenUT project
   http://openut.sourceforge.net/
 * Chris Dohnal's work on OpenGLDrv and D3D9Drv
   http://www.cwdohnal.com/utglr/
 * Original work done here.

The goal is to get Unreal Tournament running smoothly on modern systems.

For now, it is codenamed Surreal.


Dependencies
------------

For building, you need development files for the following packages:

 * SDL 1.2
 * OpenGL 1.?
 * OpenAL 1.1
 * ALURE 1.0


Building
--------

Currently, only the Linux build system is in place. The project files for
Visual C++ that were distributed with the public source for version 432 are
still in place, but don't nearly cover all packages. Help in this area is
much appreciated.

To build on Linux, you must first place the packages for which source code is
not available in the `System` directory. The files required are `Core.so`,
`Engine.so` and `Render.so`.

An xdelta patch is included for `Core.so`. This patch simply alters the
SDL dependency from SDL 1.1 to SDL 1.2. The only SDL function actually
referenced by `Core.so` is `SDL_Quit`, so it's no biggy. Apply the patch with:

    mv System/Core.so System/Core.so-SDL1.1
    xdelta Core-SDL-patch.xdelta System/Core.so-SDL1.1 System/Core.so

Finally, you need G++ 2.95. This is required to keep binary compatibility with
the above binaries. I personally use Ubuntu 6.06.2 (Dapper Drake) in a virtual
machine, which is the last Ubuntu to distribute this old version of G++.

Now you should be able to simply run `make`.


License situation
-----------------

A copy of the Artistic License is included in `LICENSE`. This is the license
that OpenUT has used during it's lifetime.

Because I could not find a license in the 432 source code release, with the
exception of the vague guideline in the Help file, I have assumed that both
these official releases are distributed with the Artistic License.

Chris Dohnal has yet to respond to me by email. I'm hoping he will forgive me
for my sins.


-- Stéphan Kochen
