CC := /usr/bin/clang
CFLAGS := -std=c11 -Wall -Wextra -Werror -O2 -Iinclude
BUILD := build

all: $(BUILD)/macos-vcand $(BUILD)/libmacos_vcan.a $(BUILD)/vcan-send $(BUILD)/vcan-dump

$(BUILD):
	mkdir -p $@

$(BUILD)/macos-vcand: src/macos-vcand.c include/macos_vcan_wire.h include/macos_vcan.h | $(BUILD)
	$(CC) $(CFLAGS) -o $@ src/macos-vcand.c

$(BUILD)/libmacos_vcan.a: $(BUILD)/libmacos_vcan.o | $(BUILD)
	ar rcs $@ $<

$(BUILD)/libmacos_vcan.o: src/libmacos_vcan.c include/macos_vcan.h include/macos_vcan_wire.h | $(BUILD)
	$(CC) $(CFLAGS) -c -o $@ src/libmacos_vcan.c

$(BUILD)/vcan-send: src/vcan-send.c $(BUILD)/libmacos_vcan.a | $(BUILD)
	$(CC) $(CFLAGS) -o $@ src/vcan-send.c $(BUILD)/libmacos_vcan.a

$(BUILD)/vcan-dump: src/vcan-dump.c $(BUILD)/libmacos_vcan.a | $(BUILD)
	$(CC) $(CFLAGS) -o $@ src/vcan-dump.c $(BUILD)/libmacos_vcan.a

test: all
	./tests/integration.sh

clean:
	rm -rf $(BUILD)

.PHONY: all test clean
