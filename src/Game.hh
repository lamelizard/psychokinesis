#pragma once

#include <glow/fwd.hh>

#include <glow-extras/camera/Camera.hh>
#include <glow-extras/glfw/GlfwApp.hh>

#include <entityx/entityx.h>

#include <btBulletDynamicsCommon.h>
#include "BulletDebugger.hh"

#include "assimpModel.hh"


class Game : public glow::glfw::GlfwApp
{
    // logic
private:
    //glm::vec3 mCubePosition = {-2, 0, 0};
    float mCubeSize = 1.0f;

    glm::vec3 mSpherePosition = {2, 0, 0};
    float mSphereSize = 1.0f;

    // for the cubes
    int cubesWorldMin = -24;
    int cubesWorldMax = 25;


    // gfx settings
private:
    glm::vec3 mBackgroundColor = {.10f, .46f, .83f};
    bool mShowWireframe = false;
    bool mShowPostProcess = false;
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

    // EntityX
private:
    entityx::EntityX ex;

    // Bullet
private:
    bool mDebugBullet = false;
    std::unique_ptr<btBoxShape> colBox;
    std::unique_ptr<btCapsuleShape> colPlayer;
    std::unique_ptr<btDefaultMotionState> playerMotionState;
    std::unique_ptr<btRigidBody>bulPlayer;
    entityx::Entity createCube(const glm::ivec3& pos);
    // main
    std::unique_ptr<BulletDebugger> bulletDebugger; // draws lines for debugging
    std::unique_ptr<btDefaultCollisionConfiguration> collisionConfiguration;
    std::unique_ptr<btCollisionDispatcher> dispatcher;
    std::unique_ptr<btBroadphaseInterface> overlappingPairCache;
    std::unique_ptr<btSequentialImpulseConstraintSolver> solver;
    std::unique_ptr<btDiscreteDynamicsWorld> dynamicsWorld;


    // test
    btDefaultMotionState* boxMotionState = nullptr;


    // ctor
public:
    Game();

    // events
public:
    void init() override;                                             // called once after OpenGL is set up
    void update(float elapsedSeconds) override;                       // called in 60 Hz fixed timestep
    void render(float elapsedSeconds) override;                       // called once per frame (variable timestep)
    void drawMech(glow::UsedProgram shader, float elapsedSeconds);
    void drawCubes(glow::UsedProgram shader);
    void onGui() override;                                            // called once per frame to set up UI
    void onResize(int w, int h) override;                             // called when window is resized
    bool onKey(int key, int scancode, int action, int mods) override; // called when a key is pressed

    void updateCamera(float elapsedSeconds);
};
