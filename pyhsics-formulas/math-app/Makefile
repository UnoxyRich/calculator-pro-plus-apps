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
.PHONY: all test test-native-ui test-static clean

all: test test-native-ui

test: build/math_core_tests
	./build/math_core_tests

test-native-ui: build/native_ui_tests
	./build/native_ui_tests

test-static:
	clang --analyze -std=c99 -Iinclude src/math_core.c
	clang --analyze -std=c99 -DMATHAPP_HOST_TEST -Iinclude -Itests -Dmain=mathapp_native_main src/native_main.c

build/math_core_tests: src/math_core.c tests/test_math_core.c include/math_core.h
	@mkdir -p build
	$(CC) -std=c99 $(CFLAGS_HOST) -Wall -Wextra -Werror -Iinclude src/math_core.c tests/test_math_core.c -o $@

build/native_ui_tests: src/math_core.c src/native_main.c tests/test_native_ui.c tests/native_ui_stubs.h include/math_core.h
	@mkdir -p build
	$(CC) -std=c99 $(CFLAGS_HOST) -Wall -Wextra -Werror -DMATHAPP_HOST_TEST -Iinclude -Itests -Dmain=mathapp_native_main -c src/native_main.c -o build/native_main_host.o
	$(CC) -std=c99 $(CFLAGS_HOST) -Wall -Wextra -Werror -Iinclude src/math_core.c tests/test_native_ui.c build/native_main_host.o -o $@

clean:
	rm -rf build
endif
