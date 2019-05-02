#pragma once

#include <glow/fwd.hh>

#include <glow-extras/camera/Camera.hh>
#include <glow-extras/glfw/GlfwApp.hh>


#include <btBulletDynamicsCommon.h>
#include "BulletDebugger.hh"

#include "assimpModel.hh"


class Game : public glow::glfw::GlfwApp
{
    // logic
private:
    glm::vec3 mCubePosition = {-2, 0, 0};
    float mCubeSize = 1.0f;

    glm::vec3 mSpherePosition = {2, 0, 0};
    float mSphereSize = 1.0f;

    // gfx settings
private:
    glm::vec3 mBackgroundColor = {.10f, .46f, .83f};
    bool mShowWireframe = false;
    bool mShowPostProcess = false;

    // gfx objects
private:
    glow::camera::SharedCamera mCamera;

    // shaders
    glow::SharedProgram mShaderOutput;
    glow::SharedProgram mShaderObject;

    // meshes
    glow::SharedVertexArray mMeshQuad;
    glow::SharedVertexArray mMeshCube;
    glow::SharedVertexArray mMeshSphere;

    // mech
    glow::SharedProgram mShaderMech;
    glow::SharedTexture2D mTexMechAlbedo;
    glow::SharedTexture2D mTexMechNormal;
    SharedAssimpModel mechModel;
    //beholder
    glow::SharedTexture2D mTexBeholderAlbedo;
    SharedAssimpModel beholderModel;
    float debugTime = 0;

    // textures
    glow::SharedTexture2D mTexCubeAlbedo;
    glow::SharedTexture2D mTexCubeNormal;
    glow::SharedTexture2D mTexDefNormal;

    // intermediate framebuffer with color and depth texture
    glow::SharedFramebuffer mFramebuffer;
    glow::SharedTextureRectangle mTargetColor;
    glow::SharedTextureRectangle mTargetDepth;

    std::vector<glow::SharedTextureRectangle> mTargets;

    // Bullet
private:
    bool mDebugBullet = true;
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
    void init() override;                       // called once after OpenGL is set up
    void update(float elapsedSeconds) override; // called in 60 Hz fixed timestep
    void render(float elapsedSeconds) override; // called once per frame (variable timestep)
    void onGui() override;                      // called once per frame to set up UI

    void onResize(int w, int h) override;                             // called when window is resized
    bool onKey(int key, int scancode, int action, int mods) override; // called when a key is pressed

    void updateCamera(float elapsedSeconds);
};
