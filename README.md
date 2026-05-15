# 🚀 Linux System Monitor (sysmon)

**A high-performance, lightweight, and native system monitor for Linux.**

`sysmon` is a C-based utility designed for users who need real-time system insights without the overhead of heavy monitoring suites. By reading directly from the kernel's `/proc` filesystem, it provides raw, accurate data with minimal CPU and RAM impact.

### Why use `sysmon`?
* **Zero Bloat:** Written in pure C with no external dependencies.
* **Data-Ready:** Outputs metrics in **JSON Lines** (`.jsonl`) format, perfect for automation and data pipelines.
* **Smart Logging:** Features built-in O(1) log rotation to ensure your storage never fills up unexpectedly.
* **Native Performance:** Interacts directly with the Linux kernel for maximum efficiency.



## 🛠 User Guide

<!--
### ⚡ Quick Install (One-Liner)
Get up and running instantly by downloading the latest binary directly to your system:
```bash
sudo bash -c 'curl -fsSL "[https://api.github.com/repos/Irsyad-gh/linux-sysmon/releases/latest](https://api.github.com/repos/Irsyad-gh/linux-sysmon/releases/latest)" | grep -oP "(?<=\"browser_download_url\": \")[^\"]*linux[^\"]*" | head -1 | xargs -I{} curl -fsSL {} -o /usr/bin/linux-sysmon && chmod +x /usr/bin/linux-sysmon'
``` -->

### 🔨 Manual Build from Source

If you prefer building it yourself or want to customize the code:

1. **Navigate to the source directory:**
```bash
cd linux-sysmon
```


2. **Compile the project:**
```bash
make       #for x64
make x86   #for x86
make arm64 #for arm64
```


3. **Install to your system path:**
```bash
sudo make install
```

4. **Run the monitor:**
Simply type `sysmon` in your terminal.

### 📈 Accessing Your Data

Logs are stored every 5 seconds in `~/.status/status.json`. You can watch the metrics live using:

```bash
tail -f ~/.status/status.json
```
---
### 🚩 Flags and Configuration
#### 1. GENERAL (3 flags)

| Flag | Function | Description |
|------|----------|-----------|
| `-h`, `--help` | Help | Show help message |
| `-v`, `--version` | Version | Show version information |
| `-V`, `--verbose` | Verbose | Enable debug mode |
| `-c`, `--config` | Config | Load configuration file |

#### 2. TIMING (3 flags)

| Flag | Function | Default |
|------|----------|---------|
| `-i`, `--interval <seconds>` | Sampling interval | 5 seconds |
| `--cpu-delay <us>` | CPU sampling delay | 200000 µs |
| `-1`, `--one-shot` | Run once then exit | - |

#### 3. LOGGING (5 flags)

| Flag | Function | Default |
|------|----------|---------|
| `-l`, `--logdir <dir>` | Log directory | `~/.status` |
| `-f`, `--logfile <name>` | Log filename | `status.json` |
| `-o`, `--output <path>` | Override full output path | - |
| `-m`, `--maxsize <MB>` | Log rotation size | 10 MB |
| `--no-log` | Disable file logging | `false` |

#### 4. DAEMON (2 flags)

| Flag | Function |
|------|----------|
| `-d`, `--daemon` | Run in background |
| `-p`, `--pidfile <file>` | Path to PID file |

#### 5. NETWORK (1 flag)

| Flag | Function | Default |
|------|----------|---------|
| `-n`, `--net-interface <iface>` | Network interface to monitor | Auto-detect |

#### 6. PROCESS (1 flag)

| Flag | Function | Default |
|------|----------|---------|
| `-t`, `--top-processes <N>` | Show top N processes | `0` (disabled) |

#### 7. DISK (1 flag)

| Flag | Function | Default |
|------|----------|---------|
| `--disk-mount <path>` | Disk mount point to monitor | `/` |

#### 8. DISPLAY (1 flag)

