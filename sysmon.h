/**
 * sysmon.h — Linux System Monitor
 * Header utama: structs, konstanta, dan deklarasi fungsi.
 *
 * Standar: C11/C17
 * Target : Linux (/proc filesystem)
 */

#ifndef SYSMON_H
#define SYSMON_H

/*
 * Aktifkan POSIX.1-2008 API:
 *   - sigaction, sigemptyset
 *   - clock_gettime (CLOCK_MONOTONIC)
 *   - PATH_MAX dari <limits.h>
 *   - strftime dan localtime_r
 * Harus dideklarasikan SEBELUM include apapun.
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

/* ── Konfigurasi ────────────────────────────────────────────── */

#define SYSMON_VERSION     "1.0.0"
#define SAMPLE_INTERVAL    5               /* detik antar sampling      */
#define CPU_SAMPLE_DELAY   200000          /* µs jeda delta CPU (200ms) */
#define LOG_DIR            ".status"       /* relatif terhadap $HOME    */
#define LOG_FILE           "status.json"
#define LOG_BACKUP         "status.1.json"
#define LOG_MAX_BYTES      (10L * 1024L * 1024L)  /* 10 MB                     */
#define NET_IFACE_LEN      20
#define TIMESTAMP_LEN      32
#define UPTIME_LEN         20

/* ── Structs ────────────────────────────────────────────────── */

/** Raw CPU jiffy counters dari /proc/stat */
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

/** Informasi memori dalam MB */
typedef struct {
    long total_mb;
    long used_mb;
    long free_mb;
    long available_mb;
    long cached_mb;
    long buffers_mb;
} MemInfo;

/** Informasi disk dalam MB */
typedef struct {
    long total_mb;
    long used_mb;
    long free_mb;
} DiskInfo;

/** Kecepatan jaringan dalam KB/s */
typedef struct {
    char   iface[NET_IFACE_LEN];
    double rx_kbps;
    double tx_kbps;
} NetInfo;

/** Struktur metrik lengkap satu snapshot */
typedef struct {
    char     timestamp[TIMESTAMP_LEN];
    double   cpu_usage;
    double   load_avg[3];   /* 1m, 5m, 15m */
    MemInfo  mem;
    DiskInfo disk;
    char     uptime_str[UPTIME_LEN];
    NetInfo  net;
} SystemMetrics;

/* ── Deklarasi Fungsi ───────────────────────────────────────── */

/* CPU */
int    read_cpu_stat(CpuStat *out);
double calculate_cpu_usage(void);

/* Memory */
int    read_mem_info(MemInfo *out);

/* Disk */
int    read_disk_info(DiskInfo *out, const char *mountpoint);

/* Load Average & Uptime */
int    read_load_avg(double *la1, double *la5, double *la15);
int    read_uptime(char *buf, size_t size);

/* Network */
int    read_net_info(NetInfo *out);

/* Utilitas */
void   get_timestamp(char *buf, size_t size);
int    collect_metrics(SystemMetrics *out);
int    write_metrics(FILE *fp, const SystemMetrics *m);

/* Manajemen Log */
int    ensure_log_dir(const char *dirpath);
int    rotate_log_if_needed(const char *log_path, const char *backup_path);
FILE  *open_log_file(const char *path);

#endif /* SYSMON_H */
