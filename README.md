# Linux System Monitor

C-based system monitor that reads directly from the kernel filesystem `/proc`.  
Writes metric data to a **JSON Lines** (`.jsonl`) file every 5 seconds.

---

## Project Structure

```
linux-sysmon/
├── sysmon.h    ← Header: structs, constants, function declarations
├── sysmon.c    ← Implementation: CPU, RAM, Disk, Net, Log
├── main.c      ← Entry point: main loop, signal handler
├── Makefile
└── README.md
```

## Install
```
sudo bash -c 'curl -fsSL "https://api.github.com/repos/Irsyad-gh/linux-sysmon/releases/latest" | grep -oP "(?<=\"browser_download_url\": \")[^\"]*linux[^\"]*" | head -1 | xargs -I{} curl -fsSL {} -o /usr/bin/linux-sysmon && chmod +x /usr/bin/linux-sysmon'
```

## Build Manually

```bash
# cd to source code's directory

# Build
make

# Build debug mode (AddressSanitizer)
make debug

# Install to /usr/local/bin
sudo make install
sysmon
```

## Output Log

Logs are saved in `~/.status/status.json` (JSON Lines format):

```json
{"time": "12/05/2026, 15:00:05", "cpu_usage": 12.5, "load_avg": [0.50, 0.40, 0.35], "ram": {"total": 8192, "used": 2048, "free": 6144, "available": 6000, "cached": 1200, "buffers": 300}, "disk": {"total_mb": 476940, "used_mb": 120000, "free_mb": 356940}, "uptime": "01:10:05:20", "net": {"iface": "eth0", "rx_kbps": 12.5, "tx_kbps": 3.2}}
```

### Log Rotation

File is rotated when it reaches **10 MB**:
```
status.json  →  status.1.json   (rename, O(1))
```
A new `status.json` file is created automatically on the next iteration.

## Monitored Metrics

| Category | Data |
|----------|------|
| **CPU**  | Usage % (delta jiffy), Load Average 1m/5m/15m |
| **RAM**  | Total, Used, Free, Available, Cached, Buffers |
| **Disk** | Total, Used, Free on `/` |
| **Net**  | Active interface, RX KB/s, TX KB/s |
| **System** | Uptime (DD:HH:MM:SS), Timestamp |

## Parsing Log with Python

```python
import json

with open("/home/user/.status/status.json") as f:
    for line in f:
        entry = json.loads(line)
        print(f"{entry['time']} | CPU: {entry['cpu_usage']}% | "
              f"RAM: {entry['ram']['used']}/{entry['ram']['total']} MB")
```

## Configuration (in `sysmon.h`)

```c
#define SAMPLE_INTERVAL   5                       // sampling interval (seconds)
#define CPU_SAMPLE_DELAY  200000                  // CPU delta delay (µs)
#define LOG_MAX_BYTES     (10L * 1024L * 1024L)  // max log size before rotation
```

## Requirements

- GCC with C11 support (`gcc >= 4.9`)
- Linux with `/proc` filesystem (all modern distros)
- No external dependencies
