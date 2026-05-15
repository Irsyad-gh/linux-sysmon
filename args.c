/**
 * args.c — CLI Argument Parser Implementation
 *
 * Implements parsing of all CLI arguments for sysmon.
 * Supports formats: -x, --long, --long=value, --long value
 *
 * Standard: C11/C17
 */

#include "args.h"
#include "sysmon.h"   /* for SYSMON_VERSION */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/* ═════════════════════════════════════════════════════════════=
 * CONFIG FILE LOADER (simple JSON/INI)
 * ══════════════════════════════════════════════════════════════ */

/** Parse integer with validation. Return -1 if invalid. */
static int parse_int(const char *s)
{
    if (!s || *s == '\0') return -1;
    char *end;
    long v = strtol(s, &end, 10);
    if (*end != '\0' || v <= 0 || v > INT_MAX) return -1;
    return (int)v;
}

/** Trim whitespace from string (in-place). */
static char *strtrim(char *s)
{
    if (!s) return s;
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s) - 1;
    while (e > s && isspace((unsigned char)*e)) *e-- = '\0';
    return s;
}

/* ══════════════════════════════════════════════════════════════
 * HELP & VERSION
 * ══════════════════════════════════════════════════════════════ */

void print_version(void)
{
    printf("sysmon version %s\n", SYSMON_VERSION);
    printf("Linux System Monitor — reads from /proc filesystem\n");
    printf("License: MIT\n");
}

void print_help(const char *progname)
{
    printf("Usage: %s [OPTIONS]\n\n", progname);
        printf("A lightweight Linux system monitor with JSON logging.\n\n");

        printf("GENERAL OPTIONS:\n");
        printf("  -h, --help                  Show this message then exit\n");
        printf("  -v, --version               Show version then exit\n");
        printf("  -V, --verbose               Show more info/debug\n");
        printf("  -c, --config <file>         Load configuration from JSON/INI file\n\n");

        printf("TIMING:\n");
        printf("  -i, --interval <seconds>    Sampling interval (default: %d seconds)\n",
            DEFAULT_INTERVAL);
        printf("      --cpu-delay <us>        Delay CPU sampling in microseconds\n");
        printf("                              (default: %d µs)\n", DEFAULT_CPU_DELAY);
        printf("  -1, --one-shot              Take one metrics snapshot and exit\n");
        printf("                              (suitable for cron/scripts)\n\n");

        printf("LOGGING:\n");
        printf("  -l, --logdir <dir>          Log directory (default: ~/.status)\n");
        printf("  -f, --logfile <name>        Log file name (default: status.json)\n");
        printf("  -o, --output <path>         Output log to a specific path\n");
        printf("                              (overrides --logdir + --logfile)\n");
        printf("  -m, --maxsize <MB>          Maximum size before rotate (default: %d MB)\n",
            DEFAULT_MAX_MB);
        printf("      --no-log                Disable file logging (console only)\n\n");

        printf("DAEMON:\n");
        printf("  -d, --daemon                Run as daemon (background)\n");
        printf("  -p, --pidfile <file>        PID file path (for daemon)\n\n");

        printf("NETWORK:\n");
        printf("  -n, --net-interface <iface> Select specific interface (e.g., eth0, wlan0)\n\n");

        printf("DISPLAY:\n");
        printf("  -q, --quiet                 Quiet mode (no console display, logging only)\n");
        printf("  -t, --top-processes <N>     Show top N processes (CPU/RAM)\n\n");

        printf("DISK:\n");
        printf("      --disk-mount <path>     Monitor disk on specific mount point\n");
        printf("                              (default: /)\n\n");

        printf("EXAMPLES:\n");
        printf("  %s                          Run with all defaults\n", progname);
        printf("  %s -i 10 -q -d             Daemon mode, interval 10 seconds, no console\n", progname);
        printf("  %s -1 -o /tmp/snap.json    One-time snapshot to a specific file\n", progname);
        printf("  %s -n wlan0 -t 5 -V        Monitor wlan0, top 5 processes, verbose\n", progname);
        printf("  %s -c /etc/sysmon.ini      Load configuration from file\n\n", progname);
}

