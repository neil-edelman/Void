# Makefile 1.2 (GNU Make 3.81; MacOSX gcc 4.2.1; MacOSX MinGW 4.3.0)

PROJ  := Loader
VA    := 1
VB    := 0

MKDIR := mkdir #-p

# dirs
SDIR  := src
BDIR  := bin
BACK  := backup

# files in sdir
FILES := Reader Error Type Record Lore Loader
FORMAT := lodepng nanojpeg #Bitmap <- took this out

# extra stuff we should back up
EXTRA := $(SDIR)/Functions.h

INST  := $(PROJ)-$(VA)_$(VB)

OBJS  := $(patsubst %,$(BDIR)/%.o,$(FILES))
FMTOBJS := $(patsubst %,$(BDIR)/%.o,$(FORMAT))
SRCS  := $(patsubst %,$(SDIR)/%.c,$(FILES))
FMTSRCS := $(patsubst %,../../src/%.c,$(FORMAT))
H     := $(patsubst %,$(SDIR)/%.h,$(FILES))
FMTH  := $(patsubst %,../../src/format/%.h,$(FORMAT))

CC    ?= gcc
CF    ?= -Wall -Wextra -O3 -fasm -fomit-frame-pointer -ffast-math -funroll-loops -pedantic -std=c99 # snprintf
OF    ?=
MAKE  := make
MKDIR := mkdir -p
RM    := rm -f
RMDIR := rm -rf
ZIP   := zip
CP    := cp
# user-defined variable TARGET, if TARGET is defined, include that thing
ifneq ($(origin TARGET), undefined)
include ../../$(TARGET).make
endif

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

# linking
$(BDIR)/$(PROJ): $(OBJS) $(FMTOBJS)
	$(CC) $(CF) $(OF) $(OBJS) $(FMTOBJS) -o $@

# compiling
$(FMTOBJS): $(BDIR)/%.o: ../../src/format/%.c $(FMTH)
	-@$(MKDIR) $(BDIR)
	$(CC) $(CF) -c ../../src/format/$*.c -o $@

$(OBJS): $(BDIR)/%.o: $(SDIR)/%.c $(H)
	-@$(MKDIR) $(BDIR)
	$(CC) $(CF) -c $(SDIR)/$*.c -o $@

######
# phoney targets

.PHONY: setup clean backup

clean:
	-$(RM) $(OBJS)

backup:
	-@$(MKDIR) $(BACK)
	$(ZIP) $(BACK)/$(INST)-`date +%Y-%m-%dT%H%M%S`$(BRGS).zip Makefile $(SRCS) $(H) $(EXTRA)
