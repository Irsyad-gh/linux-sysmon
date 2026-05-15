/**
 * args.c — CLI Argument Parser Implementation
 *
 * Mengimplementasikan parsing semua CLI arguments untuk sysmon.
 * Mendukung format: -x, --long, --long=value, --long value
 *
 * Standard: C11/C17
 */

#include "args.h"
#include "sysmon.h"   /* untuk SYSMON_VERSION */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

/* ══════════════════════════════════════════════════════════════
 * INTERNAL HELPERS
 * ══════════════════════════════════════════════════════════════ */

/** Parse integer dengan validasi. Return -1 jika invalid. */
static int parse_int(const char *s)
{
    if (!s || *s == '\0') return -1;
    char *end;
    long v = strtol(s, &end, 10);
    if (*end != '\0' || v <= 0 || v > INT_MAX) return -1;
    return (int)v;
}

/** Trim whitespace dari string (in-place). */
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
    printf("  -h, --help                  Tampilkan pesan ini lalu keluar\n");
    printf("  -v, --version               Tampilkan versi lalu keluar\n");
    printf("  -V, --verbose               Tampilkan lebih banyak info/debug\n");
    printf("  -c, --config <file>         Load konfigurasi dari file JSON/INI\n\n");

    printf("TIMING:\n");
    printf("  -i, --interval <detik>      Sampling interval (default: %d detik)\n",
           DEFAULT_INTERVAL);
    printf("      --cpu-delay <us>        Delay CPU sampling dalam mikrodetik\n");
    printf("                              (default: %d µs)\n", DEFAULT_CPU_DELAY);
    printf("  -1, --one-shot              Ambil satu kali metrics lalu keluar\n");
    printf("                              (cocok untuk cron/script)\n\n");

    printf("LOGGING:\n");
    printf("  -l, --logdir <dir>          Directory log (default: ~/.status)\n");
    printf("  -f, --logfile <nama>        Nama file log (default: status.json)\n");
    printf("  -o, --output <path>         Output log ke path tertentu\n");
    printf("                              (override --logdir + --logfile)\n");
    printf("  -m, --maxsize <MB>          Ukuran maks sebelum rotate (default: %d MB)\n",
           DEFAULT_MAX_MB);
    printf("      --no-log                Disable file logging (hanya console)\n\n");

    printf("DAEMON:\n");
    printf("  -d, --daemon                Jalankan sebagai daemon (background)\n");
    printf("  -p, --pidfile <file>        Path PID file (untuk daemon)\n\n");

    printf("NETWORK:\n");
    printf("  -n, --net-interface <iface> Pilih interface tertentu (misal: eth0, wlan0)\n\n");

    printf("DISPLAY:\n");
    printf("  -q, --quiet                 Mode tanpa tampilan console (hanya logging)\n");
    printf("  -t, --top-processes <N>     Tampilkan top N processes (CPU/RAM)\n\n");

    printf("DISK:\n");
    printf("      --disk-mount <path>     Monitor disk pada mount point tertentu\n");
    printf("                              (default: /)\n\n");

    printf("EXAMPLES:\n");
    printf("  %s                          Jalankan dengan semua default\n", progname);
    printf("  %s -i 10 -q -d             Daemon mode, interval 10 detik, tanpa console\n", progname);
    printf("  %s -1 -o /tmp/snap.json    Satu kali snapshot ke file tertentu\n", progname);
    printf("  %s -n wlan0 -t 5 -V        Monitor wlan0, top 5 proses, verbose\n", progname);
    printf("  %s -c /etc/sysmon.ini      Load konfigurasi dari file\n\n", progname);
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

    /* log_dir akan diisi dari $HOME saat runtime jika tidak diset */
    cfg->log_dir[0]     = '\0';
    cfg->log_output[0]  = '\0';
    cfg->pid_file[0]    = '\0';
    cfg->net_iface[0]   = '\0';
    cfg->config_file[0] = '\0';
}

/* ══════════════════════════════════════════════════════════════
 * CONFIG FILE LOADER (JSON/INI sederhana)
 * ══════════════════════════════════════════════════════════════ */

