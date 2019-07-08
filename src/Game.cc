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


#define NOGUI

GLOW_SHARED(class, btRigidBody);
GLOW_SHARED(class, btMotionState);
using defMotionState = std::shared_ptr<btDefaultMotionState>;

struct AttMat {
  glm::vec4 a, b, c, d;
};

using namespace std;

struct Cube {
  glm::ivec3 pos;
  bool destroyable = false; // floor
  bool moves = false;
};

struct LineVertex {
  glm::vec3 pos;
  glm::vec3 color;
};

struct ExplVertex {
  glm::vec3 pos;
  glm::vec3 normal;
};

struct Rocket {
  rtype type = rtype::forward;
  bool real = true;
  bool explode = false;
  bool willSplit = false; // see banjo-tooie final boss
};

Game *Game::instance = nullptr;

#ifndef NOGUI
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

  setTitle("Psychokinesis - I know how you'll feel"); // N.I. ?

  // start assimp logging
  Assimp::DefaultLogger::create();

  // sphere
  {
      spherePoints.reserve(500);
      if(spherePoints.empty()){
          std::uniform_real_distribution<double> dist(0.0, 1.0);
          std::mt19937 gen (87234587123648); // just guessing
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
    mCamera->setFarPlane(200);
    mCamera->setLookAt({.5, 3, -13}, {.5, 1.5, -10});

    // shadow
    {
        // probably should resize as well but what size?
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &mMaxShadowSize);
        mShadowMapSize = mMaxShadowSize / mCurrentShadowFactor;
        mBufferShadow = glow::TextureRectangle::create(mShadowMapSize, mShadowMapSize, GL_DEPTH_COMPONENT32F);
        mBufferShadow->bind().setWrap(GL_CLAMP_TO_BORDER, GL_CLAMP_TO_BORDER);
        mBufferShadow->bind().setCompareMode(GL_COMPARE_REF_TO_TEXTURE);
        mFramebufferShadow = glow::Framebuffer::createDepthOnly(mBufferShadow);
    }

    // depth
    {
      mTargets.push_back(mGBufferDepth = glow::TextureRectangle::create(1, 1, GL_DEPTH_COMPONENT32F));
      mFramebufferDepth = glow::Framebuffer::createDepthOnly(mGBufferDepth);
    }

    // fusion
    {
      //mTargets.push_back(mBufferFuse = glow::Texture2D::create(1, 1, GL_RGB16F)); // need sampler2D for fxaa
      mBufferFuse = glow::Texture2D::create(1, 1, GL_RGB16F);
      mBufferFuse->bind().setMinFilter(GL_LINEAR); // disable mipmaps
      mFramebufferFuse = glow::Framebuffer::create("fColor", mBufferFuse);
    }

    // mode
    {
        //can't blend integers?
      //mTargets.push_back(mBufferMode = glow::TextureRectangle::create(1, 1, GL_R8I)); // GL_RED_INTEGER misses implementation in glow?
        mTargets.push_back(mBufferMode = glow::TextureRectangle::create(1, 1, GL_R16F));
        mFramebufferMode = glow::Framebuffer::create("fMode", mBufferMode, mGBufferDepth);
    }

    // GBuffer
    {
      // size is 1x1 for now and is changed onResize
      mTargets.push_back(mGBufferAlbedo = glow::TextureRectangle::create(1, 1, GL_RGB16F));
      mTargets.push_back(mGBufferMaterial = glow::TextureRectangle::create(1, 1, GL_RG16F));
      mTargets.push_back(mGBufferNormal = glow::TextureRectangle::create(1, 1, GL_RGB16F));
      auto fix = glow::TextureRectangle::create(1, 1, GL_R16F);
      mTargets.push_back(fix);
      mFramebufferGBuffer = glow::Framebuffer::create(
          {{"fAlbedo", mGBufferAlbedo},

           {"fMaterial", mGBufferMaterial},
           {"fNormal", mGBufferNormal},
           {"fFIXMYBUG", fix} // WHY DOES THIS LINE FIX the last rectangle
                  },
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
      string cubeMetallicFN = texPath + "cube.metallic.png";
      string cubeRoughnessFN = texPath + "cube.roughness.png";
      string smallFN = texPath + "normal.png";
      string bgPath = texPath + "galaxy/";
      string mechAlbedoFN = texPath + "mech.albedo.";
      string mechNormalFN = texPath + "mech.normal.";
      string mechMaterialFN = texPath + "mech.material.";
      string mechModelFN = "../data/models/mech/mech.fbx";
      string healthBarFn = texPath + "ui/Health bar";
      string rocketFN = texPath + "rocket.";
      string paperFN = texPath + "paper.png";
      string noise1FN = texPath + "noise_1.png";

      //loading async
      const auto policy = launch::async;

      auto cubeAlbedoData = async(policy, glow::TextureData::createFromFile, cubeAlbedoFN, glow::ColorSpace::sRGB);
      auto cubeNormalData = async(policy, glow::TextureData::createFromFile, cubeNormalFN, glow::ColorSpace::Linear);
      auto cubeMetallicData = async(policy, glow::TextureData::createFromFile, cubeMetallicFN, glow::ColorSpace::Linear);
      auto cubeRoughnessData = async(policy, glow::TextureData::createFromFile, cubeRoughnessFN, glow::ColorSpace::Linear);
      auto bgData = async(policy, glow::TextureData::createFromFileCube, //
                          bgPath + "right.png",                          //
                          bgPath + "left.png",                           //
                          bgPath + "top.png",                            //
                          bgPath + "bottom.png",                         //
                          bgPath + "front.png",                          //
                          bgPath + "back.png",                           //
                          glow::ColorSpace::sRGB);
      auto mechAlbedoData0 = async(policy, glow::TextureData::createFromFile, mechAlbedoFN + "0.png", glow::ColorSpace::sRGB);
      auto mechNormalData0 = async(policy, glow::TextureData::createFromFile, mechNormalFN + "0.png", glow::ColorSpace::Linear);
      auto mechMaterialData0 = async(policy, glow::TextureData::createFromFile, mechMaterialFN + "0.png", glow::ColorSpace::Linear);
      auto mechAlbedoData1 = async(policy, glow::TextureData::createFromFile, mechAlbedoFN + "1.png", glow::ColorSpace::sRGB);
      auto mechNormalData1 = async(policy, glow::TextureData::createFromFile, mechNormalFN + "1.png", glow::ColorSpace::Linear);
      auto mechMaterialData1 = async(policy, glow::TextureData::createFromFile, mechMaterialFN + "1.png", glow::ColorSpace::Linear);
      auto mechAlbedoData2 = async(policy, glow::TextureData::createFromFile, mechAlbedoFN + "2.png", glow::ColorSpace::sRGB);
      future<glow::SharedTextureData> healthBarData[MAX_HEALTH + 1];
      for (int i = 0; i <= MAX_HEALTH; i++)
        healthBarData[i] = std::async(policy, glow::TextureData::createFromFile, healthBarFn + to_string(i) + ".png", glow::ColorSpace::sRGB);
      future<glow::SharedTextureData> rocketAlbedoData[NUM_ROCKET_TYPES];
      future<glow::SharedTextureData> rocketNormalData[NUM_ROCKET_TYPES];
      future<glow::SharedTextureData> rocketMetallicData[NUM_ROCKET_TYPES];
      future<glow::SharedTextureData> rocketRoughnessData[NUM_ROCKET_TYPES];
      for (int i = 0; i < NUM_ROCKET_TYPES; i++) {
        rocketAlbedoData[i] = async(policy, glow::TextureData::createFromFile, rocketFN + "albedo." + to_string(i) + ".png", glow::ColorSpace::sRGB);
        rocketNormalData[i] = async(policy, glow::TextureData::createFromFile, rocketFN + "normal." + to_string(i) + ".png", glow::ColorSpace::sRGB);
        rocketMetallicData[i] = async(policy, glow::TextureData::createFromFile, rocketFN + "metallic." + to_string(i) + ".png", glow::ColorSpace::sRGB);
        rocketRoughnessData[i] = async(policy, glow::TextureData::createFromFile, rocketFN + "roughness." + to_string(i) + ".png", glow::ColorSpace::sRGB);
      }
      auto paperData = async(policy, glow::TextureData::createFromFile, paperFN, glow::ColorSpace::sRGB);
      auto noise1Data = async(policy, glow::TextureData::createFromFile, noise1FN, glow::ColorSpace::Linear);
      //linear:
      auto mechModelData = async(launch::deferred, AssimpModel::load, mechModelFN); // WHY DOES THIS BREAK WITH ASYNC?
      //not into GL yet, could change
      Mech::mesh = mechModelData.get();

      //into GL
      mTexCubeAlbedo = glow::Texture2D::createFromData(cubeAlbedoData.get());
      mTexCubeAlbedo->setObjectLabel(cubeAlbedoFN);
      mTexCubeNormal = glow::Texture2D::createFromData(cubeNormalData.get());
      mTexCubeNormal->setObjectLabel(cubeNormalFN);
      mTexCubeMetallic = glow::Texture2D::createFromData(cubeMetallicData.get());
      mTexCubeMetallic->setObjectLabel(cubeMetallicFN);
      mTexCubeRoughness = glow::Texture2D::createFromData(cubeRoughnessData.get());
      mTexCubeRoughness->setObjectLabel(cubeRoughnessFN);
      mSkybox = glow::TextureCubeMap::createFromData(bgData.get());
      mSkybox->setObjectLabel(bgPath);
      mechs[player].texAlbedo = glow::Texture2D::createFromData(mechAlbedoData0.get());
      mechs[player].texAlbedo->setObjectLabel(mechAlbedoFN);
      mechs[player].texNormal = glow::Texture2D::createFromData(mechNormalData0.get());
      mechs[player].texNormal->setObjectLabel(mechNormalFN);
      mechs[player].texMaterial = glow::Texture2D::createFromData(mechMaterialData0.get());
      mechs[player].texMaterial->setObjectLabel(mechMaterialFN);
      mechs[small].texAlbedo = glow::Texture2D::createFromData(mechAlbedoData1.get());
      mechs[small].texNormal = glow::Texture2D::createFromData(mechNormalData1.get());
      mechs[small].texMaterial = glow::Texture2D::createFromData(mechMaterialData1.get());
      mechs[big].texAlbedo = glow::Texture2D::createFromData(mechAlbedoData2.get());
      mechs[big].texNormal = mechs[small].texNormal;
      mechs[big].texMaterial = mechs[small].texMaterial;

      for (int i = 0; i <= MAX_HEALTH; i++) {
        mHealthBar[i] = glow::Texture2D::createFromData(healthBarData[i].get());
        mHealthBar[i]->setObjectLabel(healthBarFn);
      }
      for (int i = 0; i < NUM_ROCKET_TYPES; i++) {
        mTexRocketAlbedo[i] = glow::Texture2D::createFromData(rocketAlbedoData[i].get());
        mTexRocketAlbedo[i]->setObjectLabel(rocketFN);
        mTexRocketNormal[i] = glow::Texture2D::createFromData(rocketNormalData[i].get());
        mTexRocketNormal[i]->setObjectLabel(rocketFN);
        mTexRocketMetallic[i] = glow::Texture2D::createFromData(rocketMetallicData[i].get());
        mTexRocketMetallic[i]->setObjectLabel(rocketFN);
        mTexRocketRoughness[i] = glow::Texture2D::createFromData(rocketRoughnessData[i].get());
        mTexRocketRoughness[i]->setObjectLabel(rocketFN);
      }
      mTexPaper = glow::Texture2D::createFromData(paperData.get());
      mTexPaper->setObjectLabel(paperFN);
      mTexNoise1 = glow::Texture2D::createFromData(noise1Data.get());
      mTexNoise1->setObjectLabel(noise1FN);

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
    //Lines
    auto abLines = glow::ArrayBuffer::create();
    abLines->setObjectLabel("Line ab");
    abLines->defineAttributes({{&LineVertex::pos, "position"}, //
                               {&LineVertex::color, "color"}});
    mVALine = glow::VertexArray::create(abLines, GL_LINES);
    mVALine->setObjectLabel("Line va");
    //Explosion
    {
        auto abExpl = glow::ArrayBuffer::create();
        abExpl->setObjectLabel("Explosion ab");
        abExpl->defineAttributes({{&ExplVertex::pos, "position"}, //
                                   {&ExplVertex::normal, "normal"}});
        auto sSize = spherePoints.size() - (spherePoints.size() % 3);
        vector<ExplVertex> ver(sSize);
        for(int i = 0; i < sSize; i+=3){
            ver[i].pos = spherePoints[i];
            ver[i+1].pos = spherePoints[i+1];
            ver[i+2].pos = spherePoints[i+2];
            auto n = normalize(cross(ver[i+1].pos - ver[i].pos, ver[i+2].pos - ver[i].pos));
            ver[i].normal = n;
            ver[i+1].normal = n;
            ver[i+2].normal = n;
        }
        abExpl->bind().setData(ver);
        mVAExplosion = glow::VertexArray::create(abExpl, GL_TRIANGLES);
        mVAExplosion->setObjectLabel("Explosion va");
    }






    //shader
    mShaderCube = glow::Program::createFromFile("../data/shaders/cube");
    mShaderCubePrepass = glow::Program::createFromFile("../data/shaders/cube.pre");
    mShaderOutput = glow::Program::createFromFile("../data/shaders/output");
    mShaderMode = glow::Program::createFromFile("../data/shaders/mode");
    mShaderMech = glow::Program::createFromFile("../data/shaders/mech");
    mShaderUI = glow::Program::createFromFile("../data/shaders/ui");
    mShaderFuse = glow::Program::createFromFile("../data/shaders/fuse");
    mShaderLine = glow::Program::createFromFile("../data/shaders/line");
    mShaderExplosion = glow::Program::createFromFile("../data/shaders/explosion");
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
    //sounds
    {
      const string snddir = "../data/sounds/";
      music.load((snddir + "heroic_demise_loop.ogg").c_str());
      //placeholder
      sfxBootUp.setText("");
      sfxExpl1.load((snddir + "explosion2.wav").c_str());
      sfxExpl1.mVolume = .7;
      sfxExpl2.load((snddir + "explosion2.wav").c_str());
      sfxExpl2.mVolume = .7;
      sfxShot.load((snddir + "Shot.wav").c_str());
      sfxStep.load((snddir + "FootSteps.wav").c_str());
      sfxStep.mVolume = .3;

      intro.setText("You put your mind into a machine? I will have you know your emotions are hackable now.");
      introShort.setText("Your emotions are hackable now.");
    }
  }

  // initialize Bullet
  {
    collisionConfiguration = make_unique<btDefaultCollisionConfiguration>();
    dispatcher = make_unique<btCollisionDispatcher>(collisionConfiguration.get());
    overlappingPairCache = make_unique<btDbvtBroadphase>();
    solver = make_unique<btSequentialImpulseConstraintSolver>();

    colPoint = make_shared<btSphereShape>(0.1);
    colBox = make_shared<btBoxShape>(btVector3(0.5, 0.5, 0.5)); // half extend

   }

  initPhase1();

}

void Game::initPhaseBoth()
{
    soloud->stopAll();

    //bullet
    {
        dynamicsWorld = make_unique<btDiscreteDynamicsWorld>(dispatcher.get(), overlappingPairCache.get(), solver.get(), collisionConfiguration.get());
        dynamicsWorld->setGravity(btVector3(0, -9.81, 0));
        bulletDebugger = make_unique<BulletDebugger>();
        dynamicsWorld->setDebugDrawer(bulletDebugger.get());
        dynamicsWorld->setInternalTickCallback(bulletCallbackStatic);
    }
    //Entity
    ex.entities.reset();
    // mechs
    {
        //player
        // starting to hardcode everything, assuming that I don't ever need to change stuff
        {
    Mech &m = mechs[player];
    m.type = player;
    m.HP = MAX_HEALTH;
    m.blink = 5;
    m.setAction(Mech::startPlayer);
    m.moveDir = glm::vec3(-.1, 0, 1.1);
    m.viewDir = glm::vec3(0, 0, 1.1);
    m.scale = .2;
    m.floatOffset = 0.25;
    m.collision = make_shared<btCapsuleShape>(0.4, 0.5);
    auto groundOffset = m.collision->getHalfHeight() + m.collision->getRadius() + m.floatOffset;
    m.meshOffset = glm::vec3(0, -groundOffset, 0); // -.3
    m.motionState = make_shared<btDefaultMotionState>(bttransform(glm::vec3(0, groundOffset, -10)));
    m.rigid = make_shared<btRigidBody>(btRigidBody::btRigidBodyConstructionInfo(1.f, m.motionState.get(), m.collision.get()));
    m.rigid->setAngularFactor(0);
    m.rigid->setCustomDebugColor(btVector3(255, 1, 1));
    m.rigid->setUserIndex(BID_PLAYER);
    m.rigid->setUserIndex2(0);
    m.rigid->setUserPointer(&mechs[player]);
    dynamicsWorld->addRigidBody(m.rigid.get());
  }

  //small
  {
    Mech &m = mechs[small];
    m.type = small;
    m.HP = 5;
    m.blink = 5;
    m.setAction(Mech::startSmall);
    m.moveDir = glm::vec3(0, 0, -1);
    m.viewDir = glm::vec3(0, 0, -1);
    m.scale = 1;
    m.floatOffset = 0.01;
    m.collision = make_shared<btCapsuleShape>(2, 2.5);
    auto groundOffset = m.collision->getHalfHeight() + m.collision->getRadius() + m.floatOffset;
    m.meshOffset = glm::vec3(0, -groundOffset, 0); // -1.5
    m.motionState = make_shared<btDefaultMotionState>(bttransform(glm::vec3(0, groundOffset, 18.5)));
    m.rigid = make_shared<btRigidBody>(btRigidBody::btRigidBodyConstructionInfo(0, m.motionState.get(), m.collision.get()));
    m.rigid->setAngularFactor(0);
    m.rigid->setCustomDebugColor(btVector3(255, 1, 1));
    m.rigid->setCollisionFlags(m.rigid->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
    m.rigid->setUserIndex(BID_SMALL);
    m.rigid->setUserIndex2(0); // not rushing
    m.rigid->setUserPointer(&mechs[small]);
    dynamicsWorld->addRigidBody(m.rigid.get());

    Mech::nextGoal = 1;
    Mech::currentWay = 0;
    Mech::reachGoalInTicks = Mech::timeNeeded;
    Mech::lastPosition = glm::vec3(0.5, 0, 18.5);
  }
  //big
  {
    Mech &m = mechs[big];
    m.type = big;
    m.HP = 5;
    m.blink = 5;
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
    m.rigid->setUserIndex2(0);
    m.rigid->setUserPointer(&mechs[big]);
    dynamicsWorld->addRigidBody(m.rigid.get());
  }
}

    //boxes
    {

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
        for(int i = 0; i < 4; i++)
            pillars[i].clear();
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

    // test mode area
    {
      auto entity = ex.entities.create();
      entity.assign<ModeArea>(ModeArea{neon, {10, 0, 10}, 5});
    }

      {
        auto entity = ex.entities.create();
        entity.assign<ModeArea>(ModeArea{drawn, {-15, 0, -15}, 10});
      }

      {
        auto entity = ex.entities.create();
        entity.assign<ModeArea>(ModeArea{disco, {-15, 0, 15}, 10});
      }
    // test rocket
    //createRocket(glm::vec3(-23,10,-23), glm::vec3(0,-1,0), rtype::falling);
}

void Game::initPhase1()
{
    initPhaseBoth();

    //music
    music.setLooping(true);
    musicHandle = soloud->playBackground(music, 0);
    soloud->fadeVolume(musicHandle, .7, 30);
    static bool firstRun = true;
    if(firstRun)
        soloud->playBackground(intro, 1);
    else
        soloud->playBackground(introShort, 1);
    firstRun = false;

}

void Game::initPhase2()
{
    initPhaseBoth();
}


void Game::bulletCallback(btDynamicsWorld *, btScalar){
    int numManifolds = dynamicsWorld->getDispatcher()->getNumManifolds();
    for (int i = 0; i < numManifolds; i++) {
        btPersistentManifold *contactManifold = dynamicsWorld->getDispatcher()->getManifoldByIndexInternal(i);
        auto *a = contactManifold->getBody0();
        auto *b = contactManifold->getBody1();
        bool hasRocket = false;
        for(auto o : {a,b})
            if(o->getUserIndex() & BID_ROCKET){
               ((btRigidBody*) o)->setUserIndex2(1);
                hasRocket = true;
            }
        if(hasRocket){
            if(a->getUserIndex() & BID_ROCKET)
                std::swap(a,b);
            if(a->getUserIndex() & BID_PLAYER)
                if(mechs[player].blink > 1){
                mechs[player].HP -= 2;
                mechs[player].blink = 0;
            }

            if(a->getUserIndex() & BID_SMALL){
                if(((btRigidBody*) b)->getUserIndex() == BID_ROCKET_HOMING)
                     ((btRigidBody*) a)->setUserIndex2(SMALL_GOTHIT);
                else
                     ((btRigidBody*) b)->setUserIndex2(0);
            }


        }



    }
}

entityx::Entity Game::createCube(const glm::ivec3 &pos, bool moves) {
  auto motionState = make_shared<btDefaultMotionState>(bttransform(glm::vec3(pos.x, (float)pos.y - colBox->getHalfExtentsWithMargin().getY(), pos.z))); // y = 0 is floor
  auto rbCube = make_shared<btRigidBody>(btRigidBody::btRigidBodyConstructionInfo(moves ? 1. : 0., motionState.get(), colBox.get(), btVector3(0, 0, 0)));
  dynamicsWorld->addRigidBody(rbCube.get());
  bool destructible = false;
  if(!pos.y)
  { // destructible?
      glm::ivec3 p = pos;
      p.x = p.x < 1 ? -p.x + 1 : p.x;
      p.z = p.z < 1 ? -p.z + 1 : p.z;
      // not symmetrical
      destructible = (p.x >= 24 || //
                      p.z >= 24 || //
                      ((p.x == 17 || p.x == 16 || p.x == 5 || p.x == 4) //
                       && (p.z <= 17 && p.z >= 4)) || //
                      ((p.z == 17 || p.z == 16 || p.z == 5 || p.z == 4) //
                       && (p.x <= 17 && p.x >= 4)) //
                      );
      // const auto des = set<int>{25,24,15,14,5,4};
  }

  auto entity = ex.entities.create();
  entity.assign<SharedbtRigidBody>(rbCube);
  entity.assign<defMotionState>(motionState);
  entity.assign<Cube>(Cube{pos, destructible, moves});
  rbCube->setUserIndex(BID_CUBE);
  static_assert(sizeof(void *) == sizeof(uint64_t), "oh...");
  rbCube->setUserPointer((void *)entity.id().id()); // lost any sense of what I learned 'bout good code

  return entity;
}

entityx::Entity Game::createRocket(const glm::vec3 &pos, const glm::vec3 &acc, rtype type) {
    if(mNoAttacks)
        return ex.entities.create(); // hope that doesn't break
  auto motionState = make_shared<btDefaultMotionState>(bttransform(pos));
  auto rbRocket = make_shared<btRigidBody>(btRigidBody::btRigidBodyConstructionInfo(1., motionState.get(), colPoint.get()));
  rbRocket->setLinearVelocity(btcast(glm::normalize(acc)*2));
  dynamicsWorld->addRigidBody(rbRocket.get());
  //rbRocket->setGravity(btVector3(0, 0, 0)); // after adding to world!
  rbRocket->setGravity(btcast(acc));

  auto entity = ex.entities.create();
  entity.assign<SharedbtRigidBody>(rbRocket);
  entity.assign<defMotionState>(motionState);
  entity.assign<Rocket>(Rocket{type, true});
  rbRocket->setUserIndex(BID_ROCKET);
  if(type == rtype::homing){
      rbRocket->setUserIndex(BID_ROCKET_HOMING);
      rbRocket->setDamping(0.2,0);
  }
  if(type == rtype::falling){
      rbRocket->setAngularVelocity(btVector3(0, 1, 0)); // test
      rbRocket->setUserIndex(BID_ROCKET_FALLING);
  }
  rbRocket->setUserIndex2(0);//not yet explode
  //rbRocket->setUserPointer((void *)entity.id().id()); // doesn't work...
  return entity;
}

void Game::update(float) {
  // update game in 60 Hz fixed timestep, most of the time
  setUpdateRate(updateRate);

  //update mechs
  for (auto &m : mechs){
    //activate to be sure
       m.rigid->activate();
       m.tick();
       m.updateLook();
  }

  //cheat into the second quest
  //why doesn't it work?
  if((isKeyPressed(GLFW_KEY_Z) ||
      isKeyPressed(GLFW_KEY_Y)) &&
     //isKeyPressed(GLFW_KEY_E) &&
     //isKeyPressed(GLFW_KEY_L) &&
     //isKeyPressed(GLFW_KEY_D) &&
     isKeyPressed(GLFW_KEY_A)){
     mechs[small].HP = 3;
  }

  //update physics
  dynamicsWorld->stepSimulation(1. / 60., 10);



  //explode Rockets, steer Rockets
  {
      auto rocket = entityx::ComponentHandle<Rocket>();
      for (auto eRocket : ex.entities.entities_with_components(rocket)){
          auto rigid = eRocket.component<SharedbtRigidBody>()->get();
          auto motionState = eRocket.component<defMotionState>()->get();
          btTransform trans;
          motionState->getWorldTransform(trans);
          auto pos = getWorldPos(trans);

          if(length(getWorldPos(rigid->getWorldTransform())) > 200){
              //remove far away rockets
              dynamicsWorld->removeRigidBody(rigid);
              eRocket.destroy();
          }
          else if(rigid->getUserIndex2() == 1)
          {
              //BOOM?
              if(rocket->real){
                  //BOOM!
                  //Sound for not real inside mode? unlikely
                  if(rigid->getUserIndex() != BID_ROCKET_FALLING)
                    soloud->play3d(sfxExpl1, pos.x,pos.y,pos.z);
                  else
                    soloud->play3d(sfxExpl2, pos.x,pos.y,pos.z);
                  explosions.push_back(Explosion{pos, 0});
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
                  //destroy the world
                  if(rigid->getUserIndex() == BID_ROCKET_FALLING){
                      auto cubeHandle = entityx::ComponentHandle<Cube>();
                      for (entityx::Entity entity : ex.entities.entities_with_components(cubeHandle)) {
                          auto c = cubeHandle.get();
                          if(c->destroyable && !c->moves && length(glm::vec3(c->pos) - getWorldPos(rigid->getWorldTransform())) < 5){
                              //destroy and create moving one
                              auto body = entity.component<SharedbtRigidBody>().get()->get();
                              dynamicsWorld->removeRigidBody(body);
                              createCube(c->pos, true);
                              entity.destroy();
                          }
                      }
                  }
              }
              dynamicsWorld->removeRigidBody(rigid);
              eRocket.destroy();
          }else if(rocket->type == rtype::homing){
              //steer
              //trying this ("arrival" method):
              //https://gamedev.stackexchange.com/questions/52988/implementing-a-homing-missile
              //auto a = glm::length(glcast(rigid->getGravity())); // finaly a good use for gravity...
              auto a = homingSpeed;
              auto ppos = mechs[player].getPos();
              auto rpos = ppos - pos;
              auto pvel = glcast(mechs[player].rigid->getLinearVelocity());
              auto vel = glcast(rigid->getLinearVelocity());
              auto rvel = pvel - vel;
              auto v = glm::dot(-rvel, rpos);
              auto eta = -v/a + sqrtf(v*v/(a*a) + 2*(glm::length(rpos))/a); // length was guessed
              auto ipos = ppos + rvel * eta; // impact, assuming player has no acc or change of dir
              auto steerdir = ipos - pos;
              //if(glm::length(steerdir) > a)
                 // steerdir = glm::normalize(steerdir) * a;
              rigid->setGravity(btcast(glm::normalize(steerdir) * a));
              if(pos.y <= 1){
                  btTransform trans = rigid->getWorldTransform();
                  auto p = trans.getOrigin();
                  p.setY(1);
                  trans.setOrigin(p);
                  rigid->setWorldTransform(trans);
                  rigid->setLinearFactor(btVector3(1,0,1));
              }
              //rotate
              auto rotAxis = -normalize(cross(vel, glm::vec3(0,1,0)));
              btQuaternion quat(btcast(rotAxis), min(2.5, (double)pow(length(vel) / 4, 2))); // make this look better
              vec4out.x = quat.getAngle();
              float x, y, z;
              quat.getEulerZYX(z,y,x);
              rigid->setAngularVelocity(btVector3(x,y,z));
           }
      }
  }

  //the dice have been thrown... and may not fall
  {
      auto cubeHandle = entityx::ComponentHandle<Cube>();
      for (entityx::Entity entity : ex.entities.entities_with_components(cubeHandle))
          if(cubeHandle.get()->destroyable){
              auto body = entity.component<SharedbtRigidBody>().get()->get();
              auto pos = getWorldPos(body->getWorldTransform());
              //It's magic, I ain't gotta explain it
              if(pos.y < 7){
                  body->applyCentralForce(btVector3(0,15,0));
                  if(body->getLinearVelocity().getY() < -5)
                      body->applyCentralForce(btVector3(0,5,0));
              }


          }

  }

  //reinit if HP
  if(mechs[player].HP <= 0){
      if(secondPhase)
          initPhase2();
      else
          initPhase1();
      return;
  }

}

//*************************************
// todo: safe normals from one frame and use those for light for some time -> similar to afterimage

void Game::render(float elapsedSeconds) {
  // render game variable timestep
    drawTime += elapsedSeconds;

    // Change display settings
    if(mCurrentSSAAFactor != mSSAAFactor){
        onResize(getWindowWidth(), getWindowHeight());
        glow::log(glow::LogLevel::Info) << "SSAA: " << to_string(mCurrentSSAAFactor);
    }
    if(mCurrentShadowFactor != mShadowFactor){
        mShadowMapSize = mMaxShadowSize / mShadowFactor;
        mCurrentShadowFactor = mShadowFactor;
        mBufferShadow->bind().resize(mShadowMapSize, mShadowMapSize);
        glow::log(glow::LogLevel::Info) << "Shadows: " << to_string(mShadowMapSize) << "^2";
    }

    //dynamicsWorld->stepSimulation( elapsedSeconds); // I want

  // camera update here because it should be coupled tightly to rendering!
  updateCamera(elapsedSeconds);
  elapsedSeconds *= 60. / updateRate; // only camera is free from slowdown

  // get camera matrices
  auto proj = mCamera->getProjectionMatrix();
  auto view = mCamera->getViewMatrix();
  // from glow-samples
  //change parameters!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!1
  glm::mat4 shadowProj = glm::perspective(glm::pi<float>() / 5.0f, 1.0f, 80.0f, 105.0f);
  glm::mat4 shadowView= glm::lookAt(mLightPos, glm::vec3(0.0f), glm::vec3(1, 0, 0));
  glm::mat4 shadowViewProj = shadowProj* shadowView;

  //update animations
  for (auto &m : mechs){
      auto t1 = m.animationsTime[1];
       m.updateTime(elapsedSeconds);
      m.didStep = m.mesh->MechdidStep(t1, m.animationsTime[1]);
  }


  // Shadow
  {
      auto fb = mFramebufferShadow->bind();
      glClear(GL_DEPTH_BUFFER_BIT);
      GLOW_SCOPED(enable, GL_DEPTH_TEST);
      GLOW_SCOPED(enable, GL_CULL_FACE);
      //GLOW_SCOPED(cullFace, GL_FRONT); // bad idea for my cubes
      GLOW_SCOPED(depthFunc, GL_LESS);

      drawCubes(mShaderCubePrepass->use(), shadowProj, shadowView);
      drawRockets(mShaderCubePrepass->use(), shadowProj, shadowView);
      drawMech(mShaderMech->use(), shadowProj, shadowView);
      drawExplosion(mShaderExplosion->use(), shadowProj, shadowView, elapsedSeconds);
  }

  // Depth
  {
    auto fb = mFramebufferMode->bind();
    GLOW_SCOPED(enable, GL_DEPTH_TEST);
    GLOW_SCOPED(enable, GL_CULL_FACE);
    GLOW_SCOPED(clearColor, glm::vec3(0, 0, 0));
    glClear(GL_DEPTH_BUFFER_BIT);

    drawCubes(mShaderCubePrepass->use(), proj, view);
    drawRockets(mShaderCubePrepass->use(), proj, view);
    //drawMech(mShaderMech->use(), proj, view);
    drawExplosion(mShaderExplosion->use(), proj, view);
    if (mDebugBullet) {
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
    GLOW_SCOPED(clearColor, glm::vec3(0,0,0));
    glClear(GL_COLOR_BUFFER_BIT);

    drawCubes(mShaderCube->use(), proj, view);
    drawRockets(mShaderCube->use(), proj, view);
    drawExplosion(mShaderExplosion->use(), proj, view);
    {
        GLOW_SCOPED(enable, GL_CULL_FACE);
        GLOW_SCOPED(depthMask, GL_TRUE);
        GLOW_SCOPED(depthFunc, GL_LESS);
        drawMech(mShaderMech->use(), proj, view);
        drawLines(mShaderLine->use(), proj, view);
        // Render Bullet Debug
        if (mDebugBullet) {
          GLOW_SCOPED(disable, GL_DEPTH_TEST);
          bulletDebugger->draw(proj * view);
        }
    }
  }

  // Mode
  {
    auto fb = mFramebufferMode->bind();
    glClear(GL_COLOR_BUFFER_BIT);
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
      auto modeID = (int32_t)areaHandle->mode;
      shader.setUniform("uMode", modeID);
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

  //screen space:
  {
      GLOW_SCOPED(disable, GL_DEPTH_TEST);
      GLOW_SCOPED(disable, GL_CULL_FACE);
      //fuse
      {
          auto fb = mFramebufferFuse->bind();
          auto shader = mShaderFuse->use();
          shader.setTexture("uTexColor", mGBufferAlbedo);
          shader.setTexture("uTexNormal", mGBufferNormal);
          shader.setTexture("uTexMaterial", mGBufferMaterial);
          shader.setTexture("uTexDepth", mGBufferDepth);
          shader.setTexture("uTexMode", mBufferMode);
          shader.setTexture("uSkybox", mSkybox);
          shader.setTexture("uTexPaper", mTexPaper);
          shader.setTexture("uTexNoise1", mTexNoise1);
          shader.setUniform("uView", view);
          shader.setUniform("uInvProj", inverse(proj));
          shader.setUniform("uInvView", inverse(view));
          shader.setUniform("uZNear", mCamera->getNearClippingPlane());
          shader.setUniform("uZFar", mCamera->getFarClippingPlane());
          shader.setUniform("uCamPos", mCamera->getPosition());
          shader.setUniform("uTime", drawTime);
          //from glow samples
          shader.setUniform("uLightPos", mLightPos);
          shader.setUniform("uTexShadowSize", (float)mShadowMapSize);
          shader.setUniform("uShadowViewProjMatrix", shadowViewProj);
          shader.setTexture("uTexShadow", mBufferShadow);
          mMeshQuad->bind().draw();
      }
      // draw ui // after fxaa too pixely...
      {
          auto health = mechs[player].HP;
          if(health >= 0 && health <= MAX_HEALTH)
          {
              auto fb = mFramebufferFuse->bind();
              auto shader = mShaderUI->use();
              shader.setTexture("uTexHealth", mHealthBar[health]);
              auto model = glm::scale(glm::translate(glm::mat4(), glm::vec3(-.87, -.87, 0)), glm::vec3(.1,.1,1));
              shader.setUniform("uModel", model);
              mMeshQuad->bind().draw();
          }
       }
      // render framebuffer content to output with small post-processing effect
      {
        // draw a fullscreen quad for outputting the framebuffer and applying a post-process
        auto shader = mShaderOutput->use();
        shader.setTexture("uTexColor", mBufferFuse);
        shader.setUniform("uResolution", glm::vec2(mBufferFuse->getWidth(), mBufferFuse->getHeight()));
        shader.setUniform("ufxaaQualitySubpix", fxaaQualitySubpix);
        shader.setUniform("ufxaaQualityEdgeThreshold", fxaaQualityEdgeThreshold);
        shader.setUniform("ufxaaQualityEdgeThresholdMin", fxaaQualityEdgeThresholdMin);
        mMeshQuad->bind().draw();
      }
  }

  bulletDebugger->clearLines();

}

void Game::drawMech(glow::UsedProgram shader, glm::mat4 proj, glm::mat4 view) {
    shader.setUniform("uProj", proj);
    shader.setUniform("uView", view);
    //shader.setTexture("uTexMode", mBufferMode);
  mechs[player].draw(shader);
  if(!secondPhase)
      mechs[small].draw(shader);
  else
      mechs[big].draw(shader);
}

void Game::drawCubes(glow::UsedProgram shader, glm::mat4 proj, glm::mat4 view) {
    shader.setUniform("uProj", proj);
    shader.setUniform("uView", view);

  shader.setTexture("uTexAlbedo", mTexCubeAlbedo);
  shader.setTexture("uTexNormal", mTexCubeNormal);
  shader.setTexture("uTexMetallic", mTexCubeMetallic);
  shader.setTexture("uTexRoughness", mTexCubeRoughness);

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
    modelCube = glm::scale(modelCube, glm::vec3(0.5)); // cube.obj has size 2, +0001 to close gaps -> they are no gaps but z fighting
    //scale down in a mode
    {
      auto trans = translation(modelCube);
      for (const auto &area : scaleAreas) {
        auto distanceFactor = (glm::distance(area.pos, trans) / area.radius);
        if (distanceFactor < 1)
             modelCube = glm::scale(modelCube, glm::vec3(0.95));
      }
    }
    models.push_back(modelCube);
  }

  auto abModels = mMeshCube->getAttributeBuffer("aModel");
  assert(abModels);
  abModels->bind().setData(models);
  mMeshCube->bind().draw(models.size());
}

void Game::drawRockets(glow::UsedProgram shader, glm::mat4 proj, glm::mat4 view) {
  shader.setUniform("uProj", proj);
  shader.setUniform("uView", view);
  //shader.setTexture("uTexMode", mBufferMode);

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
    auto rigid = *entity.component<SharedbtRigidBody>().get();
    switch(RocketHandle->type){
    case rtype::forward:
        model = glm::scale(model, glm::vec3(.5));
        break;
    case rtype::falling:
        model = rotate(model, glm::vec3(0,0,1), glm::vec3(0,-1,0));
    default:
        ;
    }
    if(RocketHandle->type != rtype::homing)
        model = rotate(model, glm::normalize(glcast(rigid->getLinearVelocity())));


    models[(int)(RocketHandle->type)].push_back(model);
  }

  for (int i = 0; i < NUM_ROCKET_TYPES; i++) {
    shader.setTexture("uTexAlbedo", mTexRocketAlbedo[i]);
    shader.setTexture("uTexNormal", mTexRocketNormal[i]);
    shader.setTexture("uTexMetallic", mTexRocketMetallic[i]);
    shader.setTexture("uTexRoughness", mTexRocketRoughness[i]);

    auto abModels = mMeshRocket[i]->getAttributeBuffer("aModel");
    assert(abModels);
    abModels->bind().setData(models[i]);
    mMeshRocket[i]->bind().draw(models[i].size());
  }
}

glm::vec3 HSV2RGB(float h, float s, float v){
    //based on:
    //https://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-range-0-255-for-both
     auto rgb = saturate(glm::vec3(abs(h * 6 - 3) - 1, //
                        2 - abs(h * 6 - 2), //
                        2 - abs(h * 6 - 4)));

    return glm::vec3(((rgb - glm::vec3(1.)) * s + glm::vec3(1.)) * v);
}

void Game::drawLines(glow::UsedProgram shader, glm::mat4 proj, glm::mat4 view)
{

  auto ab = mVALine->getAttributeBuffer("position");
  static bool init = false;
  if(!init){
      vector<LineVertex> lines(spherePoints.size());
      random_shuffle(spherePoints.begin(), spherePoints.end());
      static mt19937_64 rng;
      uniform_real_distribution<double> unif(0, 1);
      for(int i = 0; i < spherePoints.size(); i+=2){
          lines[i] = LineVertex({spherePoints[i], HSV2RGB(unif(rng), 1, 1)});
          lines[i+1] = LineVertex({spherePoints[i+1], HSV2RGB(unif(rng), 1, 1)});
      }
      ab->bind().setData(lines);
      init = true;
   }

  // TODO rotate around center...
  auto area = entityx::ComponentHandle<ModeArea>();
  for (auto entity : ex.entities.entities_with_components(area))
    if (area->mode == neon){
        auto a = *area.get();
        auto model = glm::translate(glm::mat4(), a.pos);
        model = scale(model, glm::vec3(a.radius));
        shader.setUniform("uProjViewModel", proj * view * model);
        mVALine->bind().draw(500 / max(30 - (int)a.radius, 1)); // bigger -> more lines (in 30 steps)
    }

}

void Game::drawExplosion(glow::UsedProgram shader, glm::mat4 proj, glm::mat4 view, float dT)
{
    const auto explosionTime = .3;
    const auto explosionParts = 8;
    const auto explosionRadius = 1.;
    list<Explosion> keepList;
    while(!explosions.empty()){
        Explosion e = explosions.back();
        explosions.pop_back();
        e.time += dT;
        if(e.time > explosionTime)
            continue;
        int part = min((int)((e.time * explosionParts) / explosionTime),7);
        auto model = glm::translate(glm::mat4(), e.pos);
        auto r = explosionRadius * e.time / explosionTime;
        model = scale(model, glm::vec3(r));
        shader.setUniform("uProjView", proj * view);
        shader.setUniform("uModel", model);
        mVAExplosion->bind().drawRange(part * 60, part * 60 + 59);
        keepList.push_back(e);
    }
    explosions = keepList;
}


// Update the GUI
void Game::onGui() {
#ifndef NOGUI
  if (ImGui::Begin("GameDev Project SS19")) {
    ImGui::Text("Settings:");
    {
      ImGui::Indent();
      ImGui::SliderInt("updateRate", &updateRate, 1, 200);
      ImGui::Checkbox("Debug Bullet", &mDebugBullet);
      //ImGui::Checkbox("Show Mech", &mDrawMech);
      ImGui::Checkbox("free Camera", &mFreeCamera);
      ImGui::Checkbox("no attacks", &mNoAttacks);
      ImGui::Unindent();
      //ImGui::SliderFloat3("UI", (float*)&mUIPos, 0.0f, 1.0f);
    }
    ImGui::Text(((string)"Out: " + to_string(vec4out.x) + " " + to_string(vec4out.y)).c_str());
    auto ppos = mechs[player].getPos();
    auto py = ppos.y - mechs[player].collision->getHalfHeight() - mechs[player].collision->getRadius();
    ImGui::Text(((string)"Y: " + to_string(py) + ", Y-f:" + to_string(py - mechs[player].floatOffset)).c_str());
    ImGui::Text(((string)"P: " + to_string(mechs[player].HP) + ", S:" + to_string(mechs[small].HP)).c_str());
    ImGui::Text("Controll:");
    {
      ImGui::Indent();
      ImGui::SliderFloat("High", &jumpGravityHigh, -1, -30);
      ImGui::SliderFloat("Low", &jumpGravityLow, -1, -30);
      ImGui::SliderFloat("Fall", &jumpGravityFall, -1, -30);
      ImGui::SliderFloat("Damping", &damping, 0.1, 1.0);
      ImGui::SliderFloat("MoveForce", &moveForce, 3, 20);
      ImGui::SliderFloat("HomingSpeed", &homingSpeed, .1, 10);
      ImGui::Unindent();
    }
    ImGui::Text("FXAA:");
    {
        ImGui::Indent();
        ImGui::SliderFloat("QualitySubpix", &fxaaQualitySubpix, .5, 1);
        ImGui::SliderFloat("QualityEdgeThreshold", &fxaaQualityEdgeThreshold, 0.333, 0.063);
        ImGui::SliderFloat("QualityEdgeThresholdMin", &fxaaQualityEdgeThresholdMin, 0.0312, 0.0833);
        ImGui::Unindent();
    }
    ImGui::Text("DebugAnimations:");
    {
        ImGui::Text(((string)"Speed: " + to_string(mechs[player].rigid->getLinearVelocity().length())).c_str());
        ImGui::Indent();
        ImGui::Checkbox("Yes?", &DebugingAnimations);
        ImGui::SliderFloat("alpha", &debugAnimationAlpha, 0, 1);
        ImGui::SliderInt3("animations", (int*)&debugAnimations, Mech::run, Mech::sbigB);
        ImGui::SliderFloat3("times", (float*)&debugAnimationTimes, 0, 1.7);
        ImGui::SliderFloat("angle", &debugAnimationAngle, -4, 4);
        ImGui::Unindent();
    }

  }
  ImGui::End();
#endif
}

// Called when window is resized
void Game::onResize(int w, int h) {
  // SSAA
  w *= mSSAAFactor;
  h *= mSSAAFactor;
  mCurrentSSAAFactor = mSSAAFactor;

  // camera viewport size is important for correct projection matrix
  mCamera->setViewportSize(w, h);

  // resize all framebuffer textures
  for (auto const &t : mTargets)
    t->bind().resize(w, h);
  mBufferFuse->bind().resize(w,h);
}

bool Game::onKey(int key, int scancode, int action, int mods) {
  // handle imgui and more
  glow::glfw::GlfwApp::onKey(key, scancode, action, mods);

  // Alt +
  if(action == GLFW_PRESS && (isKeyPressed(GLFW_KEY_LEFT_ALT) || isKeyPressed(GLFW_KEY_RIGHT_ALT)))
      switch(key){
      case GLFW_KEY_ENTER:
          // TODO seems like vsync is of in fullscreen
          toggleFullscreen();
          break;
      case GLFW_KEY_F:
          mFreeCamera = !mFreeCamera;
          break;
      case GLFW_KEY_A:
          mSSAAFactor += .5;
          if(mSSAAFactor > 2)
              mSSAAFactor = 1;
          break;
      case GLFW_KEY_S:
          mShadowFactor *= 2;
          if(mShadowFactor > 4)
              mShadowFactor = 1;
          break;
      default:
          ;
     }

  return false;
}

void Game::updateCamera(float elapsedSeconds) {
  auto const speed = elapsedSeconds * 3;
  //todo move closer to char when looking up

  bool ImGuiwantMouse = false;
#ifndef NOGUI
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
      mCamera->update(elapsedSeconds);
  } else {
      // 1 looks good for cinematic
      //mCamera->handle.sensitivity.distance = 100;
      //mCamera->handle.sensitivity.position = 100;
      //mCamera->handle.sensitivity.rotation = 100;
      //mCamera->handle.sensitivity.target = 100;

      GLFWgamepadstate gamepadState;
      bool hasController = glfwJoystickIsGamepad(GLFW_JOYSTICK_1) && glfwGetGamepadState(GLFW_JOYSTICK_1, &gamepadState);
#ifdef NOGUI
    setCursorMode(glow::glfw::CursorMode::Disabled);
#endif


    auto lastPos = mCamera->handle.getPosition();
    glm::vec3 target = glcast(mechs[player].rigid->getWorldTransform().getOrigin());
    static glm::vec3 lastTarget = target;
    mCamera->handle.move(target - lastTarget);
    mCamera->handle.setTarget(target);
    mCamera->handle.setTargetDistance(2 + 2* sin((lastPos - target).y));
    //mCamera->handle.setTargetDistance(5);
    /*{
        auto dif = (lastPos - lastTarget);
        dif.y = 0;
        dif = normalize(dif);
        auto angle = acos(dot(dif, normalize((lastPos - lastTarget))));
        if(lastPos.y < lastTarget.y)
            angle *= -1;
        mCamera->handle.setTargetDistance(2 + 2* sin(angle));
    }*/

    float dX = 0, dY = 0;
        //get dX, dY
        {
        if (!ImGuiwantMouse) {
          auto mouse_delta = input().getLastMouseDelta() / 100.0f;
          if (mouse_delta.x < 1000){ // ???
              dX += mouse_delta.x;
              dY += mouse_delta.y;
            //mCamera->handle.orbit(mouse_delta.x, mouse_delta.y);
          }
          //pos.y = std::max(0.1f, pos.y);
          //mCamera->handle.setPosition(pos);
          //mCamera->handle.move(glm::vec3(0, std::max(.0, 0.1 - pos.y), 0));

        }
        if(hasController){
            dX += limitAxis( gamepadState.axes[GLFW_GAMEPAD_AXIS_RIGHT_X]) / 15;
            dY += limitAxis(-gamepadState.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]) / 15;
        }
        if(isKeyPressed(GLFW_KEY_KP_4))
            dX -= .1f;
        if(isKeyPressed(GLFW_KEY_KP_6))
            dX += .1f;
        if(isKeyPressed(GLFW_KEY_KP_8))
            dY -= .1f;
        if(isKeyPressed(GLFW_KEY_KP_2))
            dY += .1f;
        }
     mCamera->handle.orbit(dX, dY);


     mCamera->update(elapsedSeconds);
    //mCamera->handle.setTargetDistance(2 + 2* sin((mCamera->getPosition() - target).y));

    lastTarget = target;
  }



  //new camera -> new ears:
  {
      auto pos = mCamera->getPosition();
      auto forw = mCamera->getForwardVector();
      auto up = mCamera->getUpVector();
      soloud->set3dListenerParameters(
                  pos.x, pos.y, pos.z, //
                  forw.x, forw.y, forw.z, //
                  up.x, up.y, up.z, //
                  0, 0, 0);
      soloud->update3dAudio();
  }
}
