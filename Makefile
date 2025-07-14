# Makefile for Qvortex Hash on macOS (Apple Silicon)

CC = clang
CFLAGS = -O3 -march=native -Wall -Wextra -std=c11
# CFLAGS += -fomit-frame-pointer -funroll-loops
LDFLAGS = 

# Debug build flags
DEBUG_FLAGS = -g -O0 -DDEBUG -fsanitize=address -fsanitize=undefined

# Files
SOURCES = qvortex.c qvortex_test.c
HEADERS = qvortex.h
OBJECTS = qvortex.o qvortex_test.o
TARGET = qvortex_test

# Check for ARM64 and enable NEON
ARCH := $(shell uname -m)
ifeq ($(ARCH),arm64)
    CFLAGS += -DUSE_NEON=1
    $(info ✓ Detected ARM64 - NEON optimizations enabled)
endif

# Default target
all: $(TARGET)

# Main build
$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(TARGET) $(LDFLAGS)
	@echo "✓ Build complete: $(TARGET)"

# Object files
qvortex.o: qvortex.c qvortex.h
	$(CC) $(CFLAGS) -c qvortex.c -o qvortex.o

qvortex_test.o: qvortex_test.c qvortex.h
	$(CC) $(CFLAGS) -c qvortex_test.c -o qvortex_test.o

# Debug build
debug: CFLAGS += $(DEBUG_FLAGS)
debug: clean $(TARGET)
	@echo "✓ Debug build complete"

# Run tests
test: $(TARGET)
	./$(TARGET)

# Benchmark only
bench: $(TARGET)
	@echo "Running performance benchmark..."
	@./$(TARGET) | grep -A20 "Performance Benchmark"

# Clean
clean:
	rm -f $(OBJECTS) $(TARGET)
	@echo "✓ Cleaned build artifacts"

# Install (optional - installs to /usr/local)
install: $(TARGET)
	@echo "Installing would require:"
	@echo "  sudo cp qvortex.h /usr/local/include/"
	@echo "  sudo cp qvortex.o /usr/local/lib/"

# Show compiler info
info:
	@echo "Compiler version:"
	@$(CC) --version
	@echo ""
	@echo "Compiler flags:"
	@echo "  $(CFLAGS)"
	@echo ""
	@echo "Architecture: $(ARCH)"

# Disassembly (for optimization checking)
disasm: qvortex.o
	otool -tV qvortex.o | less

# Generate assembly
asm: qvortex.c qvortex.h
	$(CC) $(CFLAGS) -S qvortex.c -o qvortex.s
	@echo "✓ Assembly output: qvortex.s"

.PHONY: all clean test debug bench info install disasm asm
