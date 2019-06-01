#include "Game.hh"

#include <set>
#include <algorithm>
#include <future>

#include <glm/ext.hpp>

// glow OpenGL wrapper
#include <glow/common/log.hh>
#include <glow/common/scoped_gl.hh>
#include <glow/objects/ArrayBuffer.hh>
#include <glow/objects/Framebuffer.hh>
#include <glow/objects/Program.hh>
#include <glow/objects/Texture2D.hh>
#include <glow/objects/TextureRectangle.hh>
#include <glow/objects/VertexArray.hh>
#include <glow/objects/TextureCubeMap.hh>

#include <glow/data/TextureData.hh>

// extra functionality of glow
#include <glow-extras/geometry/Quad.hh>
#include <glow-extras/geometry/UVSphere.hh>

#include <GLFW/glfw3.h> // window/input framework

#include <imgui/imgui.h> // UI framework

#include <glm/glm.hpp> // math library

#include <assimp/DefaultLogger.hpp>

#include "load_mesh.hh" // helper function for loading .obj into VertexArrays

#include <soloud_speech.h>

GLOW_SHARED(class, btRigidBody);
GLOW_SHARED(class, btMotionState);
using defMotionState = std::shared_ptr<btDefaultMotionState>;

struct AttMat {
  glm::vec4 a, b, c, d;
};

using namespace std;

struct Cube {
  glm::ivec3 pos;
};

struct Health {
  int amount = 10;
};

enum Mode {
  normal = 0,
  drawn = 1,
  renme = 2 // rename me
};

struct ModeArea {
  Mode mode = normal;
  glm::vec3 pos;
  float radius;
};

glm::vec3 glcast(const btVector3 &v) {
  return glm::vec3(v.x(), v.y(), v.z());
}
btVector3 btcast(const glm::vec3 &v) {
  return btVector3(v.x, v.y, v.z);
}
btTransform bttransform(const glm::vec3 &offset) {
  auto identity = btMatrix3x3();
  identity.setIdentity();
  return btTransform(identity, btcast(offset));
}
glm::vec3 getWorldPos(const btTransform &t) {
  return glcast(t.getOrigin());
}
glm::vec3 translation(const glm::mat4 &m) { return glm::vec3(m[3]); }

Game::Game() : GlfwApp(Gui::ImGui) {}

