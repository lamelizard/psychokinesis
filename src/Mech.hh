#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <glow/fwd.hh>

#include "assimpModel.hh"

#include <btBulletDynamicsCommon.h>

#include <glm/glm.hpp>

#include "SM.hh"

// https://www.khronos.org/opengl/wiki/Skeletal_Animation

// TODO create state machine -> make class to derive from? -> create it also for bullets n' stuff?

enum mechType { // got less to write if it's outside of Mech...
  player = 0,
  small = 1,
  big = 2
};

struct Mech : SM {
  enum animation {
      //30 tps
    run, //24
    runjump, // 24
    walk, // 50
    getup, // 34
    hit, // 50
    sbigA, // 15
    sbigB, // 15
    none
  };

  std::map<animation, bool> loops = {
    {run,true},
    {runjump,true},
    {walk,true},
    {getup,false},
    {hit,false},
  };
  std::map<animation, std::string> names = {
    {run,"Run_InPlace"},
    {runjump,"RunJump_InPlace"},
    {walk,"WalkInPlace"},
    {getup,"SleepToDefault"},
    //{getup,"DefaultToWalk"},
    {hit,"Hit"},
    {sbigA, "ShootBigCanon_A"},
    {sbigB, "ShootBigCanon_B"},
    {none, ""}
  };


  mechType type;
  int HP;
  glm::vec3 moveDir;
  glm::vec3 viewDir;
  glm::vec3 meshOffset;
  double floatOffset; // from bottom
  double scale = 1;


  std::shared_ptr<btCapsuleShape> collision;
  std::shared_ptr<btDefaultMotionState> motionState;
  std::shared_ptr<btRigidBody> rigid;


  animation animations[2] = {run, run}; // bottom, a, b
  animation animationTop = none;
  float animationAlpha = 1;
  double animationsTime[2] = {0, 0};        // bottom, top && a, b
  double animationTimeTop = 0;
  double animationsFaktor[2] = {1,1}; // don't need one for top
  double animationAngle = 0;
  void setAnimation(animation, animation, double bt = 0, double tt = 0);
  void setAnimation(animation, animation, animation, float ba = 1, double bta = 0, double btb = 0, double tt = 0);

  glow::SharedTexture2D texAlbedo;
  glow::SharedTexture2D texNormal;
  static SharedAssimpModel mesh;

  //void updateLogic();
  void updateTime(double delta);
  void draw(glow::UsedProgram &shader);
  glm::vec3 getPos();
  void setPosition(glm::vec3);

  //actions
  static void emptyAction(int);
  static void controlPlayer(int);
  static void startSmall(int);
  static void startBig(int);
  static void runSmall(int);
};


