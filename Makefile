BUILD ?= build

all: build

build:
	mkdir -p $(BUILD)
	cd build && cmake ../ && make

clean:
	mkdir -p $(BUILD)
	cd build && cmake ../ && make clean

veryclean:
	rm -rf $(BUILD)

test: build
	cd build && env GTEST_COLOR=1 make test ARGS=-VV

test-00: build
	env GTEST_COLOR=1 build/test/testall/testall -VV

test-ff: build
	env GTEST_COLOR=1 build/test/testall/testall -VV

.PHONY: build clean