void Game::init() {
  // enable VSync
  setVSync(true);


  // IMPORTANT: call to base class
  GlfwApp::init();

  setTitle("Game Development 2019");

  // start assimp logging
  Assimp::DefaultLogger::create();

  // create gfx resources
  {
    mCamera = glow::camera::Camera::create();
    mCamera->setLookAt({0, 2, 3}, {0, 0, 0});

    // depth
    {
      mTargets.push_back(mGBufferDepth = glow::TextureRectangle::create(1, 1, GL_DEPTH_COMPONENT32F));
      mFramebufferDepth = glow::Framebuffer::createDepthOnly(mGBufferDepth);
    }

    // mode
    {
      mTargets.push_back(mBufferMode = glow::TextureRectangle::create(1, 1, GL_R8)); // GL_RED_INTEGER misses implementation in glow?
      mFramebufferMode = glow::Framebuffer::create("fMode", mBufferMode, mGBufferDepth);
    }

    // GBuffer
    {
      // size is 1x1 for now and is changed onResize
      mTargets.push_back(mGBufferAlbedo = glow::TextureRectangle::create(1, 1, GL_RGB16F));
      mTargets.push_back(mGBufferMaterial = glow::TextureRectangle::create(1, 1, GL_RG16F));
      mTargets.push_back(mGBufferNormal = glow::TextureRectangle::create(1, 1, GL_RGB16F));
      mFramebufferGBuffer = glow::Framebuffer::create();
      auto fbuffer = mFramebufferGBuffer->bind();
      fbuffer.attachDepth(mGBufferDepth);
      fbuffer.attachColor("fAlbedo", mGBufferAlbedo);
      fbuffer.attachColor("fMaterial", mGBufferMaterial);
      fbuffer.attachColor("fNormal", mGBufferNormal);
    }

    // Light
    {
      mTargets.push_back(mBufferLight = glow::TextureRectangle::create(1, 1, GL_RGB16F));
      mFramebufferLight = glow::Framebuffer::create("fLight", mBufferLight, mGBufferDepth);
    }
  }

  // load gfx resources
  {
    //load data async (PNG is slow!)
    {
      //filenames
      const string texPath = "../data/textures/";
      const string cubeAlbedoFN = texPath + "cube.albedo.png";
      const string cubeNormalFN = texPath + "cube.normal.png";
      const string bgPath = texPath + "galaxy/";
      const string mechAlbedoFN = texPath + "mech.albedo.png";
      const string mechNormalFN = texPath + "mech.normal.png";
      const string mechModelFN = "../data/models/mech/mech.fbx";

      //loading async
      const auto policy = std::launch::deferred; // TODO, why does this break with async
      //todo glow::TextureData::createFromFile creates warnings?

      auto cubeAlbedoData = std::async(policy, glow::TextureData::createFromFile, cubeAlbedoFN, glow::ColorSpace::sRGB);
      auto cubeNormalData = std::async(policy, glow::TextureData::createFromFile, cubeNormalFN, glow::ColorSpace::Linear);
      auto bgData = std::async(policy, glow::TextureData::createFromFileCube, //
                               bgPath + "right.png",                          //
                               bgPath + "left.png",                           //
                               bgPath + "top.png",                            //
                               bgPath + "bottom.png",                         //
                               bgPath + "front.png",                          //
                               bgPath + "back.png",                           //
                               glow::ColorSpace::sRGB);
      auto mechAlbedoData = std::async(policy, glow::TextureData::createFromFile, mechAlbedoFN, glow::ColorSpace::sRGB);
      auto mechNormalData = std::async(policy, glow::TextureData::createFromFile, mechNormalFN, glow::ColorSpace::Linear);
      auto mechModelData = std::async(policy, AssimpModel::load, mechModelFN);

      //into GL
      mTexCubeAlbedo = glow::Texture2D::createFromData(cubeAlbedoData.get());
      mTexCubeAlbedo->setObjectLabel(cubeAlbedoFN);
      mTexCubeNormal = glow::Texture2D::createFromData(cubeNormalData.get());
      mTexCubeNormal->setObjectLabel(cubeNormalFN);
      mSkybox = glow::TextureCubeMap::createFromData(bgData.get());
      mSkybox->setObjectLabel(bgPath);
      mTexMechAlbedo = glow::Texture2D::createFromData(mechAlbedoData.get());
      mTexMechAlbedo->setObjectLabel(mechAlbedoFN);
      mTexMechNormal = glow::Texture2D::createFromData(mechNormalData.get());
      mTexMechNormal->setObjectLabel(mechNormalFN);
      //not into GL yet, could change
      mechModel = mechModelData.get();

      //mTexDefNormal = glow::Texture2D::createFromFile(texPath + "normal.png", glow::ColorSpace::Linear);
    }

    //basic shapes
    mMeshQuad = glow::geometry::make_quad(); // simple procedural quad with vec2 aPosition
    mMeshSphere = glow::geometry::UVSphere<>(glow::geometry::UVSphere<>::attributesOf(nullptr), 64, 32).generate();
    // cube.obj contains a cube with normals, tangents, and texture coordinates
    mMeshCube = load_mesh_from_obj("../data/meshes/cube.obj", false /* do not interpolate tangents for cubes */);
    auto modelMatrices = glow::ArrayBuffer::create();
    // could be done better with offset I guess
    modelMatrices->defineAttributes({
        // mat4, divisor = 1 so each instance new matrix
        glow::ArrayBufferAttribute(&AttMat::a, "aModel", glow::AttributeMode::Float, 1),  //
        glow::ArrayBufferAttribute(&AttMat::b, "aModelb", glow::AttributeMode::Float, 1), //
        glow::ArrayBufferAttribute(&AttMat::c, "aModelc", glow::AttributeMode::Float, 1), //
        glow::ArrayBufferAttribute(&AttMat::d, "aModeld", glow::AttributeMode::Float, 1)  //
    });
    mMeshCube->bind().attach(modelMatrices);

    //shader
    mShaderCube = glow::Program::createFromFile("../data/shaders/cube");
    mShaderCubePrepass = glow::Program::createFromFile("../data/shaders/cube.pre");
    mShaderOutput = glow::Program::createFromFile("../data/shaders/output");
    mShaderMode = glow::Program::createFromFile("../data/shaders/mode");
    mShaderMech = glow::Program::createFromFile("../data/shaders/mech");
  }

  // Sound
  {
    soloud = unique_ptr<SoLoud::Soloud, void (*)(SoLoud::Soloud *)>(
        ([]() -> SoLoud::Soloud * {
          auto s = new SoLoud::Soloud;
          if (s->init() == SoLoud::SO_NO_ERROR)
            return s;
          glow::log(glow::LogLevel::Error) << "could not init sound subsystem";
          if (s->init(SoLoud::Soloud::CLIP_ROUNDOFF, SoLoud::Soloud::NULLDRIVER, 44100, 16384, 2) == SoLoud::SO_NO_ERROR)
            return s;
          throw std::exception("big problems in sound subsystem");
        })(),
        [](SoLoud::Soloud *s) { s->deinit(); delete s; });
    assert(soloud);
    if (soloud->getBackendString())
      glow::log() << "audio backend: " << soloud->getBackendString();
    //bg music
    {
      music.load("../data/sounds/heroic_demise_loop.ogg");
      music.setLooping(true);
      musicHandle = soloud->playBackground(music, 0);
      soloud->fadeVolume(musicHandle, 1, 30);
    }
  }

  // initialize Bullet
  {
    collisionConfiguration = make_unique<btDefaultCollisionConfiguration>();
    dispatcher = make_unique<btCollisionDispatcher>(collisionConfiguration.get());
    overlappingPairCache = make_unique<btDbvtBroadphase>();
    solver = make_unique<btSequentialImpulseConstraintSolver>();
    dynamicsWorld = make_unique<btDiscreteDynamicsWorld>(dispatcher.get(), overlappingPairCache.get(), solver.get(), collisionConfiguration.get());
    dynamicsWorld->setGravity(btVector3(0, -9.81, 0));
    bulletDebugger = make_unique<BulletDebugger>();
    dynamicsWorld->setDebugDrawer(bulletDebugger.get());

    // ground
    btCollisionShape *groundShape = new btBoxShape(btVector3(btScalar(50.), btScalar(2.), btScalar(50.))); // lost memory
    btRigidBody::btRigidBodyConstructionInfo rbInfo(0., nullptr, groundShape, btVector3(0, 0, 0));
    auto ground = new btRigidBody(rbInfo);
    ground->setWorldTransform(bttransform(glm::vec3(0, -5, 0)));
    dynamicsWorld->addRigidBody(ground);

    colBox = make_unique<btBoxShape>(btVector3(0.5, 0.5, 0.5)); // half extend
  }

  // player
  {
    auto entity = ex.entities.create();
    colPlayer = make_unique<btCapsuleShape>(0.25, 1);
    playerMotionState = make_shared<btDefaultMotionState>(bttransform(glm::vec3(0, 5, 0)));
    bulPlayer = make_shared<btRigidBody>(btRigidBody::btRigidBodyConstructionInfo(1.f, playerMotionState.get(), colPlayer.get(), btVector3(0, 0, 0)));
    bulPlayer->setAngularFactor(0);
    bulPlayer->setCustomDebugColor(btVector3(255, 1, 1));
    dynamicsWorld->addRigidBody(bulPlayer.get());
    entity.assign<SharedbtRigidBody>(bulPlayer);
    entity.assign<defMotionState>(playerMotionState);
    entity.assign<Health>();
  }

  // create world
  // floor
  {
    auto y = -colBox->getHalfExtentsWithMargin().getY(); // floor is y = 0
    for (auto x = cubesWorldMin; x <= cubesWorldMax; x++)
      for (auto z = cubesWorldMin; z <= cubesWorldMax; z++)
        createCube(glm::vec3(x, y, z));
  }

  // pillars
  {
    for (auto x = cubesWorldMin + 15; x <= cubesWorldMax - 10; x += 20)
      for (auto z = cubesWorldMin + 15; z <= cubesWorldMax - 10; z += 20)
        for (auto yBottom = 0; yBottom <= 5; yBottom++) {
          auto y = yBottom + colBox->getHalfExtentsWithMargin().getY();
          createCube(glm::vec3(x, y, z));
          createCube(glm::vec3(x + 1, y, z));
          createCube(glm::vec3(x, y, z + 1));
          createCube(glm::vec3(x + 1, y, z + 1));
        }
  }

  // test mode area
  {
    auto entity = ex.entities.create();
    entity.assign<ModeArea>(ModeArea{drawn, {10, 0, 10}, 5});
  }

  dynamicsWorld->stepSimulation(1. / 60);
}

