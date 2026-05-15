/**
 * main.c — Linux System Monitor
 * Entry point: CLI parsing, main loop, daemon support, console display.
 *
 * Standard: C11/C17
 * Compilation: gcc -std=c11 -Wall -Wextra -O2 -o sysmon main.c sysmon.c args.c
 */

#include "sysmon.h"
#include "args.h"
#include <signal.h>

/* ── Signal Handler ─────────────────────────────────────────── */

static volatile sig_atomic_t g_running = 1;

static void handle_signal(int sig)
{
    (void)sig;
    g_running = 0;
}

/* ── Banner & Status Display ────────────────────────────────── */

static void print_banner(const SysmonConfig *cfg, const char *log_path)
{
    printf("\n");
    printf(" ╔══════════════════════════════════════════════════════╗\n");
    printf(" ║     Linux System Monitor v%-6s                   ║\n", SYSMON_VERSION);
    printf(" ╠══════════════════════════════════════════════════════╣\n");

    if (cfg->no_log) {
        printf(" ║ Log     : [disabled]                                 ║\n");
    } else {
        printf(" ║ Log     : %-42s ║\n", log_path);
        printf(" ║ Rotate  : every %-3d MB                               ║\n",
               cfg->max_size_mb);
    }

    printf(" ║ Interval: every %-3d seconds                          ║\n",
           cfg->interval);
    printf(" ║ Mount   : %-42s ║\n", cfg->disk_mount);

    if (cfg->net_iface[0] != '\0') {
        printf(" ║ Iface   : %-42s ║\n", cfg->net_iface);
    }

    if (cfg->top_n > 0) {
        printf(" ║ Top     : %d processes                                ║\n",
               cfg->top_n);
    }

    if (cfg->one_shot) {
        printf(" ║ Mode    : one-shot                                   ║\n");
    } else if (cfg->daemon_mode) {
        printf(" ║ Mode    : daemon                                     ║\n");
    }

    printf(" ╠══════════════════════════════════════════════════════╣\n");
    printf(" ║  Press Ctrl+C to stop the monitor                   ║\n");
    printf(" ╚══════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf(" %-22s %6s %12s %10s %16s %s\n",
           "TIME", "CPU%", "RAM(used/total)", "DISK free",
           "NET rx/tx KB/s", "UPTIME");
    printf(" %s\n",
           "──────────────────────────────────────────────────────"
           "───────────────────────────────────────────");
}

static void print_status_line(int iter, const SystemMetrics *m)
{
    printf(" [%04d] %-20s %5.1f%% %5ld/%-5ld MB %6ld MB "
           "%7.1f/%-7.1f %s\n",
           iter,
           m->timestamp,
           m->cpu_usage,
           m->mem.used_mb, m->mem.total_mb,
           m->disk.free_mb,
           m->net.rx_kbps, m->net.tx_kbps,
           m->uptime_str);
    fflush(stdout);
}

static void print_top_processes(const SystemMetrics *m)
{
    if (m->top_n <= 0 || !m->top_procs) return;

    printf("\n   %-6s %-20s %8s %8s\n", "PID", "NAME", "CPU%", "RAM(MB)");
    printf("   %s\n", "──────────────────────────────────────────");
    for (int i = 0; i < m->top_n; i++) {
        printf("   %-6d %-20s %7.1f%% %7ld\n",
               m->top_procs[i].pid,
               m->top_procs[i].name,
               m->top_procs[i].cpu_pct,
               m->top_procs[i].rss_mb);
    }
    printf("\n");
}

static void print_verbose(const SystemMetrics *m)
{
    printf("   [VERBOSE] CPU delay captured | "
           "iowait included in idle | "
           "disk: %ld/%ld MB\n",
           m->disk.used_mb, m->disk.total_mb);
}

/* ── Sleep with signal interruption ─────────────────────────── */

static void interruptible_sleep(int seconds)
{
    for (int i = 0; i < seconds && g_running; i++) {
        sleep(1);
    }
}

/* ── Build log paths dari config ─────────────────────────────── */

static int build_log_paths(const SysmonConfig *cfg,
                            char *log_dir,   size_t log_dir_size,
                            char *log_path,  size_t log_path_size,
                            char *bak_path,  size_t bak_path_size)
{
    /* Jika --output diberikan, override semua */
    if (cfg->log_output[0] != '\0') {
        strncpy(log_path, cfg->log_output, log_path_size - 1);
        /* Buat backup: tambahkan .1 sebelum ekstensi */
        snprintf(bak_path, bak_path_size, "%s.1", cfg->log_output);
        /* log_dir = direktori dari output path */
        strncpy(log_dir, cfg->log_output, log_dir_size - 1);
        char *slash = strrchr(log_dir, '/');
        if (slash) {
            *slash = '\0';
        } else {
            strncpy(log_dir, ".", log_dir_size - 1);
        }
        return 0;
    }

    /* Bangun dari logdir + logfile */
    const char *base_dir;
    char home_dir[PATH_MAX] = {0};

    if (cfg->log_dir[0] != '\0') {
        base_dir = cfg->log_dir;
    } else {
        const char *home = getenv("HOME");
        if (!home) {
            fprintf(stderr, "[sysmon] ERROR: $HOME tidak ditemukan.\n");
            return -1;
        }
        snprintf(home_dir, sizeof(home_dir), "%s/%s", home, DEFAULT_LOG_DIR);
        base_dir = home_dir;
    }

    strncpy(log_dir, base_dir, log_dir_size - 1);
    snprintf(log_path, log_path_size, "%s/%s", base_dir, cfg->log_file);

    /* Buat nama backup: ganti ekstensi atau tambah .1 */
    const char *dot = strrchr(cfg->log_file, '.');
    if (dot) {
        /* file.json → file.1.json */
        size_t stem_len = (size_t)(dot - cfg->log_file);
        char stem[PATH_MAX] = {0};
        if (stem_len >= sizeof(stem)) stem_len = sizeof(stem) - 1;
        strncpy(stem, cfg->log_file, stem_len);
        snprintf(bak_path, bak_path_size, "%s/%s.1%s", base_dir, stem, dot);
    } else {
        snprintf(bak_path, bak_path_size, "%s/%s.1", base_dir, cfg->log_file);
    }

    return 0;
}

