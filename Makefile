BUILD ?= $(abspath build)

all: build test

automated: build test

build: gitdeps
	mkdir -p $(BUILD)
	cd build && cmake ../ && make

clean:
	rm -rf $(BUILD)

veryclean:
	rm -rf $(BUILD) gitdeps

test: build test-00 test-ff

test-00: build
	env GTEST_OUTPUT="xml:$(BUILD)/tests-00.xml" GTEST_COLOR=1 build/test/testall/testall --erase-00

test-ff: build
	env GTEST_OUTPUT="xml:$(BUILD)/tests-ff.xml" GTEST_COLOR=1 build/test/testall/testall --erase-ff

gitdeps:
	simple-deps --config examples/arduino_zero_large_files/arduino-libraries
	simple-deps --config examples/arduino_zero_prealloc/arduino-libraries
	simple-deps --config examples/arduino_zero_serial_flash/arduino-libraries
	simple-deps --config examples/arduino_zero_tree/arduino-libraries
	simple-deps --config test/testall/arduino-libraries

.PHONY: build clean
