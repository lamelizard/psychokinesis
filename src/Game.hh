#pragma once

#include <vector>

#include <glow/fwd.hh>

#include <glow-extras/camera/Camera.hh>
#include <glow-extras/glfw/GlfwApp.hh>

#include <entityx/entityx.h>

#include <btBulletDynamicsCommon.h>
#include "BulletDebugger.hh"

#include <soloud.h>
#include <soloud_wavstream.h>
#include <soloud_speech.h>

#include "Mech.hh"

enum Mode {
  normal = 0,
  drawn = 1,
  fake = 2,
  fast = 3
};

struct ModeArea {
  Mode mode = normal;
  glm::vec3 pos;
  float radius;
};

//rockettype
enum class rtype {
  forward = 0,
  homing = 1,
  falling = 2
};

#define MAX_HEALTH 15
#define CUBES_MIN -24
#define CUBES_MAX 25
#define CUBES_TOTAL 50 //(CUBES_MAX - CUBES_MIN + 1)
#define NUM_ROCKET_TYPES 3


//bullet User index:
#define BID_NONE 
#define BID_PLAYER (1 << 0)
#define BID_SMALL (1 << 1)
#define BID_BIG (1 << 2)
#define BID_ROCKET (1 << 3)
#define BID_CUBE (1 << 4)
#define BID_ROCKET_HOMING ((1 << 6) | BID_ROCKET)
#define BID_ROCKET_FALLING ((1 << 7) | BID_ROCKET)

#define SMALL_NONE 0
#define SMALL_RUN (1 << 0) // run at player
#define SMALL_GOTHIT (1 << 1) // by Rocket

inline double limitAxis(double a) { return a < .1 && a > -.1? 0 : a; }

class Game : public glow::glfw::GlfwApp {
    friend class Mech;
  // bad
public:
  static Game *instance;
  // logic
private:
  int updateRate = 60;
  bool mJumps = false;
  bool mJumpWasPressed = false; // last frame
  float jumpGravityHigh = -8;
  float jumpGravityLow = -15;
  float jumpGravityFall = -18;
  float damping = .95;
  float moveForce = 15;

  std::vector<glm::vec3> spherePoints;

  // gfx settings
private:
  glm::vec3 mtestVec = {0,0,0};
  bool mShowWireframe = false;
  bool mShowMenu = false;
  bool mFreeCamera = false;
  bool mNoAttacks = false;
  int mShadowMapSize = 16384 / 4; // urks, high
  glm::vec3 mLightPos = {0,100,0};
  float fxaaQualitySubpix = .75;
  float fxaaQualityEdgeThreshold = .166;
  float fxaaQualityEdgeThresholdMin = .0833;
  bool DebugingAnimations = false;
  float debugAnimationAlpha = 0;
  glm::ivec3 debugAnimations = {0,0,0};
  glm::vec3 debugAnimationTimes = {0,0,0};
  float debugAnimationAngle = 0;

  // gfx objects
private:
  glow::camera::SharedCamera mCamera;

  // shaders
  glow::SharedProgram mShaderOutput;
  glow::SharedProgram mShaderFuse; // was a word with C
  glow::SharedProgram mShaderCube;
  glow::SharedProgram mShaderCubePrepass;
  glow::SharedProgram mShaderMode;
  glow::SharedProgram mShaderUI;

  // meshes
  glow::SharedVertexArray mMeshQuad;
  glow::SharedVertexArray mMeshCube;
  glow::SharedVertexArray mMeshSphere;
  glow::SharedVertexArray mMeshRocket[NUM_ROCKET_TYPES];

  // mech
  glow::SharedProgram mShaderMech;
  Mech mechs[3];

