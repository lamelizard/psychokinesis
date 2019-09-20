#include "Mech.hh"

#include <cmath>
#include <cassert>
#include <set>
#include <algorithm>
#include <utility>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include <glow/common/log.hh>
#include <glow/objects/Program.hh>
#include <glow/objects/Texture2D.hh>
#include <glow/objects/VertexArray.hh>

#include <GLFW/glfw3.h>

#include "conversion.hh"

#include "Game.hh"

using namespace std;
using namespace glow;

SharedAssimpModel Mech::mesh;

//main cannon (L): (-0.97,4.8,4.1)
//second cannon (L): (-1.32,4.52,3.5)

int Mech::nextGoal = 1;
int Mech::currentWay = 0;
const int Mech::timeNeeded = 3 * 60;
int Mech::reachGoalInTicks = timeNeeded;
glm::vec3 Mech::lastPosition = glm::vec3(0.5, 4.51, 18.5);

void Mech::setAnimation(Mech::animation ab, Mech::animation at, double bt, double tt) {
  animations[0] = ab;
  animationTop = at;
  animationAlpha = 0;
  animationsTime[0] = bt;
  animationTimeTop = tt;
  animationsFaktor[0] = 1;
}

void Mech::setAnimation(Mech::animation aba, Mech::animation abb, Mech::animation at, float ba, double bta, double btb, double tt) {
  animations[0] = aba;
  animations[1] = abb;
  animationTop = at;
  animationAlpha = ba;
  animationsTime[0] = bta;
  animationsTime[1] = btb;
  animationTimeTop = tt;
  animationsFaktor[0] = 1;
  animationsFaktor[1] = 1;
}

void Mech::walkAnimation(float speed) {
  //assert(speed >= 0 && speed <= 1); // fails, but what we get is good enough
  speed = max(0.f, min(1.f, speed));
  animations[1] = walk;
  animationsFaktor[1] = 2; // ...
  if (speed < .5) {
    animations[0] = startWalk;
    animationsFaktor[0] = 0;
    animationAlpha = speed * 2;
    if (speed < .1)
      animationsTime[1] = 0;
  } else {
    animations[0] = run;
    animationsFaktor[0] = animationsFaktor[1] * 24. / 50.;
    animationsTime[0] = animationsTime[1] * 24. / 50.;
    animationAlpha = (1 - speed) * 2;
  }
}

void Mech::updateTime(double delta) {
  animationsTime[0] += delta * animationsFaktor[0];
  animationsTime[1] += delta * animationsFaktor[1];
  animationTimeTop += delta * 1; // lame
  blink += delta;
}

void Mech::updateLook() {
  drawPos = getPos(); // fix position
}

float Mech::getAngleMove() {
  moveDir = normalize(moveDir); // ?
  auto angleMove = acos(dot(moveDir, glm::vec3(0, 0, 1)));
  if (moveDir.x < 0)
    angleMove = -angleMove;
  return angleMove;
}

float Mech::getAngleView() {
  //auto angle = glm::angle(moveDir, glm::vec3(0, 0, 1));
  viewDir = normalize(viewDir);
  auto angleView = acos(dot(viewDir, glm::vec3(0, 0, 1)));
  if (viewDir.x < 0)
    angleView = -angleView;
  angleView -= getAngleMove(); //relative
  return angleView;
}

glm::mat4 Mech::getModelMatrix() {
  glm::mat4 model;
  model = glm::translate(model, drawPos);
  //rotate
  model = glm::rotate(model, getAngleMove(), glm::vec3(0, 1, 0));
  model = glm::translate(model, meshOffset);
  model = glm::scale(model, glm::vec3(scale));
  //model = glm::translate(model, meshOffset);
  return model;
}

