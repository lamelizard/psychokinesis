#include "Mech.hh"

#include <cmath>
#include <cassert>
#include <set>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <glow/common/log.hh>
#include <glow/objects/Program.hh>
#include <glow/objects/Texture2D.hh>

#include <GLFW/glfw3.h>

#include "conversion.hh"

#include "Game.hh" 

using namespace std;
using namespace glow;

SharedAssimpModel Mech::mesh;

void Mech::setAnimation(Mech::animation ab, Mech::animation at, double bt, double tt){
    animations[0] = ab;
    animationTop = at;
    animationAlpha = 1;
    animationsTime[0] = bt;
    animationTimeTop = tt;
    animationsFaktor[0] = 1;
}

  void Mech::setAnimation(Mech::animation aba, Mech::animation abb, Mech::animation at, float ba, double bta, double btb, double tt){
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

void Mech::updateTime(double delta) {
  animationsTime[0] += delta * animationsFaktor[0];
  animationsTime[1] += delta * animationsFaktor[1];
  animationTimeTop += delta * 1; // lame
}

void Mech::draw(glow::UsedProgram &shader) {
  glm::mat4 model;
  model = glm::translate(model, getPos());
  //rotate
  {
      auto angle = glm::angle(moveDir, glm::vec3(0, 0, 1));
      auto cross = glm::normalize(-glm::cross(moveDir, glm::vec3(0, 0, 1)));
      if(isnan(glm::length(cross))) // couldn't get cross product // still doesn'towrk...
          //FIXME TODO -> mech vs unity fix...
          //if(angle < 0.01 && angle > -0.01)
               //cross = glm::vec3(0,1,0);
          //else
          if(abs(angle) < 3.15 && abs(angle) > 3.13){
              cross = glm::vec3(0,1,0);
              //model = glm::rotate(model, 3.141592f, glm::vec3(0, 0, 1)); // unity fix???
          }
      model = glm::rotate(model, angle, cross);
  }
  model = glm::translate(model, meshOffset);
  model = glm::scale(model, glm::vec3(scale));
  //model = glm::translate(model, meshOffset);

  shader.setUniform("uModel", model);
  shader.setTexture("uTexAlbedo", texAlbedo);
  shader.setTexture("uTexNormal", texNormal);


  mesh->draw(shader, animationsTime[0], loops[animations[0]], names[animations[0]]);
  //mechModel->draw(shader, debugTime, true, "Hit"); //"WalkInPlace");
  // skeleton
  // mechModel->debugRenderer.render(proj * view * glm::scale(glm::vec3(0.01)));
}

glm::vec3 Mech::getPos() {
  btTransform transform;
  motionState->getWorldTransform(transform);
  return getWorldPos(transform);
}


void Mech::controlPlayer(int){
    auto g = Game::instance;
    auto& m = g->mechs[player];
    const auto playerPos = m.getPos();
    const auto bulPos = btcast(playerPos);

    m.rigid->setDamping(0.7, 0); //damps y...


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
      m.rigid->setLinearFactor(btVector3(1,1,1));
      m.rigid->setGravity(btVector3(0,-9.81,0));
      auto forceLength = 5;
      // handle slowdown
      {
        static bool musicSlow = true;
        if (playerModes.count(drawn)) {
          //maxSpeed = 3;
            m.rigid->setLinearFactor(btVector3(.7,.7,.7));
            m.rigid->setGravity(m.rigid->getGravity() * .7);
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
      bool jumpPressed = g->isKeyPressed(GLFW_KEY_SPACE) ||                                      //
                         (hasController && (                                                  //
                                               gamepadState.buttons[GLFW_GAMEPAD_BUTTON_A] || //
                                               gamepadState.buttons[GLFW_GAMEPAD_BUTTON_B] || //
                                               gamepadState.buttons[GLFW_GAMEPAD_BUTTON_X] || //
                                               gamepadState.buttons[GLFW_GAMEPAD_BUTTON_Y]));

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

        if (glm::length(relDir) > .1f)
          glm::normalize(relDir);
        else if (glm::length(relDir) < 0.1 && hasController) // a.k.a. 0
          relDir = glm::normalize(glm::vec3(limitAxis(gamepadState.axes[GLFW_GAMEPAD_AXIS_LEFT_X]), 0, limitAxis(gamepadState.axes[GLFW_GAMEPAD_AXIS_LEFT_Y])));
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
      auto force = (relDir.x * camRight + relDir.z * camForward) * forceLength;
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

      // close to ground?
      bool closeToGround = false;
      float ground = 0; // valid if closeToGround
      float closeToGroundBorder = m.floatOffset * 1.2;
      {
        auto from = bulPos - btVector3(0, m.collision->getHalfHeight() + m.collision->getRadius(), 0); //heigth is notthe height...
        if(from.y() < 0 && from.y() > -0.01)//stuck slighlty in ground...
            from.setY(0.01);
        auto to = from - btVector3(0, closeToGroundBorder, 0);
        g->dynamicsWorld->getDebugDrawer()->drawLine(from, to, btVector4(1, 0, 0, 1));
        auto closest = btCollisionWorld::ClosestRayResultCallback(from, to);
        g->dynamicsWorld->rayTest(from, to, closest);
        if (closest.hasHit()) {
          closeToGround = true;
          ground = closest.m_hitPointWorld.y();
        }
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
        }
        // jump
        if (!g->mJumps && jumpPressed) {
          g->mJumps = true;
          m.rigid->setLinearFactor(btVector3(1, 1, 1));
          m.rigid->applyCentralForce(btVector3(0, 500, 0));
        }
      } else
        m.rigid->setLinearFactor(btVector3(1, 1, 1));
    }
  }

}

