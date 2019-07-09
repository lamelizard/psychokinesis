#include "Mech.hh"

#include <cmath>
#include <cassert>
#include <set>
#include <algorithm>

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
glm::vec3 Mech::lastPosition = glm::vec3(0.5, 0, 18.5);

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

void Mech::walkAnimation(float speed){
//assert(speed >= 0 && speed <= 1); // fails, but what we get is good enough
speed = max(0.f, min(1.f, speed));
animations[1] = walk;
animationsFaktor[1] = 2; // ...
if(speed < .5){
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

void Mech::updateLook(){
    drawPos = getPos(); // fix position
}

float Mech::getAngleMove(){
    moveDir = normalize(moveDir); // ?
    auto angleMove = acos(dot(moveDir, glm::vec3(0,0,1)));
    if(moveDir.x < 0)
        angleMove = -angleMove;
    return angleMove;
}

float Mech::getAngleView(){
    //auto angle = glm::angle(moveDir, glm::vec3(0, 0, 1));
    viewDir = normalize(viewDir);
    auto angleView = acos(dot(viewDir, glm::vec3(0,0,1)));
    if(viewDir.x < 0)
        angleView = -angleView;
    angleView -= getAngleMove();//relative
    return angleView;
}

glm::mat4 Mech::getModelMatrix(){
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
  if(!g->DebugingAnimations)
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
  
  assert(!rigid->isKinematicObject());
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
    // handle slowdown
    {
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
      if(playerModes.count(neon))
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
        else*/ if (glm::length(relDir) < 0.1 && hasController) {// a.k.a. 0
            relDir = glm::vec3(limitAxis(gamepadState.axes[GLFW_GAMEPAD_AXIS_LEFT_X]), 0, limitAxis(-gamepadState.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]));

        }
        relDir /= max(1.f, length(relDir));// no fast diagonal walk, we aint goldeneye

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
          if(playerModes.count(disco))
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
      m.walkAnimation(btSpeed.length() / maxSpeed);

      // pushed away by small
      auto spos = g->mechs[small].getPos();
      auto small2Player = (playerPos - spos) * glm::vec3(1,0,1);
      if(length(small2Player) < 3)
          m.rigid->applyCentralImpulse(btcast(normalize(small2Player) * 20));


      // close to ground?
      bool closeToGround = false;
      float ground = 0; // valid if closeToGround
      float closeToGroundBorder = m.floatOffset * 1.2;
      vector<float> results;
      for(int angle = -3; angle < 3; angle += 1){
        auto from = bulPos - btVector3(sin(angle)*.3, m.collision->getHalfHeight() + m.collision->getRadius(), cos(angle)*.3); //heigth is not the height...
        if (from.y() <= 0.05 && from.y() > -0.01)                                                          //stuck slighlty in ground...
          from.setY(0.1);
        auto to = from - btVector3(0, closeToGroundBorder, 0);
        g->dynamicsWorld->getDebugDrawer()->drawLine(from, to, btVector4(1, 0, 0, 1));
        auto closest = btCollisionWorld::ClosestRayResultCallback(from, to);
        g->dynamicsWorld->rayTest(from, to, closest);
        if (closest.hasHit())
          results.push_back(closest.m_hitPointWorld.y());
      }
      if(results.size() > 3){ // half hit
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
          if(m.didStep)
              g->soloud->play3d(g->sfxStep, o.x(), o.y(), o.z(), 0,0,0, .07);
        }
        // jump
        if (!g->mJumps && jumpPressed) {
          g->mJumps = true;
          m.rigid->setLinearFactor(btVector3(1, 1, 1));
          m.rigid->applyCentralForce(btVector3(0, 500, 0));
        }
      } else
        m.rigid->setLinearFactor(btVector3(1, 1, 1));
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
  m.moveDir = glm::vec3(-1,0,0);
  if (t <= 6 * 60)
    m.setAnimation(getup, none);
  if (t == 6 * 60) {
    auto pos = m.getPos();
    g->soloud->play3d(g->sfxBootUp, pos.x, pos.y, pos.z);
  }

  if (t > 6 * 60 + 68 + 60) { //getup finished + 1s
    m.setAnimation(run, none);
    m.setAction(runSmall);
  }
}

void Mech::startPlayer(int t) {
  auto g = Game::instance;
  auto &m = g->mechs[player];
  m.moveDir = glm::vec3(0,0,1);
  auto pos = m.getPos();
  m.setPosition(glm::vec3(pos.x, m.collision->getHalfHeight() + m.collision->getRadius() + m.floatOffset, pos.y));
  if(t == 0){
      m.setAnimation(getup, none);
      m.animationsFaktor[0] = 0;
  }
  if (t == 2 * 60) {
    m.animationsFaktor[0] = 1;
    g->soloud->play3d(g->sfxBootUp, pos.x, pos.y, pos.z);
  }

  if (t > 2 * 60 + 68) { //getup finished
    m.setAnimation(startWalk, none);
    m.animationsFaktor[0] = 0;
    m.setAction(controlPlayer);
  }
}

void Mech::startBig(int) {
  assert(0);
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

  auto groundOffset = glm::vec3(0, m.collision->getHalfHeight() + m.collision->getRadius() + m.floatOffset, 0);
  auto pos = m.getPos();

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
  if (t % (60 + ((m.HP) * 15)) == 0 && m.bones.size()) {
      glm::vec3 cpos;
      if(rand() % 2 == 0)
          //ARG UNITY: z = -y, y = z, +.2 offset?
          cpos = glm::vec3(m.getModelMatrix() * (m.bones[mesh->getMechBoneID("BigCanon01_L")] * glm::vec4(-0.97,4.1 + .2,-4.8,1.)));
      else
          cpos = glm::vec3(m.getModelMatrix() * (m.bones[mesh->getMechBoneID("BigCanon01_R")] * glm::vec4(0.97,4.1 + .2,-4.8,1.)));
    auto dir = glm::normalize(p.getPos() - cpos);
    g->createRocket(cpos, dir * 10, rtype::forward); // 4?
    g->soloud->play3d(g->sfxShot, cpos.x,cpos.y,cpos.z);
    //m.animationTop = sbigA;
    //m.animationTimeTop = 0;
  }

  //move somewhere else
  //falling
  if(m.HP < 4) // start at 5
      if (t % (60 + ((m.HP) * 30)) == 0)
        g->createRocket(p.getPos() + glm::vec3(0,10,0), glm::vec3(0,-1,0), rtype::falling);

  //walk
  auto way = ways[currentWay];
  auto futurePos = way[nextGoal] + groundOffset;
  // todo smoothstep the walking speed

  if (reachGoalInTicks == 0) {
    nextGoal++;
    if (nextGoal == way.size())
      nextGoal = 0;
    if (futurePos == glm::vec3{0.5, 0, 0.5} + groundOffset) {
      if (currentWay < 5 - m.HP)
        currentWay++;
      else {
        //homing rocket
        auto &p = g->mechs[player];
        auto dir = glm::normalize(p.getPos() - pos);
        auto entity = g->createRocket(pos - glm::vec3(0, 1, 0) + (dir * 3), dir * 4, rtype::homing); // y-1 since else likely to hit ground  //TODO higher acc? and constraints to limeit velocity -> better homing
        if (m.HP > 3) {                                                                                // wait to be easier
          //TODO need default animation...
          m.setAction([entity](int t) {
            if (!entity.valid())
              Game::instance->mechs[small].setAction(runSmall);
          });
        }
      }
    }

    way = ways[currentWay];
    futurePos = way[nextGoal];
    reachGoalInTicks = timeNeeded;
    auto futureMoveDir = normalize(futurePos - pos);

    //jump
    auto angle = glm::angle(m.moveDir, futureMoveDir);
    if(dot(cross(glm::vec3(0,1,0), m.moveDir), futureMoveDir) < 0)
        angle *= -1;
    if(angle > .5 || angle < -.5){
        //m.setAnimation(runjump, none);
        //m.animationsFaktor[0] = 24./60.;
        m.setAction([angle](int ticks){
            // looks clunky, but it IS a robot, so...
            static const auto ticksNeeded = 55;
            auto g = Game::instance;
            auto &m = g->mechs[small];

            //animate
            if(ticks == 0){
                m.setAnimation(getup, none, 30. / 30.); // end is 34
                m.animationsFaktor[0] = -1.8;
            }
            if(ticks == 10) // 17
                m.animationsFaktor[0] = 1.8;
            if(ticks == 20)
                m.animationsFaktor[0] = 0;

            //landing
            if(ticks == 45)
                m.animationsFaktor[0] = -1.5;
            if(ticks == 50)
                m.animationsFaktor[0] = 1.5;

            //rotate
            if(ticks >= 20 && ticks < 40)
                m.moveDir = glm::rotate(m.moveDir, angle / 20, glm::vec3(0,1,0));

            // up and down
            auto trans = m.rigid->getWorldTransform();
            if(ticks >= 15 && ticks < 30)
                trans.setOrigin(trans.getOrigin() + btVector3(0,.1,0));
            if(ticks >= 30 && ticks < 45)
                trans.setOrigin(trans.getOrigin() - btVector3(0,.1,0));
            m.motionState->setWorldTransform(trans);
            m.rigid->setActivationState(DISABLE_DEACTIVATION);

            //stop
            if(ticks >= ticksNeeded){
                m.setAnimation(run, none);
                m.animationsFaktor[0] = 1;
                m.setAction(runSmall);
            }
        });
        return;
    }
  }

  auto nextStepPos = pos + ((futurePos - pos) * (1. / reachGoalInTicks));
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
