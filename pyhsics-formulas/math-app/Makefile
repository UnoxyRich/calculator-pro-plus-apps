NAME = MATHAPP
DESCRIPTION = "Exact symbolic math solver"
COMPRESSED = YES
CFLAGS = -Wall -Wextra -Oz -Iinclude
.DEFAULT_GOAL := all

.PHONY: ce

ce:
	@if ! command -v cedev-config >/dev/null 2>&1; then \
		echo "CEdev is not installed or is not on PATH; native .8xp build is unavailable."; \
		exit 2; \
	fi
	$(MAKE) CEDEV_BUILD=1 -f Makefile.ce

ifeq ($(CEDEV_BUILD),1)
include $(shell cedev-config --makefile)
else
.PHONY: all test clean

all: test

test: build/math_core_tests
	./build/math_core_tests

build/math_core_tests: src/math_core.c tests/test_math_core.c include/math_core.h
	@mkdir -p build
	$(CC) -std=c99 $(CFLAGS_HOST) -Wall -Wextra -Werror -Iinclude src/math_core.c tests/test_math_core.c -o $@

clean:
	rm -rf build
endif
