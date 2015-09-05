# Makefile 1.1 (GNU Make 3.81; MacOSX gcc 4.2.1; MacOSX MinGW 4.3.0)

PROJ  := Void
VA    := 3
VB    := 2

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
FILES := ../bin/Lore EntryPosix $(GEN)/ArrayList $(GEN)/Map $(GEN)/Sorting $(SYS)/Draw $(SYS)/Glew $(SYS)/Timer $(SYS)/Key $(SYS)/Window $(GME)/Light $(GME)/Game $(GME)/Sprite $(GME)/Far $(GME)/Debris $(GME)/Ship $(GME)/Wmd $(FMT)/Bitmap $(FMT)/lodepng $(FMT)/nanojpeg
VS   := $(SDR)/Texture $(SDR)/Lighting $(SDR)/Background
FS   := $(SDR)/Texture $(SDR)/Lighting $(SDR)/Background
ICON := icon.ico

# files in mdir
# holy cow, we'll just backup all; the Loader takes care of it

# files in bdir
RSRC  := icon.rsrc
INST  := $(PROJ)-$(VA)_$(VB)

# extra stuff we should back up
EXTRA := $(SDIR)/icon.rc todo.txt msvc2010.txt unix.txt performance.txt $(SDIR)/test/SortingTest.c $(SDIR)/test/Collision.c $(SDIR)/test/Fileformat/Makefile $(SDIR)/test/Fileformat/src/Asteroid_png.h $(SDIR)/test/Fileformat/src/Pluto_jpeg.h $(SDIR)/test/Fileformat/src/Fileformat.c

OBJS  := $(patsubst %,$(BDIR)/%.o,$(FILES))
SRCS  := $(patsubst %,$(SDIR)/%.c,$(FILES))
H     := $(patsubst %,$(SDIR)/%.h,$(FILES))
VS_VS := $(patsubst %,$(SDIR)/%.vs,$(VS))
FS_FS := $(patsubst %,$(SDIR)/%.fs,$(FS))
VS_H  := $(patsubst %,$(BDIR)/%_vs.h,$(VS))
FS_H  := $(patsubst %,$(BDIR)/%_fs.h,$(FS))
TSV_TSV:=$(patsubst %,$(MDIR)/%.tsv,$(TSV))
TSV_H :=$(patsubst %,$(BDIR)/%_tsv.h,$(TSV))

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

# Loader recouces
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

CC   := gcc
CF   := -Wall -O3 -fasm -fomit-frame-pointer -ffast-math -funroll-loops -pedantic -ansi #-std=c99 # ansi doesn't have fmath fn's # UNIX/PC: -DGLEW
OF   := -framework OpenGL -framework GLUT # UNIX: -lglut -lGLEW PC: depends

# props Jakob Borg and Eldar Abusalimov
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
	# . . . setting icon on a Mac.
	cp $(SDIR)/$(ICON) $(BDIR)/$(ICON)
	sips --addIcon $(BDIR)/$(ICON)
	DeRez -only icns $(BDIR)/$(ICON) > $(BDIR)/$(RSRC)
	Rez -append $(BDIR)/$(RSRC) -o $(BDIR)/$(PROJ)
	SetFile -a C $(BDIR)/$(PROJ)
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

$(BDIR)/%_vs.h: $(SDIR)/%.vs $(TEXT2H)
	# . . . vertex shaders into headers.
	@mkdir -p $(BDIR)
	@mkdir -p $(BDIR)/$(SDR)
	$(TEXT2H) $< > $@

$(BDIR)/%_fs.h: $(SDIR)/%.fs $(TEXT2H)
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

$(BDIR)/Open.o: $(VS_H) $(FS_H) $(SDIR)/Open.c
	# . . . compiling Void (OpenGL)
	@mkdir -p $(BDIR)
	$(CC) $(CF) -c $(SDIR)/Open.c -o $@

$(BDIR)/Game.o: $(TSV_H) $(SDIR)/Game.c
	# . . . compiling Void (Game)
	@mkdir -p $(BDIR)
	$(CC) $(CF) -c $(SDIR)/Game.c -o $@	

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

.PHONY: clean backup setup

clean:
	-make --directory $(TEXT2H_DIR) clean
	-make --directory $(FILE2H_DIR) clean
	-make --directory $(LOADER_DIR) clean
	-rm -fd $(OBJS) $(BDIR)/$(ICON) $(BDIR)/$(RSRC) $(VS_H) $(FS_H) $(TEXT2H) $(FILE2H) $(LOADER) $(BDIR)/sort $(BDIR)/cd $(BDIR)/$(SYS) $(BDIR)/$(GEN) $(BDIR)/$(GME) $(BDIR)/$(SDR) $(BDIR)/$(FMT) $(LORE_H) $(LORE_C) $(PNG_H) $(JPEG_H) $(BMP_H)

backup:
	@mkdir -p $(BACK)
	zip $(BACK)/$(INST)-`date +%Y-%m-%dT%H%M%S`$(BRGS).zip readme.txt gpl.txt copying.txt Makefile $(SRCS) $(H) $(SDIR)/$(ICON) $(VS_VS) $(FS_FS) $(RES_F) $(TSV_TSV) $(EXTRA) $(BMP_TXT) $(TEXT2H_DEP) $(FILE2H_DEP) $(LOADER_DEP) #$(MDIR)/*.*

setup: default
	@mkdir -p $(BDIR)/$(INST)
	cp $(BDIR)/$(PROJ) readme.txt gpl.txt copying.txt $(BDIR)/$(INST)
	rm -f $(BDIR)/$(INST)-MacOSX.dmg
	hdiutil create $(BDIR)/$(INST)-MacOSX.dmg -volname "$(PROJ) $(VA).$(VB)" -srcfolder $(BDIR)/$(INST)
	rm -R $(BDIR)/$(INST)