void Mech::draw(glow::UsedProgram &shader) {
  shader.setUniform("uBlink", (blink < 1 && fmod(blink, .2) > .1));
  shader.setUniform("uModel", getModelMatrix());
  shader.setTexture("uTexAlbedo", texAlbedo);
  shader.setTexture("uTexNormal", texNormal);
  shader.setTexture("uTexMaterial", texMaterial);


  //mesh->draw(shader, animationsTime[0], loops[animations[0]], names[animations[0]]);
  auto g = Game::instance;
  if (!g->DebugingAnimations)
    bones = mesh->getMechBones(names[animations[0]], names[animations[1]], names[animationTop], animationAlpha, animationsTime[0], animationsTime[1], animationTimeTop, getAngleView());
  else
    bones = mesh->getMechBones(names[(animation)g->debugAnimations[0]], names[(animation)g->debugAnimations[1]], names[(animation)g->debugAnimations[2]], //
                               g->debugAnimationAlpha, g->debugAnimationTimes[0], g->debugAnimationTimes[1], g->debugAnimationTimes[2], g->debugAnimationAngle);

  shader.setUniform("uBones[0]", MAX_BONES, bones.data()); // really, uBones[0] instead of uBones...

  mesh->getVA()->bind().draw();

  //mechModel->draw(shader, debugTime, true, "Hit"); //"WalkInPlace");
  // skeleton
  // mechModel->debugRenderer.render(proj * view * glm::scale(glm::vec3(0.01)));
}

glm::vec3 Mech::getPos() {
  btTransform transform;
  motionState->getWorldTransform(transform);
  return getWorldPos(transform);
}

void Mech::setPosition(glm::vec3 pos) {
  //assert(!rigid->isKinematicObject());//works???
  rigid->setLinearVelocity(btVector3(0, 0, 0));
  rigid->clearForces();
  btTransform transform;
  transform.setIdentity();
  transform.setOrigin(btcast(pos));

  rigid->setWorldTransform(transform);
  motionState->setWorldTransform(transform);
}

