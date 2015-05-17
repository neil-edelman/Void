PROJ  := Bmp2h
PROJw := $(PROJ).exe
FILES := Bitmap Bmp2h
BDIR  := bin
BACK  := backup
OBJS  := $(patsubst %,$(BDIR)/%.o,$(FILES))
SRCS  := $(patsubst %,%.c,$(FILES))
H     := Bitmap.h # $(patsubst %,%.h,$(FILES))

CC   := gcc
OF   := -Wall -O3 -fasm -fomit-frame-pointer -ffast-math -funroll-loops -fasm -fomit-frame-pointer -ffast-math -funroll-loops -pedantic -ansi
CF   := -ansi

CCw  := /usr/local/i386-mingw32-4.3.0/bin/i386-mingw32-gcc
OFw  := -Wall -O3 -fasm -fomit-frame-pointer -ffast-math -funroll-loops -fasm -fomit-frame-pointer -ffast-math -funroll-loops -pedantic -mwindows

default: $(BDIR)/$(PROJ)

$(BDIR)/$(PROJ): $(OBJS)
	$(CC) $(OF) $(CF) $^ -o $@

$(BDIR)/%.o: %.c
	@mkdir -p $(BDIR)
	$(CC) $(OF) -c $< -o $@

.PHONY: clean backup
clean:
	-rm -f $(OBJS)

backup:
	@mkdir -p $(BACK)
	zip $(BACK)/$(PROJ)-`date +%Y-%m-%dT%H%M%S`.zip $(SRCS) $(H) Makefile
