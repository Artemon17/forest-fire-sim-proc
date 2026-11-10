#ifndef TERM_H
#define TERM_H

#include <stddef.h>

#define ESC_CLEAR   "\033[2J"
#define ESC_HOME    "\033[H"
#define ESC_HIDE    "\033[?25l"
#define ESC_SHOW    "\033[?25h"
#define ESC_RESET   "\033[0m"

#define ESC_ALT_ON  "\033[?1049h"
#define ESC_ALT_OFF "\033[?1049l"

#define C_GRASS     "\033[38;5;28m"
#define C_FOREST    "\033[38;5;34m"
#define C_FIRE      "\033[38;5;196m"
#define C_BURNT     "\033[38;5;240m"
#define C_WATER     "\033[38;5;27m"
#define C_FIREBRK   "\033[38;5;226m"
#define C_SENSOR    "\033[38;5;51m"
#define C_STATION   "\033[38;5;201m"
#define C_TEAM      "\033[38;5;226m"
#define C_RESET     ESC_RESET

int  term_size(int *rows, int *cols);

void term_clear(void);
void term_home(void);
void term_goto(int row, int col);
void term_hide_cursor(void);
void term_show_cursor(void);
void term_reset_color(void);

void term_alt_screen_enter(void);
void term_alt_screen_leave(void);

int  term_raw_mode(void);
int  term_restore_mode(void);
int  term_poll_key(void);
void term_sleep_ms(long ms);

void term_install_signals(void);
int  term_should_stop(void);
void term_request_stop(void);

#endif