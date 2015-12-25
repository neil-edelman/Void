# Makefile 1.1 (GNU Make 3.81; MacOSX gcc 4.2.1; MacOSX MinGW 4.3.0)

PROJ  := Void
VA    := 3
VB    := 3

# dirs
SDIR  := src
BDIR  := bin
TDIR  := tools
MDIR  := media
BACK  := backup

SYS := system
SDR := shaders
GEN := general
GME := game
FMT := format

# files in sdir
ICON := icon.ico

# files in bdir (RSRC is Windows icon, made programmatically)
RSRC  := icon.rsrc
INST  := $(PROJ)-$(VA)_$(VB)

MSVC := build/msvc2013

# extra stuff we should back up
EXTRA := $(SDIR)/icon.rc todo.txt build/msvc2010.txt unix.txt performance.txt tests/SortingTest.c tests/Collision.c tests/Fileformat/Makefile tests/Fileformat/src/Asteroid_png.h tests/Fileformat/src/Pluto_jpeg.h tests/Fileformat/src/Fileformat.c $(MSVC)/Void.sln $(MSVC)/Void.v12.suo $(MSVC)/Void.vcxproj $(MSVC)/Void.vcxproj.filters $(MSVC)/Void.vcxproj.user build/Makefile-Redhat #$(MSVC)/Void.sdf

# John Graham-Cumming:
# rwildcard is a recursive wildcard
rwildcard=$(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))

