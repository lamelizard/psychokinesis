#pragma once

#include <iostream>
#include <sstream>

#include <glm/ext.hpp>
#include <glm/glm.hpp>
#include <glm/gtc/epsilon.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/transform.hpp>

namespace glow
{
struct transform
{
public:
    const static transform Identity;

    /// Directions
    static glm::vec3 Forward() { return {0, 0, -1}; }
    static glm::vec3 Up() { return {0, 1, 0}; }
    static glm::vec3 Right() { return {1, 0, 0}; }

    static glm::quat RotationFromDirection(glm::vec3 const& direction, glm::vec3 const& up = Up())
    {
        float directionLength = glm::length(direction);
        if (directionLength <= 0.001f)
            return glm::quat();

        glm::vec3 const normalizedDirection = direction / directionLength;
        glm::vec3 const rightVector = glm::normalize(glm::cross(normalizedDirection, up));
        glm::vec3 const upVector = glm::cross(rightVector, normalizedDirection);

        glm::mat3 rotMatrix;
        rotMatrix[0][0] = rightVector.x;
        rotMatrix[0][1] = upVector.x;
        rotMatrix[0][2] = -normalizedDirection.x;
        rotMatrix[1][0] = rightVector.y;
        rotMatrix[1][1] = upVector.y;
        rotMatrix[1][2] = -normalizedDirection.y;
        rotMatrix[2][0] = rightVector.z;
        rotMatrix[2][1] = upVector.z;
        rotMatrix[2][2] = -normalizedDirection.z;

        return glm::quat(rotMatrix);
    }

public:
    glm::vec3 position{0};
    glm::quat rotation{};
    glm::vec3 scale{1};

public:
    transform() = default;

    transform(glm::vec3 const& pos, glm::quat const& rot = glm::quat{}, glm::vec3 const& scale = glm::vec3{1})
      : position(pos), rotation(rot), scale(scale)
    {
    }

    /// Constructor from Model Matrix
    transform(glm::mat4 m)
    {
        // NOTE: The following is a cut-down version of glm::decompose
        // (No Shear, no Perspective, slightly optimized / cleaned up)
        // TODO: Optimize this further

        // Normalize the matrix.
        if (m[3][3] == 0.f)
            return;

        for (auto i = 0; i < 4; ++i)
            for (auto j = 0; j < 4; ++j)
                m[i][j] /= m[3][3];

        // Clear the perspective partition
        m[0][3] = m[1][3] = m[2][3] = 0.f;
        m[3][3] = 1.f;

        // Extract translation
        position = glm::vec3(m[3]);
        m[3] = glm::vec4(0, 0, 0, m[3].w);

        glm::vec3 Row[3], Pdum3;

        // Extract Scale
        for (auto i = 0; i < 3; ++i)
            for (auto j = 0; j < 3; ++j)
                Row[i][j] = m[i][j];

        // Compute X scale factor and normalize first row.
        scale.x = length(Row[0]);

        Row[0] = glm::normalize(Row[0]);
        Row[1] += Row[0] * -glm::dot(Row[0], Row[1]);

        // Now, compute Y scale and normalize 2nd row.
        scale.y = length(Row[1]);
        Row[1] = glm::normalize(Row[1]);

        // Compute XZ and YZ shears, orthogonalize 3rd row.
        Row[2] += Row[0] * -glm::dot(Row[0], Row[2]);
        Row[2] += Row[1] * -glm::dot(Row[1], Row[2]);

        // Next, get Z scale and normalize 3rd row.
        scale.z = length(Row[2]);
        Row[2] = glm::normalize(Row[2]);

        // At this point, the matrix (in rows[]) is orthonormal.
        // Check for a coordinate system flip.  If the determinant
        // is -1, then negate the matrix and the scaling factors.
        Pdum3 = cross(Row[1], Row[2]);
        if (dot(Row[0], Pdum3) < 0.f)
        {
            scale *= -1.f;

            for (auto i = 0; i < 3; i++)
                Row[i] *= -1.f;
        }

        int i, j, k = 0;
        float root, trace = Row[0].x + Row[1].y + Row[2].z;
        if (trace > 0.f)
        {
            root = sqrt(trace + 1.f);
            rotation.w = .5f * root;
            root = .5f / root;
            rotation.x = root * (Row[1].z - Row[2].y);
            rotation.y = root * (Row[2].x - Row[0].z);
            rotation.z = root * (Row[0].y - Row[1].x);
        }
        else
        {
            static constexpr int nextIndex[3] = {1, 2, 0};
            i = 0;
            if (Row[1].y > Row[0].x)
                i = 1;
            if (Row[2].z > Row[i][i])
                i = 2;
            j = nextIndex[i];
            k = nextIndex[j];

            root = sqrt(Row[i][i] - Row[j][j] - Row[k][k] + 1.f);

            rotation[i] = .5f * root;
            root = .5f / root;
            rotation[j] = root * (Row[i][j] + Row[j][i]);
            rotation[k] = root * (Row[i][k] + Row[k][i]);
            rotation.w = root * (Row[j][k] - Row[k][j]);
        }
    }

