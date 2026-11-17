# Rez decompilation. See README.md.
#
#   make setup                                 the pinned compiler and binutils, into .tools/
#   make extract DISC="path/to/Rez (USA).cue"  your own disc image (or EXE=... CVM=...)
#   make build                                 link SLUS_203.44 and the 29 overlays from source
#   make verify                                all 30 files must have the original sha256
#   make disc                                  write the build back into a copy of your disc image

PYTHON ?= python3
JOBS ?= $(shell nproc 2>/dev/null || echo 4)

.PHONY: all setup extract build verify disc clean

all: build verify

setup:
	scripts/setup-ps2-toolchain.sh

EXTRACT_ARGS = $(strip $(if $(DISC),--disc "$(DISC)") $(if $(EXE),--exe "$(EXE)") \
	$(if $(CVM),--cvm "$(CVM)") $(if $(OVERLAYS),--overlays "$(OVERLAYS)") \
	$(if $(DISC)$(EXE)$(CVM)$(OVERLAYS),,--check))

extract:
	$(PYTHON) scripts/extract.py $(EXTRACT_ARGS)

build:
	$(PYTHON) scripts/extract.py --check >/dev/null || { $(PYTHON) scripts/extract.py --check; \
		echo "game files missing or different: run make extract first"; exit 1; }
	$(PYTHON) scripts/relink.py --overlays --jobs $(JOBS)

verify:
	$(PYTHON) scripts/verify_build.py

disc:
	$(PYTHON) scripts/build_disc.py --from build/relink/ps2-us-1.01/exact

clean:
	rm -rf build/relink build/disc
