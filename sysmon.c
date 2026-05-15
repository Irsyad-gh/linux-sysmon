/**
 * sysmon.c — Linux System Monitor
 * Implementation of reading system metrics from /proc filesystem.
 *
 * Standard: C11/C17
 * Compilation: gcc -std=c11 -Wall -Wextra -O2
 */

#include "sysmon.h"
#include <signal.h>
#include <fcntl.h>
#include <dirent.h>
#include <ctype.h>

/* ══════════════════════════════════════════════════════════════
 * CPU USAGE — read /proc/stat twice, calculate delta
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
        &out->user, &out->nice, &out->system, &out->idle,
        &out->iowait, &out->irq, &out->softirq, &out->steal);

    fclose(fp);
    return (ret == 8) ? 0 : -1;
}

/**
 * Calculates CPU usage with configurable delay (in microseconds).
 * Return: percent (0.0–100.0), or -1.0 if failed.
 */
double calculate_cpu_usage_delay(int delay_us)
{
    CpuStat s1, s2;

    if (read_cpu_stat(&s1) != 0) return -1.0;

    struct timespec ts_delay = {
        .tv_sec  = (time_t)(delay_us / 1000000),
        .tv_nsec = (long)((delay_us % 1000000) * 1000L)
    };
    nanosleep(&ts_delay, NULL);

    if (read_cpu_stat(&s2) != 0) return -1.0;

    unsigned long long total1 = s1.user + s1.nice + s1.system + s1.idle
                              + s1.iowait + s1.irq + s1.softirq + s1.steal;
    unsigned long long total2 = s2.user + s2.nice + s2.system + s2.idle
                              + s2.iowait + s2.irq + s2.softirq + s2.steal;

    unsigned long long idle1 = s1.idle + s1.iowait;
    unsigned long long idle2 = s2.idle + s2.iowait;

    unsigned long long delta_total = total2 - total1;
    unsigned long long delta_idle  = idle2 - idle1;

    if (delta_total == 0) return 0.0;
    return 100.0 * (double)(delta_total - delta_idle) / (double)delta_total;
}

/**
 * Wrapper dengan default delay (CPU_SAMPLE_DELAY).
 */
double calculate_cpu_usage(void)
{
    return calculate_cpu_usage_delay(CPU_SAMPLE_DELAY);
}

/* ══════════════════════════════════════════════════════════════
 * MEMORY — read /proc/meminfo
 * ══════════════════════════════════════════════════════════════ */

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
    out->used_mb      = out->total_mb - out->available_mb;

    return 0;
}

/* ══════════════════════════════════════════════════════════════
 * DISK — use statvfs() on mountpoint
 * ══════════════════════════════════════════════════════════════ */

int read_disk_info(DiskInfo *out, const char *mountpoint)
{
    struct statvfs st;
    if (statvfs(mountpoint, &st) != 0) {
        perror("statvfs");
        return -1;
    }

    unsigned long long blk   = (unsigned long long)st.f_frsize;
    unsigned long long total  = (unsigned long long)st.f_blocks * blk;
    unsigned long long free_  = (unsigned long long)st.f_bfree  * blk;
    unsigned long long used   = total - free_;

    out->total_mb = (long)(total  / (1024ULL * 1024ULL));
    out->free_mb  = (long)(free_  / (1024ULL * 1024ULL));
    out->used_mb  = (long)(used   / (1024ULL * 1024ULL));

    return 0;
}

/* ══════════════════════════════════════════════════════════════
 * LOAD AVERAGE — read /proc/loadavg
 * ══════════════════════════════════════════════════════════════ */

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
 * UPTIME — read /proc/uptime
 * ══════════════════════════════════════════════════════════════ */

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
    int mins   = (int)((total % 3600)  / 60);
    int secs   = (int)(total % 60);

    snprintf(buf, size, "%02d:%02d:%02d:%02d", days, hours, mins, secs);
    return 0;
}

/* ══════════════════════════════════════════════════════════════
 * NETWORK I/O — read /proc/net/dev
 * ══════════════════════════════════════════════════════════════ */

