#ifndef MYMATH_H
#define MYMATH_H

#include <glm/glm.hpp>

using glm::vec2;
using glm::vec3;
using glm::vec4;
using glm::mat2;
using glm::mat3;
using glm::mat4;


class Rect {
public:
    vec2 min, max;

    Rect() {}
    Rect(float x0, float y0, float x1, float y1) : min(x0, y0), max(x1, y1) {}
    Rect(vec2 min, vec2 max) : min(min), max(max) {}

    float width() { return max.x - min.x; }
    float height() { return max.y - min.y; }
    vec2 dimensions() { return vec2(width(), height()); }
    vec2 center() { return min + dimensions() * 0.5f; }

    bool contains(vec2 p) {
        return !(p.x < min.x ||
                 p.y < min.y ||
                 p.x > max.x ||
                 p.y > max.y);
    }

    bool contains(vec2 p, float fuzz) {
        return !(p.x < min.x - fuzz ||
                 p.y < min.y - fuzz ||
                 p.x > max.x + fuzz ||
                 p.y > max.y + fuzz);
    }

    bool intersects(Rect r) {
        return !(r.min.x > max.x ||
                 r.max.x < min.x ||
                 r.min.y > max.y ||
                 r.max.y < min.y);
    }
};

struct Plane {
    vec3 n;
    float d;

    Plane(vec3 n, float d = 0) : n(n), d(d) {}

    vec3 ray_intersect(vec3 p0, vec3 p1) {
        vec3 v = glm::normalize(p1 - p0);
        float t = -(glm::dot(p0, n) + d) / glm::dot(v, n);
        return p0 + v * t;
    }

    static Plane XY(float d=0) { return Plane(vec3(0, 0, 1), d); }
    static Plane XZ(float d=0) { return Plane(vec3(0, 1, 0), d); }
    static Plane YZ(float d=0) { return Plane(vec3(1, 0, 0), d); }
};

#endif
