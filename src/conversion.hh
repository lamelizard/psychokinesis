#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/vector_angle.hpp>

#include <btBulletDynamicsCommon.h>

inline glm::vec3 glcast(const btVector3 &v) {
  return glm::vec3(v.x(), v.y(), v.z());
}
inline btVector3 btcast(const glm::vec3 &v) {
  return btVector3(v.x, v.y, v.z);
}
inline btTransform bttransform(const glm::vec3 &offset) {
  auto identity = btMatrix3x3();
  identity.setIdentity();
  return btTransform(identity, btcast(offset));
}
inline glm::vec3 getWorldPos(const btTransform &t) {
  return glcast(t.getOrigin());
}
inline glm::vec3 translation(const glm::mat4 &m) { return glm::vec3(m[3]); }
inline glm::mat4 rotate(const glm::mat4 &m, glm::vec3 to, glm::vec3 from = glm::vec3(0, 0, 1)){ // only optimized for z = forward
        auto angle = glm::angle(to, from);
        auto cross = glm::normalize(-glm::cross(to, from));
        if(isnan(glm::length(cross))) // couldn't get cross product
            cross = glm::vec3(to.x, -to.z, to.y);
        return glm::rotate(m, angle, cross);
}
