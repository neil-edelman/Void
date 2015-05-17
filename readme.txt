Copyright (C) 2000, 2013 Neil Edelman, see copying.txt.
neil dot edelman each mail dot mcgill dot ca

Version 3.2.

Usage: Void

To win, blow up everything.

Player controls: left, right, up, down, space
Window controls: F1 fullscreen

To build this, you need a C compiler, OpenGL 1.1, and GLUT.

https://www.opengl.org/
http://freeglut.sourceforge.net/

I do my primary development on Mac10.6.8. To build, type make. If you don't use GCC,
and even if you do, you may need to tweak it to compile for your system. See unix.txt
and msvc2010.txt. I tried getting it to compile in MinGW, but gave up. If you want to
try to make it without the Makefile in some other environment, you need to first
compile the helper programmes in tools. These convert all the bitmaps, shaders, and
info into a .h C file format so it can be compiled statically into the programme (see
the Makefile for dependencies.) Void itself is pretty flexible, conforming to ANSI
where possible, POSIX where possible.

Void Copyright 2000, 2012 Neil Edelman
This program comes with ABSOLUTELY NO WARRANTY.
This is free software, and you are welcome to redistribute it
under certain conditions; see copying.txt.

Version 3.2:

Getting back to it in 2015.

Version 3.1:

GL_LUMINANCE is not a thing anymore; worked around that. Should work on more machines.
Improved GL error reporting.

Version 3.0:

OpenGL; uses GLSL for a significant speed increase. Multi-platform.

Version 2.0:

DirectX 3.0; worked until Windows NT/98.

Version 1.0:

ModeX; lost the source code.
