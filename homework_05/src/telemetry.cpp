#include "telemetry.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>

namespace {

const int MAX_LINE_LENGTH = 256;

// Температурний діапазон для платформи.
const double MIN_TEMPERATURE_C = -40.0;
const double MAX_TEMPERATURE_C = 120.0;

bool is_separator(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

bool is_blank(const char* line) {
    for (const char* c = line; *c != '\0'; ++c) {
        if (!is_separator(*c)) {
            return false;
        }
    }
    return true;
}


// Розбиває `line` на місці на токени. Записує до `max_fields` покажчиків токенів
// у `fields` та повертає загальну кількість знайдених токенів.
int split_line(char* line, char* fields[], int max_fields) {
    int count = 0;
    char* cursor = line;

    while (*cursor != '\0') {
        while (is_separator(*cursor)) {
            ++cursor;
        }
        if (*cursor == '\0') {
            break;
        }

        if (count < max_fields) {
            fields[count] = cursor;
        }
        ++count;

        while (*cursor != '\0' && !is_separator(*cursor)) {
            ++cursor;
        }
        if (*cursor != '\0') {
            *cursor = '\0';
            ++cursor;
        }
    }

    return count;
}

// Дефект №2 (недійсні числові значення): тут викликано std::abort(),
// що завершувало всю програму на першому ж поганому значенні.
bool parse_long(const char* text, long& out) {
    char* end = nullptr;
    const long value = std::strtol(text, &end, 10);
    if (end == text || *end != '\0') {
        return false;
    }
    out = value;
    return true;
}

bool parse_int(const char* text, int& out) {
    long value = 0;
    if (!parse_long(text, value)) {
        return false;
    }
    out = static_cast<int>(value);
    return true;
}

bool parse_double(const char* text, double& out) {
    char* end = nullptr;
    const double value = std::strtod(text, &end);
    if (end == text || *end != '\0') {
        return false;
    }
    out = value;
    return true;
}

void report(int line_number, const char* reason) {
    std::cerr << "error: invalid frame at line " << line_number << ": " << reason << '\n';
}

bool parse_and_validate(char* line, int line_number, bool have_previous, const Frame& previous,
                        Frame& out) {
    // Дефект №1 (неправильна форма вхідних даних): було проігноровано кількість значень на старті. 
    // Тепер нам потрібно рівно 7 полів.
    char* fields[TELEMETRY_FIELD_COUNT] = {};
    const int field_count = split_line(line, fields, TELEMETRY_FIELD_COUNT);
    if (field_count != TELEMETRY_FIELD_COUNT) {
        report(line_number, "expected 7 fields");
        return false;
    }

    Frame frame{};
    if (!parse_long(fields[0], frame.timestamp_ms) || !parse_int(fields[1], frame.seq) ||
        !parse_double(fields[2], frame.voltage_v) || !parse_double(fields[3], frame.current_a) ||
        !parse_double(fields[4], frame.temperature_c) || !parse_int(fields[5], frame.gps_fix) ||
        !parse_int(fields[6], frame.satellites)) {
        report(line_number, "field is not a number");
        return false;
    }

    if (frame.voltage_v <= 0.0) {
        report(line_number, "voltage_v must be positive");
        return false;
    }
    if (frame.temperature_c < MIN_TEMPERATURE_C || frame.temperature_c > MAX_TEMPERATURE_C) {
        report(line_number, "temperature_c out of range [-40, 120]");
        return false;
    }
    if (frame.gps_fix != 0 && frame.gps_fix != 1) {
        report(line_number, "gps_fix must be 0 or 1");
        return false;
    }
    if (frame.satellites < 0) {
        report(line_number, "satellites must be >= 0");
        return false;
    }
    if (frame.seq < 1) {
        report(line_number, "seq must be >= 1");
        return false;
    }

    if (!have_previous) {
        if (frame.seq != 1) {
            report(line_number, "seq must start at 1");
            return false;
        }
    } else {
    // Дефект №3 (небезпечні часові дельти): рівні або зменшувальні часові позначки пізніше спричинили ділення на нуль у розрахунку. 
    // Відхилення незростаючих часових позначок тут зберігає цю дельту виключно позитивною.
        if (frame.timestamp_ms <= previous.timestamp_ms) {
            report(line_number, "timestamp_ms must increase");
            return false;
        }
    }

    out = frame;
    return true;
}

double compute_frame_rate_hz(const Frame frames[], int frame_count) {
    if (frame_count < 2) {
        return 0.0;
    }
    const long elapsed_ms = frames[frame_count - 1].timestamp_ms - frames[0].timestamp_ms;
    if (elapsed_ms <= 0) {
        return 0.0;
    }
    return static_cast<double>(frame_count - 1) * 1000.0 / static_cast<double>(elapsed_ms);
}

}

int read_frames(const char* path, Frame frames[], int max_frames, int& total_out) {
    total_out = 0;

    std::ifstream input{path};
    if (!input) {
        std::cerr << "error: failed to open input file: " << path << '\n';
        return 0;
    }

    int valid_count = 0;
    int line_number = 0;
    bool have_previous = false;
    Frame previous{};
    char line[MAX_LINE_LENGTH];

    while (input.getline(line, MAX_LINE_LENGTH)) {
        ++line_number;
        if (is_blank(line)) {
            continue;
        }
        ++total_out;

        Frame frame{};
        if (!parse_and_validate(line, line_number, have_previous, previous, frame)) {
            continue;
        }

        if (valid_count < max_frames) {
            frames[valid_count] = frame;
            ++valid_count;
        }
        previous = frame;
        have_previous = true;
    }

    return valid_count;
}

Summary summarize(const Frame frames[], int frames_valid, int frames_total) {
    Summary summary{};
    summary.frames_total = frames_total;
    summary.frames_valid = frames_valid;

    // Дефект №4 (порожні логи): початковий код зчитує frames[0] перед перевіркою підрахунку, 
    // тому порожній лог зчитується за межами дозволеного. 
    // Без дійсних фреймів кожна метрика просто дорівнює нулю.
    if (frames_valid == 0) {
        return summary;
    }

    summary.voltage_min = frames[0].voltage_v;
    summary.voltage_max = frames[0].voltage_v;

    double temperature_sum = 0.0;
    for (int i = 0; i < frames_valid; ++i) {
        if (frames[i].voltage_v < summary.voltage_min) {
            summary.voltage_min = frames[i].voltage_v;
        }
        if (frames[i].voltage_v > summary.voltage_max) {
            summary.voltage_max = frames[i].voltage_v;
        }
        temperature_sum += frames[i].temperature_c;
        if (frames[i].voltage_v < LOW_VOLTAGE_THRESHOLD_V) {
            ++summary.low_voltage_frames;
        }
    }

    summary.temperature_avg = temperature_sum / static_cast<double>(frames_valid);
    summary.frame_rate_hz = compute_frame_rate_hz(frames, frames_valid);
    return summary;
}

void print_summary(const Summary& summary) {
    std::cout << "frames_total " << summary.frames_total << '\n';
    std::cout << "frames_valid " << summary.frames_valid << '\n';
    std::cout << "voltage_min " << summary.voltage_min << '\n';
    std::cout << "voltage_max " << summary.voltage_max << '\n';
    std::cout << "temperature_avg " << summary.temperature_avg << '\n';
    std::cout << "low_voltage_frames " << summary.low_voltage_frames << '\n';
    std::cout << "frame_rate_hz " << summary.frame_rate_hz << '\n';
}