/* ══════════════════════════════════════════════════════════════
 * CONFIG INIT
 * ══════════════════════════════════════════════════════════════ */

void config_init(SysmonConfig *cfg)
{
    memset(cfg, 0, sizeof(*cfg));

    cfg->interval       = DEFAULT_INTERVAL;
    cfg->cpu_delay_us   = DEFAULT_CPU_DELAY;
    cfg->max_size_mb    = DEFAULT_MAX_MB;
    cfg->top_n          = DEFAULT_TOP_N;

    strncpy(cfg->log_file,    DEFAULT_LOG_FILE,    sizeof(cfg->log_file) - 1);
    strncpy(cfg->disk_mount,  DEFAULT_DISK_MOUNT,  sizeof(cfg->disk_mount) - 1);

    /* log_dir will be filled from $HOME at runtime if not set */
    cfg->log_dir[0]     = '\0';
    cfg->log_output[0]  = '\0';
    cfg->pid_file[0]    = '\0';
    cfg->net_iface[0]   = '\0';
    cfg->config_file[0] = '\0';
}

/* ══════════════════════════════════════════════════════════════
 * CONFIG FILE LOADER (simple JSON/INI)
 * ══════════════════════════════════════════════════════════════ */

/**
 * Parse one INI line: key = value
 * JSON: "key": value  (simple subset, strings/numbers)
 */
static void apply_config_line(SysmonConfig *cfg, const char *key, const char *val)
{
    if (!key || !val) return;

    if (strcmp(key, "interval") == 0) {
        int v = parse_int(val);
        if (v > 0) cfg->interval = v;
    } else if (strcmp(key, "logdir") == 0 || strcmp(key, "log_dir") == 0) {
        strncpy(cfg->log_dir, val, sizeof(cfg->log_dir) - 1);
    } else if (strcmp(key, "logfile") == 0 || strcmp(key, "log_file") == 0) {
        strncpy(cfg->log_file, val, sizeof(cfg->log_file) - 1);
    } else if (strcmp(key, "output") == 0) {
        strncpy(cfg->log_output, val, sizeof(cfg->log_output) - 1);
    } else if (strcmp(key, "maxsize") == 0 || strcmp(key, "max_size_mb") == 0) {
        int v = parse_int(val);
        if (v > 0) cfg->max_size_mb = v;
    } else if (strcmp(key, "daemon") == 0) {
        cfg->daemon_mode = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0) ? 1 : 0;
    } else if (strcmp(key, "pidfile") == 0 || strcmp(key, "pid_file") == 0) {
        strncpy(cfg->pid_file, val, sizeof(cfg->pid_file) - 1);
    } else if (strcmp(key, "net_interface") == 0 || strcmp(key, "net-interface") == 0) {
        strncpy(cfg->net_iface, val, sizeof(cfg->net_iface) - 1);
    } else if (strcmp(key, "quiet") == 0) {
        cfg->quiet = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0) ? 1 : 0;
    } else if (strcmp(key, "verbose") == 0) {
        cfg->verbose = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0) ? 1 : 0;
    } else if (strcmp(key, "no_log") == 0 || strcmp(key, "no-log") == 0) {
        cfg->no_log = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0) ? 1 : 0;
    } else if (strcmp(key, "top_processes") == 0 || strcmp(key, "top-processes") == 0) {
        int v = parse_int(val);
        if (v > 0) cfg->top_n = v;
    } else if (strcmp(key, "disk_mount") == 0 || strcmp(key, "disk-mount") == 0) {
        strncpy(cfg->disk_mount, val, sizeof(cfg->disk_mount) - 1);
    } else if (strcmp(key, "cpu_delay") == 0 || strcmp(key, "cpu-delay") == 0) {
        int v = parse_int(val);
        if (v > 0) cfg->cpu_delay_us = v;
    } else if (strcmp(key, "one_shot") == 0 || strcmp(key, "one-shot") == 0) {
        cfg->one_shot = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0) ? 1 : 0;
    }
}

