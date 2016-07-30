CC := mingw32-gcc
CF := -Wall -Wextra -O3 -fasm -fomit-frame-pointer -ffast-math -funroll-loops -pedantic --std=c99 -DGLEW
OF := -L\glew-2.0.0\bin\Release\Win32 -lglew32 -L\freeglut-3.0.0-MinGW\bin -lfreeglut
MAKE  := mingw32-make
MKDIR := mkdir
RM    := del
RMDIR := del
ZIP   := zip
CP    := copy
