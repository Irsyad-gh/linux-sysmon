/**
 * args.h — CLI Argument Parser for Linux System Monitor
 *
 * Defines SysmonConfig struct dan fungsi parse_args().
 * Standard: C11/C17
 */

#ifndef ARGS_H
#define ARGS_H

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* ── Defaults (bisa di-override lewat CLI) ───────────────────── */
#define DEFAULT_INTERVAL     5
#define DEFAULT_LOG_DIR      ".status"
#define DEFAULT_LOG_FILE     "status.json"
#define DEFAULT_LOG_BACKUP   "status.1.json"
#define DEFAULT_MAX_MB       10
#define DEFAULT_DISK_MOUNT   "/"
#define DEFAULT_CPU_DELAY    200000   /* µs */
#define DEFAULT_TOP_N        0        /* 0 = tidak aktif */

/* ── Konfigurasi Runtime ─────────────────────────────────────── */
typedef struct {
    /* Interval & timing */
    int     interval;           /* -i/--interval <detik> */
    int     cpu_delay_us;       /* --cpu-delay <us>      */

    /* Logging */
    char    log_dir[PATH_MAX];  /* -l/--logdir            */
    char    log_file[PATH_MAX]; /* -f/--logfile <nama>    */
    char    log_output[PATH_MAX];/* -o/--output <path>   (override logdir+file) */
    int     max_size_mb;        /* -m/--maxsize <MB>      */
    int     no_log;             /* --no-log               */

    /* Daemon */
    int     daemon_mode;        /* -d/--daemon            */
    char    pid_file[PATH_MAX]; /* -p/--pidfile <file>    */

    /* Network */
    char    net_iface[64];      /* -n/--net-interface     */

    /* Display */
    int     quiet;              /* -q/--quiet             */
    int     verbose;            /* -V/--verbose           */

    /* Process monitoring */
    int     top_n;              /* -t/--top-processes <N> */

    /* Disk mount */
    char    disk_mount[PATH_MAX]; /* --disk-mount         */

    /* Mode */
    int     one_shot;           /* -1/--one-shot          */

    /* Config file */
    char    config_file[PATH_MAX]; /* -c/--config <file>  */
} SysmonConfig;

/* ── Function Declarations ───────────────────────────────────── */

/**
 * Inisialisasi config dengan nilai default.
 */
void config_init(SysmonConfig *cfg);

/**
 * Parse argv ke dalam SysmonConfig.
 * Return: 0 = OK, 1 = keluar normal (--help/--version), -1 = error.
 */
int parse_args(int argc, char *argv[], SysmonConfig *cfg);

/**
 * Load konfigurasi dari file JSON/INI sederhana.
 * Dipanggil setelah parse_args jika cfg->config_file diisi.
 * Return: 0 = OK, -1 = gagal.
 */
int load_config_file(SysmonConfig *cfg);

/**
 * Tampilkan usage/help.
 */
void print_help(const char *progname);

/**
 * Tampilkan versi.
 */
void print_version(void);

#endif /* ARGS_H */