typedef struct {
    unsigned long long rx_bytes;
    unsigned long long tx_bytes;
    struct timespec    ts;
    char               iface[NET_IFACE_LEN];
    int                valid;
} NetState;

static NetState g_net_state = { 0 };

/**
 * Paksa penggunaan interface tertentu (dari --net-interface).
 */
void set_net_iface(const char *iface)
{
    if (iface && iface[0] != '\0') {
        strncpy(g_net_state.iface, iface, sizeof(g_net_state.iface) - 1);
        g_net_state.iface[sizeof(g_net_state.iface) - 1] = '\0';
        g_net_state.valid = 0;  /* reset state agar pertama kali tidak hitung delta */
    }
}

static char *find_active_iface(char *buf, size_t size)
{
    FILE *fp = fopen("/proc/net/dev", "r");
    if (!fp) return NULL;

    char line[256];
    char *_skip;
    _skip = fgets(line, sizeof(line), fp); (void)_skip;
    _skip = fgets(line, sizeof(line), fp); (void)_skip;

    while (fgets(line, sizeof(line), fp)) {
        char name[NET_IFACE_LEN];
        unsigned long long dummy;
        if (sscanf(line, " %19[^:]: %llu", name, &dummy) == 2) {
            if (strcmp(name, "lo") != 0) {
                snprintf(buf, size, "%s", name);
                fclose(fp);
                return buf;
            }
        }
    }
    fclose(fp);
    return NULL;
}

static int read_iface_bytes(const char *iface,
                             unsigned long long *rx,
                             unsigned long long *tx)
{
    FILE *fp = fopen("/proc/net/dev", "r");
    if (!fp) return -1;

    char line[256];
    char *_skip;
    _skip = fgets(line, sizeof(line), fp); (void)_skip;
    _skip = fgets(line, sizeof(line), fp); (void)_skip;

    while (fgets(line, sizeof(line), fp)) {
        char name[NET_IFACE_LEN];
        unsigned long long r, t, d;

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

int read_net_info(NetInfo *out)
{
    if (g_net_state.iface[0] == '\0') {
        if (!find_active_iface(g_net_state.iface, sizeof(g_net_state.iface))) {
            strncpy(g_net_state.iface, "eth0", sizeof(g_net_state.iface) - 1);
        }
    }

    strncpy(out->iface, g_net_state.iface, sizeof(out->iface) - 1);
    out->iface[sizeof(out->iface) - 1] = '\0';

    unsigned long long rx_now = 0, tx_now = 0;
    struct timespec ts_now;
    clock_gettime(CLOCK_MONOTONIC, &ts_now);

    if (read_iface_bytes(g_net_state.iface, &rx_now, &tx_now) != 0) {
        out->rx_kbps = 0.0;
        out->tx_kbps = 0.0;
        return -1;
    }

    if (g_net_state.valid) {
        double elapsed = (double)(ts_now.tv_sec - g_net_state.ts.tv_sec)
                       + (double)(ts_now.tv_nsec - g_net_state.ts.tv_nsec) * 1e-9;

        if (elapsed > 0.0 && rx_now >= g_net_state.rx_bytes) {
            out->rx_kbps = (double)(rx_now - g_net_state.rx_bytes) / elapsed / 1024.0;
            out->tx_kbps = (double)(tx_now - g_net_state.tx_bytes) / elapsed / 1024.0;
        } else {
            out->rx_kbps = 0.0;
            out->tx_kbps = 0.0;
        }
    } else {
        out->rx_kbps = 0.0;
        out->tx_kbps = 0.0;
    }

    g_net_state.rx_bytes = rx_now;
    g_net_state.tx_bytes = tx_now;
    g_net_state.ts       = ts_now;
    g_net_state.valid    = 1;

    return 0;
}

/* ══════════════════════════════════════════════════════════════
 * TOP PROCESSES — read /proc/<pid>/stat and /proc/<pid>/status
 * ══════════════════════════════════════════════════════════════ */

/** Helper: read /proc/<pid>/comm for the process name */
static int read_proc_name(int pid, char *buf, size_t size)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    if (!fgets(buf, (int)size, fp)) { fclose(fp); return -1; }
    fclose(fp);

    /* Hapus newline */
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
    return 0;
}

/** Helper: baca RSS dalam kB dari /proc/<pid>/status */
static long read_proc_rss_kb(int pid)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;

    char line[128];
    long rss = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "VmRSS: %ld kB", &rss) == 1) break;
    }
    fclose(fp);
    return rss;
}

