/*
 * rvo.h
 * Adapted from the RVO2-3D Library
 *
 * Copyright 2008 University of North Carolina at Chapel Hill
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Please send all bug reports to <geom@cs.unc.edu>.
 *
 * The authors may be contacted via:
 *
 * Jur van den Berg, Stephen J. Guy, Jamie Snape, Ming C. Lin, Dinesh Manocha
 * Dept. of Computer Science
 * 201 S. Columbia St.
 * Frederick P. Brooks, Jr. Computer Science Bldg.
 * Chapel Hill, N.C. 27599-3175
 * United States of America
 *
 * <http://gamma.cs.unc.edu/RVO2/>
 */

#ifndef RVO_AGENT_H_
#define RVO_AGENT_H_

#include <cmath>

namespace RVO {
    class Vector3 {
    public:
        float x, y, z;

        Vector3() : x(0.0f), y(0.0f), z(0.0f) { }
        Vector3(const Vector3 &v) : x(v.x), y(v.y), z(v.z) { }
        Vector3(float x, float y, float z) : x(x), y(y), z(z) { }

        Vector3 operator-() const { return Vector3(-x, -y, -z); }
        float operator*(const Vector3 &v) const { return x * v.x + y * v.y + z * v.z; }
        Vector3 operator*(float s) const { return Vector3(x * s, y * s, z * s); }
        Vector3 operator/(float s) const { const float inv = 1.0f / s; return Vector3(x * inv, y * inv, z * inv); }
        Vector3 operator+(const Vector3 &v) const { return Vector3(x + v.x, y + v.y, z + v.z); }
        Vector3 operator-(const Vector3 &v) const { return Vector3(x - v.x, y - v.y, z - v.z); }

        Vector3 &operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
        Vector3 &operator/=(float s) { const float inv = 1.0f / s; x *= inv; y *= inv; z *= inv; return *this; }
        Vector3 &operator+=(const Vector3 &v) { x += v.x; y += v.y; z += v.z; return *this; }
        Vector3 &operator-=(const Vector3 &v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
    };

    inline Vector3 operator*(float s, const Vector3 &v) { return Vector3(s * v.x, s * v.y, s * v.z); }
    inline Vector3 cross(const Vector3 &v1, const Vector3 &v2) { return Vector3(v1.y * v2.z - v1.z * v2.y, v1.z * v2.x - v1.x * v2.z, v1.x * v2.y - v1.y * v2.x); }
    inline float abs(const Vector3 &v) { return std::sqrt(v * v); }
    inline float absSq(const Vector3 &v) { return v * v; }
    inline Vector3 normalize(const Vector3 &v) { return v / abs(v); }

    class Agent {
    public:
        Vector3 position;
        Vector3 velocity;
        float radius;

        Agent() : radius(0.0f) { }
        Agent(Vector3 position, Vector3 velocity, float radius) : position(position), velocity(velocity), radius(radius) { }

        Vector3 computeNewVelocity(float timeStep, float timeHorizon, Vector3 prefVelocity, float maxSpeed, const Agent* neighbors[], size_t neighborCount);
    };
}

#endif
