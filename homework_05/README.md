# ДЗ 5: telemetry analyzer

Аналізатор телеметрії робота. Програма читає лог із пробіло-розділеними кадрами,
валідує кожен кадр і друкує підсумок: кількість кадрів, мін/макс напруги, середню
температуру, кількість кадрів із низькою напругою та частоту кадрів.

## Структура та CMake

```
homework_05/
  CMakeLists.txt        # бібліотека telemetry + виконуваний telemetry_check
  include/telemetry.hpp
  src/telemetry.cpp     # логіка читання, парсингу, валідації, підсумку
  src/main.cpp          # обгортка: argv[1] -> read_frames -> summarize -> print
  data/                 # один валідний і чотири проблемні вхідні файли
```

CMake targets:

- `telemetry` — бібліотека з логікою; `include/` підключено через
  `target_include_directories(telemetry PUBLIC include)`;
- `telemetry_check` — виконуваний файл, читає шлях із `argv[1]`;
- `telemetry_check` залежить від `telemetry` через `target_link_libraries`.

`homework_05` підключається у кореневому `CMakeLists.txt` через
`add_subdirectory(homework_05)`.

## Збірка і запуск

```sh
cmake --preset debug
cmake --build --preset debug

./build/debug/homework_05/telemetry_check homework_05/data/good.txt
```

Окрім `debug`, у кореневих presets додано `release` та `relwithdebinfo`
(configure + build presets). У `.vscode/launch.json` є конфігурація
`Debug homework_05: telemetry_check`, що запускає GDB на `data/good.txt`.

Очікуваний вивід для `good.txt`:

```
frames_total 4
frames_valid 4
voltage_min 24.5
voltage_max 24.8
temperature_avg 41.6
low_voltage_frames 0
frame_rate_hz 10
```

## Формат кадру

Кожен рядок — рівно 7 полів:

```
timestamp_ms  seq  voltage_v  current_a  temperature_c  gps_fix  satellites
```

Правила валідації (некоректний кадр пропускається з повідомленням у `stderr`,
обробка триває далі):

- рівно 7 полів;
- кожне поле парситься у число;
- `seq >= 1`, перший кадр має `seq == 1`;
- `timestamp_ms` строго зростає від кадру до кадру;
- `voltage_v > 0`;
- `temperature_c` у діапазоні `[-40, 120]`;
- `gps_fix` дорівнює 0 або 1;
- `satellites >= 0`.

Формат повідомлення про помилку:

```
error: invalid frame at line 2: expected 7 fields
```

Код завершення — `0`, якщо всі кадри валідні; ненульовий, якщо був хоч один
некоректний кадр або у лозі немає жодного придатного кадру.

## Знайдені дефекти та виправлення

Стартовий `telemetry.cpp` компілювався, але мав чотири runtime-дефекти.

1. **Некоректна форма вводу — `data/bad_missing_field.txt`.**
   `parse_frame` ігнорував результат `split_line` (`(void)field_count;`) і
   читав `fields[0..6]`. На рядку з 6 полями `fields[6]` лишався
   `nullptr`, а `strtol(nullptr, …)` — це невизначена поведінка / падіння.
   *Виправлення:* `parse_and_validate` вимагає рівно `TELEMETRY_FIELD_COUNT`
   полів, інакше друкує `expected 7 fields` і пропускає рядок.

2. **Некоректні числа — `data/bad_invalid_number.txt`.**
   `parse_long`/`parse_double` викликали `std::abort()` на нечисловому значенні,
   завершуючи всю програму на першому ж невалідному рядку.
   *Виправлення:* парсери повертають `bool` і вимагають, щоб `strtol`/`strtod`
   спожили весь токен; кадр пропускається з повідомленням замість `abort()`.

3. **Небезпечна різниця часу — `data/bad_zero_delta.txt`.**
   `compute_frame_rate_hz` ділив на `last.ts - first.ts`. Два кадри з однаковим
   `timestamp` давали дільник `0` → цілочисельне ділення на нуль.
   *Виправлення:* валідатор відхиляє незростаючі `timestamp_ms`, а сам розрахунок
   додатково захищений (повертає 0 при `< 2` кадрах або нульовому інтервалі).
   Заразом частота рахується в `double`, а не цілими.

4. **Порожній лог — `data/empty.txt`.**
   `summarize` читав `frames[0]`, а `compute_frame_rate_hz` — `frames[count-1]`
   ще до перевірки кількості. На порожньому лозі це вихід за межі масиву.
   *Виправлення:* при `frames_valid == 0` усі метрики дорівнюють 0, індексація
   порожнього масиву не виконується.

## Перевірка на всіх вхідних даних

```sh
./build/debug/homework_05/telemetry_check homework_05/data/good.txt
./build/debug/homework_05/telemetry_check homework_05/data/bad_missing_field.txt
./build/debug/homework_05/telemetry_check homework_05/data/bad_invalid_number.txt
./build/debug/homework_05/telemetry_check homework_05/data/bad_zero_delta.txt
./build/debug/homework_05/telemetry_check homework_05/data/empty.txt
```

`good.txt` друкує підсумок і завершується з кодом 0. Жоден із проблемних файлів
не призводить до падіння: некоректні кадри повідомляються у `stderr`, програма
завершується з ненульовим кодом.