| Flag | Function |
|------|----------|
| `-q`, `--quiet` | No console output |

---

#### **CONFIGURATION VIA FILE**

**INI Format** (`sysmon.ini`)

```ini
interval = 5
cpu_delay = 200000
logdir = ~/.status
logfile = status.json
maxsize = 10
quiet = false
verbose = false
no_log = false
daemon = false
pidfile = 
net-interface = 
disk-mount = /
top-processes = 0
one-shot = false
```

**JSON Format** (`sysmon.json`)

```json
{
    "interval": 5,
    "cpu_delay": 200000,
    "logdir": "~/.status",
    "logfile": "status.json",
    "maxsize": 10,
    "quiet": false,
    "verbose": false,
    "no_log": false,
    "daemon": false,
    "pidfile": "",
    "net-interface": "",
    "disk-mount": "/",
    "top-processes": 0,
    "one-shot": false
}
```

---

#### **PRACTICAL EXAMPLES**

```bash
# Normal mode
sysmon

# Daemon mode with custom settings
sysmon -d -i 30 -l /var/log/sysmon -p /var/run/sysmon.pid -q

# Monitor specific interface + top 5 processes
sysmon -n eth0 -t 5 -i 2 -V

# One-time snapshot to file
sysmon -1 -o /tmp/snapshot.json

# Load from config file
sysmon --config /etc/sysmon.ini

# Override config file with CLI flags
sysmon -c sysmon.ini -i 10 -t 5
```

---

#### **TOTAL NEW ARGUMENTS**

**18 Arguments Total**

- **9 short flags**: `-h`, `-v`, `-i`, `-d`, `-l`, `-f`, `-m`, `-o`, `-n`, `-q`, `-c`, `-p`, `-t`, `-1`, `-V`
- **12 long options**: `--help`, `--version`, `--interval`, `--daemon`, `--logdir`, `--logfile`, `--maxsize`, `--output`, `--net-interface`, `--quiet`, `--config`, `--pidfile`, `--top-processes`, `--disk-mount`, `--cpu-delay`, `--no-log`, `--one-shot`, `--verbose`

**2 Configuration File Formats** (INI & JSON)  
**Fully backward compatible** with the original sysmon


---

## ⚙️ Technical Reference

### Project Architecture

The project is structured for simplicity and modularity:

* `sysmon.h`: Constants, data structures, and function declarations.
* `sysmon.c`: Core implementation for CPU, RAM, Disk, and Network tracking.
* `main.c`: Entry point handling the main execution loop and signals.
* `Makefile`: Standard build instructions.

### Monitored Metrics

| Category | Data Captured |
| --- | --- |
| **CPU** | Usage % (delta jiffy) and Load Average (1m/5m/15m) |
| **RAM** | Total, Used, Free, Available, Cached, and Buffers |
| **Disk** | Total, Used, and Free space on the root (`/`) partition |
| **Network** | Active interface, RX (Download) KB/s, and TX (Upload) KB/s |
| **System** | Uptime (DD:HH:MM:SS) and precise timestamps |

### Advanced Configuration

Modify these values in `sysmon.h` to tune the monitor's behavior:

```c
#define SAMPLE_INTERVAL   5                       // Seconds between samples
#define LOG_MAX_BYTES     (10L * 1024L * 1024L)  // Rotate log at 10 MB

```

### Integration Example (Python)

Easily parse your system logs for custom dashboards or analysis:

```python
import json

with open("/home/user/.status/status.json") as f:
    for line in f:
        entry = json.loads(line)
        print(f"{entry['time']} | CPU: {entry['cpu_usage']}% | "
              f"RAM: {entry['ram']['used']}/{entry['ram']['total']} MB")

```

## 📄 Requirements & Licensing

* **OS:** Linux with `/proc` filesystem support.
* **Compiler:** GCC with C11 support (`gcc >= 4.9`).
* **License:** Distributed with MIT License. Feel free to fork, modify, and contribute!
