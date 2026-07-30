CC ?= cc
CPPFLAGS ?= -Isrc -Ivendor/lz4 -Ivendor/utf8proc
UTF8PROC_CPPFLAGS = -DUTF8PROC_STATIC
CFLAGS ?= -O2
C89FLAGS = -std=c89 -pedantic -Wall -Wextra -Werror

ifeq ($(OS),Windows_NT)
EXE = .exe
MKDIR_BUILD = if not exist build mkdir build
CLEAN = if exist gdse.exe del /Q gdse.exe & if exist tests\test_rules.exe del /Q tests\test_rules.exe & if exist build rmdir /S /Q build
RUN_TEST = tests\test_rules.exe
else
EXE =
MKDIR_BUILD = mkdir -p build
CLEAN = rm -rf gdse tests/test_rules build
RUN_TEST = ./tests/test_rules
endif

APP_OBJECTS = build/main.o build/util.o build/archive.o build/rules.o
VENDOR_OBJECTS = build/lz4.o build/utf8proc.o

.PHONY: all clean test check

all: gdse$(EXE)

gdse$(EXE): $(APP_OBJECTS) $(VENDOR_OBJECTS)
	$(CC) $(CFLAGS) $(APP_OBJECTS) $(VENDOR_OBJECTS) -o $@

build:
	$(MKDIR_BUILD)

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
	$(CC) $(CPPFLAGS) $(UTF8PROC_CPPFLAGS) $(CFLAGS) -w -c $< -o $@

tests/test_rules$(EXE): tests/test_rules.c build/util.o build/rules.o build/archive.o build/lz4.o build/utf8proc.o
	$(CC) $(CPPFLAGS) $(CFLAGS) $(C89FLAGS) tests/test_rules.c build/util.o build/rules.o build/archive.o build/lz4.o build/utf8proc.o -o $@

test: tests/test_rules$(EXE)
	$(RUN_TEST)

check: all test

clean:
	$(CLEAN)
