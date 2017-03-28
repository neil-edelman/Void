# Makefile 1.1 (GNU Make 3.81; MacOSX gcc 4.2.1; MacOSX MinGW 4.3.0)

PROJ  := Void
VA    := 3
VB    := 3

MAKE  := make
MKDIR := mkdir -p

# dirs
src     := src
bin     := bin
build   := build
media   := media
backup  := backup
doc     := doc
external:= external
tools   := tools

system  := system
shaders := shaders
general := general
game    := game

PREFIX  := /usr/local

# files in mdir
ICON := icon.ico

# files in bdir (RSRC is Windows icon, made programmatically)
RSRC  := icon.rsrc
INST  := $(PROJ)-$(VA)_$(VB)

MSVC := build/msvc2013

# extra stuff we should back up
EXTRA := $(media)/icon.rc todo.txt build/msvc2010.txt build/unix.txt \
performance.txt tests/TimerIsTime.c tests/SortingTest.c tests/Collision.c \
tests/Fileformat/Makefile tests/Fileformat/src/Asteroid_png.h \
tests/Fileformat/src/Pluto_jpeg.h tests/Fileformat/src/Fileformat.c \
$(MSVC)/Void.sln $(MSVC)/Void.v12.suo $(MSVC)/Void.vcxproj \
$(MSVC)/Void.vcxproj.filters $(MSVC)/Void.vcxproj.user \
build/Makefile-Redhat

# John Graham-Cumming: recursive wildcard
rwildcard=$(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) \
$(filter $(subst *,%,$2),$d))

# C files; Auto is generated automatically by a tool
SRCS  := $(call rwildcard,$(src),*.c) $(src)/../$(build)/Auto.c
SRCSH := $(call rwildcard,$(src),*.h) $(src)/../$(build)/Auto.h
SRCSO := $(patsubst $(src)/%.c, $(build)/%.o, $(SRCS))
# these have laxer warnings because they aren't mine
EXTS  := $(call rwildcard, $(external), *.c)
EXTSH := $(call rwildcard, $(external), *.h)
EXTSO := $(patsubst $(external)/%.c, $(build)/$(external)/%.o, $(EXTS))
# shaders
VS    := $(call rwildcard, $(shaders), *.vs)
FS    := $(call rwildcard, $(shaders), *.fs)
VS_H  := $(patsubst $(shaders)/%.vs, $(build)/$(shaders)/%_vs.h, $(VS))
FS_H  := $(patsubst $(shaders)/%.fs, $(build)/$(shaders)/%_fs.h, $(FS))
# documentation
DOCS  := $(patsubst $(src)/%.c, $(doc)/%.html, $(SRCS))

# $(tools): we need to build these first

FILE2H_DIR := $(tools)/File2h
FILE2H_DEP := $(tools)/File2h/src/File2h.c $(tools)/File2h/Makefile
FILE2H     := $(tools)/File2h/bin/File2h

TEXT2H_DIR := $(tools)/Text2h
TEXT2H_DEP := $(tools)/Text2h/Text2h.c $(tools)/Text2h/Makefile
TEXT2H     := $(tools)/Text2h/bin/Text2h

LOADER_FILES := Error Loader Lore Reader Record Type
LOADER_DIR   := $(tools)/Loader
LOADER_DEP   := $(LOADER_DIR)/Makefile \
$(patsubst %,$(LOADER_DIR)/$(src)/%.c,$(LOADER_FILES)) \
$(patsubst %,$(LOADER_DIR)/$(src)/%.h,$(LOADER_FILES)) \
$(LOADER_DIR)/$(src)/Functions.h
LOADER       := $(tools)/Loader/bin/Loader