entityx::Entity Game::createCube(const glm::ivec3 &pos) {
  auto motionState = make_shared<btDefaultMotionState>(bttransform(glm::vec3(pos.x, (float)pos.y - colBox->getHalfExtentsWithMargin().getY(), pos.z))); // y = 0 is floor
  auto rbCube = make_shared<btRigidBody>(btRigidBody::btRigidBodyConstructionInfo(0., motionState.get(), colBox.get(), btVector3(0, 0, 0)));
  dynamicsWorld->addRigidBody(rbCube.get());

  auto entity = ex.entities.create();
  entity.assign<SharedbtRigidBody>(rbCube);
  entity.assign<defMotionState>(motionState);
  entity.assign<Cube>(Cube{pos});
  return entity;
}

void Game::update(float elapsedSeconds) {
  // update game in 60 Hz fixed timestep
  // fix debugging?
  if (elapsedSeconds > 1)
    elapsedSeconds = 1;


  bulPlayer->activate();
  const auto playerPos = getWorldPos(bulPlayer->getWorldTransform());
  const auto bulPos = btcast(playerPos);

  // modes of player, they change the logic
  // could change mode for any entity!!!
  set<Mode> playerModes;
  {
    auto area = entityx::ComponentHandle<ModeArea>();
    for (auto entity : ex.entities.entities_with_components(area))
      if (glm::distance(area->pos, playerPos) < area->radius)
        playerModes.insert(area->mode);
  }

  // Physics
  // Bullet uses fixed timestep and interpolates
  // -> probably should put this in render as well to get the interpolation
  // move character
  {
    auto maxSpeed = 3;
    auto forceLength = 5;
    // handle slowdown
    {
      static bool musicSlow = true;
      if (playerModes.count(drawn)) {
        maxSpeed = 1;
        if (!musicSlow) {
          musicSlow = true;
          soloud->fadeRelativePlaySpeed(musicHandle, 0.7, 2);
        }
      } else if (musicSlow) {
        musicSlow = false;
        soloud->fadeRelativePlaySpeed(musicHandle, 1, 2);
      }
    }



    bool jumpPressed = isKeyPressed(GLFW_KEY_SPACE);


    btVector3 force;
    {
      // reldir = dir without camera
      glm::vec3 relDir;
      if (isKeyPressed(GLFW_KEY_LEFT) || (isKeyPressed(GLFW_KEY_A) && !mFreeCamera))
        relDir.x -= 1;
      if (isKeyPressed(GLFW_KEY_RIGHT) || (isKeyPressed(GLFW_KEY_D) && !mFreeCamera))
        relDir.x += 1;
      if (isKeyPressed(GLFW_KEY_UP) || (isKeyPressed(GLFW_KEY_W) && !mFreeCamera))
        relDir.z += 1;
      if (isKeyPressed(GLFW_KEY_DOWN) || (isKeyPressed(GLFW_KEY_S) && !mFreeCamera))
        relDir.z -= 1;

      if (glm::length(relDir) > .1f)
        glm::normalize(relDir);

      // movement relative to camera
      glm::vec3 camForward;
      glm::vec3 camRight;
      {
        camForward = mCamera->getForwardVector();
        camForward.y = 0;
        camForward = glm::normalize(camForward);
        camRight = mCamera->getRightVector();
        camRight.y = 0;
        camRight = glm::normalize(camRight);
      }

      force = btcast((relDir.x * camRight + relDir.z * camForward) * forceLength);
    }
    bulPlayer->applyCentralForce(force);


    // slow down
    // TODO ice!
    bulPlayer->setDamping(0.7, 0); //damps y...



    auto btSpeed = bulPlayer->getLinearVelocity();
    auto ySpeed = btSpeed.y();
    btSpeed.setY(0);
    // but what about the forces?
    if (btSpeed.length() > maxSpeed)
      btSpeed = btSpeed.normalize() * maxSpeed;
    btSpeed.setY(ySpeed);
    bulPlayer->setLinearVelocity(btSpeed);

    // close to ground?
    bool closeToGround = false;
    float ground = 0; // valid if closeToGround
    float closeToGroundBorder = 0.3;
    float floatingHeight = 0.25;
    {
      auto from = bulPos - btVector3(0, colPlayer->getHalfHeight(), 0);

      auto to = from - btVector3(0, closeToGroundBorder, 0);
      auto closest = btCollisionWorld::ClosestRayResultCallback(from, to);
      dynamicsWorld->rayTest(from, to, closest);
      if (closest.hasHit()) {
        closeToGround = true;
        ground = closest.m_hitPointWorld.y();
      }
    }

    // jumping
    if (closeToGround) {
      // falling + standing
      if (bulPlayer->getLinearVelocity().y() <= 0.01 /*&& !jumpPressed*/) {
        mJumps = false;
        // reset y-velocity
        auto v = bulPlayer->getLinearVelocity();
        v.setY(0);
        bulPlayer->setLinearVelocity(v);
        // reset y-position
        auto t = bulPlayer->getWorldTransform();
        auto o = t.getOrigin();
        o.setY(ground + floatingHeight + colPlayer->getHalfHeight());
        t.setOrigin(o);
        bulPlayer->setWorldTransform(t);
        // no y-movement anymore!
        bulPlayer->setLinearFactor(btVector3(1, 0, 1));
      }
      // jump
      if (!mJumps && jumpPressed) {
        mJumps = true;
        bulPlayer->setLinearFactor(btVector3(1, 1, 1));
        bulPlayer->applyCentralForce(btVector3(0, 300, 0));
      }
    } else
      bulPlayer->setLinearFactor(btVector3(1, 1, 1));
  }


  dynamicsWorld->stepSimulation(elapsedSeconds, 10);
}

