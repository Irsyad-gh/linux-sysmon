/**
 * sysmon.c — Linux System Monitor
 * Implementation of reading system metrics from /proc filesystem.
 *
 * Standard: C11/C17
 * Compilation: gcc -std=c11 -Wall -Wextra -O2
 */

#include "sysmon.h"

/* ══════════════════════════════════════════════════════════════
 *  CPU USAGE  —  read /proc/stat twice, calculate delta
 * ══════════════════════════════════════════════════════════════ */

/**
 * Reads one 'cpu' line from /proc/stat into CpuStat.
 * Return: 0 success, -1 failure.
 */
int read_cpu_stat(CpuStat *out)
{
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) {
        perror("fopen /proc/stat");
        return -1;
    }

    /* Format: cpu user nice system idle iowait irq softirq steal ... */
    int ret = fscanf(fp,
        "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
        &out->user, &out->nice,   &out->system, &out->idle,
        &out->iowait, &out->irq, &out->softirq, &out->steal);

    fclose(fp);
    return (ret == 8) ? 0 : -1;
}

/**
 * Calculates CPU usage percentage using delta method.
 * Performs two readings with CPU_SAMPLE_DELAY µs delay.
 *
 * Return: percent (0.0–100.0), or -1.0 if failed.
 */
double calculate_cpu_usage(void)
{
    CpuStat s1, s2;

    if (read_cpu_stat(&s1) != 0) return -1.0;

    /* nanosleep replaces usleep (deprecated in POSIX 2008) */
    struct timespec ts_delay = {
        .tv_sec  = 0,
        .tv_nsec = (long)CPU_SAMPLE_DELAY * 1000L  /* µs → ns */
    };
    nanosleep(&ts_delay, NULL);

    if (read_cpu_stat(&s2) != 0) return -1.0;

    /* Total jiffies in each snapshot */
    unsigned long long total1 = s1.user + s1.nice + s1.system + s1.idle
                              + s1.iowait + s1.irq + s1.softirq + s1.steal;
    unsigned long long total2 = s2.user + s2.nice + s2.system + s2.idle
                              + s2.iowait + s2.irq + s2.softirq + s2.steal;

    /* Idle includes idle + iowait */
    unsigned long long idle1 = s1.idle + s1.iowait;
    unsigned long long idle2 = s2.idle + s2.iowait;

    unsigned long long delta_total = total2 - total1;
    unsigned long long delta_idle  = idle2  - idle1;

    if (delta_total == 0) return 0.0;

    return 100.0 * (double)(delta_total - delta_idle) / (double)delta_total;
}

/* ══════════════════════════════════════════════════════════════
 *  MEMORY  —  read /proc/meminfo
 * ══════════════════════════════════════════════════════════════ */

/**
 * Reads RAM information from /proc/meminfo.
 * All values stored in MB.
 *
 * Return: 0 success, -1 failure.
 */
int read_mem_info(MemInfo *out)
{
    FILE *fp = fopen("/proc/meminfo", "r");
    if (!fp) {
        perror("fopen /proc/meminfo");
        return -1;
    }

    long total_kb = 0, free_kb = 0, available_kb = 0;
    long buffers_kb = 0, cached_kb = 0;
    char line[128];

    while (fgets(line, sizeof(line), fp)) {
        /* sscanf is not space-sensitive, suitable for /proc/meminfo format */
        if      (sscanf(line, "MemTotal: %ld kB",     &total_kb)     == 1) { /* ok */ }
        else if (sscanf(line, "MemFree: %ld kB",      &free_kb)      == 1) { /* ok */ }
        else if (sscanf(line, "MemAvailable: %ld kB", &available_kb) == 1) { /* ok */ }
        else if (sscanf(line, "Buffers: %ld kB",      &buffers_kb)   == 1) { /* ok */ }
        else if (sscanf(line, "Cached: %ld kB",       &cached_kb)    == 1) { /* ok */ }
    }
    fclose(fp);

    out->total_mb     = total_kb     / 1024;
    out->free_mb      = free_kb      / 1024;
    out->available_mb = available_kb / 1024;
    out->buffers_mb   = buffers_kb   / 1024;
    out->cached_mb    = cached_kb    / 1024;
    /* "Used" = Total - Available (not Total - Free!) */
    out->used_mb      = out->total_mb - out->available_mb;

    return 0;
}