/**
 * Parse satu baris INI: key = value
 * JSON: "key": value  (subset sederhana, string saja)
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
 * Strip karakter dari string: hapus char c dari awal/akhir.
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

    /* Deteksi format: jika baris pertama non-comment mengandung '{', anggap JSON */
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

        /* Skip komentar, baris kosong, dan karakter JSON structural */
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

            /* Hapus tanda kutip dan koma trailing */
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

            /* Hapus inline comment */
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

        /* Cek apakah long option */
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
                if (!v) { fprintf(stderr, "[sysmon] --interval membutuhkan nilai\n"); return -1; }
                int iv = parse_int(v);
                if (iv < 1) { fprintf(stderr, "[sysmon] --interval harus >= 1 detik\n"); return -1; }
                cfg->interval = iv;

            } else if (strcmp(longopt, "daemon") == 0) {
                cfg->daemon_mode = 1;

            } else if (strcmp(longopt, "logdir") == 0) {
                const char *v = get_optval(eq_val, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] --logdir membutuhkan nilai\n"); return -1; }
                strncpy(cfg->log_dir, v, sizeof(cfg->log_dir) - 1);

            } else if (strcmp(longopt, "logfile") == 0) {
                const char *v = get_optval(eq_val, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] --logfile membutuhkan nilai\n"); return -1; }
                strncpy(cfg->log_file, v, sizeof(cfg->log_file) - 1);

            } else if (strcmp(longopt, "maxsize") == 0) {
                const char *v = get_optval(eq_val, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] --maxsize membutuhkan nilai\n"); return -1; }
                int mv = parse_int(v);
                if (mv < 1) { fprintf(stderr, "[sysmon] --maxsize harus >= 1 MB\n"); return -1; }
                cfg->max_size_mb = mv;

            } else if (strcmp(longopt, "output") == 0) {
                const char *v = get_optval(eq_val, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] --output membutuhkan nilai\n"); return -1; }
                strncpy(cfg->log_output, v, sizeof(cfg->log_output) - 1);

            } else if (strcmp(longopt, "net-interface") == 0) {
                const char *v = get_optval(eq_val, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] --net-interface membutuhkan nilai\n"); return -1; }
                strncpy(cfg->net_iface, v, sizeof(cfg->net_iface) - 1);

            } else if (strcmp(longopt, "quiet") == 0) {
                cfg->quiet = 1;

            } else if (strcmp(longopt, "config") == 0) {
                const char *v = get_optval(eq_val, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] --config membutuhkan nilai\n"); return -1; }
                strncpy(cfg->config_file, v, sizeof(cfg->config_file) - 1);

            } else if (strcmp(longopt, "pidfile") == 0) {
                const char *v = get_optval(eq_val, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] --pidfile membutuhkan nilai\n"); return -1; }
                strncpy(cfg->pid_file, v, sizeof(cfg->pid_file) - 1);

            } else if (strcmp(longopt, "no-log") == 0) {
                cfg->no_log = 1;

            } else if (strcmp(longopt, "top-processes") == 0) {
                const char *v = get_optval(eq_val, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] --top-processes membutuhkan nilai\n"); return -1; }
                int tv = parse_int(v);
                if (tv < 1) { fprintf(stderr, "[sysmon] --top-processes harus >= 1\n"); return -1; }
                cfg->top_n = tv;

            } else if (strcmp(longopt, "disk-mount") == 0) {
                const char *v = get_optval(eq_val, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] --disk-mount membutuhkan nilai\n"); return -1; }
                strncpy(cfg->disk_mount, v, sizeof(cfg->disk_mount) - 1);

            } else if (strcmp(longopt, "cpu-delay") == 0) {
                const char *v = get_optval(eq_val, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] --cpu-delay membutuhkan nilai\n"); return -1; }
                int cv = parse_int(v);
                if (cv < 1) { fprintf(stderr, "[sysmon] --cpu-delay harus >= 1 µs\n"); return -1; }
                cfg->cpu_delay_us = cv;

            } else if (strcmp(longopt, "verbose") == 0) {
                cfg->verbose = 1;

            } else if (strcmp(longopt, "one-shot") == 0) {
                cfg->one_shot = 1;

            } else {
                fprintf(stderr, "[sysmon] Unknown option: --%s\n", longopt);
                fprintf(stderr, "Run '%s --help' untuk daftar opsi.\n", argv[0]);
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
                if (!v) { fprintf(stderr, "[sysmon] -i membutuhkan nilai\n"); return -1; }
                int iv = parse_int(v);
                if (iv < 1) { fprintf(stderr, "[sysmon] -i harus >= 1 detik\n"); return -1; }
                cfg->interval = iv;
                break;
            }

            case 'd':
                cfg->daemon_mode = 1;
                break;

            case 'l': {
                const char *v = get_optval(NULL, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] -l membutuhkan nilai\n"); return -1; }
                strncpy(cfg->log_dir, v, sizeof(cfg->log_dir) - 1);
                break;
            }

            case 'f': {
                const char *v = get_optval(NULL, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] -f membutuhkan nilai\n"); return -1; }
                strncpy(cfg->log_file, v, sizeof(cfg->log_file) - 1);
                break;
            }

            case 'm': {
                const char *v = get_optval(NULL, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] -m membutuhkan nilai\n"); return -1; }
                int mv = parse_int(v);
                if (mv < 1) { fprintf(stderr, "[sysmon] -m harus >= 1 MB\n"); return -1; }
                cfg->max_size_mb = mv;
                break;
            }

            case 'o': {
                const char *v = get_optval(NULL, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] -o membutuhkan nilai\n"); return -1; }
                strncpy(cfg->log_output, v, sizeof(cfg->log_output) - 1);
                break;
            }

            case 'n': {
                const char *v = get_optval(NULL, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] -n membutuhkan nilai\n"); return -1; }
                strncpy(cfg->net_iface, v, sizeof(cfg->net_iface) - 1);
                break;
            }

            case 'q':
                cfg->quiet = 1;
                break;

            case 'c': {
                const char *v = get_optval(NULL, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] -c membutuhkan nilai\n"); return -1; }
                strncpy(cfg->config_file, v, sizeof(cfg->config_file) - 1);
                break;
            }

            case 'p': {
                const char *v = get_optval(NULL, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] -p membutuhkan nilai\n"); return -1; }
                strncpy(cfg->pid_file, v, sizeof(cfg->pid_file) - 1);
                break;
            }

            case 't': {
                const char *v = get_optval(NULL, &i, argc, argv);
                if (!v) { fprintf(stderr, "[sysmon] -t membutuhkan nilai\n"); return -1; }
                int tv = parse_int(v);
                if (tv < 1) { fprintf(stderr, "[sysmon] -t harus >= 1\n"); return -1; }
                cfg->top_n = tv;
                break;
            }

            case '1':
                cfg->one_shot = 1;
                break;

            default:
                fprintf(stderr, "[sysmon] Unknown flag: -%c\n", flag);
                fprintf(stderr, "Run '%s --help' untuk daftar opsi.\n", argv[0]);
                return -1;
            }

        } else {
            fprintf(stderr, "[sysmon] Unknown argument: %s\n", arg);
            fprintf(stderr, "Run '%s --help' untuk daftar opsi.\n", argv[0]);
            return -1;
        }
    }

    return 0;
}
