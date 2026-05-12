# Linux System Monitor

Monitor sistem berbasis C yang membaca langsung dari kernel filesystem `/proc`.  
Menulis data metrik ke file **JSON Lines** (`.jsonl`) setiap 5 detik.

---

## Struktur Proyek

```
linux-sysmon/
├── sysmon.h    ← Header: structs, konstanta, deklarasi fungsi
├── sysmon.c    ← Implementasi: CPU, RAM, Disk, Net, Log
├── main.c      ← Entry point: main loop, signal handler
├── Makefile
└── README.md
```

## Build & Menjalankan

```bash
# Build
make

# Jalankan
./sysmon

# Build mode debug (AddressSanitizer)
make debug

# Install ke /usr/local/bin
sudo make install
sysmon
```

## Output Log

Log disimpan di `~/.status/status.json` (JSON Lines format):

```json
{"time": "12/05/2026, 15:00:05", "cpu_usage": 12.5, "load_avg": [0.50, 0.40, 0.35], "ram": {"total": 8192, "used": 2048, "free": 6144, "available": 6000, "cached": 1200, "buffers": 300}, "disk": {"total_mb": 476940, "used_mb": 120000, "free_mb": 356940}, "uptime": "01:10:05:20", "net": {"iface": "eth0", "rx_kbps": 12.5, "tx_kbps": 3.2}}
```

### Rotasi Log

File dirotasi saat mencapai **10 MB**:
```
status.json  →  status.1.json   (rename, O(1))
```
File baru `status.json` dibuat otomatis pada iterasi berikutnya.

## Metrics yang Dipantau

| Kategori | Data |
|----------|------|
| **CPU**  | Usage % (delta jiffy), Load Average 1m/5m/15m |
| **RAM**  | Total, Used, Free, Available, Cached, Buffers |
| **Disk** | Total, Used, Free pada `/` |
| **Net**  | Interface aktif, RX KB/s, TX KB/s |
| **Sistem** | Uptime (DD:HH:MM:SS), Timestamp |

## Parsing Log dengan Python

```python
import json

with open("/home/user/.status/status.json") as f:
    for line in f:
        entry = json.loads(line)
        print(f"{entry['time']} | CPU: {entry['cpu_usage']}% | "
              f"RAM: {entry['ram']['used']}/{entry['ram']['total']} MB")
```

## Konfigurasi (di `sysmon.h`)

```c
#define SAMPLE_INTERVAL   5                       // interval sampling (detik)
#define CPU_SAMPLE_DELAY  200000                  // jeda delta CPU (µs)
#define LOG_MAX_BYTES     (10L * 1024L * 1024L)  // ukuran maks log sebelum rotasi
```

## Requirements

- GCC dengan dukungan C11 (`gcc >= 4.9`)
- Linux dengan `/proc` filesystem (semua distro modern)
- Tidak ada dependency eksternal
