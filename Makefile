# Build/upload helpers for DomeLight on a Waveshare RP2040-Zero.
#
#   make           - compile
#   make upload    - compile + upload over USB (auto-detects the port)
#   make uf2       - compile + copy the .uf2 to a board in BOOTSEL mode
#   make monitor   - USB serial, shows the brightness the current cap chose
#   make PORT=/dev/cu.usbmodemXXXX upload   - pin the port explicitly

SKETCH  := src
PROFILE := rp2040zero
BAUD    := 115200
BUILD   := $(SKETCH)/build

# The RP2040 enumerates as a USB CDC device (usbmodem), not a serial bridge.
PORT ?= $(shell arduino-cli board list --format json 2>/dev/null | \
	sed -n 's/.*"address": *"\(\/dev\/cu\.[^"]*\)".*/\1/p' | \
	grep -E 'usbmodem' | head -1)

# The drive a board in BOOTSEL mode presents.
BOOTSEL_VOL := /Volumes/RPI-RP2

.PHONY: all compile upload uf2 monitor list clean

all: compile

compile:
	arduino-cli compile -m $(PROFILE) $(SKETCH)

upload: compile
	@test -n "$(PORT)" || { echo "No RP2040 serial port found. Plug the board in, or hold BOOT while connecting it and run: make uf2"; exit 1; }
	@echo "Uploading DomeLight to $(PORT)"
	arduino-cli upload -m $(PROFILE) -p $(PORT) $(SKETCH)

# Fallback for when the board doesn't show up as a serial port.
uf2:
	arduino-cli compile -m $(PROFILE) --output-dir $(BUILD) $(SKETCH)
	@test -d $(BOOTSEL_VOL) || { echo "$(BOOTSEL_VOL) not mounted. Hold BOOT while plugging the board in, then re-run."; exit 1; }
	cp $(BUILD)/$(SKETCH).ino.uf2 $(BOOTSEL_VOL)/
	@echo "Copied. The board reboots into DomeLight on its own."

monitor:
	@test -n "$(PORT)" || { echo "No RP2040 serial port found."; exit 1; }
	arduino-cli monitor -p $(PORT) -c baudrate=$(BAUD)

list:
	arduino-cli board list

clean:
	rm -rf $(BUILD)