void Mech::controlPlayer(int) {
  auto g = Game::instance;
  auto &m = g->mechs[player];
  const auto playerPos = m.getPos();
  const auto bulPos = btcast(playerPos);

  m.rigid->setDamping(g->damping, 0); //damps y... //TODO damp manually

  // reset position if fall
  if (playerPos.y < -.5) {
    //should change action here
    m.HP--;
    m.blink = 0;
    m.setPosition(glm::vec3(0, 2, -10));
  }

  // modes of player, they change the logic
  // could change mode for any entity!!!
  set<Mode> playerModes;
  {
    auto area = entityx::ComponentHandle<ModeArea>();
    for (auto entity : g->ex.entities.entities_with_components(area))
      if (glm::distance(area->pos, playerPos) < area->radius)
        playerModes.insert(area->mode);
  }

  // Physics
  // Bullet uses fixed timestep and interpolates
  // -> probably should put this in render as well to get the interpolation
  // move character
  {
    auto maxSpeed = 5;
    m.rigid->setLinearFactor(btVector3(1, 1, 1));
    m.rigid->setGravity(btVector3(0, -9.81, 0));

    {
      // music neon
      {
        static bool musicWobbly = false;
        if (playerModes.count(neon)) {
          if (!musicWobbly) {
            musicWobbly = true;
            g->soloud->oscillateRelativePlaySpeed(g->musicHandle, .98, 1.02, 1);
          }
        } else if (musicWobbly) {
          musicWobbly = false;
          g->soloud->fadeRelativePlaySpeed(g->musicHandle, 1, 2);
        }
      }
      // handle slowdown
      static bool musicSlow = true;
      if (playerModes.count(drawn)) {
        //maxSpeed = 3;
        m.rigid->setLinearFactor(btVector3(.7, .7, .7));
        if (!musicSlow) {
          musicSlow = true;
          g->soloud->fadeRelativePlaySpeed(g->musicHandle, .7, 2);
        }
      } else if (musicSlow) {
        musicSlow = false;
        g->soloud->fadeRelativePlaySpeed(g->musicHandle, 1, 2);
      }
    }


    //movement
    {
      GLFWgamepadstate gamepadState;
      bool hasController = glfwJoystickIsGamepad(GLFW_JOYSTICK_1) && glfwGetGamepadState(GLFW_JOYSTICK_1, &gamepadState);


      //jump
      bool jumpPressed = g->isKeyPressed(GLFW_KEY_SPACE) ||                                   //
                         (hasController && (                                                  //
                                               gamepadState.buttons[GLFW_GAMEPAD_BUTTON_A] || //
                                               gamepadState.buttons[GLFW_GAMEPAD_BUTTON_B] || //
                                               gamepadState.buttons[GLFW_GAMEPAD_BUTTON_X] || //
                                               gamepadState.buttons[GLFW_GAMEPAD_BUTTON_Y]));
      if (playerModes.count(neon))
        jumpPressed = true;

      // reldir = dir without camera
      glm::vec3 relDir;
      {
        if (g->isKeyPressed(GLFW_KEY_LEFT) || (g->isKeyPressed(GLFW_KEY_A) && !g->mFreeCamera))
          relDir.x -= 1;
        if (g->isKeyPressed(GLFW_KEY_RIGHT) || (g->isKeyPressed(GLFW_KEY_D) && !g->mFreeCamera))
          relDir.x += 1;
        if (g->isKeyPressed(GLFW_KEY_UP) || (g->isKeyPressed(GLFW_KEY_W) && !g->mFreeCamera))
          relDir.z += 1;
        if (g->isKeyPressed(GLFW_KEY_DOWN) || (g->isKeyPressed(GLFW_KEY_S) && !g->mFreeCamera))
          relDir.z -= 1;

        /*if (glm::length(relDir) > .1f)
          glm::normalize(relDir);
        else*/
        if (glm::length(relDir) < 0.1 && hasController) { // a.k.a. 0
          relDir = glm::vec3(limitAxis(gamepadState.axes[GLFW_GAMEPAD_AXIS_LEFT_X]), 0, limitAxis(-gamepadState.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]));
        }
        relDir /= max(1.f, length(relDir)); // no fast diagonal walk, we aint goldeneye
      }


      // movement relative to camera
      glm::vec3 camForward;
      glm::vec3 camRight;
      {
        camForward = g->mCamera->getForwardVector();
        camForward.y = 0;
        camForward = glm::normalize(camForward);
        camRight = g->mCamera->getRightVector();
        camRight.y = 0;
        camRight = glm::normalize(camRight);
      }

      m.viewDir = camForward;
      auto force = (relDir.x * camRight + relDir.z * camForward) * g->moveForce;
      // disco:
      {
        static float discoAlpha = 0;
        static float discoRot = M_PI / 2;
        if (playerModes.count(disco))
          discoAlpha = min(discoAlpha + .02f, 1.f);
        else
          discoAlpha = max(discoAlpha - .02f, 0.f);
        //discoRot = fmod(discoRot + .001, M_PI * 2);
        m.viewDir = glm::rotateY(m.viewDir, discoRot * discoAlpha);
        force = glm::rotateY(force, discoRot * discoAlpha);
      }
      if (glm::length(force) > 0.001) {
        m.moveDir = glm::normalize(force);
        m.rigid->applyCentralForce(btcast(force));
      }


      auto btSpeed = m.rigid->getLinearVelocity();
      auto ySpeed = btSpeed.y();
      btSpeed.setY(0);
      // but what about the forces?
      if (btSpeed.length() > maxSpeed)
        btSpeed = btSpeed.normalize() * maxSpeed;
      btSpeed.setY(ySpeed);
      m.rigid->setLinearVelocity(btSpeed);

      // pushed away by small
      auto spos = g->mechs[small].getPos();
      auto small2Player = (playerPos - spos) * glm::vec3(1, 0, 1);
      if (!g->secondPhase && length(small2Player) < 3)
        m.rigid->applyCentralImpulse(btcast(normalize(small2Player) * 20));


      // close to ground?
      bool closeToGround = false;
      float ground = 0; // valid if closeToGround
      float closeToGroundBorder = m.floatOffset * 1.2;
      vector<float> results;
      for (int angle = -3; angle < 3; angle += 1) {
        auto from = bulPos - btVector3(sin(angle) * .3, m.collision->getHalfHeight() + m.collision->getRadius(), cos(angle) * .3); //heigth is not the height...
        if (from.y() <= 0.05 && from.y() > -0.01)                                                                                  //stuck slighlty in ground...
          from.setY(0.1);
        auto to = from - btVector3(0, closeToGroundBorder, 0);
        g->dynamicsWorld->getDebugDrawer()->drawLine(from, to, btVector4(1, 0, 0, 1));
        auto closest = btCollisionWorld::ClosestRayResultCallback(from, to);
        g->dynamicsWorld->rayTest(from, to, closest);
        if (closest.hasHit())
          results.push_back(closest.m_hitPointWorld.y());
      }
      if (results.size() > 3) { // half hit
        closeToGround = true;
        sort(results.begin(), results.end());
        ground = results[results.size() / 2];
      }

      // jumping
      if (closeToGround) {
        // falling + standing
        if (m.rigid->getLinearVelocity().y() <= 0.01 /*&& !jumpPressed*/) {
          g->mJumps = false;
          // reset y-velocity
          auto v = m.rigid->getLinearVelocity();
          v.setY(0);
          m.rigid->setLinearVelocity(v);
          // reset y-position
          auto t = m.rigid->getWorldTransform();
          auto o = t.getOrigin();
          o.setY(ground + m.floatOffset + m.collision->getHalfHeight() + m.collision->getRadius());
          t.setOrigin(o);
          m.rigid->setWorldTransform(t);
          // no y-movement anymore!
          m.rigid->setLinearFactor(btVector3(1, 0, 1));
          //sound
          if (m.didStep)
            g->soloud->play3d(g->sfxStep, o.x(), o.y(), o.z(), 0, 0, 0, .07);
          m.walkAnimation(btSpeed.length() / maxSpeed); // walk
        }
        // jump
        if (!g->mJumps && jumpPressed) {
          g->mJumps = true;
          m.rigid->setLinearFactor(btVector3(1, 1, 1));
          m.rigid->applyCentralForce(btVector3(0, 500, 0));
          m.setAnimation(getup, runjump, none, max(.5, min(1., (double)btSpeed.length() / maxSpeed)), 0, 0, 0);
          m.animationsFaktor[0] = 0;
          m.animationsFaktor[1] = 1.8;
        }
      } else { // !close to ground
        m.rigid->setLinearFactor(btVector3(1, 1, 1));
        if (m.animations[1] == runjump) {
          m.animationAlpha = max(.5, min(1., (double)btSpeed.length() / maxSpeed));
          if (m.animationsTime[1] > 24. / 30.) {
            m.animationsTime[1] = 24. / 30.;
            m.animationsFaktor[1] = 0;
          }
        }
      }

      //gravity according to jump
      //https://www.youtube.com/watch?v=7KiK0Aqtmzc
      if (g->mJumps) {
        if (jumpPressed)
          m.rigid->setGravity(btVector3(0, 1, 0) * g->jumpGravityHigh);
        else
          m.rigid->setGravity(btVector3(0, 1, 0) * g->jumpGravityLow);
      } else
        m.rigid->setGravity(btVector3(0, 1, 0) * g->jumpGravityFall);
    }
  }
}