//*************************************
// todo: safe normals from one frame and use those for light for some time -> similar to afterimage

void Game::render(float elapsedSeconds) {
  // render game variable timestep


  // camera update here because it should be coupled tightly to rendering!
  updateCamera(elapsedSeconds);
  // get camera matrices
  auto proj = mCamera->getProjectionMatrix();
  auto view = mCamera->getViewMatrix();

  // Depth
  {
    auto fb = mFramebufferMode->bind();
    GLOW_SCOPED(enable, GL_DEPTH_TEST);
    GLOW_SCOPED(enable, GL_CULL_FACE);
    GLOW_SCOPED(clearColor, glm::vec3(0, 0, 0));
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    drawCubes(mShaderCubePrepass->use());

    drawMech(mShaderMech->use(), elapsedSeconds);

    if (mDebugBullet) {
      GLOW_SCOPED(disable, GL_DEPTH_TEST);
      dynamicsWorld->debugDrawWorld();
      bulletDebugger->draw(proj * view);
    }
  }

  // GBuffer
  {
    auto fb = mFramebufferGBuffer->bind();
    // glViewport is automatically set by framebuffer
    GLOW_SCOPED(enable, GL_DEPTH_TEST);
    GLOW_SCOPED(enable, GL_CULL_FACE);
    GLOW_SCOPED(depthMask, GL_FALSE);
    GLOW_SCOPED(depthFunc, GL_EQUAL);
    GLOW_SCOPED(polygonMode, mShowWireframe ? GL_LINE : GL_FILL);
    GLOW_SCOPED(clearColor, mBackgroundColor);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    drawCubes(mShaderCube->use());

    drawMech(mShaderMech->use(), elapsedSeconds);

    // Render Bullet Debug
    if (mDebugBullet) {
      GLOW_SCOPED(disable, GL_DEPTH_TEST);
      // dynamicsWorld->debugDrawWorld();
      bulletDebugger->draw(proj * view);
    }
  }

  // Mode
  // move up after depth prepass ?
  {
    auto fb = mFramebufferMode->bind();
    GLOW_SCOPED(enable, GL_CULL_FACE);
    GLOW_SCOPED(enable, GL_DEPTH_TEST);
    GLOW_SCOPED(depthMask, GL_FALSE); // Disable depth write
    GLOW_SCOPED(enable, GL_BLEND);
    GLOW_SCOPED(blendEquation, GL_MAX); // priority
    GLOW_SCOPED(cullFace, GL_FRONT);    // write backfaces
    GLOW_SCOPED(depthFunc, GL_GREATER); // Inverse z test

    auto shader = mShaderMode->use();
    auto sphere = mMeshSphere->bind();
    auto areaHandle = entityx::ComponentHandle<ModeArea>();
    auto areaEntities = ex.entities.entities_with_components(areaHandle);
    for (auto entity : areaEntities) {
      auto r = areaHandle->radius;
      shader.setTexture("uTexDepth", mGBufferDepth);
      shader.setUniform("uPos", areaHandle->pos);
      shader.setUniform("uPVM", proj * view * glm::translate(areaHandle->pos) * glm::scale(glm::vec3(r, r, r)));
      shader.setUniform("uInvProj", glm::inverse(proj));
      shader.setUniform("uInvView", glm::inverse(view));
      shader.setUniform("uRadius", r);
      shader.setUniform("uMode", (int32_t)areaHandle->mode);
      sphere.draw();
    }
  }


  // Light
  /*
    {
        auto fb = mFramebufferLight->bind();
        // partly from RTGLive
        GLOW_SCOPED(enable, GL_CULL_FACE);
        GLOW_SCOPED(enable, GL_DEPTH_TEST);
        GLOW_SCOPED(depthMask, GL_FALSE); // Disable depth write

        GLOW_SCOPED(clearColor, glm::vec3(0, 0, 0));
        glClear(GL_COLOR_BUFFER_BIT);

        // todo actuall lights
        {
            GLOW_SCOPED(enable, GL_BLEND);
            GLOW_SCOPED(blendFunc, GL_ONE, GL_ONE);
            GLOW_SCOPED(cullFace, GL_FRONT);    // write backfaces
            GLOW_SCOPED(depthFunc, GL_GREATER); // Inverse z test
        }
    }
    */


  // render framebuffer content to output with small post-processing effect
  {
    // no framebuffer is bound, i.e. render-to-screen
    GLOW_SCOPED(disable, GL_DEPTH_TEST);
    GLOW_SCOPED(disable, GL_CULL_FACE);

    // draw a fullscreen quad for outputting the framebuffer and applying a post-process
    auto shader = mShaderOutput->use();
    shader.setTexture("uTexColor", mGBufferAlbedo);
    shader.setTexture("uTexNormal", mGBufferNormal);
    shader.setTexture("uTexDepth", mGBufferDepth);
    shader.setTexture("uTexMode", mBufferMode);
    //sky
    shader.setTexture("uSkybox", mSkybox);
    shader.setUniform("uInvProj", glm::inverse(proj));
    shader.setUniform("uInvView", inverse(view));

    // let light rotate around the objects
    // put higher
    auto lightDir = glm::vec3(glm::cos(getCurrentTime()), 0, glm::sin(getCurrentTime()));
    shader.setUniform("uLightDir", lightDir);
    shader.setUniform("uShowPostProcess", mShowPostProcess);
    mMeshQuad->bind().draw();
  }
}