/**
 * Baca utime dari /proc/<pid>/stat.
 * Return: total utime+stime dalam jiffies, atau 0 jika gagal.
 */
static unsigned long long read_proc_utime(int pid)
{
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/stat", pid);
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;

    unsigned long long utime = 0, stime = 0;
    /* Field 14 (utime) and 15 (stime), but the process name may contain spaces */
    /* Skip sampai ')' lalu baca sisa field */
    int c;
    while ((c = fgetc(fp)) != ')' && c != EOF) { /* skip */ }
    if (c == EOF) { fclose(fp); return 0; }

    char state;
    int ppid, pgrp, session, tty, tpgid;
    unsigned long flags, minflt, cminflt, majflt, cmajflt;
    unsigned long long _utime, _stime;

    if (fscanf(fp, " %c %d %d %d %d %d %lu %lu %lu %lu %lu %llu %llu",
               &state, &ppid, &pgrp, &session, &tty, &tpgid,
               &flags, &minflt, &cminflt, &majflt, &cmajflt,
               &_utime, &_stime) == 13) {
        utime = _utime;
        stime = _stime;
    }
    fclose(fp);
    return utime + stime;
}

/** Comparator for qsort: sort by CPU descending */
int compare_procs_cpu(const void *a, const void *b)
{
    const ProcInfo *pa = (const ProcInfo *)a;
    const ProcInfo *pb = (const ProcInfo *)b;
    if (pb->cpu_pct > pa->cpu_pct) return  1;
    if (pb->cpu_pct < pa->cpu_pct) return -1;
    return 0;
}

/**
 * Baca top N proses berdasarkan CPU usage.
 * Menggunakan dua snapshot dengan delay singkat.
 *
 * procs: array ProcInfo berukuran minimal n.
 * Return: jumlah proses yang berhasil dibaca (0 jika gagal).
 */
int read_top_processes(ProcInfo *procs, int n)
{
    if (!procs || n <= 0) return 0;

    /* Collect all PIDs from /proc */
    DIR *d = opendir("/proc");
    if (!d) return 0;

    /* Allocate temporary arrays for all processes */
    int      max_pids = 4096;
    int     *pids     = (int *)malloc((size_t)max_pids * sizeof(int));
    unsigned long long *t1 = (unsigned long long *)calloc((size_t)max_pids, sizeof(unsigned long long));
    unsigned long long *t2 = (unsigned long long *)calloc((size_t)max_pids, sizeof(unsigned long long));

    if (!pids || !t1 || !t2) {
        free(pids); free(t1); free(t2);
        closedir(d);
        return 0;
    }

    int npids = 0;
    struct dirent *entry;

    /* Snapshot 1: baca semua utime+stime */
    while ((entry = readdir(d)) != NULL && npids < max_pids) {
        if (!isdigit((unsigned char)entry->d_name[0])) continue;
        int pid = atoi(entry->d_name);
        pids[npids] = pid;
        t1[npids]   = read_proc_utime(pid);
        npids++;
    }
    closedir(d);

    /* Small delay to compute CPU delta */
    struct timespec ts = { .tv_sec = 0, .tv_nsec = 100000000L }; /* 100ms */
    nanosleep(&ts, NULL);

    /* Read total system jiffies for normalization */
    CpuStat cs1, cs2;
    read_cpu_stat(&cs1);

    /* Snapshot 2 */
    for (int i = 0; i < npids; i++) {
        t2[i] = read_proc_utime(pids[i]);
    }
    read_cpu_stat(&cs2);

    unsigned long long sys_total1 = cs1.user + cs1.nice + cs1.system + cs1.idle
                                  + cs1.iowait + cs1.irq + cs1.softirq + cs1.steal;
    unsigned long long sys_total2 = cs2.user + cs2.nice + cs2.system + cs2.idle
                                  + cs2.iowait + cs2.irq + cs2.softirq + cs2.steal;
    unsigned long long sys_delta  = (sys_total2 > sys_total1) ? (sys_total2 - sys_total1) : 1;

    /* Fill temporary process array */
    ProcInfo *all = (ProcInfo *)calloc((size_t)npids, sizeof(ProcInfo));
    if (!all) {
        free(pids); free(t1); free(t2);
        return 0;
    }

    for (int i = 0; i < npids; i++) {
        all[i].pid = pids[i];
        if (read_proc_name(pids[i], all[i].name, sizeof(all[i].name)) != 0) {
            snprintf(all[i].name, sizeof(all[i].name), "[%d]", pids[i]);
        }

        unsigned long long proc_delta = (t2[i] >= t1[i]) ? (t2[i] - t1[i]) : 0;
        all[i].cpu_pct = (sys_delta > 0)
            ? 100.0 * (double)proc_delta / (double)sys_delta
            : 0.0;

        all[i].rss_mb = read_proc_rss_kb(pids[i]) / 1024;
    }

    /* Sort by CPU descending */
    qsort(all, (size_t)npids, sizeof(ProcInfo), compare_procs_cpu);

    /* Take top N */
    int take = (npids < n) ? npids : n;
    for (int i = 0; i < take; i++) {
        procs[i] = all[i];
    }

    free(pids); free(t1); free(t2); free(all);
    return take;
}

