# Makefile 1.2 (GNU Make 3.81; MacOSX gcc 4.2.1; MacOSX MinGW 4.3.0)

PROJ  := File2h
VA    := 2
VB    := 0

# dirs
SDIR  := src
BDIR  := bin
BACK  := backup

# files in sdir
FILES := File2h

# files in bdir
INST  := $(PROJ)-$(VA)_$(VB)

# extra stuff we should back up
EXTRA := # $(SDIR)/icon.rc

OBJS  := $(patsubst %,$(BDIR)/%.o,$(FILES)) # or $(patsubst %,$(BDIR)/%.class,$(FILES))
SRCS  := $(patsubst %,$(SDIR)/%.c,$(FILES)) # or $(patsubst %,$(SDIR)/%.java,$(FILES))
H     := $(patsubst %,$(SDIR)/%.h,$(FILES))

CC    := gcc
CF    := -Wall -Wextra -O3 -fasm -fomit-frame-pointer -ffast-math -funroll-loops -pedantic -ansi
OF    := -framework OpenGL -framework GLUT
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
	# . . . success; executable is in $(BDIR)/$(PROJ)

# linking
$(BDIR)/$(PROJ): $(OBJS)
	$(CC) $(CF) $(OF) $(OBJS) -o $@

# compiling
$(OBJS): $(BDIR)/%.o: $(SDIR)/%.c
	-@$(MKDIR) $(BDIR)
	$(CC) $(CF) -c $(SDIR)/$*.c -o $@

######
# phoney targets

.PHONY: setup clean backup

clean:
	-$(RM) $(OBJS)

backup:
	-@$(MKDIR) $(BACK)
	$(ZIP) $(BACK)/$(INST)-`date +%Y-%m-%dT%H%M%S`$(BRGS).zip readme.txt gpl.txt copying.txt Makefile $(SRCS) $(H) $(SDIR)/$(ICON) $(EXTRA)
	#git commit -am "$(ARGS)"

setup: default
	-@$(MKDIR) $(BDIR)/$(INST)
	$(CP) $(BDIR)/$(PROJ) readme.txt gpl.txt copying.txt $(BDIR)/$(INST)
	$(RM) $(BDIR)/$(INST)-MacOSX.dmg
	# or $(RM) $(BDIR)/$(INST)-Win32.zip
	hdiutil create $(BDIR)/$(INST)-MacOSX.dmg -volname "$(PROJ) $(VA).$(VB)" -srcfolder $(BDIR)/$(INST)
	# or $(ZIP) $(BDIR)/$(INST)-Win32.zip -r $(BDIR)/$(INST)
	rm -R $(BDIR)/$(INST)