void Game::drawMech(glow::UsedProgram shader, float elapsedSeconds) {
  auto proj = mCamera->getProjectionMatrix();
  auto view = mCamera->getViewMatrix();

  glm::mat4 modelMech;                                                        // glm::translate(mSpherePosition) * glm::scale(glm::vec3(mSphereSize));
  modelMech = glm::rotate(modelMech, glm::radians(90.f), glm::vec3(1, 0, 0)); // unity?
  modelMech = glm::mat4();
  shader.setUniform("uProj", proj);
  shader.setUniform("uView", view);
  shader.setUniform("uModel", modelMech);

  shader.setTexture("uTexAlbedo", mTexMechAlbedo);
  shader.setTexture("uTexNormal", mTexMechNormal);
  //shader.setTexture("uTexAlbedo", mTexDefNormal);
  //shader.setTexture("uTexNormal", mTexDefNormal);

  static auto timer = 0;
  timer += elapsedSeconds;
  // mechModel->draw(shader, timer, true, "WalkInPlace");
  mechModel->draw(shader, debugTime, true, "Hit"); //"WalkInPlace");
  // skeleton
  mechModel->debugRenderer.render(proj * view * glm::scale(glm::vec3(0.01)));
}

void Game::drawCubes(glow::UsedProgram shader) {
  auto proj = mCamera->getProjectionMatrix();
  auto view = mCamera->getViewMatrix();

  shader.setUniform("uProj", proj);
  shader.setUniform("uView", view);

  shader.setTexture("uTexAlbedo", mTexCubeAlbedo);
  shader.setTexture("uTexNormal", mTexCubeNormal);

  // model matrices
  vector<glm::mat4> models;
  models.reserve(3000);
  auto cubeHandle = entityx::ComponentHandle<Cube>();
  auto cubeEntities = ex.entities.entities_with_components(cubeHandle);

  vector<ModeArea> scaleAreas;
  {
    auto area = entityx::ComponentHandle<ModeArea>();
    for (auto entity : ex.entities.entities_with_components(area))
      if (area->mode == drawn)
        scaleAreas.push_back(*area.get());
  }


  for (entityx::Entity entity : cubeEntities) {
    glm::mat4x4 modelCube;
    {
      auto motionState = *entity.component<defMotionState>().get();
      btTransform trans;
      motionState->getWorldTransform(trans);
      static_assert(sizeof(AttMat) == sizeof(glm::mat4x4), "bwah");
      trans.getOpenGLMatrix(glm::value_ptr(modelCube));
    }
    modelCube *= glm::scale(glm::vec3(0.5)); // cube.obj has size 2
    //scale down in an mode
    //TODO change mode
    {
      auto trans = translation(modelCube);
      for (const auto &area : scaleAreas) {
        auto distanceFactor = (glm::distance(area.pos, trans) / area.radius);
        if (distanceFactor < 1)
          modelCube *= glm::scale(glm::vec3((1 - distanceFactor) * 0.05 + 0.95));
      }
    }
    models.push_back(modelCube);
  }

  auto abModels = mMeshCube->getAttributeBuffer("aModel");
  assert(abModels);
  abModels->bind().setData(models);
  mMeshCube->bind().draw(models.size());
}