# Loader resources
TYPE   := $(wildcard $(media)/*.type)
LORE_H := $(build)/Auto.h
LORE   := $(wildcard $(media)/*.lore)
LORE_C := $(build)/Auto.c
PNG    := $(wildcard $(media)/*.png)
PNG_H  := $(patsubst $(media)/%.png,$(build)/%_png.h,$(PNG))
JPEG   := $(wildcard $(media)/*.jpeg)
JPEG_H := $(patsubst $(media)/%.jpeg,$(build)/%_jpeg.h,$(JPEG))
BMP    := $(wildcard $(media)/*.bmp)
BMP_H  := $(patsubst $(media)/%.bmp,$(build)/%_bmp.h,$(BMP))
TEXT   := $(wildcard $(media)/*.txt)

# just guess
CC    := gcc
CDOC  := cdoc
MAKE  := make
MKDIR := mkdir -p
RM    := rm -f
RMDIR := rm -rf
ZIP   := zip
CP    := cp

# flags
CF    := -Wall -Wextra -Wno-format-y2k -Wstrict-prototypes \
-Wmissing-prototypes -Wpointer-arith -Wreturn-type -Wcast-qual -Wwrite-strings \
-Wswitch -Wshadow -Wcast-align -Wbad-function-cast -Wchar-subscripts -Winline \
-Wnested-externs -Wredundant-decls -O3 -fasm -fomit-frame-pointer -ffast-math \
-funroll-loops -pedantic -ansi
CF_LAX:= -Wall -Wextra -O3 -fasm -fomit-frame-pointer -ffast-math \
-funroll-loops -pedantic -std=c99
OF    := -framework OpenGL -framework GLUT

# user-defined variable TARGET, if TARGET is defined, include that thing
# presumably, it overrides the stuff above with more accurate guesses
ifneq ($(origin TARGET), undefined)
include $(TARGET).make
endif

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

#var:
	# SRCS=$(SRCS)
	# SRCSH=$(SRCSH)
	# SRCSO=$(SRCSO)
	# EXTS=$(EXTS)
	# EXTSH=$(EXTSH)
	# EXTSO=$(EXTSO)

default: $(bin)/$(PROJ) $(DOCS)
	# . . . success; executable is in $(bin)/$(PROJ)

# linking
$(bin)/$(PROJ): $(LORE_H) $(LORE_C) $(VS_H) $(FS_H) $(EXTSO) $(SRCSO)
	$(CC) $(CF) $(OF) $(EXTSO) $(SRCSO) -o $@

# compiling
$(SRCSO): $(build)/%.o: $(src)/%.c $(VS_VS) $(FS_FS) $(SRCSH)
	# internal C
	-@$(MKDIR) $(bin)
	-@$(MKDIR) $(build)
	-@$(MKDIR) $(build)/$(system)
	-@$(MKDIR) $(build)/$(general)
	-@$(MKDIR) $(build)/$(game)
	-@$(MKDIR) $(build)/$(external)
	$(CC) $(CF) -c $(src)/$*.c -o $@

# these files are subject to less scrutiny since they are not mine
$(EXTSO): $(build)/$(external)/%.o: $(external)/%.c $(EXTSH)
	# external C
	-@$(MKDIR) $(bin)
	-@$(MKDIR) $(build)
	-@$(MKDIR) $(build)/$(external)
	$(CC) $(CF_LAX) -c $(external)/$*.c -o $@

$(build)/$(shaders)/%_vs.h: $(shaders)/%.vs $(TEXT2H)
	# vertex shaders into headers
	-@$(MKDIR) $(build)
	-@$(MKDIR) $(build)/$(shaders)
	$(TEXT2H) $< > $@

$(build)/$(shaders)/%_fs.h: $(shaders)/%.fs $(TEXT2H)
	# fragment shaders into headers
	-@$(MKDIR) $(build)
	-@$(MKDIR) $(build)/$(shaders)
	$(TEXT2H) $< > $@

$(LORE_H): $(LOADER) $(FILE2H) $(TYPE)
	# resources lore.h
	-@$(MKDIR) $(build)
	$(LOADER) $(media) > $(LORE_H)

$(LORE_C): $(LOADER) $(FILE2H) $(TYPE) $(LORE) $(PNG_H) $(JPEG_H) $(BMP_H)
	# resources lore.c
	-@$(MKDIR) $(build)
	$(LOADER) $(media) $(media) > $(LORE_C)

$(build)/%_png.h: $(media)/%.png $(FILE2H)
	# File2h png
	-@$(MKDIR) $(build)
	$(FILE2H) $< > $@

$(build)/%_jpeg.h: $(media)/%.jpeg $(FILE2H)
	# File2h jpeg
	-@$(MKDIR) $(build)
	$(FILE2H) $< > $@

$(DOCS): $(doc)/%.html: $(src)/%.c $(src)/%.h
	# documentation
	-@$(MKDIR) $(doc)
	-@$(MKDIR) $(doc)/$(system)
	-@$(MKDIR) $(doc)/$(general)
	-@$(MKDIR) $(doc)/$(game)
	-cat $^ | $(CDOC) > $@

# additional dependancies

$(bin)/Open.o: $(VS_H) $(FS_H)

######
# helper programmes

$(TEXT2H): $(TEXT2H_DEP)
	# . . . compiling Text2h.
	$(MAKE) --directory $(TEXT2H_DIR)

$(FILE2H): $(FILE2H_DEP)
	# . . . compiling File2h.
	$(MAKE) --directory $(FILE2H_DIR)

$(LOADER): $(LOADER_DEP)
	# . . . compiling Loader.
	$(MAKE) --directory $(LOADER_DIR)

######
# test programmes (fixme)

$(bin)/sort: $(src)/$(general)/Sorting.c $(TEST)/SortingTest.c
	$(CC) $(CF) $^ -o $(bin)/sort

$(bin)/cd: $(TEST)/Collision.c
	$(CC) $(CF) $< -o $(bin)/cd

$(bin)/time: $(TEST)/TimerIsTime.c
	$(CC) $(CF) $< -o $(bin)/time

######
# phoney targets

.PHONY: clean backup source setup icon

clean:
	-$(MAKE) --directory $(TEXT2H_DIR) clean
	-$(MAKE) --directory $(FILE2H_DIR) clean
	-$(MAKE) --directory $(LOADER_DIR) clean
	-$(RM) $(SRCSO) $(bin)/$(RSRC) $(VS_H) $(FS_H) $(TEXT2H) $(FILE2H) $(LOADER) \
$(bin)/sort $(bin)/cd $(LORE_H) $(LORE_C) $(PNG_H) $(JPEG_H) $(BMP_H) $(DOCS)
	-$(RMDIR) $(bin)/$(system) $(bin)/$(general) $(bin)/$(game) $(bin)/$(shaders) $(bin)/$(external)

backupUP := readme.txt gpl.txt copying.txt Makefile $(SRCS) $(SRCSH) $(media)/$(ICON) $(VS) $(FS) $(EXTRA) $(TEXT2H_DEP) $(FILE2H_DEP) $(LOADER_DEP) $(TYPE) $(LORE) $(TEXT)

backup:
	-@$(MKDIR) $(backup)
	$(ZIP) $(backup)/$(INST)-`date +%Y-%m-%dT%H%M%S`$(BRGS).zip $(backupUP)

# this backs up everything including the media files for publication
source:
	-@$(MKDIR) $(bin)
	$(ZIP) $(bin)/$(INST)-`date +%Y-%m-%dT%H%M%S`.zip $(backupUP) $(PNG) $(JPEG)

icon: default
	# . . . setting icon on a Mac.
	$(CP) $(media)/$(ICON) $(build)/$(ICON)
	-sips --addIcon $(build)/$(ICON)
	-DeRez -only icns $(build)/$(ICON) > $(build)/$(RSRC)
	-Rez -append $(build)/$(RSRC) -o $(bin)/$(PROJ)
	-SetFile -a C $(bin)/$(PROJ)

setup: default icon
	# . . . setup on a Mac.
	-@$(MKDIR) $(bin)/$(INST)
	$(CP) $(bin)/$(PROJ) readme.txt gpl.txt copying.txt $(bin)/$(INST)
	$(RM) $(bin)/$(INST)-MacOSX.dmg
	hdiutil create $(bin)/$(INST)-MacOSX.dmg -volname "$(PROJ) $(VA).$(VB)" -srcfolder $(bin)/$(INST)
	$(RMDIR) $(bin)/$(INST)
