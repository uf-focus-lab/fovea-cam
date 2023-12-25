ASSETS = $(filter-out $(wildcard assets/*.h) $(wildcard assets/*.c), $(wildcard assets/*))
ASSETS_TARGETS := $(patsubst %, %.h, $(ASSETS))

all_assets: $(ASSETS_TARGETS)

assets/%.h: assets/% scripts/xxd.py
	@ echo "Generating asset $< -> $@"
	@ python3 scripts/xxd.py $<

.PHONY: assets
