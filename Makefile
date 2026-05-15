# ──────────────────────────────────────────────────────────────
#  Makefile — Linux System Monitor
#  Usage:
#    make          → build executable 'sysmon'
#    make debug    → build with debug symbols & AddressSanitizer
#    make clean    → remove build artifacts
#    make install  → install to /usr/local/bin (requires sudo)
#    make uninstall→ remove from /usr/local/bin
# ──────────────────────────────────────────────────────────────

CC        := gcc
TARGET    := sysmon
SRCS      := main.c sysmon.c
OBJS      := $(SRCS:.c=.o)
DEPS      := sysmon.h

CFLAGS    := -std=c11 -Wall -Wextra -Wpedantic -Wshadow \
             -Wformat=2 -Wconversion -O2
LDFLAGS   :=
ARCH_FLAGS := 

# Cross-compiler for ARM64
ARM_CC    := aarch64-linux-gnu-gcc

PREFIX    := /usr/local/bin

.PHONY: all clean debug x86 arm64

# ── Default Target (Native x64) ───────────────────────────────
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(ARCH_FLAGS) $(LDFLAGS) -o $@ $^
	@echo ""
	@echo "  ✔  Build successful [$(shell uname -m)] → ./$(TARGET)"

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) $(ARCH_FLAGS) -c -o $@ $<

# ── Build for x86 (32-bit) ──────────────────────────────────
# Using native gcc with -m32 flag
x86: ARCH_FLAGS := -m32
x86: LDFLAGS    := -m32
x86: clean $(TARGET)
	@echo "  [TARGET ARCH: x86 32-bit]"

# ── Build for ARM64 (aarch64) ───────────────────────────────
# Using specific cross-compiler
arm64: CC         := $(ARM_CC)
arm64: clean $(TARGET)
	@echo "  [TARGET ARCH: ARM64 / aarch64]"

# ── Debug Build ───────────────────────────────────────────────
DEBUG_FLAGS := -std=c11 -Wall -Wextra -Wpedantic -g3 -O0 \
               -fsanitize=address,undefined -fno-omit-frame-pointer

debug: CFLAGS := $(DEBUG_FLAGS)
debug: clean $(TARGET)
	@echo "  [DEBUG BUILD — AddressSanitizer active]"

# ... (install/uninstall/clean targets remain the same) ...
clean:
	rm -f *.o $(TARGET)
	@echo "  ✔  Build artifacts removed."

# Add at the bottom of Makefile
.PHONY: all x86_64 x86 arm64

x86_64: all
	@echo "  [TARGET ARCH: x86_64 native]"

# Override for x86_64 to be consistent
x86_64: ARCH_FLAGS := 
x86_64: LDFLAGS    :=
