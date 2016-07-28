CC := mingw32-gcc
CF := -Wall -Wextra -O3 -fasm -fomit-frame-pointer -ffast-math -funroll-loops -pedantic --std=c99 -DGLEW
OF := glut32.lib -lopengl32.lib -lGLEW
