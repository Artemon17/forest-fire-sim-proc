#ifndef TERM_H
#define TERM_H

#include <stddef.h>

/* ANSI-последовательности */
#define ESC_CLEAR   "\033[2J"
#define ESC_HOME    "\033[H"
#define ESC_HIDE    "\033[?25l"
#define ESC_SHOW    "\033[?25h"
#define ESC_RESET   "\033[0m"

/* Цвета (256-цветный режим) */
#define C_GRASS     "\033[38;5;28m"
#define C_FOREST    "\033[38;5;34m"
#define C_FIRE      "\033[38;5;196m"
#define C_BURNT     "\033[38;5;240m"
#define C_WATER     "\033[38;5;27m"
#define C_FIREBRK   "\033[38;5;226m"
#define C_SENSOR    "\033[38;5;51m"
#define C_STATION   "\033[38;5;201m"
#define C_TEAM      "\033[38;5;226m"
#define C_RESET     "\033[0m"

/* Размер терминала. Возвращает 0 при успехе, -1 если не терминал. */
int  term_size(int *rows, int *cols);

/* Управление экраном и курсором */
void term_clear(void);
void term_home(void);
void term_goto(int row, int col);        /* 1-индексация */
void term_hide_cursor(void);
void term_show_cursor(void);
void term_reset_color(void);

/* Сырой режим терминала (без эха, посимвольный ввод) */
int  term_raw_mode(void);                /* 0 при успехе, -1 при ошибке */
int  term_restore_mode(void);

/* Неблокирующее чтение клавиши. -1 если ничего нет. */
int  term_poll_key(void);

/* Задержка в миллисекундах */
void term_sleep_ms(long ms);

/* Обработка Ctrl+C. Флаг взводится в 1 при SIGINT/SIGTERM. */
void term_install_signals(void);
int  term_should_stop(void);
void term_request_stop(void);

#endif