/**
 * main.c — Linux System Monitor
 * Entry point: main loop, signal handler, console display.
 *
 * Standard: C11/C17
 * Compilation: gcc -std=c11 -Wall -Wextra -O2 -o sysmon main.c sysmon.c
 */

#include "sysmon.h"
#include <signal.h>

/* ── Signal Handler ─────────────────────────────────────────── */

/** Volatile flag for safe access from signal handler */
static volatile sig_atomic_t g_running = 1;

static void handle_signal(int sig)
{
    (void)sig; /* suppress unused parameter warning */
    g_running = 0;
}

/* ── Banner & Status Display ────────────────────────────────── */

static void print_banner(const char *log_path)
{
    printf("\n");
    printf("  ╔══════════════════════════════════════════════════╗\n");
    printf("  ║          Linux System Monitor v%-6s           ║\n", SYSMON_VERSION);
    printf("  ╠══════════════════════════════════════════════════╣\n");
    printf("  ║  Log    : %-38s  ║\n", log_path);
    printf("  ║  Rotation: every %-5d MB                        ║\n",
           (int)(LOG_MAX_BYTES / (1024 * 1024)));
    printf("  ║  Interval: every %-4d seconds                    ║\n", SAMPLE_INTERVAL);
    printf("  ╠══════════════════════════════════════════════════╣\n");
    printf("  ║  Press Ctrl+C to stop the monitor                ║\n");
    printf("  ╚══════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("  %-22s  %6s  %12s  %10s  %16s  %s\n",
           "TIME", "CPU%", "RAM(used/total)", "DISK free",
           "NET rx/tx KB/s", "UPTIME");
    printf("  %s\n",
           "─────────────────────────────────────────────────────"
           "──────────────────────────────────────────");
}

/**
 * Prints one summary line of metrics to stdout.
 * Format: iteration | time | CPU | RAM | disk | net | uptime
 */
static void print_status_line(int iter, const SystemMetrics *m)
{
    printf("  [%04d] %-20s  %5.1f%%  %5ld/%-5ld MB  %6ld MB  "
           "%7.1f/%-7.1f  %s\n",
           iter,
           m->timestamp,
           m->cpu_usage,
           m->mem.used_mb, m->mem.total_mb,
           m->disk.free_mb,
           m->net.rx_kbps, m->net.tx_kbps,
           m->uptime_str);
    fflush(stdout);
}

/* ── Sleep with signal interruption ─────────────────────────── */

/**
 * Sleep for 'seconds' seconds, but exit early
 * if signal (Ctrl+C) is received.
 */
static void interruptible_sleep(int seconds)
{
    for (int i = 0; i < seconds && g_running; i++) {
        sleep(1);
    }
}

/* ── Main ───────────────────────────────────────────────────── */

int main(void)
{
    /* Setup signal handler for Ctrl+C and SIGTERM */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* ── Build log path ──────────────────────────────────── */
    const char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "[sysmon] ERROR: $HOME variable not found.\n");
        return EXIT_FAILURE;
    }

    char log_dir[PATH_MAX + 32];
    char log_path[PATH_MAX + 64];
    char backup_path[PATH_MAX + 64];

    snprintf(log_dir,     sizeof(log_dir),
             "%s/%s",      home, LOG_DIR);
    snprintf(log_path,    sizeof(log_path),
             "%s/%s",      log_dir, LOG_FILE);
    snprintf(backup_path, sizeof(backup_path),
             "%s/%s",      log_dir, LOG_BACKUP);

    /* ── Ensure log directory exists ───────────────────────── */
    if (ensure_log_dir(log_dir) != 0) {
        fprintf(stderr,
                "[sysmon] ERROR: Cannot create directory %s: %s\n",
                log_dir, strerror(errno));
        return EXIT_FAILURE;
    }

    print_banner(log_path);

    /* ── Warm-up: discard first CPU sampling ─────────────
     *   This ensures accurate CPU delta for first iteration.
     *   (internal function already blocks 200ms)
     */
    calculate_cpu_usage();

    /* ── Main Loop ────────────────────────────────────────── */
    int iteration = 0;

    while (g_running) {

        /* 1. Check & rotate log if needed */
        rotate_log_if_needed(log_path, backup_path);

        /* 2. Open log file (append mode) */
        FILE *fp = open_log_file(log_path);
        if (!fp) {
            /* If failed to open, wait and try again */
            fprintf(stderr,
                    "[sysmon] WARNING: Waiting for log access...\n");
            interruptible_sleep(SAMPLE_INTERVAL);
            continue;
        }

        /* 3. Collect all metrics (blocking ~200ms for CPU) */
        SystemMetrics m;
        if (collect_metrics(&m) == 0) {

            /* 4. Write to log file */
            if (write_metrics(fp, &m) != 0) {
                fprintf(stderr,
                        "[sysmon] WARNING: Failed to write to log.\n");
            }
            fflush(fp);

            /* 5. Display summary on console */
            print_status_line(++iteration, &m);
        }

        fclose(fp);

        /* 6. Wait for next interval (with signal interruption) */
        interruptible_sleep(SAMPLE_INTERVAL);
    }

    /* ── Cleanup & Exit ───────────────────────────────────── */
    printf("\n");
    printf("  ╔══════════════════════════════════════════════════╗\n");
    printf("  ║  Monitor stopped. Total samples: %-5d           ║\n", iteration);
    printf("  ║  Log saved at:                                   ║\n");
    printf("  ║    %-44s  ║\n", log_path);
    printf("  ╚══════════════════════════════════════════════════╝\n\n");

    return EXIT_SUCCESS;
}
