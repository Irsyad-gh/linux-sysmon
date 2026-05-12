/**
 * main.c — Linux System Monitor
 * Entry point: main loop, signal handler, tampilan konsol.
 *
 * Standar: C11/C17
 * Kompilasi: gcc -std=c11 -Wall -Wextra -O2 -o sysmon main.c sysmon.c
 */

#include "sysmon.h"
#include <signal.h>

/* ── Signal Handler ─────────────────────────────────────────── */

/** Flag volatile agar aman diakses dari signal handler */
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
    printf("  ║  Rotasi : setiap %-5d MB                        ║\n",
           (int)(LOG_MAX_BYTES / (1024 * 1024)));
    printf("  ║  Interval: setiap %-4d detik                     ║\n", SAMPLE_INTERVAL);
    printf("  ╠══════════════════════════════════════════════════╣\n");
    printf("  ║  Tekan Ctrl+C untuk menghentikan monitor         ║\n");
    printf("  ╚══════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("  %-22s  %6s  %12s  %10s  %16s  %s\n",
           "WAKTU", "CPU%", "RAM(used/total)", "DISK free",
           "NET rx/tx KB/s", "UPTIME");
    printf("  %s\n",
           "─────────────────────────────────────────────────────"
           "──────────────────────────────────────────");
}

/**
 * Mencetak satu baris ringkasan metrik ke stdout.
 * Format: iterasi | waktu | CPU | RAM | disk | net | uptime
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

/* ── Sleep dengan interupsi sinyal ─────────────────────────── */

/**
 * Sleep selama 'seconds' detik, tapi keluar lebih awal
 * jika sinyal (Ctrl+C) diterima.
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
    /* Setup signal handler untuk Ctrl+C dan SIGTERM */
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    /* ── Bangun path log ──────────────────────────────────── */
    const char *home = getenv("HOME");
    if (!home) {
        fprintf(stderr, "[sysmon] ERROR: Variabel $HOME tidak ditemukan.\n");
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

    /* ── Pastikan direktori log ada ───────────────────────── */
    if (ensure_log_dir(log_dir) != 0) {
        fprintf(stderr,
                "[sysmon] ERROR: Tidak bisa membuat direktori %s: %s\n",
                log_dir, strerror(errno));
        return EXIT_FAILURE;
    }

    print_banner(log_path);

    /* ── Warm-up: sampling CPU pertama dibuang ─────────────
     *   Ini memastikan delta CPU iterasi pertama akurat.
     *   (fungsi internal sudah blocking 200ms)
     */
    calculate_cpu_usage();

    /* ── Main Loop ────────────────────────────────────────── */
    int iteration = 0;

    while (g_running) {

        /* 1. Cek & rotasi log jika diperlukan */
        rotate_log_if_needed(log_path, backup_path);

        /* 2. Buka file log (append mode) */
        FILE *fp = open_log_file(log_path);
        if (!fp) {
            /* Jika gagal buka, tunggu dan coba lagi */
            fprintf(stderr,
                    "[sysmon] WARNING: Menunggu akses log...\n");
            interruptible_sleep(SAMPLE_INTERVAL);
            continue;
        }

        /* 3. Kumpulkan semua metrik (blocking ~200ms untuk CPU) */
        SystemMetrics m;
        if (collect_metrics(&m) == 0) {

            /* 4. Tulis ke file log */
            if (write_metrics(fp, &m) != 0) {
                fprintf(stderr,
                        "[sysmon] WARNING: Gagal menulis ke log.\n");
            }
            fflush(fp);

            /* 5. Tampilkan ringkasan di konsol */
            print_status_line(++iteration, &m);
        }

        fclose(fp);

        /* 6. Tunggu interval berikutnya (dengan interupsi sinyal) */
        interruptible_sleep(SAMPLE_INTERVAL);
    }

    /* ── Cleanup & Exit ───────────────────────────────────── */
    printf("\n");
    printf("  ╔══════════════════════════════════════════════════╗\n");
    printf("  ║  Monitor dihentikan. Total sampel: %-5d          ║\n", iteration);
    printf("  ║  Log tersimpan di:                               ║\n");
    printf("  ║    %-44s  ║\n", log_path);
    printf("  ╚══════════════════════════════════════════════════╝\n\n");

    return EXIT_SUCCESS;
}