/**
 * Strip character from string: remove char c from entire string.
 */
static void strip_char(char *s, char c)
{
    char *r = s, *w = s;
    while (*r) {
        if (*r != c) *w++ = *r;
        r++;
    }
    *w = '\0';
}

int load_config_file(SysmonConfig *cfg)
{
    if (cfg->config_file[0] == '\0') return 0;

    FILE *fp = fopen(cfg->config_file, "r");
    if (!fp) {
        fprintf(stderr, "[sysmon] ERROR: Cannot open config file '%s': %s\n",
                cfg->config_file, strerror(errno));
        return -1;
    }

    char line[512];
    int  lineno = 0;
    int  is_json = 0;

        /* Detect format: if first non-comment line contains '{', assume JSON */
    while (fgets(line, sizeof(line), fp)) {
        lineno++;
        char *t = strtrim(line);
        if (*t == '\0' || *t == '#' || *t == ';') continue;
        if (*t == '{') { is_json = 1; }
        break;
    }
    rewind(fp);
    lineno = 0;

    while (fgets(line, sizeof(line), fp)) {
        lineno++;
        char *t = strtrim(line);

        /* Skip comments, empty lines, and JSON structural characters */
        if (*t == '\0' || *t == '#' || *t == ';' || *t == '{' || *t == '}') continue;

        char key[128] = {0};
        char val[256] = {0};

        if (is_json) {
            /* Format: "key": "value"  atau  "key": number */
            char *colon = strchr(t, ':');
            if (!colon) continue;
            *colon = '\0';
            strncpy(key, strtrim(t), sizeof(key) - 1);
            strncpy(val, strtrim(colon + 1), sizeof(val) - 1);

            /* Remove quotes and trailing comma */
            strip_char(key, '"');
            strip_char(key, '\'');

            /* Hapus trailing koma dari value */
            size_t vlen = strlen(val);
            if (vlen > 0 && val[vlen - 1] == ',') val[vlen - 1] = '\0';

            strtrim(val);
            strip_char(val, '"');
            strip_char(val, '\'');
        } else {
            /* Format INI: key = value */
            char *eq = strchr(t, '=');
            if (!eq) continue;
            *eq = '\0';
            strncpy(key, strtrim(t), sizeof(key) - 1);
            strncpy(val, strtrim(eq + 1), sizeof(val) - 1);

            /* Remove inline comment */
            char *comment = strchr(val, '#');
            if (!comment) comment = strchr(val, ';');
            if (comment) { *comment = '\0'; strtrim(val); }
        }

        if (key[0] != '\0' && val[0] != '\0') {
            apply_config_line(cfg, key, val);
        }
    }

    fclose(fp);

    if (cfg->verbose) {
        fprintf(stderr, "[sysmon] Config loaded from '%s' (%s format)\n",
                cfg->config_file, is_json ? "JSON" : "INI");
    }

    return 0;
}

/* ══════════════════════════════════════════════════════════════
 * MAIN ARGUMENT PARSER
 * ══════════════════════════════════════════════════════════════ */

/**
 * Ambil nilai dari argumen berikutnya.
 * Mendukung: --opt value  dan  --opt=value
 *
 * eq_val: jika format --opt=value, ini menunjuk ke value-nya.
 * Return: pointer ke nilai, atau NULL jika tidak ada.
 */
static const char *get_optval(const char *eq_val, int *i, int argc, char *argv[])
{
    if (eq_val) return eq_val;           /* --opt=value */
    if (*i + 1 < argc) {
        (*i)++;
        return argv[*i];                 /* --opt value */
    }
    return NULL;
}