/* ══════════════════════════════════════════════════════════════
 * TIMESTAMP
 * ══════════════════════════════════════════════════════════════ */

void get_timestamp(char *buf, size_t size)
{
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    if (tm_info) {
        strftime(buf, size, "%d/%m/%Y, %H:%M:%S", tm_info);
    } else {
        strncpy(buf, "00/00/0000, 00:00:00", size - 1);
    }
}

/* ══════════════════════════════════════════════════════════════
 * COLLECT ALL METRICS
 * ══════════════════════════════════════════════════════════════ */

/**
 * Collects all metrics into one SystemMetrics.
 *
 * mountpoint   : disk mount point (default "/")
 * cpu_delay_us : CPU sampling delay in microseconds
 * top_n        : number of top processes to collect (0 = none)
 *
 * Return: 0 success.
 */
int collect_metrics(SystemMetrics *out, const char *mountpoint,
                    int cpu_delay_us, int top_n)
{
    memset(out, 0, sizeof(*out));

    get_timestamp(out->timestamp, sizeof(out->timestamp));

    /* CPU — blocks cpu_delay_us µs internally */
    out->cpu_usage = calculate_cpu_usage_delay(cpu_delay_us);
    if (out->cpu_usage < 0.0) out->cpu_usage = 0.0;

    read_load_avg(&out->load_avg[0], &out->load_avg[1], &out->load_avg[2]);
    read_mem_info(&out->mem);
    read_disk_info(&out->disk, mountpoint ? mountpoint : "/");
    read_uptime(out->uptime_str, sizeof(out->uptime_str));
    read_net_info(&out->net);

    /* Top processes */
    out->top_n = 0;
    out->top_procs = NULL;
    if (top_n > 0) {
        out->top_procs = (ProcInfo *)calloc((size_t)top_n, sizeof(ProcInfo));
        if (out->top_procs) {
            out->top_n = read_top_processes(out->top_procs, top_n);
        }
    }

    return 0;
}

/** Free dynamic memory inside SystemMetrics */
void free_metrics(SystemMetrics *m)
{
    if (m && m->top_procs) {
        free(m->top_procs);
        m->top_procs = NULL;
        m->top_n     = 0;
    }
}

