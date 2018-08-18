BUILD ?= $(abspath build)

all: build

build: gitdeps
	mkdir -p $(BUILD)
	cd build && cmake ../ && make

clean:
	rm -rf $(BUILD)

veryclean:
	rm -rf $(BUILD) gitdeps

test: build
	cd build && env GTEST_OUTPUT="xml:$(BUILD)/tests.xml" GTEST_COLOR=1 make test ARGS="-VV"

test-00: build
	env GTEST_COLOR=1 build/test/testall/testall -VV

test-ff: build
	env GTEST_COLOR=1 build/test/testall/testall -VV

gitdeps:
	simple-deps --config examples/arduino_zero_large_files/arduino-libraries
	simple-deps --config examples/arduino_zero_prealloc/arduino-libraries
	simple-deps --config examples/arduino_zero_serial_flash/arduino-libraries
	simple-deps --config examples/arduino_zero_tree/arduino-libraries
	simple-deps --config test/testall/arduino-libraries

.PHONY: build clean