void Mech::emptyAction(int) {}

void Mech::startSmall(int t) {
  auto g = Game::instance;
  auto &m = g->mechs[small];
  m.moveDir = glm::vec3(-1, 0, 0);
  if (t <= 6 * 60)
    m.setAnimation(getup, none);
  if (t == 6 * 60) {
    auto pos = m.getPos();
    g->soloud->play3d(g->sfxBootUp, pos.x, pos.y, pos.z);
  }

  if (t > 6 * 60 + 68 + 60) { //getup finished + 1s
    //m.setAnimation(run, none);
    m.setAction(runSmall);
  }
}

void Mech::startPlayer(int t) {
  auto g = Game::instance;
  auto &m = g->mechs[player];
  m.moveDir = glm::vec3(0, 0, 1);
  auto pos = m.getPos();
  m.setPosition(glm::vec3(pos.x, m.collision->getHalfHeight() + m.collision->getRadius() + m.floatOffset, pos.y));
  //pos = m.getPos(); // should not break a thing
  if (t == 0) {
    m.setAnimation(getup, none);
    m.animationsFaktor[0] = 0;
    g->mCameraLocked = true;
  }
  if (!g->mFreeCamera) {
    //g->mCamera->setLookAt({.5, 3, -13}, {.5, 1.5, -10});
    g->mRotHandle.setLookAt({0, 0, -2}, {0, 0, 0});
    g->mRotHandle.snap();
    //g->mCamera->handle.setLookAt(glm::vec3{0, pos.y, 1} + g->mRotHandle.getPosition(), glm::vec3{0, pos.y, 1});
    g->mCamera->handle.setLookAt(pos + g->mRotHandle.getPosition(), pos);
    g->mCamera->handle.snap();
  }
  if (t == 2 * 60) {
    m.animationsFaktor[0] = 1;
    g->soloud->play3d(g->sfxBootUp, pos.x, pos.y, pos.z, 0, 0, 0, .15);
  }

  if (t > 2 * 60 + 68) { //getup finished
    //m.setAnimation(startWalk, none);
    m.animationsFaktor[0] = 0;
    g->mCameraLocked = false;
    m.setAction(controlPlayer);
  }
}

