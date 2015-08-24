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

# files in sdir
FILES := EntryPosix $(GEN)/Image $(GEN)/ArrayList $(GEN)/Map $(GEN)/Sorting $(SYS)/Draw $(SYS)/Glew $(SYS)/Timer $(SYS)/Key $(SYS)/Window $(GME)/Light $(GME)/Game $(GME)/Sprite $(GME)/Background $(GME)/Debris $(GME)/Ship $(GME)/Wmd $(GME)/Resources
VS   := $(SDR)/Texture $(SDR)/Lighting $(SDR)/Background
FS   := $(SDR)/Texture $(SDR)/Lighting $(SDR)/Background
ICON := icon.ico

# files in mdir
RES_F := $(MDIR)/Resources.tsv
BMP   := Ngc4038_4039 Dorado Asteroid Nautilus Scorpion Mercury Venus Pluto
TSV   := type_of_object objects_in_space alignment ship_class

# files in bdir
RSRC  := icon.rsrc
INST  := $(PROJ)-$(VA)_$(VB)

# extra stuff we should back up
EXTRA := $(SDIR)/icon.rc todo.txt msvc2010.txt unix.txt performance.txt $(TDIR)/Text2h/Makefile $(TDIR)/Text2h/Text2h.c $(TDIR)/Bmp2h/Makefile $(TDIR)/Bmp2h/Bmp2h.c $(TDIR)/Bmp2h/Bitmap.c $(TDIR)/Bmp2h/Bitmap.h $(TDIR)/Tsv2h/Makefile $(TDIR)/Tsv2h/Tsv2h.c $(TDIR)/Automator/Automator.c $(TDIR)/Automator/Makefile $(TDIR)/Media2h/Makefile $(TDIR)/Media2h/Reader.c $(TDIR)/Media2h/Reader.h $(TDIR)/Media2h/Media2h.c $(TDIR)/Media2h/Type.c $(TDIR)/Media2h/Type.h $(TDIR)/Media2h/Record.c $(TDIR)/Media2h/Record.h $(TDIR)/Media2h/Lore.c $(TDIR)/Media2h/Lore.h $(TDIR)/Media2h/Functions.h $(TDIR)/Media2h/Error.c $(TDIR)/Media2h/Error.h $(SDIR)/test/SortingTest.c $(SDIR)/test/Collision.c

OBJS  := $(patsubst %,$(BDIR)/%.o,$(FILES))
SRCS  := $(patsubst %,$(SDIR)/%.c,$(FILES))
H     := $(patsubst %,$(SDIR)/%.h,$(FILES))
VS_VS := $(patsubst %,$(SDIR)/%.vs,$(VS))
FS_FS := $(patsubst %,$(SDIR)/%.fs,$(FS))
VS_H  := $(patsubst %,$(BDIR)/%_vs.h,$(VS))
FS_H  := $(patsubst %,$(BDIR)/%_fs.h,$(FS))
BMP_BMP:=$(patsubst %,$(MDIR)/%.bmp,$(BMP))
BMP_TXT:=$(patsubst %,$(MDIR)/%.txt,$(BMP))
BMP_H :=$(patsubst %,$(BDIR)/%_bmp.h,$(BMP))
TSV_TSV:=$(patsubst %,$(MDIR)/%.tsv,$(TSV))
TSV_H :=$(patsubst %,$(BDIR)/%_tsv.h,$(TSV))

TEXT2H_DIR := tools/Text2h
TEXT2H_DEP := tools/Text2h/Text2h.c tools/Text2h/Makefile
TEXT2H     := tools/Text2h/bin/Text2h

BMP2H_DIR  := tools/Bmp2h
TEXT2H_DEP := tools/Bmp2h/Bmp2h.c tools/Bmp2h/Bitmap.c tools/Bmp2h/Bitmap.h tools/Bmp2h/Makefile
BMP2H      := tools/Bmp2h/bin/Bmp2h

TSV2H_DIR  := tools/Tsv2h
TSV2H_DEP  := tools/Tsv2h/Tsv2h.c tools/Tsv2h/Makefile
TSV2H      := tools/Tsv2h/bin/Tsv2h

