/**
 * sysmon.h — Linux System Monitor
 * Main header: structs, constants, and function declarations.
 *
 * Standard: C11/C17
 * Target: Linux (/proc filesystem)
 */

#ifndef SYSMON_H
#define SYSMON_H

/*
 * Enable POSIX.1-2008 API:
 * - sigaction, sigemptyset
 * - clock_gettime (CLOCK_MONOTONIC)
 * - PATH_MAX from <limits.h>
 * - strftime and localtime_r
 * Must be declared BEFORE any includes.
 */
#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/statvfs.h>

/* ── Version ─────────────────────────────────────────────────── */
#define SYSMON_VERSION "1.3.0"

#define SAMPLE_INTERVAL   5                       /* seconds between sampling */
/* ── Compile-time defaults (can be overridden via CLI args) ── */
#define CPU_SAMPLE_DELAY  200000                  /* µs CPU delta delay (200ms) */
#define LOG_DIR           ".status"               /* relative to $HOME */
#define LOG_FILE          "status.json"
#define LOG_BACKUP        "status.1.json"
#define LOG_MAX_BYTES     (10L * 1024L * 1024L)  /* 10 MB */

/* ── Ukuran buffer ───────────────────────────────────────────── */
#define NET_IFACE_LEN  20
#define TIMESTAMP_LEN  32
#define UPTIME_LEN     20
#define PROC_NAME_LEN  256

/* ── Structs ────────────────────────────────────────────────── */

/** Raw CPU jiffy counters from /proc/stat */
typedef struct {
    unsigned long long user;
    unsigned long long nice;
    unsigned long long system;
    unsigned long long idle;
    unsigned long long iowait;
    unsigned long long irq;
    unsigned long long softirq;
    unsigned long long steal;
} CpuStat;

/** Memory information in MB */
typedef struct {
    long total_mb;
    long used_mb;
    long free_mb;
    long available_mb;
    long cached_mb;
    long buffers_mb;
} MemInfo;

/** Disk information in MB */
typedef struct {
    long total_mb;
    long used_mb;
    long free_mb;
} DiskInfo;

/** Network speed in KB/s */
typedef struct {
    char   iface[NET_IFACE_LEN];
    double rx_kbps;
    double tx_kbps;
} NetInfo;

/** Informasi satu proses (untuk --top-processes) */
typedef struct {
    int    pid;
    char   name[PROC_NAME_LEN];
    double cpu_pct;     /* persentase CPU */
    long   rss_mb;      /* RAM usage dalam MB */
} ProcInfo;

/** Complete metrics structure for one snapshot */
typedef struct {
    char         timestamp[TIMESTAMP_LEN];
    double       cpu_usage;
    double       load_avg[3];              /* 1m, 5m, 15m */
    MemInfo      mem;
    DiskInfo     disk;
    char         uptime_str[UPTIME_LEN];
    NetInfo      net;

    /* Top processes — hanya diisi jika top_n > 0 */
    ProcInfo    *top_procs;
    int          top_n;
} SystemMetrics;

/* ── Function Declarations ───────────────────────────────────── */

/* CPU */
int    read_cpu_stat(CpuStat *out);
double calculate_cpu_usage_delay(int delay_us);
double calculate_cpu_usage(void);

/* Memory */
int read_mem_info(MemInfo *out);

/* Disk */
int read_disk_info(DiskInfo *out, const char *mountpoint);

/* Load Average & Uptime */
int read_load_avg(double *la1, double *la5, double *la15);
int read_uptime(char *buf, size_t size);

/* Network */
int read_net_info(NetInfo *out);
void set_net_iface(const char *iface);   /* paksa interface tertentu */

/* Top Processes */
int read_top_processes(ProcInfo *procs, int n);
int compare_procs_cpu(const void *a, const void *b);

/* Utilities */
void get_timestamp(char *buf, size_t size);
int  collect_metrics(SystemMetrics *out, const char *mountpoint,
                     int cpu_delay_us, int top_n);
int  write_metrics(FILE *fp, const SystemMetrics *m);
void free_metrics(SystemMetrics *m);

/* Log Management */
int   ensure_log_dir(const char *dirpath);
int   rotate_log_if_needed(const char *log_path, const char *backup_path,
                            long max_bytes);
FILE *open_log_file(const char *path);

/* Daemon */
int  daemonize(const char *pid_file, int verbose);

#endif /* SYSMON_H */
