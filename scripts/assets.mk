ASSETS := $(patsubst assets/%, %, $(wildcard "assets/*"))

assets.dir:
	mkdir -p build/.assets/

build/.assets/assets.h: assets.dir $(patsubst %, build/.assets/assets/%.c, $(ASSETS))
	@ echo "Generating Assets"