#include "telemetry.hpp"

#include <iostream>

int main(int argc, char** argv) {
    // The executable expects exactly one telemetry log path.
    if (argc != 2) {
        std::cerr << "usage: telemetry_check <input_path>\n";
        return 1;
    }

    Frame frames[MAX_TELEMETRY_FRAMES];
    int frames_total = 0;
    const int frames_valid = read_frames(argv[1], frames, MAX_TELEMETRY_FRAMES, frames_total);

    const Summary summary = summarize(frames, frames_valid, frames_total);
    print_summary(summary);

    if (frames_total == 0 || frames_valid != frames_total) {
        return 1;
    }
    return 0;
}
