PROJ  := Vsfs2h
PROJw := $(PROJ).exe
FILES := Vsfs2h
BDIR  := bin
BACK  := backup
EXTRA :=
OBJS  := $(patsubst %,$(BDIR)/%.o,$(FILES))
SRCS  := $(patsubst %,%.c,$(FILES))
H     := $(patsubst %,%.h,$(FILES))

CC    := gcc
OF    := -Wall -Wextra -O3 -fasm -fomit-frame-pointer -ffast-math -funroll-loops -fasm -fomit-frame-pointer -ffast-math -funroll-loops -pedantic -ansi
CF    := -ansi
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

CCw  := /usr/local/i386-mingw32-4.3.0/bin/i386-mingw32-gcc
OFw  := -Wall -Wextra -O3 -fasm -fomit-frame-pointer -ffast-math -funroll-loops -fasm -fomit-frame-pointer -ffast-math -funroll-loops -pedantic -mwindows

default: $(BDIR)/$(PROJ)

$(BDIR)/$(PROJ): $(OBJS)
	$(CC) $(OF) $(CF) $^ -o $@

$(BDIR)/%.o: %.c
	-@$(MKDIR) $(BDIR)
	$(CC) $(OF) -c $< -o $@

.PHONY: clean backup
clean:
	-$(RM) $(OBJS)

backup:
	-@$(MKDIR) $(BACK)
	$(ZIP) $(BACK)/$(PROJ)-`date +%Y-%m-%dT%H%M%S`.zip $(SRCS) Makefile
