#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <mutex>
#include <random>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace hx {

constexpr float kPi = 3.14159265358979323846f;
constexpr float kRad2Deg = 180.0f / kPi;

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }

    float length() const { return std::sqrt(x * x + y * y + z * z); }
    float length2d() const { return std::sqrt(x * x + y * y); }
};

// Source style view matrix: 16 floats, row major, as stored in the engine.
struct Mat4 {
    float m[16]{};
};

// Project a world point through the view matrix into client-area pixels.
inline bool worldToScreen(const Mat4& vm, const Vec3& p, float width, float height, Vec2& out) {
    const float w = vm.m[3] * p.x + vm.m[7] * p.y + vm.m[11] * p.z + vm.m[15];
    if (w < 0.001f) return false;

    const float x = vm.m[0] * p.x + vm.m[4] * p.y + vm.m[8] * p.z + vm.m[12];
    const float y = vm.m[1] * p.x + vm.m[5] * p.y + vm.m[9] * p.z + vm.m[13];

    out.x = (x / w * 0.5f + 0.5f) * width;
    out.y = (-y / w * 0.5f + 0.5f) * height;
    return out.x > -width && out.x < width * 2.0f && out.y > -height && out.y < height * 2.0f;
}

inline bool finite3(const Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z) &&
           std::fabs(v.x) < 1.0e7f && std::fabs(v.y) < 1.0e7f && std::fabs(v.z) < 1.0e7f;
}

// Minimal timestamped logger. Writes to stdout and, when a file is opened, to
// that file as well.
void logf(const char* fmt, ...);
bool logOpenFile(const char* path);
void logCloseFile();

struct Clock {
    using clock = std::chrono::steady_clock;
    clock::time_point start = clock::now();

    void reset() { start = clock::now(); }
    double ms() const {
        return std::chrono::duration<double, std::milli>(clock::now() - start).count();
    }
};

// Sleep for at least `ms` milliseconds without busy spinning.
void sleepMs(int ms);

std::wstring widen(const std::string& s);
std::string narrow(const std::wstring& s);

}  // namespace hx
