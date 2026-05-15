/**
 * args.h — CLI Argument Parser for Linux System Monitor
 *
 * Defines SysmonConfig struct and the parse_args() function.
 * Standard: C11/C17
 */

#ifndef ARGS_H
#define ARGS_H

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* ── Defaults (can be overridden via CLI) ───────────────────── */
#define DEFAULT_INTERVAL     5
#define DEFAULT_LOG_DIR      ".status"
#define DEFAULT_LOG_FILE     "status.json"
#define DEFAULT_LOG_BACKUP   "status.1.json"
#define DEFAULT_MAX_MB       10
#define DEFAULT_DISK_MOUNT   "/"
#define DEFAULT_CPU_DELAY    200000   /* µs */
#define DEFAULT_TOP_N        0        /* 0 = disabled */

/* ── Runtime Configuration ─────────────────────────────────── */
typedef struct {
    /* Interval & timing */
    int     interval;           /* -i/--interval <seconds> */
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
 * Initialize config with default values.
 */
void config_init(SysmonConfig *cfg);

/**
 * Parse argv into SysmonConfig.
 * Return: 0 = OK, 1 = normal exit (--help/--version), -1 = error.
 */
int parse_args(int argc, char *argv[], SysmonConfig *cfg);

/**
 * Load configuration from a simple JSON/INI file.
 * Called after parse_args if cfg->config_file is set.
 * Return: 0 = OK, -1 = failure.
 */
int load_config_file(SysmonConfig *cfg);

/**
 * Show usage/help.
 */
void print_help(const char *progname);

/**
 * Show version.
 */
void print_version(void);

#endif /* ARGS_H */
