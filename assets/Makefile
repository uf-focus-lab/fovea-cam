XXD := ../scripts/xxd.py
ASSETS = $(wildcard *.png)
ASSETS_H := $(patsubst %, %.h, $(ASSETS))
ASSETS_C := $(patsubst %, %.c, $(ASSETS))
ASSETS_O := $(patsubst %, %.o, $(ASSETS))

all: $(ASSETS_O)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.c: % $(XXD)
	python3 $(XXD) $<

clean:
	@ echo "Cleaning assets"
	rm -f $(ASSETS_H) $(ASSETS_C) $(ASSETS_O)

.PHONY: all clean
.SUFFIXES:
