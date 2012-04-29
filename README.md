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

This means that what you will find here is *not* the complete source code to
the Unreal Tournament engine. Only some very specific parts have been
open-sourced. However, there is plenty to improve on.

The goal is to get Unreal Tournament running smoothly on modern systems.

For now, this project is codenamed Surreal.


Dependencies
------------

For building, you need development files for the following libraries:

 * SDL 1.2.x
 * OpenGL 1.?
 * OpenAL 1.1
 * ALURE 1.0
 * MikMod 3.x


Building on Windows
-------------------

On Windows, building was tested using Visual Studio 2008 express. This is a
free download from Microsoft. You can simply open the solution file
`Surreal.sln` and build from there.

The biggest hurdle is getting the dependencies in place. You will likely
need to reconfigure the additional include and library paths for many of the
projects in order to build.

In addition, if you want to build D3D9Drv, or build SDL from source, you will
need the DirectX SDK. This is also a free download from Microsoft.


Building on Linux
-----------------

To build on Linux, you must first place the packages for which source code is
not available in the `System` directory. The files required are `Core.so`,
`Engine.so` and `Render.so`.

An xdelta patch is included for `Core.so`. This patch simply alters the
SDL dependency from SDL 1.1 to SDL 1.2. The only SDL function actually
referenced by `Core.so` is `SDL_Quit`, so it's no biggy. Apply the patch with:

    mv System/Core.so System/Core.so-SDL1.1
    xdelta patch Core-SDL-patch.xdelta System/Core.so-SDL1.1 System/Core.so
    chmod a+x System/Core.so

Finally, you need G++ 2.95. This is required to keep binary compatibility with
the above binaries. One solution is to use Ubuntu 6.06.2 (Dapper Drake) in a
virtual machine, which is the last Ubuntu to distribute this old version.

Now you should be able to simply run `make`.


License situation
-----------------

A copy of the Artistic License is included in `LICENSE`. This is the license
that OpenUT has used during its lifetime.

Because I could not find a license in the 432 source code release, with the
exception of the vague guideline in the Help file, I have assumed that both
these official releases are distributed with the Artistic License.

The updated renderers by Chris Dohnal are based on OpenUT source code. Both his
work and original work done here will also use the Artistic License.
