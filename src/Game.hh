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


#define MAX_HEALTH 15
#define CUBES_MIN -24
#define CUBES_MAX 25
#define CUBES_TOTAL 50 //(CUBES_MAX - CUBES_MIN + 1)
#define NUM_ROCKET_TYPES 1


//bullet User index:
#define BID_NONE 
#define BID_PLAYER (1 << 0)
#define BID_SMALL (1 << 1)
#define BID_BIG (1 << 2)
#define BID_ROCKET (1 << 3)
#define BID_CUBE (1 << 4)



//rockettype
enum class rtype {
  forward = 0,
  homing = 1,
  falling = 2
};

class Game : public glow::glfw::GlfwApp {
  // bad
public:
  static Game *instance;
  // logic
private:
  bool mJumps = false;
  bool mJumpWasPressed = false; // last frame

  std::vector<glm::vec3> spherePoints;

  // gfx settings
private:
  glm::vec3 mBackgroundColor = {.10f, .46f, .83f};
  bool mShowWireframe = false;
  bool mShowMenu = false;
  bool mFreeCamera = false;

  // gfx objects
private:
  glow::camera::SharedCamera mCamera;

  // shaders
  glow::SharedProgram mShaderOutput;
  glow::SharedProgram mShaderCube;
  glow::SharedProgram mShaderCubePrepass;
  glow::SharedProgram mShaderMode;

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
  glow::SharedTexture2D mTexDefNormal;
  glow::SharedTextureCubeMap mSkybox;
  glow::SharedTexture2D mHealthBar[MAX_HEALTH + 1];
  glow::SharedTexture2D mTexRocketAlbedo[NUM_ROCKET_TYPES];
  glow::SharedTexture2D mTexRocketNormal[NUM_ROCKET_TYPES];

  // depth pre-pass
  glow::SharedTextureRectangle mGBufferDepth;
  glow::SharedFramebuffer mFramebufferDepth;

  // Mode
  glow::SharedTextureRectangle mBufferMode;
  glow::SharedFramebuffer mFramebufferMode;

  // opaque
  glow::SharedTextureRectangle mGBufferAlbedo;
  glow::SharedTextureRectangle mGBufferMaterial; // metallic roughness
  glow::SharedTextureRectangle mGBufferNormal;
  glow::SharedFramebuffer mFramebufferGBuffer;

  // light
  glow::SharedTextureRectangle mBufferLight;
  glow::SharedFramebuffer mFramebufferLight;

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
  //#ifdef NDEBUG
  bool mDebugBullet = false;
  //#else
  //bool mDebugBullet = true;
  //#endif
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
  void drawMech(glow::UsedProgram shader);
  void drawCubes(glow::UsedProgram shader);
  void drawRockets(glow::UsedProgram shader);

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
