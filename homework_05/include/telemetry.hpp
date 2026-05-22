#pragma once

// Fixed-size storage keeps the starter close to the topics from block 1.
const int MAX_TELEMETRY_FRAMES = 128;

// Every telemetry line must contain exactly this many whitespace-separated fields.
const int TELEMETRY_FIELD_COUNT = 7;

// Frames with voltage below this threshold are counted as "low voltage".
const double LOW_VOLTAGE_THRESHOLD_V = 22.0;

// One telemetry sample from the input log.
struct Frame {
    long timestamp_ms;
    int seq;
    double voltage_v;
    double current_a;
    double temperature_c;
    int gps_fix;
    int satellites;
};

// Aggregated values printed by the executable.
struct Summary {
    int frames_total;
    int frames_valid;
    double voltage_min;
    double voltage_max;
    double temperature_avg;
    int low_voltage_frames;
    double frame_rate_hz;
};

// Reads frames from a whitespace-separated telemetry log.
int read_frames(const char* path, Frame frames[], int max_frames, int& total_out);

// Calculates summary values for already validated frames. Safe for an empty
// set of frames (every metric becomes 0 instead of reading out of bounds).
Summary summarize(const Frame frames[], int frames_valid, int frames_total);

// Prints summary in the stable homework output format.
void print_summary(const Summary& summary);
