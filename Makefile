# This is a heper Makefile for CMake Project
DEPS := $(wildcard **/*.cpp)
DEPS += $(wildcard **/*.h)
DEPS += $(wildcard **/*.hpp)
# Make without output
MAKE := make --no-print-directory
# CMake Build Directory and Build Type
CMAKE_BUILD_TYPE ?= Unknown

release: CMAKE_BUILD_TYPE := Release
debug: CMAKE_BUILD_TYPE := Debug

release debug:
	$(eval BUILD_DIR := build/$@/)
	@ cd assets && $(MAKE) -j$(shell nproc --all)
	@ $(MAKE) $(BUILD_DIR)/Makefile CMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE)
	@ cd $(BUILD_DIR)/.. && ln -sf $@/compile_commands.json .
	@ cd $(BUILD_DIR) && $(MAKE) -j$(shell nproc --all) && ln -sf ./FoveaCam ../
	@ cd $(BUILD_DIR)/.. && ln -sf $@/FoveaCam .

build/%/Makefile: CMakeLists.txt
	$(eval BUILD_DIR := $(shell dirname $@))
	@ echo "Generating CMake Files For $*"
	mkdir -p $(BUILD_DIR)
	cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=$(CMAKE_BUILD_TYPE) $(PWD)

clean:
	@ rm -rf build

init:
	sudo ln -sf $(PWD)/scripts/Xorg.service /etc/systemd/system/Xorg.service
	sudo systemctl stop gdm.service
	sudo systemctl disable gdm.service
	sudo systemctl daemon-reload
	sudo systemctl enable Xorg.service
	sudo systemctl restart Xorg.service

TASK ?=
start: release
	sudo build/FoveaCam $(TASK)

include $(wildcard scripts/*.mk)

.PHONY: release debug init clean
