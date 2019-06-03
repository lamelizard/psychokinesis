#include "Mech.hh"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <glow/common/log.hh>
#include <glow/objects/Program.hh>
#include <glow/objects/Texture2D.hh>

#include "conversion.hh"

#include "Game.hh" 

using namespace glow;

SharedAssimpModel Mech::mesh;

void Mech::updateLogic() {
}

void Mech::updateTime(double delta) {
  animationTime[bottom][0] += delta;
  animationTime[bottom][1] += delta;
  animationTime[top][0] += delta;
  animationTime[top][1] += delta;
}

void Mech::draw(glow::UsedProgram &shader) {
  glm::mat4 model;
  model = glm::translate(model, getPos());
  model = glm::rotate(model, acos(glm::dot(moveDir, glm::vec3(0, 0, 1))), glm::normalize(glm::cross(glm::vec3(0, 0, 1), moveDir)));
  model = glm::translate(model, meshOffset);
  model = glm::scale(model, glm::vec3(scale));
  //model *= glm::lookAt(glm::vec3(0, 0, 1), moveDir, glm::vec3(0, 1, 0));
  //model = glm::translate(model, meshOffset);

  shader.setUniform("uModel", model);
  shader.setTexture("uTexAlbedo", texAlbedo);
  shader.setTexture("uTexNormal", texNormal);

  mesh->draw(shader, animationTime[bottom][0], true, "WalkInPlace");
  //mechModel->draw(shader, debugTime, true, "Hit"); //"WalkInPlace");
  // skeleton
  // mechModel->debugRenderer.render(proj * view * glm::scale(glm::vec3(0.01)));
}

glm::vec3 Mech::getPos() {
  btTransform transform;
  motionState->getWorldTransform(transform);
  return getWorldPos(transform);
}
