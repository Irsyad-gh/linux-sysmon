# ──────────────────────────────────────────────────────────────
# Makefile — Linux System Monitor
# Usage:
#   make           → build executable 'sysmon'
#   make debug     → build dengan debug symbols & AddressSanitizer
#   make clean     → hapus build artifacts
#   make install   → install ke /usr/local/bin (perlu sudo)
#   make uninstall → hapus dari /usr/local/bin
# ──────────────────────────────────────────────────────────────

CC      := gcc
TARGET  := sysmon
SRCS    := main.c sysmon.c args.c
OBJS    := $(SRCS:.c=.o)
DEPS    := sysmon.h args.h

CFLAGS  := -std=c11 -Wall -Wextra -Wpedantic -Wshadow \
           -Wformat=2 -Wconversion -O2
LDFLAGS :=

ARM_CC  := aarch64-linux-gnu-gcc
PREFIX  := /usr/local/bin

.PHONY: all clean debug x86 x86_64 arm64 install uninstall

# ── Default Target (native) ───────────────────────────────────
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
	@echo ""
	@echo " ✔ Build successful [$(shell uname -m)] → ./$(TARGET)"

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

# ── Build untuk x86 (32-bit) ─────────────────────────────────
x86: CFLAGS  += -m32
x86: LDFLAGS += -m32
x86: clean $(TARGET)
	@echo " [TARGET ARCH: x86 32-bit]"

# ── Build untuk x86_64 (eksplisit) ──────────────────────────
x86_64: clean $(TARGET)
	@echo " [TARGET ARCH: x86_64 native]"

# ── Build untuk ARM64 ─────────────────────────────────────────
arm64: CC := $(ARM_CC)
arm64: clean $(TARGET)
	@echo " [TARGET ARCH: ARM64 / aarch64]"

# ── Debug Build ───────────────────────────────────────────────
debug: CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -g3 -O0 \
                 -fsanitize=address,undefined -fno-omit-frame-pointer
debug: clean $(TARGET)
	@echo " [DEBUG BUILD — AddressSanitizer active]"

# ── Install ───────────────────────────────────────────────────
install: all
	install -m 755 $(TARGET) $(PREFIX)/$(TARGET)
	@echo " ✔ Installed to $(PREFIX)/$(TARGET)"

uninstall:
	rm -f $(PREFIX)/$(TARGET)
	@echo " ✔ Uninstalled $(PREFIX)/$(TARGET)"

# ── Clean ─────────────────────────────────────────────────────
clean:
	rm -f *.o $(TARGET)
	@echo " ✔ Build artifacts removed."
