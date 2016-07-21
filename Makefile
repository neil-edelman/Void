# Makefile 1.1 (GNU Make 3.81; MacOSX gcc 4.2.1; MacOSX MinGW 4.3.0)

PROJ  := Void
VA    := 3
VB    := 3

# dirs
SDIR  := src
BDIR  := bin
GDIR  := build
TDIR  := tools
MDIR  := media
BACK  := backup

SYS := system
SDR := shaders
GEN := general
GME := game
FMT := format

# files in mdir
ICON := icon.ico

# files in bdir (RSRC is Windows icon, made programmatically)
RSRC  := icon.rsrc
INST  := $(PROJ)-$(VA)_$(VB)

MSVC := build/msvc2013

# extra stuff we should back up
EXTRA := $(MDIR)/icon.rc todo.txt build/msvc2010.txt build/unix.txt performance.txt tests/TimerIsTime.c tests/SortingTest.c tests/Collision.c tests/Fileformat/Makefile tests/Fileformat/src/Asteroid_png.h tests/Fileformat/src/Pluto_jpeg.h tests/Fileformat/src/Fileformat.c $(MSVC)/Void.sln $(MSVC)/Void.v12.suo $(MSVC)/Void.vcxproj $(MSVC)/Void.vcxproj.filters $(MSVC)/Void.vcxproj.user build/Makefile-Redhat #$(MSVC)/Void.sdf

# John Graham-Cumming:
# rwildcard is a recursive wildcard
rwildcard=$(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))

# select all the things in the src/; the Lore is generated automatically by a tool
SRCS  := $(call rwildcard, $(SDIR), *.c) $(SDIR)/../$(GDIR)/Auto.c
H     := $(call rwildcard, $(SDIR), *.h) $(SDIR)/../$(GDIR)/Auto.h
OBJS  := $(patsubst $(SDIR)/%.c, $(GDIR)/%.o, $(SRCS))
VS    := $(call rwildcard, $(SDIR)/$(SDR), *.vs)
FS    := $(call rwildcard, $(SDIR)/$(SDR), *.fs)
VS_H  := $(patsubst $(SDIR)/$(SDR)/%.vs, $(GDIR)/$(SDR)/%_vs.h, $(VS))
FS_H  := $(patsubst $(SDIR)/$(SDR)/%.fs, $(GDIR)/$(SDR)/%_fs.h, $(FS))

FILE2H_DIR := tools/File2h
FILE2H_DEP := tools/File2h/src/File2h.c tools/File2h/Makefile
FILE2H     := tools/File2h/bin/File2h

TEXT2H_DIR := tools/Text2h
TEXT2H_DEP := tools/Text2h/Text2h.c tools/Text2h/Makefile
TEXT2H     := tools/Text2h/bin/Text2h

LOADER_FILES := Error Loader Lore Reader Record Type
LOADER_DIR   := tools/Loader
LOADER_DEP   := $(LOADER_DIR)/Makefile $(patsubst %,$(LOADER_DIR)/$(SDIR)/%.c,$(LOADER_FILES)) $(patsubst %,$(LOADER_DIR)/$(SDIR)/%.h,$(LOADER_FILES)) $(LOADER_DIR)/$(SDIR)/Functions.h
LOADER       := tools/Loader/bin/Loader

