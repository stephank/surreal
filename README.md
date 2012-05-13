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


Building on Windows
-------------------

Make sure you have the Windows and DirectX SDKs installed. Also either opt to
install the Visual Studio C compilers during the Windows SDK install, or use
the compilers from an existing install of Visual Studio.

TODO: Describe how to install GYP on Windows.

Roughly, you will then want to generate the project files and the build them.
To get any meaningful result, make sure you're building the Release
configuration. On the command line, this would look as follows:

    gyp --depth=.
    msbuild surreal.sln /p:Configuration:Release


Other platforms
---------------

Building on other platforms is currently not possible. Care has been taken to
make as much of the source compile on Linux and Mac OS X, but there are no
compatible binaries to link to.


License situation
-----------------

A copy of the Artistic License is included in `LICENSE`.

This license does *not* apply to the included dependencies SDL, OpenAL Soft,
ALURE and MikMod. For their licenses, see documentation found in the
individual subdirectories underneath `Deps`.

All other source code found here effectively uses the Artistics License:

 * The OpenUT project used the Artistic License during its lifetime.

 * No license was included with the 432 source code release. It is assumed
   both official releases (OpenUT, and the 432 source code) were using the
   Artistic License.

 * The updated renderers by Chris Dohnal are based on OpenUT source code.
   The Artistic License applies to this work.

 * Original work done here also uses the Artistic License.