/* ══════════════════════════════════════════════════════════════
 *  DISK  —  use statvfs() on mountpoint
 * ══════════════════════════════════════════════════════════════ */

/**
 * Reads disk information for a specific mountpoint (usually "/").
 * All values in MB.
 *
 * Return: 0 success, -1 failure.
 */
int read_disk_info(DiskInfo *out, const char *mountpoint)
{
    struct statvfs st;
    if (statvfs(mountpoint, &st) != 0) {
        perror("statvfs");
        return -1;
    }

    /* f_frsize = fundamental block size (more accurate than f_bsize) */
    unsigned long long blk  = (unsigned long long)st.f_frsize;
    unsigned long long total = (unsigned long long)st.f_blocks * blk;
    unsigned long long free_ = (unsigned long long)st.f_bfree  * blk;
    unsigned long long used  = total - free_;

    out->total_mb = (long)(total / (1024ULL * 1024ULL));
    out->free_mb  = (long)(free_ / (1024ULL * 1024ULL));
    out->used_mb  = (long)(used  / (1024ULL * 1024ULL));

    return 0;
}

/* ══════════════════════════════════════════════════════════════
 *  LOAD AVERAGE  —  read /proc/loadavg
 * ══════════════════════════════════════════════════════════════ */

/**
 * Reads load average 1m, 5m, 15m from /proc/loadavg.
 *
 * Return: 0 success, -1 failure.
 */
int read_load_avg(double *la1, double *la5, double *la15)
{
    FILE *fp = fopen("/proc/loadavg", "r");
    if (!fp) {
        perror("fopen /proc/loadavg");
        return -1;
    }

    int ret = fscanf(fp, "%lf %lf %lf", la1, la5, la15);
    fclose(fp);
    return (ret == 3) ? 0 : -1;
}

/* ══════════════════════════════════════════════════════════════
 *  UPTIME  —  read /proc/uptime
 * ══════════════════════════════════════════════════════════════ */

/**
 * Reads uptime and formats it to "DD:HH:MM:SS".
 *
 * Return: 0 success, -1 failure.
 */
int read_uptime(char *buf, size_t size)
{
    FILE *fp = fopen("/proc/uptime", "r");
    if (!fp) {
        perror("fopen /proc/uptime");
        return -1;
    }

    double uptime_sec = 0.0;
    if (fscanf(fp, "%lf", &uptime_sec) != 1) uptime_sec = 0.0;
    fclose(fp);

    long total = (long)uptime_sec;
    int days   = (int)(total / 86400);
    int hours  = (int)((total % 86400) / 3600);
    int mins   = (int)((total % 3600) / 60);
    int secs   = (int)(total % 60);

    snprintf(buf, size, "%02d:%02d:%02d:%02d", days, hours, mins, secs);
    return 0;
}

/* ══════════════════════════════════════════════════════════════
 *  NETWORK I/O  —  read /proc/net/dev, calculate delta KB/s
 * ══════════════════════════════════════════════════════════════ */

/** Persistent state between calls for delta calculation */
typedef struct {
    unsigned long long rx_bytes;
    unsigned long long tx_bytes;
    struct timespec    ts;
    char               iface[NET_IFACE_LEN];
    int                valid;   /* 1 if there is a previous sample */
} NetState;

static NetState g_net_state = { 0 };

/**
 * Finds the first network interface that is not "lo".
 * Writes to buf, return pointer to buf or NULL if not found.
 */
static char *find_active_iface(char *buf, size_t size)
{
    FILE *fp = fopen("/proc/net/dev", "r");
    if (!fp) return NULL;

    char line[256];
    /* Skip two header lines in /proc/net/dev */
    char *_skip;
    _skip = fgets(line, sizeof(line), fp); (void)_skip;
    _skip = fgets(line, sizeof(line), fp); (void)_skip;

    while (fgets(line, sizeof(line), fp)) {
        char name[NET_IFACE_LEN];
        unsigned long long dummy;
        if (sscanf(line, " %19[^:]: %llu", name, &dummy) == 2) {
            if (strcmp(name, "lo") != 0) {
                /* Ensure null-terminated: snprintf is safer than strncpy */
                snprintf(buf, size, "%s", name);
                fclose(fp);
                return buf;
            }
        }
    }
    fclose(fp);
    return NULL;
}