int parse_args(int argc, char *argv[], SysmonConfig *cfg)
{
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];

        /* Check if long option */
        int is_long = (strncmp(arg, "--", 2) == 0);
        int is_short = (!is_long && arg[0] == '-' && arg[1] != '\0' && arg[2] == '\0');

        if (is_long) {
            const char *opt = arg + 2;       /* lewati "--" */
            const char *eq  = strchr(opt, '=');
            char        longopt[64] = {0};
            const char *eq_val = NULL;

            if (eq) {
                size_t optlen = (size_t)(eq - opt);
                if (optlen >= sizeof(longopt)) optlen = sizeof(longopt) - 1;
                strncpy(longopt, opt, optlen);
                eq_val = eq + 1;
            } else {
                strncpy(longopt, opt, sizeof(longopt) - 1);
            }

            /* ── Long options ─────────────────────────────────── */
            if (strcmp(longopt, "help") == 0) {
                print_help(argv[0]);
                return 1;

            } else if (strcmp(longopt, "version") == 0) {
                print_version();
                return 1;

            } else if (strcmp(longopt, "interval") == 0) {
                const char *v = get_optval(eq_val, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] --interval requires a value\n"); return -1; }
                int iv = parse_int(v);
                if (iv < 1) { fprintf(stderr, "[sysmon] --interval must be >= 1 second\n"); return -1; }
                cfg->interval = iv;

            } else if (strcmp(longopt, "daemon") == 0) {
                cfg->daemon_mode = 1;

            } else if (strcmp(longopt, "logdir") == 0) {
                const char *v = get_optval(eq_val, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] --logdir requires a value\n"); return -1; }
                strncpy(cfg->log_dir, v, sizeof(cfg->log_dir) - 1);

            } else if (strcmp(longopt, "logfile") == 0) {
                const char *v = get_optval(eq_val, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] --logfile requires a value\n"); return -1; }
                strncpy(cfg->log_file, v, sizeof(cfg->log_file) - 1);

            } else if (strcmp(longopt, "maxsize") == 0) {
                const char *v = get_optval(eq_val, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] --maxsize requires a value\n"); return -1; }
                int mv = parse_int(v);
                if (mv < 1) { fprintf(stderr, "[sysmon] --maxsize must be >= 1 MB\n"); return -1; }
                cfg->max_size_mb = mv;

            } else if (strcmp(longopt, "output") == 0) {
                const char *v = get_optval(eq_val, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] --output requires a value\n"); return -1; }
                strncpy(cfg->log_output, v, sizeof(cfg->log_output) - 1);

            } else if (strcmp(longopt, "net-interface") == 0) {
                const char *v = get_optval(eq_val, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] --net-interface requires a value\n"); return -1; }
                strncpy(cfg->net_iface, v, sizeof(cfg->net_iface) - 1);

            } else if (strcmp(longopt, "quiet") == 0) {
                cfg->quiet = 1;

            } else if (strcmp(longopt, "config") == 0) {
                const char *v = get_optval(eq_val, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] --config requires a value\n"); return -1; }
                strncpy(cfg->config_file, v, sizeof(cfg->config_file) - 1);

            } else if (strcmp(longopt, "pidfile") == 0) {
                const char *v = get_optval(eq_val, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] --pidfile requires a value\n"); return -1; }
                strncpy(cfg->pid_file, v, sizeof(cfg->pid_file) - 1);

            } else if (strcmp(longopt, "no-log") == 0) {
                cfg->no_log = 1;

            } else if (strcmp(longopt, "top-processes") == 0) {
                const char *v = get_optval(eq_val, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] --top-processes requires a value\n"); return -1; }
                int tv = parse_int(v);
                if (tv < 1) { fprintf(stderr, "[sysmon] --top-processes must be >= 1\n"); return -1; }
                cfg->top_n = tv;

            } else if (strcmp(longopt, "disk-mount") == 0) {
                const char *v = get_optval(eq_val, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] --disk-mount requires a value\n"); return -1; }
                strncpy(cfg->disk_mount, v, sizeof(cfg->disk_mount) - 1);

            } else if (strcmp(longopt, "cpu-delay") == 0) {
                const char *v = get_optval(eq_val, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] --cpu-delay requires a value\n"); return -1; }
                int cv = parse_int(v);
                if (cv < 1) { fprintf(stderr, "[sysmon] --cpu-delay must be >= 1 µs\n"); return -1; }
                cfg->cpu_delay_us = cv;

            } else if (strcmp(longopt, "verbose") == 0) {
                cfg->verbose = 1;

            } else if (strcmp(longopt, "one-shot") == 0) {
                cfg->one_shot = 1;

            } else {
                fprintf(stderr, "[sysmon] Unknown option: --%s\n", longopt);
                fprintf(stderr, "Run '%s --help' for a list of options.\n", argv[0]);
                return -1;
            }

        } else if (is_short) {
            char flag = arg[1];

            switch (flag) {
            case 'h':
                print_help(argv[0]);
                return 1;

            case 'v':
                print_version();
                return 1;

            case 'V':
                cfg->verbose = 1;
                break;

            case 'i': {
                const char *v = get_optval(NULL, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] -i requires a value\n"); return -1; }
                int iv = parse_int(v);
                if (iv < 1) { fprintf(stderr, "[sysmon] -i must be >= 1 second\n"); return -1; }
                cfg->interval = iv;
                break;
            }

            case 'd':
                cfg->daemon_mode = 1;
                break;

            case 'l': {
                const char *v = get_optval(NULL, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] -l requires a value\n"); return -1; }
                strncpy(cfg->log_dir, v, sizeof(cfg->log_dir) - 1);
                break;
            }

            case 'f': {
                const char *v = get_optval(NULL, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] -f requires a value\n"); return -1; }
                strncpy(cfg->log_file, v, sizeof(cfg->log_file) - 1);
                break;
            }

            case 'm': {
                const char *v = get_optval(NULL, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] -m requires a value\n"); return -1; }
                int mv = parse_int(v);
                if (mv < 1) { fprintf(stderr, "[sysmon] -m must be >= 1 MB\n"); return -1; }
                cfg->max_size_mb = mv;
                break;
            }

            case 'o': {
                const char *v = get_optval(NULL, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] -o requires a value\n"); return -1; }
                strncpy(cfg->log_output, v, sizeof(cfg->log_output) - 1);
                break;
            }

            case 'n': {
                const char *v = get_optval(NULL, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] -n requires a value\n"); return -1; }
                strncpy(cfg->net_iface, v, sizeof(cfg->net_iface) - 1);
                break;
            }

            case 'q':
                cfg->quiet = 1;
                break;

            case 'c': {
                const char *v = get_optval(NULL, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] -c requires a value\n"); return -1; }
                strncpy(cfg->config_file, v, sizeof(cfg->config_file) - 1);
                break;
            }

            case 'p': {
                const char *v = get_optval(NULL, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] -p requires a value\n"); return -1; }
                strncpy(cfg->pid_file, v, sizeof(cfg->pid_file) - 1);
                break;
            }

            case 't': {
                const char *v = get_optval(NULL, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] -t requires a value\n"); return -1; }
                int tv = parse_int(v);
                if (tv < 1) { fprintf(stderr, "[sysmon] -t must be >= 1\n"); return -1; }
                cfg->top_n = tv;
                break;
            }

            case '1':
                cfg->one_shot = 1;
                break;

            default:
                fprintf(stderr, "[sysmon] Unknown flag: -%c\n", flag);
                fprintf(stderr, "Run '%s --help' for a list of options.\n", argv[0]);
                return -1;
            }

        } else {
            fprintf(stderr, "[sysmon] Unknown argument: %s\n", arg);
            fprintf(stderr, "Run '%s --help' for a list of options.\n", argv[0]);
            return -1;
        }
    }

    return 0;
}
