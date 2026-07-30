CC ?= cc
CPPFLAGS ?= -Isrc -Ivendor/lz4 -Ivendor/utf8proc
CFLAGS ?= -O2
C89FLAGS = -std=c89 -pedantic -Wall -Wextra -Werror

APP_OBJECTS = build/main.o build/util.o build/archive.o build/rules.o
VENDOR_OBJECTS = build/lz4.o build/utf8proc.o

.PHONY: all clean test check

all: gdse

gdse: $(APP_OBJECTS) $(VENDOR_OBJECTS)
	$(CC) $(CFLAGS) $(APP_OBJECTS) $(VENDOR_OBJECTS) -o $@

build:
	mkdir -p build

build/main.o: src/main.c src/gdse.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(C89FLAGS) -c $< -o $@
build/util.o: src/util.c src/gdse.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(C89FLAGS) -c $< -o $@
build/archive.o: src/archive.c src/gdse.h vendor/lz4/lz4.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(C89FLAGS) -c $< -o $@
build/rules.o: src/rules.c src/gdse.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(C89FLAGS) -c $< -o $@
build/lz4.o: vendor/lz4/lz4.c vendor/lz4/lz4.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -w -c $< -o $@
build/utf8proc.o: vendor/utf8proc/utf8proc.c vendor/utf8proc/utf8proc.h vendor/utf8proc/utf8proc_data.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -w -c $< -o $@

tests/test_rules: tests/test_rules.c build/util.o build/rules.o build/archive.o build/lz4.o build/utf8proc.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $(C89FLAGS) tests/test_rules.c build/util.o build/rules.o build/archive.o build/lz4.o build/utf8proc.o -o $@

test: tests/test_rules
	./tests/test_rules

check: all test

clean:
	rm -rf gdse tests/test_rules build