AUTOMATOR_DIR  := tools/Automator
AUTOMATOR_DEP  := tools/Automator/Automator.c tools/Automator/Makefile
AUTOMATOR      := tools/Automator/bin/Automator

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

default: $(TEXT2H) $(BMP2H) $(TSV2H) $(AUTOMATOR) $(BDIR)/$(PROJ)
	# . . . setting icon on a Mac.
	cp $(SDIR)/$(ICON) $(BDIR)/$(ICON)
	sips --addIcon $(BDIR)/$(ICON)
	DeRez -only icns $(BDIR)/$(ICON) > $(BDIR)/$(RSRC)
	Rez -append $(BDIR)/$(RSRC) -o $(BDIR)/$(PROJ)
	SetFile -a C $(BDIR)/$(PROJ)
	# . . . success; executable is in $(BDIR)/$(PROJ)

# linking
$(BDIR)/$(PROJ): $(VS_H) $(FS_H) $(BMP_H) $(TSV_H) $(BDIR)/Automator.c $(OBJS)
	$(CC) $(CF) $(OF) $(OBJS) -o $@

# compiling
$(OBJS): $(BDIR)/%.o: $(SDIR)/%.c $(VS_VS) $(FS_FS) $(H)
	@mkdir -p $(BDIR)
	@mkdir -p $(BDIR)/$(SYS)
	@mkdir -p $(BDIR)/$(GEN)
	@mkdir -p $(BDIR)/$(GME)
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

$(BDIR)/%_bmp.h: $(MDIR)/%.bmp $(BMP2H)
	# . . . pictures into headers.
	@mkdir -p $(BDIR)
	$(BMP2H) $< > $@

$(BDIR)/%_tsv.h: $(MDIR)/%.tsv $(TSV2H)
	# . . . text resources into headers.
	@mkdir -p $(BDIR)
	$(TSV2H) $(RES_F) $< > $@

$(BDIR)/Automator.c: $(AUTOMATOR_DIR)/Automator.c $(BMP_BMP)
	# . . . making $(BDIR)/Automator.c
	@mkdir -p $(BDIR)
	$(AUTOMATOR) $(BDIR) > $@

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

$(BMP2H): $(BMP2H_DEP)
	@echo . . . compiling Bmp2h.
	make --directory $(BMP2H_DIR)

$(TSV2H): $(TSV2H_DEP)
	@echo . . . compiling Tsv2h.
	make --directory $(TSV2H_DIR)

$(AUTOMATOR): $(AUTOMATOR_DEP)
	@echo . . . compiling Automator.
	make --directory $(AUTOMATOR_DIR)

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
	-make --directory $(BMP2H_DIR) clean
	-make --directory $(TSV2H_DIR) clean
	-make --directory $(AUTOMATOR_DIR) clean
	# *.h is a hack
	-rm -fd $(OBJS) $(BDIR)/$(ICON) $(BDIR)/$(RSRC) $(BDIR)/*.h $(VS_H) $(FS_H) $(BDIR)/sort $(BDIR)/cd $(BDIR)/$(SYS) $(BDIR)/$(GEN) $(BDIR)/$(GME) $(BDIR)/$(SDR) $(BDIR)/Automator.c

backup:
	@mkdir -p $(BACK)
	zip $(BACK)/$(INST)-`date +%Y-%m-%dT%H%M%S`$(BRGS).zip readme.txt gpl.txt copying.txt Makefile $(SRCS) $(H) $(SDIR)/$(ICON) $(VS_VS) $(FS_FS) $(RES_F) $(TSV_TSV) $(EXTRA) $(BMP_TXT) # $(BMP_BMP) (that last one is too large)

setup: default
	@mkdir -p $(BDIR)/$(INST)
	cp $(BDIR)/$(PROJ) readme.txt gpl.txt copying.txt $(BDIR)/$(INST)
	rm -f $(BDIR)/$(INST)-MacOSX.dmg
	hdiutil create $(BDIR)/$(INST)-MacOSX.dmg -volname "$(PROJ) $(VA).$(VB)" -srcfolder $(BDIR)/$(INST)
	rm -R $(BDIR)/$(INST)