void Mech::startBig(int t) {
  auto g = Game::instance;
  auto &m = g->mechs[big];
  auto &p = g->mechs[player];
  if (t == 0) {
    m.scale = 8.5;
    m.setAnimation(sbigA, none);
  }
  m.viewDir = glm::vec3(0, 0, -1); //normalize((p.getPos() - m.getPos()) * glm::vec3(1,0,1));
  m.moveDir = glm::vec3(0, 0, -1);
  m.setPosition(glm::vec3(.5, -40 - (1. / 3. * (300 - t)), 60));
  if (t == 300) {
    m.HP = -1;
    m.setAction(runBig);
  }
}

enum bigAct {
  shootArc,
  shootHoming,
  shootFalling,
  noneAct
};

void Mech::runBig(int t) {
  auto g = Game::instance;
  auto &m = g->mechs[big];
  auto &p = g->mechs[player];

  static const int acttime = 360;
  static const auto dl = normalize(glm::vec3(1, 0, -1));
  static const auto dr = normalize(glm::vec3(-1, 0, -1));
  static const auto df = glm::vec3(0, 0, -1);

  // if we want to shoot:
  auto cpos1 = glm::vec3(m.getModelMatrix() * (m.bones[mesh->getMechBoneID("BigCanon01_L")] * glm::vec4(-0.97, 4.1 + .2, -4.8, 1.)));
  auto cpos2 = glm::vec3(m.getModelMatrix() * (m.bones[mesh->getMechBoneID("BigCanon01_R")] * glm::vec4(0.97, 4.1 + .2, -4.8, 1.)));


  if (m.HP > 3)
    if (t % (180) == 0)
      g->createRocket(p.getPos() + glm::vec3(0, 15, 0), glm::vec3(0, -1, 0), rtype::falling);

  static int tnow = 0;
  tnow++;
  static const vector<pair<bigAct, int>> actionList = {
      {noneAct, 1},
      {shootArc, 1},
      {shootArc, 2},
      {shootHoming, 1},
      {shootFalling, 1},
      {shootArc, 1},
      {shootArc, 2},
      {shootHoming, 2},
      {noneAct, 1},
      {shootHoming, 1},
      {noneAct, 1},
      {shootArc, 3},
      {shootArc, 1},
      {shootArc, 2},
      {noneAct, 1},
      {shootHoming, 3},
      {shootHoming, 3},
      {noneAct, 1},
      {shootFalling, 1},
      {shootHoming, 3},
      {shootArc, 3},
      {shootArc, 3},
      {shootArc, 3},
  };
  static bigAct thisAct = noneAct;
  static int attr = 1;

  if (t == 0)
    m.HP = -1;
  if (t % acttime == 0) {
    m.viewDir = df;
    m.HP++;
    tnow = 0;
    if (m.HP >= actionList.size()) {
      m.setAction(dieBig);
      return;
    } else {
      thisAct = actionList[m.HP].first;
      attr = actionList[m.HP].second;
      g->createRocket(cpos1, m.viewDir * 12, rtype::forward);
      g->soloud->play3d(g->sfxShot, cpos1.x, cpos1.y, cpos1.z);
      g->createRocket(cpos2, m.viewDir * 12, rtype::forward);
      g->soloud->play3d(g->sfxShot, cpos2.x, cpos2.y, cpos2.z);
      m.setAnimation(sbigA, none);
      m.animationsFaktor[0] = 1;
      if (m.HP == actionList.size() - 5)
        g->soloud->play(g->speechHeat);
      if (m.HP > actionList.size() - 6)
        m.blink = 0;
    }
  }

  switch (thisAct) {
  case shootArc: {
    static auto startDir = glm::vec3();
    if (tnow < 60) {
      float a = 1 - (float)(59 - tnow) / 59.;
      m.viewDir = rotateY(df, angle(df, dl) * (-a));
    } else if (tnow < 180) {
      float a = 1 - (float)(120 - (tnow - 60)) / 120.;
      m.viewDir = rotateY(dl, angle(dl, dr) * a);
      if (tnow % 5 == 0) {
        if (attr & 1) {
          g->createRocket(cpos1, m.viewDir * 12, rtype::forward);
          g->soloud->play3d(g->sfxShot, cpos1.x, cpos1.y, cpos1.z, 0, 0, 0, .5);
        }
        if (attr & 2) {
          g->createRocket(cpos2, m.viewDir * 12, rtype::forward);
          g->soloud->play3d(g->sfxShot, cpos2.x, cpos2.y, cpos2.z, 0, 0, 0, .5);
        }
        //m.setAnimation(sbigA, none);
        //m.animationsFaktor[0] = 2;
      }
    } else if (tnow < 300) {
      float a = 1 - (float)(120 - (tnow - 180)) / 120.;
      m.viewDir = rotateY(dr, angle(dr, dl) * (-a));
      if (tnow % 5 == 0) {
        if (attr & 1) {
          g->createRocket(cpos1, m.viewDir * 12, rtype::forward);
          g->soloud->play3d(g->sfxShot, cpos1.x, cpos1.y, cpos1.z, 0, 0, 0, .5);
        }
        if (attr & 2) {
          g->createRocket(cpos2, m.viewDir * 12, rtype::forward);
          g->soloud->play3d(g->sfxShot, cpos2.x, cpos2.y, cpos2.z, 0, 0, 0, .5);
        }
        //m.setAnimation(sbigA, none);
        //m.animationsFaktor[0] = 2;
      }
    } else if (tnow < 360) {
      float a = 1 - (float)(60 - (tnow - 300)) / 59.;
      m.viewDir = rotateY(dl, angle(dl, df) * a);
    }

    break;
  }
  case shootHoming:
    if (tnow == 60) {
      if (attr & 1) {
        g->createRocket(cpos1, m.viewDir * 4, rtype::homing);
        g->soloud->play3d(g->sfxShot, cpos1.x, cpos1.y, cpos1.z);
      }
      if (attr & 2) {
        g->createRocket(cpos2, m.viewDir * 4, rtype::homing);
        g->soloud->play3d(g->sfxShot, cpos2.x, cpos2.y, cpos2.z);
      }
      m.setAnimation(sbigA, none);
      m.animationsFaktor[0] = 1;
    }
    break;
  case shootFalling:
    if (tnow == 0)
      for (int x = CUBES_MIN; x <= CUBES_MAX; x += 5)
        for (int y = CUBES_MIN; y <= CUBES_MAX; y += 5)
          g->createRocket(glm::vec3(x, 10 + (rand() % 5), y), glm::vec3(0, -1, 0), rtype::falling);
    break;
  default:;
  }
}