/**
 * Reads rx_bytes and tx_bytes for a specific interface
 * from /proc/net/dev.
 *
 * Format per line:
 *   iface: rx_bytes rx_pkts rx_errs rx_drop rx_fifo rx_frame rx_comp rx_mcast
 *          tx_bytes tx_pkts tx_errs tx_drop tx_fifo tx_colls tx_carr  tx_comp
 *
 * Return: 0 success, -1 failure.
 */
static int read_iface_bytes(const char   *iface,
                            unsigned long long *rx,
                            unsigned long long *tx)
{
    FILE *fp = fopen("/proc/net/dev", "r");
    if (!fp) return -1;

    char line[256];
    /* Skip two header lines */
    char *_skip;
    _skip = fgets(line, sizeof(line), fp); (void)_skip;
    _skip = fgets(line, sizeof(line), fp); (void)_skip;

    while (fgets(line, sizeof(line), fp)) {
        char name[NET_IFACE_LEN];
        unsigned long long r, t, d;

        /* Parse: name rx_bytes 7 dummy fields tx_bytes */
        int n = sscanf(line,
            " %19[^:]: %llu %llu %llu %llu %llu %llu %llu %llu %llu",
            name, &r, &d, &d, &d, &d, &d, &d, &d, &t);

        if (n == 10 && strcmp(name, iface) == 0) {
            *rx = r;
            *tx = t;
            fclose(fp);
            return 0;
        }
    }
    fclose(fp);
    return -1;
}

/**
 * Fills NetInfo with RX/TX speeds in KB/s.
 * Uses static state to calculate delta between calls.
 *
 * Return: 0 success, -1 failure.
 */
int read_net_info(NetInfo *out)
{
    /* Initialize interface if not yet set */
    if (g_net_state.iface[0] == '\0') {
        if (!find_active_iface(g_net_state.iface, sizeof(g_net_state.iface))) {
            strncpy(g_net_state.iface, "eth0", sizeof(g_net_state.iface) - 1);
        }
    }

    strncpy(out->iface, g_net_state.iface, sizeof(out->iface) - 1);
    out->iface[sizeof(out->iface) - 1] = '\0';

    unsigned long long rx_now = 0, tx_now = 0;
    struct timespec    ts_now;

    clock_gettime(CLOCK_MONOTONIC, &ts_now);

    if (read_iface_bytes(g_net_state.iface, &rx_now, &tx_now) != 0) {
        out->rx_kbps = 0.0;
        out->tx_kbps = 0.0;
        return -1;
    }

    if (g_net_state.valid) {
        double elapsed = (double)(ts_now.tv_sec  - g_net_state.ts.tv_sec)
                       + (double)(ts_now.tv_nsec - g_net_state.ts.tv_nsec) * 1e-9;

        if (elapsed > 0.0 && rx_now >= g_net_state.rx_bytes) {
            out->rx_kbps = (double)(rx_now - g_net_state.rx_bytes) / elapsed / 1024.0;
            out->tx_kbps = (double)(tx_now - g_net_state.tx_bytes) / elapsed / 1024.0;
        } else {
            out->rx_kbps = 0.0;
            out->tx_kbps = 0.0;
        }
    } else {
        /* First sample: no delta yet */
        out->rx_kbps = 0.0;
        out->tx_kbps = 0.0;
    }

    /* Save state for next iteration */
    g_net_state.rx_bytes = rx_now;
    g_net_state.tx_bytes = tx_now;
    g_net_state.ts       = ts_now;
    g_net_state.valid    = 1;

    return 0;
}

/* ══════════════════════════════════════════════════════════════
 *  TIMESTAMP
 * ══════════════════════════════════════════════════════════════ */

/**
 * Fills buf with timestamp in format "DD/MM/YYYY, HH:MM:SS".
 */
void get_timestamp(char *buf, size_t size)
{
    time_t     t  = time(NULL);
    struct tm *tm = localtime(&t);
    if (tm) {
        strftime(buf, size, "%d/%m/%Y, %H:%M:%S", tm);
    } else {
        strncpy(buf, "00/00/0000, 00:00:00", size - 1);
    }
}

/* ══════════════════════════════════════════════════════════════
 *  COLLECT ALL METRICS
 * ══════════════════════════════════════════════════════════════ */

/**
 * Collects all metrics into one SystemMetrics.
 * This function takes ~200ms due to CPU delta calculation.
 *
 * Return: 0 success.
 */