/* ══════════════════════════════════════════════════════════════
 * JSON LINE OUTPUT
 * ══════════════════════════════════════════════════════════════ */

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
        "}",
        m->timestamp,
        m->cpu_usage,
        m->load_avg[0], m->load_avg[1], m->load_avg[2],
        m->mem.total_mb, m->mem.used_mb, m->mem.free_mb,
        m->mem.available_mb, m->mem.cached_mb, m->mem.buffers_mb,
        m->disk.total_mb, m->disk.used_mb, m->disk.free_mb,
        m->uptime_str,
        m->net.iface, m->net.rx_kbps, m->net.tx_kbps
    );

    /* Append top processes if present */
    if (m->top_n > 0 && m->top_procs) {
        fprintf(fp, ", \"top_processes\": [");
        for (int i = 0; i < m->top_n; i++) {
            if (i > 0) fprintf(fp, ", ");
            fprintf(fp,
                "{\"pid\": %d, \"name\": \"%s\", \"cpu_pct\": %.1f, \"rss_mb\": %ld}",
                m->top_procs[i].pid,
                m->top_procs[i].name,
                m->top_procs[i].cpu_pct,
                m->top_procs[i].rss_mb
            );
        }
        fprintf(fp, "]");
    }

    fprintf(fp, "}\n");
    return (written > 0) ? 0 : -1;
}

/* ══════════════════════════════════════════════════════════════
 * LOG MANAGEMENT
 * ══════════════════════════════════════════════════════════════ */

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
 * Check log size. If >= max_bytes, perform rotation.
 * max_bytes = 0 → use default LOG_MAX_BYTES.
 */
int rotate_log_if_needed(const char *log_path, const char *backup_path,
                          long max_bytes)
{
    if (max_bytes <= 0) max_bytes = LOG_MAX_BYTES;

    struct stat st;
    if (stat(log_path, &st) != 0) return 0;
    if (st.st_size < (off_t)max_bytes) return 0;

    remove(backup_path);

    if (rename(log_path, backup_path) != 0) {
        fprintf(stderr, "[sysmon] WARNING: Log rotation failed: %s\n",
                strerror(errno));
        return -1;
    }

    fprintf(stderr, "[sysmon] Log rotated: %s → %s\n", log_path, backup_path);
    return 0;
}

FILE *open_log_file(const char *path)
{
    FILE *fp = fopen(path, "a");
    if (!fp) {
        fprintf(stderr, "[sysmon] ERROR: Cannot open log %s: %s\n",
                path, strerror(errno));
    }
    return fp;
}

/* ══════════════════════════════════════════════════════════════
 * DAEMON
 * ══════════════════════════════════════════════════════════════ */

/**
 * Daemonize proses: fork, setsid, tutup stdin/stdout/stderr.
 * If pid_file != NULL, write the PID to that file.
 *
 * Return: 0 success (di proses daemon), -1 error.
 */
int daemonize(const char *pid_file, int verbose)
{
    pid_t pid = fork();

    if (pid < 0) {
        perror("[sysmon] fork");
        return -1;
    }

    if (pid > 0) {
        /* Parent: write PID then exit */
        if (pid_file && pid_file[0] != '\0') {
            FILE *pf = fopen(pid_file, "w");
            if (pf) {
                fprintf(pf, "%d\n", (int)pid);
                fclose(pf);
                if (verbose) {
                    fprintf(stderr, "[sysmon] PID %d written to %s\n",
                            (int)pid, pid_file);
                }
            } else {
                fprintf(stderr, "[sysmon] WARNING: Cannot write PID file %s: %s\n",
                        pid_file, strerror(errno));
            }
        }
        if (verbose) {
            fprintf(stderr, "[sysmon] Daemon started with PID %d\n", (int)pid);
        }
        exit(EXIT_SUCCESS);
    }

    /* Child: create a new session */
    if (setsid() < 0) {
        perror("[sysmon] setsid");
        return -1;
    }

    /* Redirect stdin/stdout/stderr ke /dev/null */
    int devnull = open("/dev/null", O_RDWR);
    if (devnull >= 0) {
        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        if (devnull > STDERR_FILENO) close(devnull);
    }

    return 0;
}
