#include "Game.hh"

#include <set>
#include <algorithm>
#include <future>
#include <functional>
#include <exception>
#include <random>

#include <glm/ext.hpp>

// glow OpenGL wrapper
#include <glow/common/log.hh>
#include <glow/common/scoped_gl.hh>
#include <glow/objects/ArrayBuffer.hh>
#include <glow/objects/Framebuffer.hh>
#include <glow/objects/Program.hh>
#include <glow/objects/Texture2D.hh>
#include <glow/objects/Texture2DArray.hh>
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
#include "assimpModel.hh"

#include "load_mesh.hh" // helper function for loading .obj into VertexArrays
#include <soloud_speech.h>

#include "conversion.hh"

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

struct Rocket {
  rtype type = rtype::forward;
  bool real = true;
  bool explode = false;
};

Game *Game::instance = nullptr;

#ifndef NDEBUG
Game::Game() : GlfwApp(Gui::ImGui) {}
#else
Game::Game() : GlfwApp(Gui::None) {}
#endif


void Game::init() {
  instance = this;

  // enable VSync
  setVSync(true);


  // IMPORTANT: call to base class
  GlfwApp::init();

  setTitle("Game Development 2019 TODO change ME"); // TODO

  // start assimp logging
  Assimp::DefaultLogger::create();

  // sphere
  {
      spherePoints.reserve(500);
      if(spherePoints.empty()){
          std::uniform_real_distribution<double> dist(0.0, 1.0);
          std::mt19937 gen (87234587123648);//juts guessing
          for(int i = 0; i < 500; i++){
              double t =  6.28318530718 * dist(gen);
              double p = acos(1 - 2 * dist(gen));
              spherePoints.push_back({
                                       sin(p)*cos(t), //
                                       sin(p)*sin(t), //
                                       cos(p)
                                     });
          }
      }
}

  // create gfx resources
  {
    mCamera = glow::camera::Camera::create();
    mCamera->setLookAt({-5, 3, -10}, {0, 7, -10}); //90 degree

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
      mFramebufferGBuffer = glow::Framebuffer::create(
          {{"fAlbedo", mGBufferAlbedo},
           {"fNormal", mGBufferNormal},
           {"fMaterial", mGBufferMaterial}},
          mGBufferDepth);
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
      string texPath = "../data/textures/";
      string cubeAlbedoFN = texPath + "cube.albedo.png";
      string cubeNormalFN = texPath + "cube.normal.png";
      string smallFN = texPath + "normal.png";
      string bgPath = texPath + "galaxy/";
      string mechAlbedoFN = texPath + "mech.albedo.png";
      string mechNormalFN = texPath + "mech.normal.png";
      string mechModelFN = "../data/models/mech/mech.fbx";
      string healthBarFn = texPath + "ui/Health bar";
      string rocketAlbedoFN = texPath + "rocket.albedo.";
      string rocketNormalFN = texPath + "rocket.normal.";


#ifndef NDEBUG // faster loading
      mechAlbedoFN = smallFN;
      mechNormalFN = smallFN;
#endif

      //loading async
      const auto policy = launch::deferred; // TODO, why does this break with async
      //todo glow::TextureData::createFromFile creates warnings?

      auto cubeAlbedoData = async(policy, glow::TextureData::createFromFile, cubeAlbedoFN, glow::ColorSpace::sRGB);
      auto cubeNormalData = async(policy, glow::TextureData::createFromFile, cubeNormalFN, glow::ColorSpace::Linear);
      auto bgData = async(policy, glow::TextureData::createFromFileCube, //
                          bgPath + "right.png",                          //
                          bgPath + "left.png",                           //
                          bgPath + "top.png",                            //
                          bgPath + "bottom.png",                         //
                          bgPath + "front.png",                          //
                          bgPath + "back.png",                           //
                          glow::ColorSpace::sRGB);
      auto mechAlbedoData = async(policy, glow::TextureData::createFromFile, mechAlbedoFN, glow::ColorSpace::sRGB);
      auto mechNormalData = async(policy, glow::TextureData::createFromFile, mechNormalFN, glow::ColorSpace::Linear);
      auto mechModelData = async(policy, AssimpModel::load, mechModelFN);
      future<glow::SharedTextureData> healthBarData[MAX_HEALTH + 1];
      for (int i = 0; i <= MAX_HEALTH; i++)
        healthBarData[i] = std::async(policy, glow::TextureData::createFromFile, healthBarFn + to_string(i) + ".png", glow::ColorSpace::sRGB);
      future<glow::SharedTextureData> rocketAlbedoData[NUM_ROCKET_TYPES];
      future<glow::SharedTextureData> rocketNormalData[NUM_ROCKET_TYPES];
      for (int i = 0; i < NUM_ROCKET_TYPES; i++) {
        rocketAlbedoData[i] = async(policy, glow::TextureData::createFromFile, rocketAlbedoFN + to_string(i) + ".png", glow::ColorSpace::sRGB);
        rocketNormalData[i] = async(policy, glow::TextureData::createFromFile, rocketNormalFN + to_string(i) + ".png", glow::ColorSpace::sRGB);
      }


      //into GL
      mTexCubeAlbedo = glow::Texture2D::createFromData(cubeAlbedoData.get());
      mTexCubeAlbedo->setObjectLabel(cubeAlbedoFN);
      mTexCubeNormal = glow::Texture2D::createFromData(cubeNormalData.get());
      mTexCubeNormal->setObjectLabel(cubeNormalFN);
      mSkybox = glow::TextureCubeMap::createFromData(bgData.get());
      mSkybox->setObjectLabel(bgPath);
      mechs[player].texAlbedo = glow::Texture2D::createFromData(mechAlbedoData.get());
      mechs[player].texAlbedo->setObjectLabel(mechAlbedoFN);
      mechs[player].texNormal = glow::Texture2D::createFromData(mechNormalData.get());
      mechs[player].texNormal->setObjectLabel(mechNormalFN);
      //temp, TODO
      mechs[small].texAlbedo = mechs[player].texAlbedo;
      mechs[small].texNormal = mechs[player].texNormal;
      mechs[big].texAlbedo = mechs[player].texAlbedo;
      mechs[big].texNormal = mechs[player].texNormal;

      for (int i = 0; i <= MAX_HEALTH; i++) {
        mHealthBar[i] = glow::Texture2D::createFromData(healthBarData[i].get());
        mHealthBar[i]->setObjectLabel(healthBarFn);
      }
      for (int i = 0; i < NUM_ROCKET_TYPES; i++) {
        mTexRocketAlbedo[i] = glow::Texture2D::createFromData(rocketAlbedoData[i].get());
        mTexRocketAlbedo[i]->setObjectLabel(rocketAlbedoFN);
        mTexRocketNormal[i] = glow::Texture2D::createFromData(rocketNormalData[i].get());
        mTexRocketNormal[i]->setObjectLabel(rocketNormalFN);
      }

      //not into GL yet, could change
      Mech::mesh = mechModelData.get();

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
    //other meshes
    for (int i = 0; i < NUM_ROCKET_TYPES; i++) {
      mMeshRocket[i] = load_mesh_from_obj("../data/meshes/rocket" + to_string(i) + ".obj", true);
      auto modelMatrices = glow::ArrayBuffer::create();
      // could be done better with offset I guess
      modelMatrices->defineAttributes({
          // mat4, divisor = 1 so each instance new matrix
          glow::ArrayBufferAttribute(&AttMat::a, "aModel", glow::AttributeMode::Float, 1),  //
          glow::ArrayBufferAttribute(&AttMat::b, "aModelb", glow::AttributeMode::Float, 1), //
          glow::ArrayBufferAttribute(&AttMat::c, "aModelc", glow::AttributeMode::Float, 1), //
          glow::ArrayBufferAttribute(&AttMat::d, "aModeld", glow::AttributeMode::Float, 1)  //
      });
      mMeshRocket[i]->bind().attach(modelMatrices);
    }


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
          throw exception();
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
    // sfx
    {
        //placeholder
        sfx.setText("boom");
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
    dynamicsWorld->setInternalTickCallback(bulletCallbackStatic);

    // "point"
    colPoint = make_shared<btSphereShape>(0.1);


    // mechs
    {
        //player
        // starting to hardcode everything, assuming that I don't ever need to change stuff
        {
            Mech &m = mechs[player];
    m.type = player;
    m.HP = MAX_HEALTH;
    m.setAction(Mech::controlPlayer);
    m.moveDir = glm::vec3(0, 0, 1);
    m.viewDir = glm::vec3(0, 0, 1);
    m.scale = .2;
    m.floatOffset = 0.25;
    m.collision = make_shared<btCapsuleShape>(0.4, 0.5);
    m.meshOffset = glm::vec3(0, -(m.collision->getHalfHeight() + m.collision->getRadius() + m.floatOffset), -0.3);
    m.motionState = make_shared<btDefaultMotionState>(bttransform(glm::vec3(0, 7, -10))); // fall down in an epic way
    m.rigid = make_shared<btRigidBody>(btRigidBody::btRigidBodyConstructionInfo(1.f, m.motionState.get(), m.collision.get()));
    m.rigid->setAngularFactor(0);
    m.rigid->setCustomDebugColor(btVector3(255, 1, 1));
    m.rigid->setUserIndex(BID_PLAYER);
    m.rigid->setUserPointer(&mechs[player]);
    dynamicsWorld->addRigidBody(m.rigid.get());
  }

  //small
  {
    Mech &m = mechs[small];
    m.type = small;
    m.HP = 5;
    m.setAction(Mech::startSmall);
    m.moveDir = glm::vec3(0, 0, -1);
    m.viewDir = glm::vec3(0, 0, -1);
    m.scale = 1;
    m.floatOffset = 0.01;
    m.collision = make_shared<btCapsuleShape>(2, 2.5);
    auto groundOffset = m.collision->getHalfHeight() + m.collision->getRadius() + m.floatOffset;
    m.meshOffset = glm::vec3(0, -groundOffset, -1.5);
    m.motionState = make_shared<btDefaultMotionState>(bttransform(glm::vec3(0, groundOffset, 20)));
    m.rigid = make_shared<btRigidBody>(btRigidBody::btRigidBodyConstructionInfo(0, m.motionState.get(), m.collision.get()));
    m.rigid->setAngularFactor(0);
    m.rigid->setCustomDebugColor(btVector3(255, 1, 1));
    m.rigid->setCollisionFlags(m.rigid->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
    m.rigid->setUserIndex(BID_SMALL);
    m.rigid->setUserPointer(&mechs[small]);
    dynamicsWorld->addRigidBody(m.rigid.get());
  }
  //big
  {
    Mech &m = mechs[big];
    m.type = big;
    m.HP = 5;
    m.setAction(Mech::emptyAction);
    m.moveDir = glm::vec3(0, 0, -1);
    m.viewDir = glm::vec3(0, 0, -1);
    m.scale = 30;
    m.floatOffset = 0.01;
    m.collision = make_shared<btCapsuleShape>(0.1, 0.1); //no collisions
    m.meshOffset = glm::vec3(0);
    m.motionState = make_shared<btDefaultMotionState>(bttransform(glm::vec3(0, -500, 0)));
    m.rigid = make_shared<btRigidBody>(btRigidBody::btRigidBodyConstructionInfo(0, m.motionState.get(), m.collision.get()));
    m.rigid->setAngularFactor(0);
    m.rigid->setCustomDebugColor(btVector3(255, 1, 1));
    m.rigid->setCollisionFlags(m.rigid->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
    m.rigid->setUserIndex(BID_BIG);
    m.rigid->setUserPointer(&mechs[big]);
    dynamicsWorld->addRigidBody(m.rigid.get());
  }
}

//boxes
{
  colBox = make_shared<btBoxShape>(btVector3(0.5, 0.5, 0.5)); // half extend
  // create world
  // floor
  {
    auto y = -colBox->getHalfExtentsWithMargin().getY(); // floor is y = 0
    for (auto x = 0; x < CUBES_TOTAL; x++)
      for (auto z = 0; z < CUBES_TOTAL; z++)
        ground[x][z] = createCube(glm::vec3(x + CUBES_MIN, y, z + CUBES_MIN));
  }

  // pillars
  {
    int i = 0;
    for (auto x = CUBES_MIN + 15; x <= CUBES_MAX - 10; x += 20)
      for (auto z = CUBES_MIN + 15; z <= CUBES_MAX - 10; z += 20) {
        for (auto yBottom = 0; yBottom <= 5; yBottom++) {
          auto y = yBottom + colBox->getHalfExtentsWithMargin().getY();
          pillars[i].push_back(createCube(glm::vec3(x, y, z)));
          pillars[i].push_back(createCube(glm::vec3(x + 1, y, z)));
          pillars[i].push_back(createCube(glm::vec3(x, y, z + 1)));
          pillars[i].push_back(createCube(glm::vec3(x + 1, y, z + 1)));
        }
        i++; // new pillar
      }
  }
}

// test
createRocket(glm::vec3(0, 3, 0), glm::vec3(0.001, 0, 0), rtype::forward);
}



// test mode area
{
  auto entity = ex.entities.create();
  entity.assign<ModeArea>(ModeArea{drawn, {10, 0, 10}, 5});
}
}



void Game::bulletCallback(btDynamicsWorld *, btScalar){
    int numManifolds = dynamicsWorld->getDispatcher()->getNumManifolds();
    for (int i = 0; i < numManifolds; i++) {
        btPersistentManifold *contactManifold = dynamicsWorld->getDispatcher()->getManifoldByIndexInternal(i);
        auto *a = contactManifold->getBody0();
        auto *b = contactManifold->getBody1();
        for(auto o : {a,b}){
            if(o->getUserIndex() == BID_ROCKET){
               auto r = (btRigidBody*) o;
               r->setUserIndex2(1); // explode
            }
        }
    }
}

entityx::Entity Game::createCube(const glm::ivec3 &pos) {
  auto motionState = make_shared<btDefaultMotionState>(bttransform(glm::vec3(pos.x, (float)pos.y - colBox->getHalfExtentsWithMargin().getY(), pos.z))); // y = 0 is floor
  auto rbCube = make_shared<btRigidBody>(btRigidBody::btRigidBodyConstructionInfo(0., motionState.get(), colBox.get(), btVector3(0, 0, 0)));
  dynamicsWorld->addRigidBody(rbCube.get());

  auto entity = ex.entities.create();
  entity.assign<SharedbtRigidBody>(rbCube);
  entity.assign<defMotionState>(motionState);
  entity.assign<Cube>(Cube{pos});
  rbCube->setUserIndex(BID_CUBE);
  static_assert(sizeof(void *) == sizeof(uint64_t), "oh...");
  rbCube->setUserPointer((void *)entity.id().id()); // lost any sense of what I learned 'bout good code

  return entity;
}

entityx::Entity Game::createRocket(const glm::vec3 &pos, const glm::vec3 &vel, rtype type) {
  auto motionState = make_shared<btDefaultMotionState>(bttransform(pos));
  auto rbRocket = make_shared<btRigidBody>(btRigidBody::btRigidBodyConstructionInfo(1., motionState.get(), colPoint.get()));

  rbRocket->setLinearVelocity(btcast(vel));
  dynamicsWorld->addRigidBody(rbRocket.get());
  rbRocket->setGravity(btVector3(0, 0, 0)); // after adding to world!

  auto entity = ex.entities.create();
  entity.assign<SharedbtRigidBody>(rbRocket);
  entity.assign<defMotionState>(motionState);
  entity.assign<Rocket>(Rocket{type, true});
  if (type == rtype::falling)
    rbRocket->setAngularVelocity(btVector3(0, 1, 0)); // test
  rbRocket->setUserIndex(BID_ROCKET);
  rbRocket->setUserIndex2(0);//not yet explode
  //rbRocket->setUserPointer((void *)entity.id().id()); // doesn't work...
  return entity;
}

void Game::update(float elapsedSeconds) {
  // update game in 60 Hz fixed timestep
  // fix debugging?
  if (elapsedSeconds > 1)
    elapsedSeconds = 1;

  //activate to be sure
  for (const auto &m : mechs)
    m.rigid->activate();



    for(auto& m : mechs)
        m.tick();


  //update physics
  dynamicsWorld->stepSimulation(elapsedSeconds, 10);

  //explode Rockets

  {
      auto rocket = entityx::ComponentHandle<Rocket>();
      for (auto eRocket : ex.entities.entities_with_components(rocket)){
          auto rigid = eRocket.component<SharedbtRigidBody>()->get();
          if(rigid->getUserIndex2() != 1)
              continue;
          auto motionState = eRocket.component<defMotionState>()->get();
          btTransform trans;
          motionState->getWorldTransform(trans);
          auto pos = getWorldPos(trans);
          if(rocket->real){
              //Sound for not real inside mode? unlikely
              soloud->play3d(sfx, pos.x,pos.y,pos.z); // change
              //boom
              for(const auto& point :spherePoints){
                  auto from = btcast(pos);
                  auto to = btcast(pos + (point * 1.5));//1.5m radius
    #ifndef NDEBUG
                  dynamicsWorld->getDebugDrawer()->drawLine(from, to, btVector4(1, 0, 0, 1));
    #endif
                  auto closest = btCollisionWorld::ClosestRayResultCallback(from, to);
                  dynamicsWorld->rayTest(from, to, closest);
                  if (closest.hasHit()) {
                    auto obj = closest.m_collisionObject;
                    if(! obj->isStaticObject() && ! obj->isKinematicObject() && obj->getInternalType() == btCollisionObject::CO_RIGID_BODY){
                        auto rigidHit = (btRigidBody *)obj; // pray
                        auto dir = (to - from).normalize();
                        auto strength = ((to - closest.m_hitPointWorld).length() / (to - from).length()) * 50; // 100-> get thrown through floor...
                        rigidHit->applyForce(dir*strength, closest.m_hitPointWorld - rigidHit->getWorldTransform().getOrigin());
                        //rigidHit->applyCentralForce(dir * strength);
                    }
                  }
              }
          }
          dynamicsWorld->removeRigidBody(rigid);
          eRocket.destroy();
      }


  }
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

  //update animations
  for (auto &m : mechs)
    m.updateTime(elapsedSeconds);

  // Depth
  {
    auto fb = mFramebufferMode->bind();
    GLOW_SCOPED(enable, GL_DEPTH_TEST);
    GLOW_SCOPED(enable, GL_CULL_FACE);
    GLOW_SCOPED(clearColor, glm::vec3(0, 0, 0));
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    drawCubes(mShaderCubePrepass->use());
    drawRockets(mShaderCubePrepass->use());
    drawMech(mShaderMech->use());

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
    drawRockets(mShaderCube->use());
    drawMech(mShaderMech->use());

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
    mMeshQuad->bind().draw();
  }
}

void Game::drawMech(glow::UsedProgram shader) {
  shader.setUniform("uProj", mCamera->getProjectionMatrix());
  shader.setUniform("uView", mCamera->getViewMatrix());
  for (auto &m : mechs)
    m.draw(shader);
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
          modelCube *= glm::scale(glm::vec3((1 - distanceFactor) * 0.15 + 0.85));
      }
    }
    models.push_back(modelCube);
  }

  auto abModels = mMeshCube->getAttributeBuffer("aModel");
  assert(abModels);
  abModels->bind().setData(models);
  mMeshCube->bind().draw(models.size());
}