// Update the GUI
void Game::onGui() {
#ifndef NDEBUG
  if (ImGui::Begin("GameDev Project SS19")) {
    ImGui::Text("Settings:");
    {
      ImGui::Indent();
      ImGui::Checkbox("Show Wireframe", &mShowWireframe);
      ImGui::Checkbox("Debug Bullet", &mDebugBullet);
      //ImGui::Checkbox("Show Mech", &mDrawMech);
      ImGui::Checkbox("free Camera", &mFreeCamera);
      ImGui::ColorEdit3("Background Color", &mBackgroundColor.r);
      ImGui::Unindent();
    }
    ImGui::SliderFloat("mechtime", &debugTime, 0.0, 5.0);
  }
  ImGui::End();
#endif
}

// Called when window is resized
void Game::onResize(int w, int h) {
  // camera viewport size is important for correct projection matrix
  mCamera->setViewportSize(w, h);

  // resize all framebuffer textures
  for (auto const &t : mTargets)
    t->bind().resize(w, h);
}

bool Game::onKey(int key, int scancode, int action, int mods) {
  // handle imgui and more
  glow::glfw::GlfwApp::onKey(key, scancode, action, mods);

  // fullscreen = Alt + Enter
  //TODO seems like vsync is of in fullscreen
  if (action == GLFW_PRESS && key == GLFW_KEY_ENTER)
    if (isKeyPressed(GLFW_KEY_LEFT_ALT) || isKeyPressed(GLFW_KEY_RIGHT_ALT))
      toggleFullscreen();

  return false;
}

