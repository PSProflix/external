#pragma once
#include <cmath>

struct Vector3 {
    float x, y, z;
    Vector3 operator+(const Vector3& v) const { return { x + v.x, y + v.y, z + v.z }; }
    Vector3 operator-(const Vector3& v) const { return { x - v.x, y - v.y, z - v.z }; }
};

struct Vector2 {
    float x, y;
};

struct Matrix4x4 {
    float m[4][4];
};

bool WorldToScreen(Vector3 pos, Vector2& screen, Matrix4x4 matrix, int width, int height) {
    float w = matrix.m[3][0] * pos.x + matrix.m[3][1] * pos.y + matrix.m[3][2] * pos.z + matrix.m[3][3];
    if (w < 0.01f) return false;

    float x = matrix.m[0][0] * pos.x + matrix.m[0][1] * pos.y + matrix.m[0][2] * pos.z + matrix.m[0][3];
    float y = matrix.m[1][0] * pos.x + matrix.m[1][1] * pos.y + matrix.m[1][2] * pos.z + matrix.m[1][3];

    float invw = 1.0f / w;
    x *= invw;
    y *= invw;

    float ox = width / 2.0f;
    float oy = height / 2.0f;

    screen.x = ox + (ox * x);
    screen.y = oy - (oy * y);

    return true;
}