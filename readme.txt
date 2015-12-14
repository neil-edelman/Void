Copyright (C) 2000, 2013 Neil Edelman, see copying.txt.
neil dot edelman each mail dot mcgill dot ca

Image credit: NASA; JPL; ESA; Caltech; UCLA; MPS; DLR; IDA; Johns
Hopkins University APL; Carnegie Institution of Washington; DSS
Consortium; SDSS.

src/fileformat/lodepng: Copyright (c) 2005-2015 Lode Vandevenne
http://lodev.org/lodepng/

src/fileformat/nanojpeg: by Martin J. Fiedler <martin.fiedler@gmx.net>
http://keyj.emphy.de/nanojpeg/

Version 3.2.

Usage: Void

To win, blow up everything.

Player controls: left, right, up, space, F1 (fullscreen.)

If you downloaded the source code and want to build, you need a C
compiler, OpenGL 1.1, and GLUT. If you have a Mac, xCode should do
it.

https://www.opengl.org/
http://freeglut.sourceforge.net/

I do my primary development on Mac10.6.8. To build, type make. If
you don't use GCC, and even if you do, you may need to tweak it to
compile for your system. See unix.txt and msvc2010.txt. I tried
getting it to compile in MinGW, but gave up. If you want to try to
make it without the Makefile in some other environment, you need
to first compile the helper programmes in tools. These convert all
the bitmaps, shaders, and info into a .h C file format so it can
be compiled statically into the programme (see the Makefile for
dependencies.)

Void Copyright 2000, 2012 Neil Edelman This program comes with
ABSOLUTELY NO WARRANTY.  This is free software, and you are welcome
to redistribute it under certain conditions; see copying.txt.
