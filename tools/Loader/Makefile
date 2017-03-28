# Makefile 1.2 (GNU Make 3.81; MacOSX gcc 4.2.1; MacOSX MinGW 4.3.0)

PROJ  := Loader
VA    := 1
VB    := 0

MKDIR := mkdir -p

# dirs
src    := src
bin    := bin
build  := build
backup := backup
external := ../../external

# files in sdir
SRCS  := $(wildcard $(src)/*.c)
SRCSH := $(wildcard $(src)/*.h)
SRCSO := $(patsubst $(src)/%.c, $(build)/%.o, $(SRCS))
FMTLIST := lodepng nanojpeg
FMTS  := $(patsubst %, $(external)/%.c, $(FMTLIST))
FMTSH := $(patsubst %, $(external)/%.h, $(FMTLIST))
FMTSO := $(patsubst %, $(build)/%.o, $(FMTLIST))

CC    := gcc
# c99: snprintf
CF    := -Wall -Wextra -O3 -fasm -fomit-frame-pointer -ffast-math \
-funroll-loops -pedantic -std=c99
OF    :=
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

default: $(bin)/$(PROJ)

# linking
$(bin)/$(PROJ): $(SRCSO) $(FMTSO)
	-@$(MKDIR) $(bin)
	$(CC) $(CF) $(OF) $(SRCSO) $(FMTSO) -o $@

# compiling
$(SRCSO): $(build)/%.o: $(src)/%.c $(SRCSH)
	-@$(MKDIR) $(build)
	$(CC) $(CF) -c $(src)/$*.c -o $@

$(FMTSO): $(build)/%.o: $(external)/%.c $(FMTSH)
	-@$(MKDIR) $(build)
	$(CC) $(CF) -c $(external)/$*.c -o $@

######
# phoney targets

.PHONY: setup clean backup

clean:
	-$(RM) $(OBJS)

backup:
	-@$(MKDIR) $(backup)
	$(ZIP) $(backup)/$(INST)-`date +%Y-%m-%dT%H%M%S`$(BRGS).zip Makefile $(SRCS) $(SRCSH) $(EXTRA)
