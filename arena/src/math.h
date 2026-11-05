#pragma once
#include <cmath>

namespace ar {

constexpr float kPi = 3.14159265358979f;

inline float degToRad(float degrees) { return degrees * (kPi / 180.0f); }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline Vec3 operator+(const Vec3& a, const Vec3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 operator-(const Vec3& a, const Vec3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 operator*(const Vec3& a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline Vec3& operator+=(Vec3& a, const Vec3& b) {
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    return a;
}

inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

inline float length(const Vec3& v) { return std::sqrt(dot(v, v)); }
inline float length2d(const Vec3& v) { return std::sqrt(v.x * v.x + v.y * v.y); }

inline Vec3 normalize(const Vec3& v) {
    const float len = length(v);
    return len > 1.0e-6f ? v * (1.0f / len) : Vec3{};
}

// Row major 4x4, column vector math: clip = M * p.
struct Mat4 {
    float m[16]{};
};

inline Mat4 multiply(const Mat4& a, const Mat4& b) {
    Mat4 out{};
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) sum += a.m[r * 4 + k] * b.m[k * 4 + c];
            out.m[r * 4 + c] = sum;
        }
    }
    return out;
}

inline Mat4 perspective(float fovYRad, float aspect, float nearZ, float farZ) {
    const float f = 1.0f / std::tan(fovYRad * 0.5f);
    Mat4 out{};
    out.m[0] = f / aspect;
    out.m[5] = f;
    out.m[10] = (farZ + nearZ) / (nearZ - farZ);
    out.m[11] = (2.0f * farZ * nearZ) / (nearZ - farZ);
    out.m[14] = -1.0f;
    return out;
}

// Right handed view matrix, camera looks down -Z in view space, which makes
// clip.w positive for everything in front of the camera. That is exactly what
// halcyon's worldToScreen expects.
inline Mat4 lookAt(const Vec3& eye, const Vec3& forward, const Vec3& up) {
    const Vec3 f = normalize(forward);
    const Vec3 s = normalize(cross(f, up));
    const Vec3 u = cross(s, f);

    Mat4 out{};
    out.m[0] = s.x;
    out.m[1] = s.y;
    out.m[2] = s.z;
    out.m[3] = -dot(s, eye);

    out.m[4] = u.x;
    out.m[5] = u.y;
    out.m[6] = u.z;
    out.m[7] = -dot(u, eye);

    out.m[8] = -f.x;
    out.m[9] = -f.y;
    out.m[10] = -f.z;
    out.m[11] = dot(f, eye);

    out.m[15] = 1.0f;
    return out;
}

}  // namespace ar
