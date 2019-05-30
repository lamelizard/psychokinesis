#pragma once

#include <glow/fwd.hh>

#include <glow-extras/camera/Camera.hh>
#include <glow-extras/glfw/GlfwApp.hh>

#include <entityx/entityx.h>

#include <btBulletDynamicsCommon.h>
#include "BulletDebugger.hh"

#include "assimpModel.hh"

#include <soloud.h>

class Game : public glow::glfw::GlfwApp {
  // logic
private:
  bool mJumps = false;
  bool mJumpWasPressed = false; // last frame
  int HP = 5;



  //glm::vec3 mSpherePosition = {2, 0, 0};
  //float mSphereSize = 1.0f;

  // for the cubes
  int cubesWorldMin = -24;
  int cubesWorldMax = 25;


  // gfx settings
private:
  glm::vec3 mBackgroundColor = {.10f, .46f, .83f};
  bool mShowWireframe = false;
  bool mShowPostProcess = false;
  bool mShowMenu = false;
  bool mFreeCamera = false;
  bool mDrawMech = false;

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

  // mech
  glow::SharedProgram mShaderMech;
  glow::SharedTexture2D mTexMechAlbedo;
  glow::SharedTexture2D mTexMechNormal;
  SharedAssimpModel mechModel;
  // beholder
  glow::SharedTexture2D mTexBeholderAlbedo;
  SharedAssimpModel beholderModel;
  float debugTime = 0;

  // textures
  glow::SharedTexture2D mTexCubeAlbedo;
  glow::SharedTexture2D mTexCubeNormal;
  glow::SharedTexture2D mTexDefNormal;
  glow::SharedTextureCubeMap mSkybox;

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

  // EntityX
private:
  entityx::EntityX ex;

  // Bullet
private:
#ifdef NDEBUG
  bool mDebugBullet = false;
#else
  bool mDebugBullet = true;
#endif
  std::unique_ptr<btBoxShape> colBox;
  std::unique_ptr<btCapsuleShape> colPlayer;
  std::shared_ptr<btDefaultMotionState> playerMotionState;
  std::shared_ptr<btRigidBody> bulPlayer;
  entityx::Entity createCube(const glm::ivec3 &pos);

  // main
  std::unique_ptr<BulletDebugger> bulletDebugger; // draws lines for debugging
  std::unique_ptr<btDefaultCollisionConfiguration> collisionConfiguration;
  std::unique_ptr<btCollisionDispatcher> dispatcher;
  std::unique_ptr<btBroadphaseInterface> overlappingPairCache;
  std::unique_ptr<btSequentialImpulseConstraintSolver> solver;
  std::unique_ptr<btDiscreteDynamicsWorld> dynamicsWorld;


  // test
  //btDefaultMotionState* boxMotionState = nullptr;


  // ctor
public:
  Game();

  // events
public:
  void init() override;                       // called once after OpenGL is set up
  void update(float elapsedSeconds) override; // called in 60 Hz fixed timestep
  void render(float elapsedSeconds) override; // called once per frame (variable timestep)
  void drawMech(glow::UsedProgram shader, float elapsedSeconds);
  void drawCubes(glow::UsedProgram shader);
  void onGui() override;                                            // called once per frame to set up UI
  void onResize(int w, int h) override;                             // called when window is resized
  bool onKey(int key, int scancode, int action, int mods) override; // called when a key is pressed

  void updateCamera(float elapsedSeconds);
};
