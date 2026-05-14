#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <ostream>

namespace {

constexpr long kTicksPerRevolution = 1024;
constexpr double kWheelRadiusM = 0.3;
constexpr double kWheelbaseM = 1.0;
constexpr double kPi = 3.14159265358979323846;

struct Sample {
    long timestamp_ms;
    long fl_ticks;
    long fr_ticks;
    long bl_ticks;
    long br_ticks;
};

}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: ugv_odometry <input_path>\n";
        return 1;
    }

    std::ifstream input(argv[1]);
    if (!input) {
        std::cerr << "error: cannot open input file: " << argv[1] << "\n";
        return 1;
    }

    const double distance_per_tick = 2.0 * kPi * kWheelRadiusM / static_cast<double>(kTicksPerRevolution);

    double x = 0.0;
    double y = 0.0;
    double theta = 0.0;

    Sample prev{};
    bool have_prev = false;

    std::cout << std::fixed << std::setprecision(4);

    Sample cur{};
    while (input >> cur.timestamp_ms >> cur.fl_ticks >> cur.fr_ticks >> cur.bl_ticks >> cur.br_ticks) {
        if (!have_prev) {
            prev = cur;
            have_prev = true;
            continue;
        }

        const long d_fl = cur.fl_ticks - prev.fl_ticks;
        const long d_fr = cur.fr_ticks - prev.fr_ticks;
        const long d_bl = cur.bl_ticks - prev.bl_ticks;
        const long d_br = cur.br_ticks - prev.br_ticks;

        const double d_left_ticks = (d_fl + d_bl) / 2.0;
        const double d_right_ticks = (d_fr + d_br) / 2.0;

        const double dL = d_left_ticks * distance_per_tick;
        const double dR = d_right_ticks * distance_per_tick;

        const double d = (dL + dR) / 2.0;
        const double dtheta = (dR - dL) / kWheelbaseM;

        x += d * std::cos(theta + dtheta / 2.0);
        y += d * std::sin(theta + dtheta / 2.0);
        theta += dtheta;

        std::cout << cur.timestamp_ms << ' ' << x << ' ' << y << ' ' << theta << '\n';

        prev = cur;
    }

    return 0;
}
