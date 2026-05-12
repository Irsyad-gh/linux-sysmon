# ──────────────────────────────────────────────────────────────
#  Makefile — Linux System Monitor
#  Usage:
#    make          → build executable 'sysmon'
#    make debug    → build with debug symbols & AddressSanitizer
#    make clean    → remove build artifacts
#    make install  → install to /usr/local/bin (requires sudo)
#    make uninstall→ remove from /usr/local/bin
# ──────────────────────────────────────────────────────────────

CC       := gcc
TARGET   := sysmon
SRCS     := main.c sysmon.c
OBJS     := $(SRCS:.c=.o)
DEPS     := sysmon.h

# Standard flags: C11, all warnings active, O2 optimization
CFLAGS   := -std=c11 -Wall -Wextra -Wpedantic -Wshadow \
             -Wformat=2 -Wconversion -O2
LDFLAGS  :=

# Flags for debug mode (AddressSanitizer + debug symbols)
DEBUG_FLAGS := -std=c11 -Wall -Wextra -Wpedantic -g3 -O0 \
               -fsanitize=address,undefined -fno-omit-frame-pointer

PREFIX   := /usr/local/bin

# ── Default Target ────────────────────────────────────────────
.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
	@echo ""
	@echo "  ✔  Build successful → ./$(TARGET)"
	@echo "  Run: ./$(TARGET)"
	@echo ""

# Compile each .c → .o
%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

# ── Debug Build ───────────────────────────────────────────────
.PHONY: debug
debug: CFLAGS := $(DEBUG_FLAGS)
debug: clean $(TARGET)
	@echo "  [DEBUG BUILD — AddressSanitizer active]"

# ── Installation ─────────────────────────────────────────────────
.PHONY: install
install: $(TARGET)
	install -m 755 $(TARGET) $(PREFIX)/$(TARGET)
	@echo "  ✔  Installed: $(PREFIX)/$(TARGET)"

.PHONY: uninstall
uninstall:
	rm -f $(PREFIX)/$(TARGET)
	@echo "  ✔  Uninstalled: $(PREFIX)/$(TARGET)"

# ── Cleanup ───────────────────────────────────────────────
.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)
	@echo "  ✔  Build artifacts removed."
