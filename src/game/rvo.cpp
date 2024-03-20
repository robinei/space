/*
 * rvo.cpp
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

#include "rvo.h"

namespace RVO {
    const float RVO_EPSILON = 0.00001f;
    const size_t MAX_PLANES = 128;

    class Line {
    public:
        Vector3 direction;
        Vector3 point;
    };

    class Plane {
    public:
        Vector3 point;
        Vector3 normal;
    };

    inline float sqr(float scalar) {
        return scalar * scalar;
    }

    /**
     * \brief   Solves a one-dimensional linear program on a specified line subject to linear constraints defined by planes and a spherical constraint.
     * \param   planes        Planes defining the linear constraints.
     * \param   planeIndex    The plane on which the line lies.
     * \param   line          The line on which the 1-d linear program is solved
     * \param   radius        The radius of the spherical constraint.
     * \param   optVelocity   The optimization velocity.
     * \param   directionOpt  True if the direction should be optimized.
     * \param   result        A reference to the result of the linear program.
     * \return  True if successful.
     */
    bool linearProgram1(const Plane planes[], size_t planeIndex, const Line &line, float radius, const Vector3 &optVelocity, bool directionOpt, Vector3 &result) {
        const float dotProduct = line.point * line.direction;
        const float discriminant = sqr(dotProduct) + sqr(radius) - absSq(line.point);

        if (discriminant < 0.0f) {
            /* Max speed sphere fully invalidates line. */
            return false;
        }

        const float sqrtDiscriminant = std::sqrt(discriminant);
        float tLeft = -dotProduct - sqrtDiscriminant;
        float tRight = -dotProduct + sqrtDiscriminant;

        for (size_t i = 0; i < planeIndex; ++i) {
            const float numerator = (planes[i].point - line.point) * planes[i].normal;
            const float denominator = line.direction * planes[i].normal;

            if (sqr(denominator) <= RVO_EPSILON) {
                /* Lines line is (almost) parallel to plane i. */
                if (numerator > 0.0f) {
                    return false;
                }
                continue;
            }

            const float t = numerator / denominator;

            if (denominator >= 0.0f) {
                /* Plane i bounds line on the left. */
                if (t > tLeft) {
                    tLeft = t;
                }
            } else {
                /* Plane i bounds line on the right. */
                if (t < tRight) {
                    tRight = t;
                }
            }

            if (tLeft > tRight) {
                return false;
            }
        }

        if (directionOpt) {
            /* Optimize direction. */
            if (optVelocity * line.direction > 0.0f) {
                /* Take right extreme. */
                result = line.point + tRight * line.direction;
            } else {
                /* Take left extreme. */
                result = line.point + tLeft * line.direction;
            }
        } else {
            /* Optimize closest point. */
            const float t = line.direction * (optVelocity - line.point);

            if (t < tLeft) {
                result = line.point + tLeft * line.direction;
            } else if (t > tRight) {
                result = line.point + tRight * line.direction;
            } else {
                result = line.point + t * line.direction;
            }
        }

        return true;
    }

    /**
     * \brief   Solves a two-dimensional linear program on a specified plane subject to linear constraints defined by planes and a spherical constraint.
     * \param   planes        Planes defining the linear constraints.
     * \param   planeIndex    The plane on which the 2-d linear program is solved
     * \param   radius        The radius of the spherical constraint.
     * \param   optVelocity   The optimization velocity.
     * \param   directionOpt  True if the direction should be optimized.
     * \param   result        A reference to the result of the linear program.
     * \return  True if successful.
     */
    bool linearProgram2(const Plane planes[], size_t planeIndex, float radius, const Vector3 &optVelocity, bool directionOpt, Vector3 &result) {
        const float planeDist = planes[planeIndex].point * planes[planeIndex].normal;
        const float planeDistSq = sqr(planeDist);
        const float radiusSq = sqr(radius);

        if (planeDistSq > radiusSq) {
            /* Max speed sphere fully invalidates plane planeIndex. */
            return false;
        }

        const float planeRadiusSq = radiusSq - planeDistSq;

        const Vector3 planeCenter = planeDist * planes[planeIndex].normal;

        if (directionOpt) {
            /* Project direction optVelocity on plane planeIndex. */
            const Vector3 planeOptVelocity = optVelocity - (optVelocity * planes[planeIndex].normal) * planes[planeIndex].normal;
            const float planeOptVelocityLengthSq = absSq(planeOptVelocity);

            if (planeOptVelocityLengthSq <= RVO_EPSILON) {
                result = planeCenter;
            } else {
                result = planeCenter + std::sqrt(planeRadiusSq / planeOptVelocityLengthSq) * planeOptVelocity;
            }
        } else {
            /* Project point optVelocity on plane planeIndex. */
            result = optVelocity + ((planes[planeIndex].point - optVelocity) * planes[planeIndex].normal) * planes[planeIndex].normal;

            /* If outside planeCircle, project on planeCircle. */
            if (absSq(result) > radiusSq) {
                const Vector3 planeResult = result - planeCenter;
                const float planeResultLengthSq = absSq(planeResult);
                result = planeCenter + std::sqrt(planeRadiusSq / planeResultLengthSq) * planeResult;
            }
        }

        for (size_t i = 0; i < planeIndex; ++i) {
            if (planes[i].normal * (planes[i].point - result) > 0.0f) {
                /* Result does not satisfy constraint i. Compute new optimal result. */
                /* Compute intersection line of plane i and plane planeIndex. */
                Vector3 crossProduct = cross(planes[i].normal, planes[planeIndex].normal);

                if (absSq(crossProduct) <= RVO_EPSILON) {
                    /* Planes planeIndex and i are (almost) parallel, and plane i fully invalidates plane planeIndex. */
                    return false;
                }

                Line line;
                line.direction = normalize(crossProduct);
                const Vector3 lineNormal = cross(line.direction, planes[planeIndex].normal);
                line.point = planes[planeIndex].point + (((planes[i].point - planes[planeIndex].point) * planes[i].normal) / (lineNormal * planes[i].normal)) * lineNormal;

                if (!linearProgram1(planes, i, line, radius, optVelocity, directionOpt, result)) {
                    return false;
                }
            }
        }

        return true;
    }

    /**
     * \brief   Solves a three-dimensional linear program subject to linear constraints defined by planes and a spherical constraint.
     * \param   planes        Planes defining the linear constraints.
     * \param   planeCount    Number of planes defining the linear constraints.
     * \param   radius        The radius of the spherical constraint.
     * \param   optVelocity   The optimization velocity.
     * \param   directionOpt  True if the direction should be optimized.
     * \param   result        A reference to the result of the linear program.
     * \return  The number of the plane it fails on, and the number of planes if successful.
     */
    size_t linearProgram3(const Plane planes[], size_t planeCount, float radius, const Vector3 &optVelocity, bool directionOpt, Vector3 &result) {
        if (directionOpt) {
            /* Optimize direction. Note that the optimization velocity is of unit length in this case. */
            result = optVelocity * radius;
        } else if (absSq(optVelocity) > sqr(radius)) {
            /* Optimize closest point and outside circle. */
            result = normalize(optVelocity) * radius;
        } else {
            /* Optimize closest point and inside circle. */
            result = optVelocity;
        }

        for (size_t i = 0; i < planeCount; ++i) {
            if (planes[i].normal * (planes[i].point - result) > 0.0f) {
                /* Result does not satisfy constraint i. Compute new optimal result. */
                const Vector3 tempResult = result;

                if (!linearProgram2(planes, i, radius, optVelocity, directionOpt, result)) {
                    result = tempResult;
                    return i;
                }
            }
        }

        return planeCount;
    }

    /**
     * \brief   Solves a four-dimensional linear program subject to linear constraints defined by planes and a spherical constraint.
     * \param   planes     Planes defining the linear constraints.
     * \param   planeCount    Number of planes defining the linear constraints.
     * \param   beginPlane The plane on which the 3-d linear program failed.
     * \param   radius     The radius of the spherical constraint.
     * \param   result     A reference to the result of the linear program.
     */
    void linearProgram4(const Plane planes[], size_t planeCount, size_t beginPlane, float radius, Vector3 &result) {
        float distance = 0.0f;

        for (size_t i = beginPlane; i < planeCount; ++i) {
            if (planes[i].normal * (planes[i].point - result) > distance) {
                /* Result does not satisfy constraint of plane i. */
                Plane projPlanes[MAX_PLANES];
                size_t projPlanesCount = 0;

                for (size_t j = 0; j < i; ++j) {
                    Plane plane;

                    const Vector3 crossProduct = cross(planes[j].normal, planes[i].normal);

                    if (absSq(crossProduct) <= RVO_EPSILON) {
                        /* Plane i and plane j are (almost) parallel. */
                        if (planes[i].normal * planes[j].normal > 0.0f) {
                            /* Plane i and plane j point in the same direction. */
                            continue;
                        } else {
                            /* Plane i and plane j point in opposite direction. */
                            plane.point = 0.5f * (planes[i].point + planes[j].point);
                        }
                    } else {
                        /* Plane.point is point on line of intersection between plane i and plane j. */
                        const Vector3 lineNormal = cross(crossProduct, planes[i].normal);
                        plane.point = planes[i].point + (((planes[j].point - planes[i].point) * planes[j].normal) / (lineNormal * planes[j].normal)) * lineNormal;
                    }

                    plane.normal = normalize(planes[j].normal - planes[i].normal);
                    projPlanes[projPlanesCount++] = plane;
                }

                const Vector3 tempResult = result;

                if (linearProgram3(projPlanes, projPlanesCount, radius, planes[i].normal, true, result) < projPlanesCount) {
                    /* This should in principle not happen.  The result is by definition already in the feasible region of this linear program. If it fails, it is due to small floating point error, and the current result is kept. */
                    result = tempResult;
                }

                distance = planes[i].normal * (planes[i].point - result);
            }
        }
    }

    Vector3 Agent::computeNewVelocity(float timeStep, float timeHorizon, Vector3 prefVelocity, float maxSpeed, const Agent* neighbors[], size_t neighborCount) {
        Plane orcaPlanes[MAX_PLANES];
        if (neighborCount > MAX_PLANES) {
            neighborCount = MAX_PLANES;
        }
        const float invTimeHorizon = 1.0f / timeHorizon;

        /* Create agent ORCA planes. */
        for (size_t i = 0; i < neighborCount; ++i) {
            const Agent *const other = neighbors[i];
            const Vector3 relativePosition = other->position - position;
            const Vector3 relativeVelocity = velocity - other->velocity;
            const float distSq = absSq(relativePosition);
            const float combinedRadius = radius + other->radius;
            const float combinedRadiusSq = sqr(combinedRadius);

            Plane plane;
            Vector3 u;

            if (distSq > combinedRadiusSq) {
                /* No collision. */
                const Vector3 w = relativeVelocity - invTimeHorizon * relativePosition;
                /* Vector from cutoff center to relative velocity. */
                const float wLengthSq = absSq(w);

                const float dotProduct = w * relativePosition;

                if (dotProduct < 0.0f && sqr(dotProduct) > combinedRadiusSq * wLengthSq) {
                    /* Project on cut-off circle. */
                    const float wLength = std::sqrt(wLengthSq);
                    const Vector3 unitW = w / wLength;

                    plane.normal = unitW;
                    u = (combinedRadius * invTimeHorizon - wLength) * unitW;
                } else {
                    /* Project on cone. */
                    const float a = distSq;
                    const float b = relativePosition * relativeVelocity;
                    const float c = absSq(relativeVelocity) - absSq(cross(relativePosition, relativeVelocity)) / (distSq - combinedRadiusSq);
                    const float t = (b + std::sqrt(sqr(b) - a * c)) / a;
                    const Vector3 w = relativeVelocity - t * relativePosition;
                    const float wLength = abs(w);
                    const Vector3 unitW = w / wLength;

                    plane.normal = unitW;
                    u = (combinedRadius * t - wLength) * unitW;
                }
            } else {
                /* Collision. */
                const float invTimeStep = 1.0f / timeStep;
                const Vector3 w = relativeVelocity - invTimeStep * relativePosition;
                const float wLength = abs(w);
                const Vector3 unitW = w / wLength;

                plane.normal = unitW;
                u = (combinedRadius * invTimeStep - wLength) * unitW;
            }

            plane.point = velocity + 0.5f * u;
            orcaPlanes[i] = plane;
        }

        Vector3 newVelocity = velocity;
        const size_t planeFail = linearProgram3(orcaPlanes, neighborCount, maxSpeed, prefVelocity, false, newVelocity);

        if (planeFail < neighborCount) {
            linearProgram4(orcaPlanes, neighborCount, planeFail, maxSpeed, newVelocity);
        }

        return newVelocity;
    }
}