void Mech::dieBig(int t) {
  auto g = Game::instance;
  auto &m = g->mechs[big];
  // stop music
  if (t == 0) {
    g->soloud->fadeVolume(g->musicHandle, 0, 5);
    m.setAnimation(hit, none);
    m.animationsFaktor[0] = .3;
  }
  if (m.blink > 1)
    m.blink = 0;
  if (t > 240)
    m.setPosition(glm::vec3(.5, -40 - (1. / 3. * (60 - (t - 240))), 60));

  if (t > 300) {
    // fin
    g->fin = true;
    m.setAction(emptyAction);
    g->soloud->play(g->outro);
  }
}

void Mech::runSmall(int t) {
  auto g = Game::instance;
  auto &m = g->mechs[small];
  auto &p = g->mechs[player];
  if (m.rigid->getUserIndex2() == SMALL_GOTHIT) {
    m.HP--;
    m.blink = 0;
    m.rigid->setUserIndex2(SMALL_NONE);
  }

  if (m.HP <= 1) {
    m.setAction(dieSmall);
    return;
  }

  auto groundOffset = glm::vec3(0, m.collision->getHalfHeight() + m.collision->getRadius() + m.floatOffset, 0);
  auto pos = m.getPos();

  // if we want to shoot:
  glm::vec3 cpos;
  if (rand() % 2 == 0)
    //ARG UNITY: z = -y, y = z, +.2 offset?
    cpos = glm::vec3(m.getModelMatrix() * (m.bones[mesh->getMechBoneID("BigCanon01_L")] * glm::vec4(-0.97, 4.1 + .2, -4.8, 1.)));
  else
    cpos = glm::vec3(m.getModelMatrix() * (m.bones[mesh->getMechBoneID("BigCanon01_R")] * glm::vec4(0.97, 4.1 + .2, -4.8, 1.)));
  auto shotDir = glm::normalize(p.getPos() - cpos);


  // one waypoint to another are 18 steps
  static const vector<glm::vec3> ways[4] = {{{0.5, 0, 18.5},
                                             {-17.5, 0, 18.5},
                                             {-17.5, 0, 0.5},
                                             {0.5, 0, 0.5},
                                             {18.5, 0, 0.5},
                                             {18.5, 0, -17.5},
                                             {0.5, 0, -17.5},
                                             {0.5, 0, 0.5}},
                                            {{18.5, 0, 0.5},
                                             {18.5, 0, 18.5},
                                             {0.5, 0, 18.5},
                                             {0.5, 0, 0.5},
                                             {-17.5, 0, 0.5},
                                             {-17.5, 0, -17.5},
                                             {0.5, 0, -17.5},
                                             {0.5, 0, 0.5}},
                                            {{-17.5, 0, 0.5},
                                             {-17.5, 0, 18.5},
                                             {-17.5, 0, 0.5},
                                             {-17.5, 0, -17.5},
                                             {-17.5, 0, 0.5},
                                             {0.5, 0, 0.5},
                                             {18.5, 0, 0.5},
                                             {0.5, 0, 0.5}},
                                            {{0.5, 0, 18.5},
                                             {-17.5, 0, 18.5},
                                             {-17.5, 0, 0.5},
                                             {-17.5, 0, -17.5},
                                             {0.5, 0, -17.5},
                                             {18.5, 0, -17.5},
                                             {18.5, 0, 0.5},
                                             {18.5, 0, 18.5},
                                             {0.5, 0, 18.5},
                                             {0.5, 0, 0.5}}};

  //rocket
  if (t % (30 + ((m.HP - 1) * 30)) == 0 && m.bones.size() && glm::distance(pos, glm::vec3{0.5, 0, 0.5} + groundOffset) > 3) {
    g->createRocket(cpos, shotDir * 10, rtype::forward);
    g->soloud->play3d(g->sfxShot, cpos.x, cpos.y, cpos.z);
    //m.animationTop = sbigA;
    //m.animationTimeTop = 0;
  }


  //move somewhere else
  //falling
  if (m.HP < 4) // start at 5
    if (t % (180 + ((m.HP) * 30)) == 0)
      g->createRocket(p.getPos() + glm::vec3(0, 10, 0), glm::vec3(0, -1, 0), rtype::falling);

  //walk
  auto way = ways[currentWay];
  auto futurePos = way[nextGoal] + groundOffset;

  if (reachGoalInTicks == 0) {
    nextGoal++;
    if (nextGoal == way.size())
      nextGoal = 0;
    lastPosition = futurePos;
    if (futurePos == glm::vec3{0.5, 0, 0.5} + groundOffset) {
      if (currentWay < 5 - m.HP)
        currentWay++;
      else {
        //homing rocket
        auto entity = g->createRocket(cpos, shotDir * 4, rtype::homing);
        //TODO need default animation...
        m.setAction(waitSmall); // may get overridden
      }
    }

    way = ways[currentWay];
    futurePos = way[nextGoal];
    reachGoalInTicks = timeNeeded;
    auto futureMoveDir = normalize(futurePos - pos);


    //jump
    auto angle = glm::angle(m.moveDir, futureMoveDir);
    if (dot(cross(glm::vec3(0, 1, 0), m.moveDir), futureMoveDir) < 0)
      angle *= -1;
    if (angle > .5 || angle < -.5) {
      //m.setAnimation(runjump, none);
      //m.animationsFaktor[0] = 24./60.;
      m.setAction([angle](int ticks) {
        // looks clunky, but it IS a robot, so...
        static const auto ticksNeeded = 55;
        auto g = Game::instance;
        auto &m = g->mechs[small];
        auto &p = g->mechs[player];
        m.viewDir = glm::normalize(p.getPos() - m.getPos());

        //animate
        if (ticks == 0) {
          m.setAnimation(getup, none, 30. / 30.); // end is 34
          m.animationsFaktor[0] = -1.8;
        }
        if (ticks == 10) // 17
          m.animationsFaktor[0] = 1.8;
        if (ticks == 20)
          m.animationsFaktor[0] = 0;

        //landing
        if (ticks == 45) {
          auto pos = m.rigid->getWorldTransform().getOrigin();
          m.animationsFaktor[0] = -1.5;
          g->soloud->play3d(g->sfxLand, pos.x(), pos.y(), pos.z());
        }
        if (ticks == 50)
          m.animationsFaktor[0] = 1.5;

        //rotate
        if (ticks >= 20 && ticks < 40)
          m.moveDir = glm::rotate(m.moveDir, angle / 20, glm::vec3(0, 1, 0));

        // up and down
        auto trans = m.rigid->getWorldTransform();
        if (ticks >= 15 && ticks < 30)
          trans.setOrigin(trans.getOrigin() + btVector3(0, .1, 0));
        if (ticks >= 30 && ticks < 45)
          trans.setOrigin(trans.getOrigin() - btVector3(0, .1, 0));
        m.motionState->setWorldTransform(trans);
        m.rigid->setActivationState(DISABLE_DEACTIVATION);

        //stop
        if (ticks >= ticksNeeded) {
          //m.setAnimation(run, none);
          //m.animationsFaktor[0] = 1;
          if (Game::instance->hasHoming())
            m.setAction(waitSmall);
          else
            m.setAction(runSmall);
        }
      });
      return;
    }
  }

  auto smooth = glm::smoothstep(0., 1., (double)(timeNeeded - reachGoalInTicks) / timeNeeded);
  auto nextStepPos = lastPosition + (futurePos - lastPosition) * smooth;
  auto speed = glm::length(pos - nextStepPos) * 5 - .1; // factor to make it look right
  m.walkAnimation(speed);
  if (m.didStep)
    g->soloud->play3d(g->sfxStep, pos.x, pos.y, pos.z, 0, 0, 0, .3);
  //auto nextStepPos = glm::smoothstep(lastPosition, futurePos, glm::vec3((timeNeeded - reachGoalInTicks) / timeNeeded));
  m.moveDir = glm::normalize(futurePos - pos);
  m.viewDir = glm::normalize(p.getPos() - pos);
  assert(m.rigid->isKinematicObject());
  //https://pybullet.org/Bullet/phpBB3/viewtopic.php?t=11530
  //m.motionState->m_graphicsWorldTrans.setOrigin(btcast(nextStepPos));
  btTransform trans;
  trans.setIdentity();
  trans.setOrigin(btcast(nextStepPos));
  //m.rigid->setWorldTransform(trans);
  m.motionState->setWorldTransform(trans);
  m.rigid->setActivationState(DISABLE_DEACTIVATION);
  reachGoalInTicks--;
}

void Mech::waitSmall(int) {
  auto &m = Game::instance->mechs[small];
  auto &p = Game::instance->mechs[player];
  m.viewDir = glm::normalize(p.getPos() - m.getPos());
  if (!Game::instance->hasHoming())
    m.setAction(runSmall);
}

void Mech::dieSmall(int t) {
  auto g = Game::instance;
  auto &m = g->mechs[small];
  // stop music
  if (t == 0)
    g->soloud->fadeVolume(g->musicHandle, 0, 5);
  if (m.blink > 1)
    m.blink = 0;
  //explode
  auto pos = glm::vec3(m.getModelMatrix() * (m.bones[mesh->getMechBoneID("Body")] * glm::vec4(0, 0, 0, 1.))) - m.meshOffset;
  auto p = pos + (glm::vec3(2, 4, 2) * g->spherePoints[rand() % 400]);
  g->explosions.push_back({p, 0});
  if (t % 10 == 0)
    g->soloud->play3d(g->sfxExpl1, p.x, p.y, p.z, 0, 0, 0, .5);

  if (t > 300)
    g->initPhase2();
}
