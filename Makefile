# ──────────────────────────────────────────────────────────────
#  Makefile — Linux System Monitor
#  Usage:
#    make          → build executable 'sysmon'
#    make debug    → build dengan debug symbols & AddressSanitizer
#    make clean    → hapus hasil build
#    make install  → install ke /usr/local/bin (butuh sudo)
#    make uninstall→ hapus dari /usr/local/bin
# ──────────────────────────────────────────────────────────────

CC       := gcc
TARGET   := sysmon
SRCS     := main.c sysmon.c
OBJS     := $(SRCS:.c=.o)
DEPS     := sysmon.h

# Flag standar: C11, semua warning aktif, optimasi O2
CFLAGS   := -std=c11 -Wall -Wextra -Wpedantic -Wshadow \
             -Wformat=2 -Wconversion -O2
LDFLAGS  :=

# Flag untuk mode debug (AddressSanitizer + debug symbols)
DEBUG_FLAGS := -std=c11 -Wall -Wextra -Wpedantic -g3 -O0 \
               -fsanitize=address,undefined -fno-omit-frame-pointer

PREFIX   := /usr/local/bin

# ── Default Target ────────────────────────────────────────────
.PHONY: all
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
	@echo ""
	@echo "  ✔  Build sukses → ./$(TARGET)"
	@echo "  Run: ./$(TARGET)"
	@echo ""

# Kompilasi setiap .c → .o
%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $<

# ── Debug Build ───────────────────────────────────────────────
.PHONY: debug
debug: CFLAGS := $(DEBUG_FLAGS)
debug: clean $(TARGET)
	@echo "  [DEBUG BUILD — AddressSanitizer aktif]"

# ── Instalasi ─────────────────────────────────────────────────
.PHONY: install
install: $(TARGET)
	install -m 755 $(TARGET) $(PREFIX)/$(TARGET)
	@echo "  ✔  Installed: $(PREFIX)/$(TARGET)"

.PHONY: uninstall
uninstall:
	rm -f $(PREFIX)/$(TARGET)
	@echo "  ✔  Uninstalled: $(PREFIX)/$(TARGET)"

# ── Pembersihan ───────────────────────────────────────────────
.PHONY: clean
clean:
	rm -f $(OBJS) $(TARGET)
	@echo "  ✔  Build artifacts dihapus."