    /// -- Transform Manipulation --

    /// Combine two Transforms
    transform& operator*=(transform const& rhs)
    {
        position += (rotation * rhs.position) * scale;
        rotation *= rhs.rotation;
        scale *= rhs.scale;
        return *this;
    }

    /// Moves the transform relative to the world
    void moveGlobal(glm::vec3 const& direction) { position += direction; }

    /// Moves the transform relative to its own orientation
    void moveLocal(glm::vec3 const& direction) { moveGlobal(rotation * direction); }
    void moveForward(float distance) { moveLocal(Forward() * distance); }
    void moveRight(float distance) { moveLocal(Right() * distance); }
    void moveUp(float distance) { moveLocal(Up() * distance); }
    void moveBack(float distance) { moveForward(-distance); }
    void moveLeft(float distance) { moveRight(-distance); }
    void moveDown(float distance) { moveUp(-distance); }

    void rotate(glm::quat const& rot) { rotation *= rot; }
    void rotate(glm::vec3 const& rot) { rotation *= glm::quat(rot); }
    void applyScale(glm::vec3 const& scal) { scale *= scal; }

    void lerpToPosition(glm::vec3 const& pos, float amount) { position = glm::lerp(position, pos, amount); }
    void lerpToRotation(glm::quat const& rot, float amount) { rotation = glm::slerp(rotation, rot, amount); }
    void lerpToScale(glm::vec3 const& scal, float amount) { scale = glm::lerp(scale, scal, amount); }

    void lerpTo(transform const& target, float amount)
    {
        position = glm::lerp(position, target.position, amount);
        rotation = glm::slerp(rotation, target.rotation, amount);
        scale = glm::lerp(scale, target.scale, amount);
    }

    /// -- Getters and Setters --

    glm::mat4 getModelMatrix() const
    {
        return glm::translate(glm::mat4(1), position) * glm::toMat4(rotation) * glm::scale(glm::mat4(1), scale);
    }

    glm::mat4 getModelMatrixNoRotation() const
    {
        return glm::translate(glm::mat4(1), position) * glm::scale(glm::mat4(1), scale);
    }

    /// Returns a local, relative position based on an offset
    /// Takes rotation into account
    glm::vec3 getRelativePosition(glm::vec3 const& offset) const { return position + (rotation * offset); }

    /// -- Miscellaneous --

    glm::vec3 getForwardVector() const
    {
        auto const rotMatrix = glm::toMat3(rotation);
        return glm::vec3(-rotMatrix[0][2], -rotMatrix[1][2], -rotMatrix[2][2]);
    }
    glm::vec3 getRightVector() const
    {
        auto const rotMatrix = glm::toMat3(rotation);
        return glm::vec3(rotMatrix[0][0], rotMatrix[1][0], rotMatrix[2][0]);
    }
    glm::vec3 getUpVector() const
    {
        auto const rotMatrix = glm::toMat3(rotation);
        return glm::vec3(rotMatrix[0][1], rotMatrix[1][1], rotMatrix[2][1]);
    }

public:
    /// -- Utility --

    /// Interpolation alpha helpers
    /// Usage: lerp(current, target, transform::xAlpha(dt, ..))

    /// Smoothed lerp alpha, framerate-correct
    static float smoothLerpAlpha(float smoothing, float dt) { return 1 - std::pow(smoothing, dt); }
    /// Exponential decay alpha, framerate-correct damp / lerp
    static float exponentialDecayAlpha(float lambda, float dt) { return 1 - std::exp(-lambda * dt); }
    /// alpha based on the halftime between current and target state
    static float halftimeLerpAlpha(float halftime, float dt) { return 1 - std::pow(.5f, dt / halftime); }
};

inline std::string to_string(transform const& t)
{
    std::stringstream ss;
    ss << "[" << glm::to_string(t.position) << ", " << glm::to_string(t.rotation) << ", " << glm::to_string(t.scale) << "]";
    return ss.str();
}

inline std::ostream& operator<<(std::ostream& stream, transform const& t)
{
    stream << to_string(t);
    return stream;
}

}
