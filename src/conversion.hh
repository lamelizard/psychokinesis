#pragma once

#include <glm/glm.hpp>
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