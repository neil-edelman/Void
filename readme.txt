Copyright (C) 2000, 2013 Neil Edelman, see copying.txt.  neil dot
edelman each mail dot mcgill dot ca

Usage: Void

To win, blow up everything.

This is an ongoing project that I work on sporadically. A game is
complex and resource-heavy enough that I try most of my programming
experiments here, as well as learning GIMP and Blender. However,
it is kind of fun for about 5 minutes. Player controls: left, right,
up, space, F1 -- fullscreen, x -- report position, f -- framerate,
p -- pause, 1 -- Gnuplot dump.

Image credit: NASA; JPL; ESA; Caltech; UCLA; MPS; DLR; IDA; Johns
Hopkins University APL; Carnegie Institution of Washington; DSS
Consortium; SDSS.

external/lodepng: Copyright (c) 2005-2015 Lode Vandevenne
http://lodev.org/lodepng/

external/nanojpeg: by Martin J. Fiedler <martin.fiedler@gmx.net>
http://keyj.emphy.de/nanojpeg/

If you want to build this programme, it uses GLUT and GLEW libraries.
Except MacOSX; FreeGLUT is useless in Mac because it starts X11,
but 1998GLUT is built in and works natively. GLEW is also not needed
because MacOSX has native support of OpenGL1.1. Other build
environments, we must define a preprocessor variable GLEW before
compiling (ie -DGLEW) to enable the code that loads all the libraries.

Recommend building the static release of Glew (glew32s) and link
it to Void. I have tested it on MSVC10/13/15 and gcc Ubuntu (still
trying MinGW.) Make sure you turn off "Whole Program Optimization,"
on MSVC as it will fail to make Glew. 'glew' in package managers.

http://glew.sourceforge.net/

Recommend also building the FreeGlut static library (freeglut_static.)
'freeglut' in package managers.

http://freeglut.sourceforge.net/

The main programme code is C90 and should work with any C compiler
after 1989.  However, pre-compiling resources into generated code
needs POSIX. The resource loader is a hack; it's not really important
for me to fix it because it's just a pre-compiling step. Type 'make'
on a system that supports GNU99 extensions, (also <make icon>, <make
install> can be useful on a Mac.) MSVC is the obvious compiler with
lack POSIX support; to make on that system, download a GCC, make,
and then import all the files, including the .c and .h in the build/
to MSVC.

Void Copyright 2000, 2012 Neil Edelman This program comes with
ABSOLUTELY NO WARRANTY.  This is free software, and you are welcome
to redistribute it under certain conditions; see copying.txt.

Version 2018-01:

Got rid of the version numbers.

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