void Game::drawRockets(glow::UsedProgram shader) {
  auto proj = mCamera->getProjectionMatrix();
  auto view = mCamera->getViewMatrix();

  shader.setUniform("uProj", proj);
  shader.setUniform("uView", view);

  // model matrices
  vector<glm::mat4> models[NUM_ROCKET_TYPES];
  for (int i = 0; i < NUM_ROCKET_TYPES; i++)
    models[i].reserve(1000);

  auto RocketHandle = entityx::ComponentHandle<Rocket>();
  auto Entities = ex.entities.entities_with_components(RocketHandle);

  for (entityx::Entity entity : Entities) {
    glm::mat4x4 model;
    {
      auto motionState = *entity.component<defMotionState>().get();
      btTransform trans;
      motionState->getWorldTransform(trans);
      trans.getOpenGLMatrix(glm::value_ptr(model));
    }
    //scale here

    models[(int)(RocketHandle->type)].push_back(model);
  }

  for (int i = 0; i < NUM_ROCKET_TYPES; i++) {
    shader.setTexture("uTexAlbedo", mTexRocketAlbedo[i]);
    shader.setTexture("uTexNormal", mTexRocketNormal[i]);

    auto abModels = mMeshRocket[i]->getAttributeBuffer("aModel");
    assert(abModels);
    abModels->bind().setData(models[i]);
    mMeshRocket[i]->bind().draw(models[i].size());
  }
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
      ImGui::Unindent();
    }
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
  //todo move closer to char when looking up

  bool ImGuiwantMouse = false;
#ifndef NDEBUG
  ImGuiwantMouse = ImGui::GetIO().WantCaptureMouse;
#endif

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

    if ((leftMB || rightMB) && !ImGuiwantMouse) {
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
    glm::vec3 target = glcast(mechs[player].rigid->getWorldTransform().getOrigin());
    mCamera->handle.setTarget(target);
    mCamera->handle.setTargetDistance(5);


    if (!ImGuiwantMouse) {
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