# Loader resources
TYPE   := $(wildcard $(MDIR)/*.type)
LORE_H := $(GDIR)/Auto.h
LORE   := $(wildcard $(MDIR)/*.lore)
LORE_C := $(GDIR)/Auto.c
PNG    := $(wildcard $(MDIR)/*.png)
PNG_H  := $(patsubst $(MDIR)/%.png,$(GDIR)/%_png.h,$(PNG))
JPEG   := $(wildcard $(MDIR)/*.jpeg)
JPEG_H := $(patsubst $(MDIR)/%.jpeg,$(GDIR)/%_jpeg.h,$(JPEG))
BMP    := $(wildcard $(MDIR)/*.bmp)
BMP_H  := $(patsubst $(MDIR)/%.bmp,$(GDIR)/%_bmp.h,$(BMP))
TEXT   := $(wildcard $(MDIR)/*.txt)

CC   := gcc
CF   := -Wall -Wextra -O3 -fasm -fomit-frame-pointer -ffast-math -funroll-loops -pedantic --std=c99# -DNDEBUG# UNIX/PC: -DGLEW
OF   := -framework OpenGL -framework GLUT # UNIX: -lglut -lGLEW; PC: depends

# Jakob Borg and Eldar Abusalimov:
# $(ARGS) is all the extra arguments
# $(BRGS) is_all_the_extra_arguments
EMPTY :=
SPACE := $(EMPTY) $(EMPTY)
ifeq (backup, $(firstword $(MAKECMDGOALS)))
  ARGS := $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
  BRGS := $(subst $(SPACE),_,$(ARGS))
  ifneq (,$(BRGS))
    BRGS := -$(BRGS)
  endif
  $(eval $(ARGS):;@:)
endif

######
# compiles the programme by default

default: $(BDIR)/$(PROJ)
	# . . . success; executable is in $(BDIR)/$(PROJ)

# linking
$(BDIR)/$(PROJ): $(LORE_H) $(LORE_C) $(VS_H) $(FS_H) $(OBJS)
	$(CC) $(CF) $(OF) $(OBJS) -o $@

# compiling
$(OBJS): $(GDIR)/%.o: $(SDIR)/%.c $(VS_VS) $(FS_FS) $(H)
	@mkdir -p $(BDIR)
	@mkdir -p $(GDIR)
	@mkdir -p $(GDIR)/$(SYS)
	@mkdir -p $(GDIR)/$(GEN)
	@mkdir -p $(GDIR)/$(GME)
	@mkdir -p $(GDIR)/$(FMT)
	$(CC) $(CF) -c $(SDIR)/$*.c -o $@

$(GDIR)/$(SDR)/%_vs.h: $(SDIR)/$(SDR)/%.vs $(TEXT2H)
	# . . . vertex shaders into headers.
	@mkdir -p $(GDIR)
	@mkdir -p $(GDIR)/$(SDR)
	$(TEXT2H) $< > $@

$(GDIR)/$(SDR)/%_fs.h: $(SDIR)/$(SDR)/%.fs $(TEXT2H)
	# . . . fragment shaders into headers.
	@mkdir -p $(GDIR)
	@mkdir -p $(GDIR)/$(SDR)
	$(TEXT2H) $< > $@

$(LORE_H): $(LOADER) $(FILE2H) $(TYPE)
	# . . . resources lore.h
	@mkdir -p $(GDIR)
	$(LOADER) $(MDIR) > $(LORE_H)

$(LORE_C): $(LOADER) $(FILE2H) $(TYPE) $(LORE) $(PNG_H) $(JPEG_H) $(BMP_H)
	# . . . resources lore.c
	@mkdir -p $(GDIR)
	$(LOADER) $(MDIR) $(MDIR) > $(LORE_C)

$(GDIR)/%_png.h: $(MDIR)/%.png $(FILE2H)
	# . . . File2h png
	@mkdir -p $(GDIR)
	$(FILE2H) $< > $@

$(GDIR)/%_jpeg.h: $(MDIR)/%.jpeg $(FILE2H)
	# . . . File2h jpeg
	@mkdir -p $(GDIR)
	$(FILE2H) $< > $@

# additional dependancies

$(BDIR)/Open.o: $(VS_H) $(FS_H)

######
# helper programmes

$(TEXT2H): $(TEXT2H_DEP)
	# . . . compiling Text2h.
	make --directory $(TEXT2H_DIR)

$(FILE2H): $(FILE2H_DEP)
	@echo . . . compiling File2h.
	make --directory $(FILE2H_DIR)

$(LOADER): $(LOADER_DEP)
	@echo . . . compiling Loader.
	make --directory $(LOADER_DIR)

######
# test programmes (fixme)

$(BDIR)/sort: $(SDIR)/$(GEN)/Sorting.c $(TEST)/SortingTest.c
	$(CC) $(CF) $^ -o $(BDIR)/sort

$(BDIR)/cd: $(TEST)/Collision.c
	$(CC) $(CF) $< -o $(BDIR)/cd

$(BDIR)/time: $(TEST)/TimerIsTime.c
	$(CC) $(CF) $< -o $(BDIR)/time

######
# phoney targets

.PHONY: clean backup source setup icon

clean:
	-make --directory $(TEXT2H_DIR) clean
	-make --directory $(FILE2H_DIR) clean
	-make --directory $(LOADER_DIR) clean
	-rm -f $(OBJS) $(BDIR)/$(RSRC) $(VS_H) $(FS_H) $(TEXT2H) $(FILE2H) $(LOADER) $(BDIR)/sort $(BDIR)/cd $(LORE_H) $(LORE_C) $(PNG_H) $(JPEG_H) $(BMP_H)
	-rm -rf $(BDIR)/$(SYS) $(BDIR)/$(GEN) $(BDIR)/$(GME) $(BDIR)/$(SDR) $(BDIR)/$(FMT)

BACKUP := readme.txt gpl.txt copying.txt Makefile $(SRCS) $(H) $(MDIR)/$(ICON) $(VS) $(FS) $(EXTRA) $(TEXT2H_DEP) $(FILE2H_DEP) $(LOADER_DEP) $(TYPE) $(LORE) $(TEXT)

backup:
	@mkdir -p $(BACK)
	zip $(BACK)/$(INST)-`date +%Y-%m-%dT%H%M%S`$(BRGS).zip $(BACKUP)

# this backs up everything including the media files for publication
source:
	@mkdir -p $(BDIR)
	zip $(BDIR)/$(INST)-`date +%Y-%m-%dT%H%M%S`.zip $(BACKUP) $(PNG) $(JPEG)

icon: default
	# . . . setting icon on a Mac.
	cp $(MDIR)/$(ICON) $(GDIR)/$(ICON)
	-sips --addIcon $(GDIR)/$(ICON)
	-DeRez -only icns $(GDIR)/$(ICON) > $(GDIR)/$(RSRC)
	-Rez -append $(GDIR)/$(RSRC) -o $(BDIR)/$(PROJ)
	-SetFile -a C $(BDIR)/$(PROJ)

setup: default icon
	# . . . setup on a Mac.
	@mkdir -p $(BDIR)/$(INST)
	cp $(BDIR)/$(PROJ) readme.txt gpl.txt copying.txt $(BDIR)/$(INST)
	rm -f $(BDIR)/$(INST)-MacOSX.dmg
	hdiutil create $(BDIR)/$(INST)-MacOSX.dmg -volname "$(PROJ) $(VA).$(VB)" -srcfolder $(BDIR)/$(INST)
	rm -R $(BDIR)/$(INST)
