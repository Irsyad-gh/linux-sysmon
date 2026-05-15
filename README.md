# 🚀 Linux System Monitor (sysmon)

**A high-performance, lightweight, and native system monitor for Linux.**

`sysmon` is a C-based utility designed for users who need real-time system insights without the overhead of heavy monitoring suites. By reading directly from the kernel's `/proc` filesystem, it provides raw, accurate data with minimal CPU and RAM impact.

### Why use `sysmon`?
* **Zero Bloat:** Written in pure C with no external dependencies.
* **Data-Ready:** Outputs metrics in **JSON Lines** (`.jsonl`) format, perfect for automation and data pipelines.
* **Smart Logging:** Features built-in O(1) log rotation to ensure your storage never fills up unexpectedly.
* **Native Performance:** Interacts directly with the Linux kernel for maximum efficiency.

---

## 🛠 User Guide

### ⚡ Quick Install (One-Liner)
Get up and running instantly by downloading the latest binary directly to your system:
```bash
sudo bash -c 'curl -fsSL "[https://api.github.com/repos/Irsyad-gh/linux-sysmon/releases/latest](https://api.github.com/repos/Irsyad-gh/linux-sysmon/releases/latest)" | grep -oP "(?<=\"browser_download_url\": \")[^\"]*linux[^\"]*" | head -1 | xargs -I{} curl -fsSL {} -o /usr/bin/linux-sysmon && chmod +x /usr/bin/linux-sysmon'
```

### 🔨 Manual Build from Source

If you prefer building it yourself or want to customize the code:

1. **Navigate to the source directory:**
```bash
cd linux-sysmon
```


2. **Compile the project:**
```bash
make
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
