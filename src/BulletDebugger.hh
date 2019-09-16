#pragma once

#include "btBulletDynamicsCommon.h"

#include <vector>

#include <glow/fwd.hh>

#include <glm/glm.hpp>

class BulletDebugger : public btIDebugDraw {
public:
  BulletDebugger();
  ~BulletDebugger();

  void draw(glm::mat4 pPVMatrix); // draw it with projection * view

  // btIDebugDraw API
  void drawLine(const btVector3 &from, const btVector3 &to, const btVector3 &color) override;
  void drawContactPoint(const btVector3 &, const btVector3 &, btScalar, int, const btVector3 &) override {}
  void draw3dText(const btVector3 &, const char *) override {}
  void reportErrorWarning(const char *) override;
  void setDebugMode(int p) override;
  int getDebugMode(void) const override;
  void clearLines() override;

private:
  struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
  };

  int mode = DBG_DrawWireframe;
  std::vector<Vertex> lines;
  glow::SharedProgram mLineShader;
};