void Mech::emptyAction(int){}

void Mech::startSmall(int t){
    auto g = Game::instance;
    auto& m = g->mechs[small];
    if(t <= 4 * 60)
        m.setAnimation(getup, none);
    if(t == 4*60){
        auto pos = m.getPos();
        g->soloud->play3d(g->sfx, pos.x, pos.y, pos.z);
    }

    if(t > 4*60+68 + 60){//getup finished + 1s
        m.setAnimation(walk, none);
        m.setAction(runSmall);
    }
}

void Mech::startBig(int)
{
    assert(0);
}

void Mech::runSmall(int t)
{
    auto g = Game::instance;
    auto &m = g->mechs[small];
    if(m.rigid->getUserIndex2() == SMALL_GOTHIT){
        m.HP--;
        m.rigid->setUserIndex2(SMALL_NONE);
    }

    auto groundOffset = glm::vec3(0, m.collision->getHalfHeight() + m.collision->getRadius() + m.floatOffset, 0);
    static int nextGoal = 1;
    static int currentWay = 0;
    static int timeNeeded = 10*60;
    static int reachGoalInTicks = timeNeeded;// testme, 1.75 m/s
    static glm::vec3 lastPosition  = glm::vec3(0.5, 0, 18.5) + groundOffset;
    auto pos = m.getPos();

    static const vector<glm::vec3> ways[4] = {{
        {0.5, 0, 18.5},
        {-17.5, 0, 18.5},
        {-17.5, 0, 0.5},
        {0.5, 0, 0.5},
        {18.5, 0, 0.5},
        {18.5, 0, -17.5},
        {0.5, 0, -17.5},
        {0.5, 0, 0.5}
    }, {
         {18.5, 0, 0.5},
         {18.5, 0, 18.5},
         {0.5, 0, 18.5},
         {0.5, 0, 0.5},
         {-17.5, 0, 0.5},
         {-17.5, 0, -17.5},
         {0.5, 0, -17.5},
         {0.5, 0, 0.5}
    }, {
         {-17.5, 0, 0.5},
         {-17.5, 0, 18.5},
         {-17.5, 0, 0.5},
         {-17.5, 0, -17.5},
         {-17.5, 0, 0.5},
         {0.5, 0, 0.5},
         {18.5, 0, 0.5},
         {0.5, 0, 0.5}
    }, {
         {0.5, 0, 18.5},
         {-17.5, 0, 18.5},
         {-17.5, 0, 0.5},
         {-17.5, 0, -17.5},
         {0.5, 0, -17.5},
         {18.5, 0, -17.5},
         {18.5, 0, 0.5},
         {18.5, 0, 18.5},
         {0.5, 0, 18.5},
         {0.5, 0, 0.5}
    }};

    //rocket
    if(t % (60 + ((m.HP) * 15)) == 0){
        auto &p = g->mechs[player];
        auto dir = glm::normalize(p.getPos() - pos);
        g->createRocket(pos + (dir * 3), dir * 4, rtype::forward); // 4?
    }

    //walk
    auto way = ways[currentWay];
    auto futurePos = way[nextGoal] + groundOffset;

    if(reachGoalInTicks == 0){
        nextGoal++;
        if(nextGoal == way.size())
            nextGoal = 0;
        if(futurePos == glm::vec3{0.5,0,0.5}  + groundOffset){
            if(currentWay < 5 - m.HP)
                currentWay++;
            else {
                //homing rocket
                auto &p = g->mechs[player];
                auto dir = glm::normalize(p.getPos() - pos);
                auto entity = g->createRocket(pos - glm::vec3(0,1,0) + (dir * 2.5), dir * 4, rtype::homing); // y-1 since else likely to hit ground  //TODO higher acc? and constraints to limeit velocity -> better homing
                if(m.HP > 3){ // wait to be easier
                    //TODO need default animation...
                    m.setAction([entity](int t){
                        if(!entity.valid())
                            Game::instance->mechs[small].setAction(runSmall);
                    });
                }
            }
        }
        way = ways[currentWay];
        futurePos = way[nextGoal];
        reachGoalInTicks = timeNeeded;
    }

    auto nextStepPos = pos + ((futurePos - pos) * (1. / reachGoalInTicks));
    m.moveDir = glm::normalize(futurePos - pos);
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
