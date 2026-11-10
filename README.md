# Forest Fire Simulation — Multiprocessing

Многопроцессная версия моделирования обнаружения и тушения лесного пожара.
ИЗ №3 по дисциплине АКиОС.

[**📄 Ссылка на полный отчёт по заданию (REPORT.md)**](./REPORT.md)

## Запуск

    make
    ./build/test_shm          # тест SHM
    # позже:
    ./build/fire-fork-named --map data/map_big_01.map --station 11 13

## Структура

- src/       — исходный код
- include/   — заголовки
- data/      — карты и конфиги
- tests/     — тесты