/* ── Main ───────────────────────────────────────────────────── */

int main(int argc, char *argv[])
{
    /* ── Parse CLI arguments ──────────────────────────────── */
    SysmonConfig cfg;
    config_init(&cfg);

    int parse_ret = parse_args(argc, argv, &cfg);
    if (parse_ret == 1) return EXIT_SUCCESS;  /* --help or --version */
    if (parse_ret < 0)  return EXIT_FAILURE;

    /* Load config file jika ada */
    if (cfg.config_file[0] != '\0') {
        if (load_config_file(&cfg) != 0) {
            return EXIT_FAILURE;
        }
    }

    /* ── Setup signal handler ─────────────────────────────── */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* ── Set net interface jika diberikan ────────────────── */
    if (cfg.net_iface[0] != '\0') {
        set_net_iface(cfg.net_iface);
    }

    /* ── Build log paths ──────────────────────────────────── */
    char log_dir[PATH_MAX  + 64] = {0};
    char log_path[PATH_MAX + 64] = {0};
    char bak_path[PATH_MAX + 64] = {0};

    if (!cfg.no_log) {
        if (build_log_paths(&cfg,
                            log_dir,  sizeof(log_dir),
                            log_path, sizeof(log_path),
                            bak_path, sizeof(bak_path)) != 0) {
            return EXIT_FAILURE;
        }

        /* Pastikan direktori log ada */
        if (ensure_log_dir(log_dir) != 0) {
            fprintf(stderr, "[sysmon] ERROR: Cannot create log dir %s: %s\n",
                    log_dir, strerror(errno));
            return EXIT_FAILURE;
        }

        if (cfg.verbose) {
            fprintf(stderr, "[sysmon] Log path  : %s\n", log_path);
            fprintf(stderr, "[sysmon] Backup    : %s\n", bak_path);
            fprintf(stderr, "[sysmon] Max size  : %d MB\n", cfg.max_size_mb);
        }
    }

    /* ── Daemonize jika diminta ───────────────────────────── */
    if (cfg.daemon_mode) {
        if (daemonize(cfg.pid_file, cfg.verbose) != 0) {
            fprintf(stderr, "[sysmon] ERROR: Gagal masuk mode daemon\n");
            return EXIT_FAILURE;
        }
        /* Setelah fork, hanya child yang lanjut ke sini */
        /* stdout/stderr sudah di-redirect ke /dev/null */
    }

    /* ── Banner (hanya jika tidak quiet dan tidak daemon) ── */
    if (!cfg.quiet && !cfg.daemon_mode) {
        print_banner(&cfg, log_path);
    }

    /* ── Warm-up: buang sample CPU pertama ───────────────── */
    calculate_cpu_usage_delay(cfg.cpu_delay_us);

    /* ── Hitung max_bytes dari config ────────────────────── */
    long max_bytes = (long)cfg.max_size_mb * 1024L * 1024L;

    /* ── Main Loop ────────────────────────────────────────── */
    int iteration = 0;

    do {
        /* 1. Cek & rotasi log jika perlu */
        if (!cfg.no_log) {
            rotate_log_if_needed(log_path, bak_path, max_bytes);
        }

        /* 2. Buka file log jika perlu */
        FILE *fp = NULL;
        if (!cfg.no_log) {
            fp = open_log_file(log_path);
            if (!fp) {
                fprintf(stderr, "[sysmon] WARNING: Waiting for log access...\n");
                interruptible_sleep(cfg.interval);
                continue;
            }
        }

        /* 3. Kumpulkan semua metrics */
        SystemMetrics m;
        if (collect_metrics(&m, cfg.disk_mount, cfg.cpu_delay_us, cfg.top_n) == 0) {

            /* 4. Tulis ke file log */
            if (fp) {
                if (write_metrics(fp, &m) != 0) {
                    fprintf(stderr, "[sysmon] WARNING: Failed to write log.\n");
                }
                fflush(fp);
            }

            /* 5. Tampilkan di console jika tidak quiet */
            if (!cfg.quiet) {
                print_status_line(++iteration, &m);

                if (cfg.top_n > 0) {
                    print_top_processes(&m);
                }

                if (cfg.verbose) {
                    print_verbose(&m);
                }
            } else {
                iteration++;
            }
        }

        /* 6. Bebaskan memori top_procs */
        free_metrics(&m);

        if (fp) fclose(fp);

        /* 7. One-shot: keluar setelah satu iterasi */
        if (cfg.one_shot) break;

        /* 8. Tidur sampai interval berikutnya */
        interruptible_sleep(cfg.interval);

    } while (g_running);

    /* ── Cleanup & Exit ───────────────────────────────────── */
    if (!cfg.quiet && !cfg.daemon_mode) {
        printf("\n");
        printf(" ╔══════════════════════════════════════════════════════╗\n");
        printf(" ║ Monitor stopped. Total samples: %-5d               ║\n", iteration);
        if (!cfg.no_log) {
            printf(" ║ Log saved at:                                        ║\n");
            printf(" ║  %-52s ║\n", log_path);
        }
        printf(" ╚══════════════════════════════════════════════════════╝\n\n");
    }

    return EXIT_SUCCESS;
}