  // textures
  glow::SharedTexture2D mTexCubeAlbedo;
  glow::SharedTexture2D mTexCubeNormal;
  glow::SharedTexture2D mTexCubeMetallic;
  glow::SharedTexture2D mTexCubeRoughness;
  glow::SharedTexture2D mTexDefNormal;
  glow::SharedTexture2D mTexDefMaterial;
  glow::SharedTextureCubeMap mSkybox;
  glow::SharedTexture mHealthBar[MAX_HEALTH + 1];
  glow::SharedTexture2D mTexRocketAlbedo[NUM_ROCKET_TYPES];
  glow::SharedTexture2D mTexRocketNormal[NUM_ROCKET_TYPES];
  glow::SharedTexture2D mTexRocketMetallic[NUM_ROCKET_TYPES];
  glow::SharedTexture2D mTexRocketRoughness[NUM_ROCKET_TYPES];
  glow::SharedTexture2D mTexPaper;
  glow::SharedTexture2D mTexNoise1;

  // Shadow
  glow::SharedTextureRectangle mBufferShadow;
  glow::SharedFramebuffer mFramebufferShadow;

  // depth pre-pass
  glow::SharedTextureRectangle mGBufferDepth;
  glow::SharedFramebuffer mFramebufferDepth;

  // Mode
  glow::SharedTextureRectangle mBufferMode;
  glow::SharedFramebuffer mFramebufferMode;

  // opaque
  glow::SharedTextureRectangle mGBufferAlbedo;
  glow::SharedTextureRectangle mGBufferMaterial;
  glow::SharedTextureRectangle mGBufferNormal;
  glow::SharedFramebuffer mFramebufferGBuffer;

  // light
  glow::SharedTextureRectangle mBufferLight;
  glow::SharedFramebuffer mFramebufferLight;

  //fusing
  glow::SharedTexture2D mBufferFuse;
  glow::SharedFramebuffer mFramebufferFuse;

  std::vector<glow::SharedTextureRectangle> mTargets;


  // Sound
private:
  std::unique_ptr<SoLoud::Soloud, void (*)(SoLoud::Soloud *)> soloud = //
      std::unique_ptr<SoLoud::Soloud, void (*)(SoLoud::Soloud *)>(nullptr, [](SoLoud::Soloud *) {});
  SoLoud::WavStream music;
  SoLoud::handle musicHandle;
  SoLoud::Speech sfx;

  // EntityX
private:
  entityx::EntityX ex;
  entityx::Entity ground[CUBES_TOTAL][CUBES_TOTAL]; // x, z
  std::vector<entityx::Entity> pillars[4];

  // Bullet
private:
  bool mDebugBullet = false;
  std::shared_ptr<btBoxShape> colBox;
  std::shared_ptr<btSphereShape> colPoint;
  void bulletCallback(btDynamicsWorld*, btScalar);
  static void bulletCallbackStatic(btDynamicsWorld* w, btScalar c){instance->bulletCallback(w,c);}
  entityx::Entity createCube(const glm::ivec3 &pos);
  entityx::Entity createRocket(const glm::vec3 &pos, const glm::vec3 &vel, rtype type);


  // main
  std::unique_ptr<BulletDebugger> bulletDebugger; // draws lines for debugging
  std::unique_ptr<btDefaultCollisionConfiguration> collisionConfiguration;
  std::unique_ptr<btCollisionDispatcher> dispatcher;
  std::unique_ptr<btBroadphaseInterface> overlappingPairCache;
  std::unique_ptr<btSequentialImpulseConstraintSolver> solver;
  std::unique_ptr<btDiscreteDynamicsWorld> dynamicsWorld;



  //draw
private:
  void drawMech(glow::UsedProgram shader, glm::mat4 proj, glm::mat4 view);
  void drawCubes(glow::UsedProgram shader, glm::mat4 proj, glm::mat4 view);
  void drawRockets(glow::UsedProgram shader, glm::mat4 proj, glm::mat4 view);

  // test
  //btDefaultMotionState* boxMotionState = nullptr;


  // ctor
public:
  Game();

  // events
public:
  void init() override;                                             // called once after OpenGL is set up
  void update(float elapsedSeconds) override;                       // called in 60 Hz fixed timestep
  void render(float elapsedSeconds) override;                       // called once per frame (variable timestep)
  void onGui() override;                                            // called once per frame to set up UI
  void onResize(int w, int h) override;                             // called when window is resized
  bool onKey(int key, int scancode, int action, int mods) override; // called when a key is pressed

  void updateCamera(float elapsedSeconds);
};
