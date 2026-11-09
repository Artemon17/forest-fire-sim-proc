#define _POSIX_C_SOURCE 200809L

#include "term.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <signal.h>
#include <time.h>
#include <sys/ioctl.h>
#include <errno.h>

/* ─── Внутреннее состояние ─────────────────────────────────── */

static struct termios g_saved_tio;
static int          g_raw_active = 0;

static volatile sig_atomic_t g_stop = 0;

/* ─── Размер терминала ─────────────────────────────────────── */

int term_size(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_row == 0)
        return -1;
    *rows = ws.ws_row;
    *cols = ws.ws_col;
    return 0;
}

/* ─── Экран и курсор ───────────────────────────────────────── */

static void write_str(const char *s) {
    if (!s) return;
    size_t n = strlen(s);
    ssize_t off = 0;
    while (off < (ssize_t)n) {
        ssize_t w = write(STDOUT_FILENO, s + off, n - off);
        if (w < 0) {
            if (errno == EINTR) continue;
            return;
        }
        off += w;
    }
}

void term_clear(void)      { write_str(ESC_CLEAR ESC_HOME); }
void term_home(void)       { write_str(ESC_HOME); }
void term_hide_cursor(void){ write_str(ESC_HIDE); }
void term_show_cursor(void){ write_str(ESC_SHOW); }
void term_reset_color(void){ write_str(ESC_RESET); }

void term_goto(int row, int col) {
    char buf[32];
    int n = snprintf(buf, sizeof(buf), "\033[%d;%dH", row, col);
    if (n > 0) {
        ssize_t w = write(STDOUT_FILENO, buf, (size_t)n);
        (void)w;
    }
}

/* ─── Сырой режим ──────────────────────────────────────────── */

int term_raw_mode(void) {
    if (g_raw_active) return 0;
    if (tcgetattr(STDIN_FILENO, &g_saved_tio) == -1) return -1;

    struct termios t = g_saved_tio;
    t.c_lflag &= (tcflag_t)~(ICANON | ECHO);
    t.c_cc[VMIN]  = 0;
    t.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &t) == -1) return -1;
    g_raw_active = 1;
    return 0;
}

int term_restore_mode(void) {
    if (!g_raw_active) return 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &g_saved_tio) == -1) return -1;
    g_raw_active = 0;
    return 0;
}

/* ─── Клавиши ──────────────────────────────────────────────── */

int term_poll_key(void) {
    unsigned char c;
    ssize_t n = read(STDIN_FILENO, &c, 1);
    if (n == 1) return (int)c;
    return -1;
}

/* ─── Задержка ─────────────────────────────────────────────── */

void term_sleep_ms(long ms) {
    if (ms <= 0) return;
    struct timespec ts;
    ts.tv_sec  = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) {
        if (g_stop) return;
    }
}

/* ─── Сигналы ──────────────────────────────────────────────── */

static void on_signal(int sig) {
    (void)sig;
    g_stop = 1;
}

void term_install_signals(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;   /* без SA_RESTART — пусть read/nanosleep прерываются */

    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);
}

int  term_should_stop(void)   { return g_stop != 0; }
void term_request_stop(void)  { g_stop = 1; }