int collect_metrics(SystemMetrics *out)
{
    memset(out, 0, sizeof(*out));

    get_timestamp(out->timestamp, sizeof(out->timestamp));

    /* CPU — this function blocks 200ms internally */
    out->cpu_usage = calculate_cpu_usage();
    if (out->cpu_usage < 0.0) out->cpu_usage = 0.0;

    read_load_avg(&out->load_avg[0], &out->load_avg[1], &out->load_avg[2]);
    read_mem_info(&out->mem);
    read_disk_info(&out->disk, "/");
    read_uptime(out->uptime_str, sizeof(out->uptime_str));
    read_net_info(&out->net);

    return 0;
}

/* ══════════════════════════════════════════════════════════════
 *  JSON LINE OUTPUT
 * ══════════════════════════════════════════════════════════════ */

/**
 * Writes one JSON object (one line) to the log file.
 * Format: JSON Lines — one line = one snapshot.
 *
 * Return: 0 success, -1 failure.
 */
int write_metrics(FILE *fp, const SystemMetrics *m)
{
    int written = fprintf(fp,
        "{"
        "\"time\": \"%s\", "
        "\"cpu_usage\": %.1f, "
        "\"load_avg\": [%.2f, %.2f, %.2f], "
        "\"ram\": {"
            "\"total\": %ld, "
            "\"used\": %ld, "
            "\"free\": %ld, "
            "\"available\": %ld, "
            "\"cached\": %ld, "
            "\"buffers\": %ld"
        "}, "
        "\"disk\": {"
            "\"total_mb\": %ld, "
            "\"used_mb\": %ld, "
            "\"free_mb\": %ld"
        "}, "
        "\"uptime\": \"%s\", "
        "\"net\": {"
            "\"iface\": \"%s\", "
            "\"rx_kbps\": %.2f, "
            "\"tx_kbps\": %.2f"
        "}"
        "}\n",

        m->timestamp,
        m->cpu_usage,
        m->load_avg[0], m->load_avg[1], m->load_avg[2],

        m->mem.total_mb,
        m->mem.used_mb,
        m->mem.free_mb,
        m->mem.available_mb,
        m->mem.cached_mb,
        m->mem.buffers_mb,

        m->disk.total_mb,
        m->disk.used_mb,
        m->disk.free_mb,

        m->uptime_str,

        m->net.iface,
        m->net.rx_kbps,
        m->net.tx_kbps
    );

    return (written > 0) ? 0 : -1;
}

/* ══════════════════════════════════════════════════════════════
 *  LOG MANAGEMENT
 * ══════════════════════════════════════════════════════════════ */

/**
 * Creates log directory if it doesn't exist.
 *
 * Return: 0 success/already exists, -1 failure.
 */
int ensure_log_dir(const char *dirpath)
{
    struct stat st;
    if (stat(dirpath, &st) == 0) {
        return S_ISDIR(st.st_mode) ? 0 : -1;
    }
    if (mkdir(dirpath, 0755) != 0) {
        perror("mkdir");
        return -1;
    }
    return 0;
}

/**
 * Checks log file size. If >= LOG_MAX_BYTES, perform rotation:
 *   status.json  →  status.1.json  (rename, fast O(1))
 *
 * Return: 0 (including if no rotation needed), -1 if rotation failed.
 */
int rotate_log_if_needed(const char *log_path, const char *backup_path)
{
    struct stat st;

    /* File doesn't exist yet — no rotation needed */
    if (stat(log_path, &st) != 0) return 0;

    if (st.st_size < LOG_MAX_BYTES) return 0;

    /* Remove old backup if exists */
    remove(backup_path);

    /* Rename — O(1) operation, atomic on same filesystem */
    if (rename(log_path, backup_path) != 0) {
        fprintf(stderr, "[sysmon] WARNING: Log rotation failed: %s\n",
                strerror(errno));
        return -1;
    }

    fprintf(stderr, "[sysmon] Log rotated: %s → %s\n", log_path, backup_path);
    return 0;
}

/**
 * Opens log file in append mode.
 *
 * Return: FILE* or NULL if failed.
 */
FILE *open_log_file(const char *path)
{
    FILE *fp = fopen(path, "a");
    if (!fp) {
        fprintf(stderr, "[sysmon] ERROR: Cannot open log %s: %s\n",
                path, strerror(errno));
    }
    return fp;
}
