#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <glow/fwd.hh>

#include "assimpModel.hh"

#include <btBulletDynamicsCommon.h>

#include <glm/glm.hpp>

// https://www.khronos.org/opengl/wiki/Skeletal_Animation

struct Mech {
  enum mechType {
    player = 0,
    small = 1,
    big = 2
  };
  enum animation {
    // I WANTS MOAR
    run,
    walk,
    getup,
    hit
  };
  enum part {
    bottom = 0,
    top = 1
  };

  mechType type;
  glm::vec3 moveDir;
  glm::vec3 viewDir;
  glm::vec3 meshOffset;
  double scale = 1;


  std::shared_ptr<btCapsuleShape> collision;
  std::shared_ptr<btDefaultMotionState> motionState;
  std::shared_ptr<btRigidBody> rigid;


  animation animation[2][2] = {{run, run}, {run, run}}; //bottom, top && a, b
  float animationAlpha[2] = {1, 1};                     // bottom and top
  double animationTime[2][2] = {{0, 0}, {0, 0}};        // bottom, top && a, b

  glow::SharedTexture2D texAlbedo;
  glow::SharedTexture2D texNormal;
  static SharedAssimpModel mesh;

  void Mech::updateTime(double delta);
  void draw(glow::UsedProgram &shader);
};