void Game::updateCamera(float elapsedSeconds) {
  auto const speed = elapsedSeconds * 3;

  glm::vec3 rel_move;
  if (mFreeCamera) {
    // WASD camera move
    if (isKeyPressed(GLFW_KEY_A)) // left
      rel_move.x -= speed;
    if (isKeyPressed(GLFW_KEY_D)) // right
      rel_move.x += speed;
    if (isKeyPressed(GLFW_KEY_W)) // forward
      rel_move.z -= speed;
    if (isKeyPressed(GLFW_KEY_S)) // backward
      rel_move.z += speed;
    if (isKeyPressed(GLFW_KEY_Q)) // down
      rel_move.y -= speed;
    if (isKeyPressed(GLFW_KEY_E)) // up
      rel_move.y += speed;

    // left shift -> more speed
    if (isKeyPressed(GLFW_KEY_LEFT_SHIFT))
      rel_move *= 5.0f;

    // Look around
    bool leftMB = isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
    bool rightMB = isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);

    if ((leftMB || rightMB) && !ImGui::GetIO().WantCaptureMouse) {
      // hide mouse
      setCursorMode(glow::glfw::CursorMode::Disabled);

      auto mouse_delta = input().getLastMouseDelta() / 100.0f;

      if (leftMB && rightMB)
        rel_move += glm::vec3(mouse_delta.x, 0, mouse_delta.y);
      else if (leftMB)
        mCamera->handle.orbit(mouse_delta.x, mouse_delta.y);
      else if (rightMB)
        mCamera->handle.lookAround(mouse_delta.x, mouse_delta.y);
    } else
      setCursorMode(glow::glfw::CursorMode::Normal);

    // move camera handle (position), accepts relative moves
    mCamera->handle.move(rel_move);
  } else {
#ifdef NDEBUG
    setCursorMode(glow::glfw::CursorMode::Disabled);
#endif
    glm::vec3 target = glcast(bulPlayer->getWorldTransform().getOrigin());
    mCamera->handle.setTarget(target);
    mCamera->handle.setTargetDistance(5);


    if (!ImGui::GetIO().WantCaptureMouse) {
      auto mouse_delta = input().getLastMouseDelta() / 100.0f;
      if (mouse_delta.x < 1000) // ???
        mCamera->handle.orbit(mouse_delta.x, mouse_delta.y);

      auto pos = mCamera->handle.getPosition();
      //pos.y = std::max(0.1f, pos.y);
      //mCamera->handle.setPosition(pos);
      mCamera->handle.move(glm::vec3(0, std::max(.0, 0.1 - pos.y), 0));
    }
  }

  // Camera is smoothed
  mCamera->update(elapsedSeconds);
}
