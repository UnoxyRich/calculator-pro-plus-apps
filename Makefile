PYTHON ?= python3

.PHONY: all
all: bin/PHYS1.8xp

bin/PHYS1.8xp: src/PHYS1.tib tools/build_basic.py
	$(PYTHON) tools/build_basic.py
