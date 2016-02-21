Copyright (C) 2000, 2013 Neil Edelman, see copying.txt.  neil dot
edelman each mail dot mcgill dot ca

Image credit: NASA; JPL; ESA; Caltech; UCLA; MPS; DLR; IDA; Johns
Hopkins University APL; Carnegie Institution of Washington; DSS
Consortium; SDSS.

src/fileformat/lodepng: Copyright (c) 2005-2015 Lode Vandevenne
http://lodev.org/lodepng/

src/fileformat/nanojpeg: by Martin J. Fiedler <martin.fiedler@gmx.net>
http://keyj.emphy.de/nanojpeg/

Version 3.3.

Usage: Void

To win, blow up everything.

Player controls: left, right, up, space, F1 (fullscreen.)

You downloaded the source code and want to build? If you have a
Mac, xCode tools, viz, gcc; MacOSX has native support for OpenGL1.1
and Glut-type functions are already included. Just type <make>,
<make icon>, or <make install> (Tested on MacOSX.6.8, but should
be good on different versions; there's nothing dependent on the Mac
API.)

http://glew.sourceforge.net/

Otherwise, you may have to download a loading library for OpenGL1.1.
The one I use used in my source is Glew; turn it on by defining a
preprocessor variable GLEW (viz, gcc -DGLEW.) Build the static
release of Glew (glew32s) and link it to Void. I have tested it on
MSVC10/13 and gcc Ubuntu (still trying MinGW.) Make sure you turn
off "Whole Program Optimization," on MSVC as it will fail to make
Glew.

http://freeglut.sourceforge.net/

Also, it uses these functions. If you don't have a Mac, then they
probably are not defined. Build the FreeGlut static library
(freeglut_static) and also link it to Void.

Lastly, the Makefile includes pre-compilation steps to get the
resources into the programme. If you want to make it without the
Makefile, perhaps in some other environment, you need to first
compile the helper programmes in tools. These convert all the
bitmaps, shaders, and info into a .h C file format so it can be
compiled statically into the programme. See the Makefile for
dependencies. However, these tools are not entirely Ansi; they make
use of some Gnu extensions. More on building is in build/.

Void Copyright 2000, 2012 Neil Edelman This program comes with
ABSOLUTELY NO WARRANTY.  This is free software, and you are welcome
to redistribute it under certain conditions; see copying.txt.

Version 3.3, 2015-11:

Events, gates, zones.

Version 3.2, 2015-05:

Improved running time O(n^2) -> O(n+m). Collisions. Subdirectory
structure.

Version 3.1, 2013:

GL_LUMINANCE is not a thing anymore; worked around that. Should
work on more machines.  Improved GL error reporting.

Version 3.0, 2013:

OpenGL; uses GLSL for a significant speed increase. Multi-platform.

Version 2.0, 2001:

DirectX 3.0; worked until Windows NT/98.

Version 1.0, 2000:

ModeX; lost the source code.
