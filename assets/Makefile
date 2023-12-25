ASSETS = $(wildcard *.png)
ASSETS_H := $(patsubst %, %.h, $(ASSETS))
ASSETS_C := $(patsubst %, %.cpp, $(ASSETS))

all: $(ASSETS_H)

%.h: %
	python3 ../scripts/xxd.py $<

clean:
	@ echo "Cleaning assets"
	rm -f $(ASSETS_H) $(ASSETS_C)

.PHONY: all clean