# select all the things in the src/; the Lore is generated automatically by a tool
SRCS  := $(call rwildcard, $(SDIR), *.c) $(SDIR)/../$(BDIR)/Lore.c
H     := $(call rwildcard, $(SDIR), *.h) $(SDIR)/../$(BDIR)/Lore.h
OBJS  := $(patsubst $(SDIR)/%.c, $(BDIR)/%.o, $(SRCS))
VS    := $(call rwildcard, $(SDIR)/$(SDR), *.vs)
FS    := $(call rwildcard, $(SDIR)/$(SDR), *.fs)
VS_H  := $(patsubst $(SDIR)/$(SDR)/%.vs, $(BDIR)/$(SDR)/%_vs.h, $(VS))
FS_H  := $(patsubst $(SDIR)/$(SDR)/%.fs, $(BDIR)/$(SDR)/%_fs.h, $(FS))

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
LORE_H := $(BDIR)/Lore.h
LORE   := $(wildcard $(MDIR)/*.lore)
LORE_C := $(BDIR)/Lore.c
PNG    := $(wildcard $(MDIR)/*.png)
PNG_H  := $(patsubst $(MDIR)/%.png,$(BDIR)/%_png.h,$(PNG))
JPEG   := $(wildcard $(MDIR)/*.jpeg)
JPEG_H := $(patsubst $(MDIR)/%.jpeg,$(BDIR)/%_jpeg.h,$(JPEG))
BMP    := $(wildcard $(MDIR)/*.bmp)
BMP_H  := $(patsubst $(MDIR)/%.bmp,$(BDIR)/%_bmp.h,$(BMP))
TEXT   := $(wildcard $(MDIR)/*.txt)

CC   := gcc
CF   := -Wall -Wextra -O3 -fasm -fomit-frame-pointer -ffast-math -funroll-loops -pedantic --std=c99 # UNIX/PC: -DGLEW
OF   := -framework OpenGL -framework GLUT # UNIX: -lglut -lGLEW; PC: depends

# Jakob Borg and Eldar Abusalimov:
# $(ARGS) is all the extra arguments
# $(BRGS) is_all_the_extra_arguments
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
$(OBJS): $(BDIR)/%.o: $(SDIR)/%.c $(VS_VS) $(FS_FS) $(H)
	@mkdir -p $(BDIR)
	@mkdir -p $(BDIR)/$(SYS)
	@mkdir -p $(BDIR)/$(GEN)
	@mkdir -p $(BDIR)/$(GME)
	@mkdir -p $(BDIR)/$(FMT)
	$(CC) $(CF) -c $(SDIR)/$*.c -o $@

$(BDIR)/$(SDR)/%_vs.h: $(SDIR)/$(SDR)/%.vs $(TEXT2H)
	# . . . vertex shaders into headers.
	@mkdir -p $(BDIR)
	@mkdir -p $(BDIR)/$(SDR)
	$(TEXT2H) $< > $@

$(BDIR)/$(SDR)/%_fs.h: $(SDIR)/$(SDR)/%.fs $(TEXT2H)
	# . . . fragment shaders into headers.
	@mkdir -p $(BDIR)
	@mkdir -p $(BDIR)/$(SDR)
	$(TEXT2H) $< > $@

$(LORE_H): $(LOADER) $(FILE2H) $(TYPE)
	# . . . resources lore.h
	@mkdir -p $(BDIR)
	$(LOADER) $(MDIR) > $(LORE_H)

$(LORE_C): $(LOADER) $(FILE2H) $(TYPE) $(LORE) $(PNG_H) $(JPEG_H) $(BMP_H)
	# . . . resources lore.c
	@mkdir -p $(BDIR)
	$(LOADER) $(MDIR) $(MDIR) > $(LORE_C)

$(BDIR)/%_png.h: $(MDIR)/%.png $(FILE2H)
	# . . . File2h png
	@mkdir -p $(BDIR)
	$(FILE2H) $< > $@

$(BDIR)/%_jpeg.h: $(MDIR)/%.jpeg $(FILE2H)
	# . . . File2h jpeg
	@mkdir -p $(BDIR)
	$(FILE2H) $< > $@

$(BDIR)/%_bmp.h: $(MDIR)/%.bmp $(FILE2H)
	# . . . File2h bmp
	@mkdir -p $(BDIR)
	$(FILE2H) $< > $@

# additional dependancies

$(BDIR)/Open.o: $(VS_H) $(FS_H)

$(BDIR)/Game.o: $(TSV_H)

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
# test programmes

$(BDIR)/sort: $(SDIR)/$(GEN)/Sorting.c $(SDIR)/test/SortingTest.c
	$(CC) $(CF) $^ -o $(BDIR)/sort

$(BDIR)/cd: $(SDIR)/test/Collision.c
	$(CC) $(CF) $< -o $(BDIR)/cd

######
# phoney targets

.PHONY: clean backup source setup icon

clean:
	-make --directory $(TEXT2H_DIR) clean
	-make --directory $(FILE2H_DIR) clean
	-make --directory $(LOADER_DIR) clean
	-rm -f $(OBJS) $(BDIR)/$(ICON) $(BDIR)/$(RSRC) $(VS_H) $(FS_H) $(TEXT2H) $(FILE2H) $(LOADER) $(BDIR)/sort $(BDIR)/cd $(LORE_H) $(LORE_C) $(PNG_H) $(JPEG_H) $(BMP_H)
	-rm -rf $(BDIR)/$(SYS) $(BDIR)/$(GEN) $(BDIR)/$(GME) $(BDIR)/$(SDR) $(BDIR)/$(FMT)

backup:
	@mkdir -p $(BACK)
	zip $(BACK)/$(INST)-`date +%Y-%m-%dT%H%M%S`$(BRGS).zip readme.txt gpl.txt copying.txt Makefile $(SRCS) $(H) $(SDIR)/$(ICON) $(VS) $(FS) $(RES_F) $(EXTRA) $(BMP_TXT) $(TEXT2H_DEP) $(FILE2H_DEP) $(LOADER_DEP)

# this backs up everything including the media files for publication (excuding)
source: #$(LORE_H) $(LORE_C) $(VS_H) $(FS_H) $(PNG_H) $(JPEG_H) $(BMP_H)
	@mkdir -p $(BDIR)
	zip $(BDIR)/$(INST)-`date +%Y-%m-%dT%H%M%S`.zip readme.txt gpl.txt copying.txt Makefile $(SRCS) $(H) $(SDIR)/$(ICON) $(VS_VS) $(FS_FS) $(RES_F) $(TSV_TSV) $(EXTRA) $(BMP_TXT) $(TEXT2H_DEP) $(FILE2H_DEP) $(LOADER_DEP) $(TYPE) $(LORE) $(PNG) $(JPEG) $(BMP) $(TEXT) $(VS) $(FS) #$(LORE_H) $(LORE_C) $(VS_H) $(FS_H) $(PNG_H) $(JPEG_H) $(BMP_H) #too big

icon: default
	# . . . setting icon on a Mac.
	cp $(SDIR)/$(ICON) $(BDIR)/$(ICON)
	-sips --addIcon $(BDIR)/$(ICON)
	-DeRez -only icns $(BDIR)/$(ICON) > $(BDIR)/$(RSRC)
	-Rez -append $(BDIR)/$(RSRC) -o $(BDIR)/$(PROJ)
	-SetFile -a C $(BDIR)/$(PROJ)

setup: default icon
	# . . . setup on a Mac.
	@mkdir -p $(BDIR)/$(INST)
	cp $(BDIR)/$(PROJ) readme.txt gpl.txt copying.txt $(BDIR)/$(INST)
	rm -f $(BDIR)/$(INST)-MacOSX.dmg
	hdiutil create $(BDIR)/$(INST)-MacOSX.dmg -volname "$(PROJ) $(VA).$(VB)" -srcfolder $(BDIR)/$(INST)
	rm -R $(BDIR)/$(INST)
