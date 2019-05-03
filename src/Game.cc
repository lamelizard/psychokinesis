#include "Game.hh"

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

// extra functionality of glow
#include <glow-extras/geometry/Quad.hh>
#include <glow-extras/geometry/UVSphere.hh>

#include <GLFW/glfw3.h> // window/input framework

#include <imgui/imgui.h> // UI framework

#include <glm/glm.hpp> // math library

#include <assimp/DefaultLogger.hpp>

#include "load_mesh.hh" // helper function for loading .obj into VertexArrays

GLOW_SHARED(class, btRigidBody);
GLOW_SHARED(class, btMotionState);
using defMotionState = std::shared_ptr<btDefaultMotionState>;

struct AttMat
{
    glm::vec4 a, b, c, d;
};

using namespace std;

struct Cube
{
    glm::ivec3 pos;
};

enum Mode
{
    normal = 0,
    test = 1
};

struct ModeArea
{
    Mode mode = normal;
    glm::vec3 pos;
    float radius;
};

Game::Game() : GlfwApp(Gui::ImGui) {}

void Game::init()
{
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
        // color textures are usually sRGB and data textures Linear
        mTexCubeAlbedo = glow::Texture2D::createFromFile("../data/textures/cube.albedo.png", glow::ColorSpace::sRGB);
        mTexCubeNormal = glow::Texture2D::createFromFile("../data/textures/cube.normal.png", glow::ColorSpace::Linear);
        mTexDefNormal = glow::Texture2D::createFromFile("../data/textures/normal.png", glow::ColorSpace::Linear);

        // simple procedural quad with vec2 aPosition
        mMeshQuad = glow::geometry::make_quad();

        // UV sphere
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

        // automatically takes .fsh and .vsh shaders and combines them into a program
        mShaderCube = glow::Program::createFromFile("../data/shaders/cube");
        mShaderCubePrepass = glow::Program::createFromFile("../data/shaders/cube.pre");
        mShaderOutput = glow::Program::createFromFile("../data/shaders/output");
        mShaderMode = glow::Program::createFromFile("../data/shaders/mode");

        // Models
        mShaderMech = glow::Program::createFromFile("../data/shaders/mech");
        // mTexMechAlbedo = glow::Texture2D::createFromFile("../data/textures/mech.albedo.png", glow::ColorSpace::sRGB);
        // mTexMechNormal = glow::Texture2D::createFromFile("../data/textures/mech.normal.png", glow::ColorSpace::Linear);
        mechModel = AssimpModel::load("../data/models/mech/mech.fbx");

        // mTexBeholderAlbedo = glow::Texture2D::createFromFile("../data/textures/beholder.png", glow::ColorSpace::sRGB);
        // beholderModel = AssimpModel::load("../data/models/beholder/beholder.gltf");
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

        // test cube + sphere
        // from hello world
        btCollisionShape* groundShape = new btBoxShape(btVector3(btScalar(50.), btScalar(2.), btScalar(50.)));
        btTransform groundTransform;
        groundTransform.setIdentity();
        groundTransform.setOrigin(btVector3(0, -5, 0));
        btVector3 localInertia(0, 0, 0);

        btRigidBody::btRigidBodyConstructionInfo rbInfo(0., nullptr, groundShape, localInertia);
        auto ground = new btRigidBody(rbInfo);
        ground->setWorldTransform(groundTransform);
        dynamicsWorld->addRigidBody(ground);


        colBox = make_unique<btBoxShape>(btVector3(mCubeSize / 2, mCubeSize / 2, mCubeSize / 2)); // half extend
        btRigidBody* bulCube = nullptr;
        btTransform startTransform;
        startTransform.setIdentity();
        localInertia.setX(5);
        colBox->calculateLocalInertia(1., localInertia);
        startTransform.setOrigin(btVector3(0, 10, 0));

        // using motionstate is optional, it provides interpolation capabilities, and only synchronizes 'active' objects
        boxMotionState = new btDefaultMotionState(startTransform);
        btRigidBody::btRigidBodyConstructionInfo rbInfo2(1.f, boxMotionState, colBox.get(), localInertia);
        btRigidBody* body = new btRigidBody(rbInfo2);

        dynamicsWorld->addRigidBody(body);
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
                for (auto yBottom = 0; yBottom <= 5; yBottom++)
                {
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
        entity.assign<ModeArea>(ModeArea{test, {10, 0, 10}, 5});
    }
}

entityx::Entity Game::createCube(const glm::ivec3& pos)
{
    static const btVector3 inertia(0, 0, 0);
    btTransform transform;
    transform.setIdentity();
    transform.setOrigin(btVector3(pos.x, (float)pos.y - colBox->getHalfExtentsWithMargin().getY(), pos.z)); // y = 0 is floor
    auto motionState = make_shared<btDefaultMotionState>(transform);
    auto rbCube = make_shared<btRigidBody>(btRigidBody::btRigidBodyConstructionInfo(0., motionState.get(), colBox.get(), inertia));
    dynamicsWorld->addRigidBody(rbCube.get());

    auto entity = ex.entities.create();
    entity.assign<SharedbtRigidBody>(rbCube);
    entity.assign<defMotionState>(motionState);
    entity.assign<Cube>(Cube{pos});
    return entity;
}

void Game::update(float elapsedSeconds)
{
    // update game in 60 Hz fixed timestep
    // fix debugging?
    if (elapsedSeconds > 1)
        elapsedSeconds = 1;

    // Physics
    // Bullet uses fixed timestep and interpolates
    // -> probably should put this in render as well to get the interpolation
    dynamicsWorld->stepSimulation(elapsedSeconds, 10);
}

void Game::render(float elapsedSeconds)
{
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
        glClear(GL_COLOR_BUFFER_BIT);

        drawCubes(mShaderCube->use());
        drawMech(mShaderMech->use(), elapsedSeconds);

        // Render Bullet Debug
        if (mDebugBullet)
        {
            GLOW_SCOPED(disable, GL_DEPTH_TEST);
            dynamicsWorld->debugDrawWorld();
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
        for (auto entity : areaEntities)
        {
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

        // let light rotate around the objects
        // put higher
        auto lightDir = glm::vec3(glm::cos(getCurrentTime()), 0, glm::sin(getCurrentTime()));
        shader.setUniform("uLightDir", lightDir);
        shader.setUniform("uShowPostProcess", mShowPostProcess);
        mMeshQuad->bind().draw();
    }
}

void Game::drawMech(glow::UsedProgram& shader, float elapsedSeconds)
{
    auto proj = mCamera->getProjectionMatrix();
    auto view = mCamera->getViewMatrix();

    auto modelMech = glm::translate(mSpherePosition) * glm::scale(glm::vec3(mSphereSize));
    modelMech = glm::rotate(modelMech, glm::radians(90.f), glm::vec3(1, 0, 0)); // unity?
    modelMech = glm::mat4();
    shader.setUniform("uProj", proj);
    shader.setUniform("uView", view);
    shader.setUniform("uModel", modelMech);

    // shader.setTexture("uTexAlbedo", mTexMechAlbedo);
    // shader.setTexture("uTexNormal", mTexMechNormal);
    shader.setTexture("uTexAlbedo", mTexDefNormal);
    shader.setTexture("uTexNormal", mTexDefNormal);

    static auto timer = 0;
    timer += elapsedSeconds;
    // mechModel->draw(shader, timer, true, "WalkInPlace");
    // mechModel->draw(shader, debugTime, true, "Hit");
    // skeleton
    // mechModel->debugRenderer.render(proj*view*glm::scale(glm::vec3(0.01)));

    shader.setUniform("uModel", glm::translate(mSpherePosition));
    shader.setTexture("uTexAlbedo", mTexBeholderAlbedo);
    shader.setTexture("uTexNormal", mTexDefNormal);
    // beholderModel->draw(shader, debugTime, true, "ArmaBeholder|wait");
    // beholderModel->debugRenderer.render(proj * view);
}

void Game::drawCubes(glow::UsedProgram& shader)
{
    auto proj = mCamera->getProjectionMatrix();
    auto view = mCamera->getViewMatrix();

    shader.setUniform("uProj", proj);
    shader.setUniform("uView", view);

    shader.setTexture("uTexAlbedo", mTexCubeAlbedo);
    shader.setTexture("uTexNormal", mTexCubeNormal);

    // model matrices
    vector<glm::mat4> models;
    models.reserve(3000);
    auto cubeEntities = ex.entities.entities_with_components(entityx::ComponentHandle<Cube>());
    for (auto entity : cubeEntities)
    {
        auto motionState = *entity.component<defMotionState>().get();
        btTransform trans;
        motionState->getWorldTransform(trans);
        glm::mat4x4 modelCube;
        static_assert(sizeof(AttMat) == sizeof(glm::mat4x4), "bwah");
        trans.getOpenGLMatrix(glm::value_ptr(modelCube));
        modelCube *= glm::scale(glm::vec3(mCubeSize / 2)); // cube.obj has size 2
        models.push_back(modelCube);
    }

    auto abModels = mMeshCube->getAttributeBuffer("aModel");
    assert(abModels);
    abModels->bind().setData(models);
    mMeshCube->bind().draw(models.size());
}


// Update the GUI
void Game::onGui()
{
    if (ImGui::Begin("GameDev Project SS19"))
    {
        ImGui::Text("Objects :");
        {
            ImGui::Indent();
            ImGui::SliderFloat("Cube Size", &mCubeSize, 0.0f, 10.0f);
            ImGui::SliderFloat3("Cube Position", &mCubePosition.x, -5.0f, 5.0f);

            ImGui::Spacing();

            ImGui::SliderFloat("Sphere Radius", &mSphereSize, 0.0f, 10.0f);
            ImGui::SliderFloat3("Sphere Position", &mSpherePosition.x, -5.0f, 5.0f);
            ImGui::Unindent();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::Text("Graphics Settings:");
        {
            ImGui::Indent();
            ImGui::Checkbox("Show Wireframe", &mShowWireframe);
            ImGui::Checkbox("Debug Bullet", &mDebugBullet);
            ImGui::Checkbox("Show PostProcess", &mShowPostProcess);
            ImGui::ColorEdit3("Background Color", &mBackgroundColor.r);
            ImGui::Unindent();
        }
        ImGui::SliderFloat("mechtime", &debugTime, 0.0, 2.0);
    }
    ImGui::End();
}

// Called when window is resized
void Game::onResize(int w, int h)
{
    // camera viewport size is important for correct projection matrix
    mCamera->setViewportSize(w, h);

    // resize all framebuffer textures
    for (auto const& t : mTargets)
        t->bind().resize(w, h);
}

bool Game::onKey(int key, int scancode, int action, int mods)
{
    // handle imgui and more
    glow::glfw::GlfwApp::onKey(key, scancode, action, mods);

    // fullscreen = Alt + Enter
    if (action == GLFW_PRESS && key == GLFW_KEY_ENTER)
        if (isKeyPressed(GLFW_KEY_LEFT_ALT) || isKeyPressed(GLFW_KEY_RIGHT_ALT))
            toggleFullscreen();

    return false;
}

void Game::updateCamera(float elapsedSeconds)
{
    auto const speed = elapsedSeconds * 3;

    // WASD camera move
    glm::vec3 rel_move;
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

    if ((leftMB || rightMB) && !ImGui::GetIO().WantCaptureMouse)
    {
        // hide mouse
        setCursorMode(glow::glfw::CursorMode::Disabled);

        auto mouse_delta = input().getLastMouseDelta() / 100.0f;

        if (leftMB && rightMB)
            rel_move += glm::vec3(mouse_delta.x, 0, mouse_delta.y);
        else if (leftMB)
            mCamera->handle.orbit(mouse_delta.x, mouse_delta.y);
        else if (rightMB)
            mCamera->handle.lookAround(mouse_delta.x, mouse_delta.y);
    }
    else
        setCursorMode(glow::glfw::CursorMode::Normal);

    // move camera handle (position), accepts relative moves
    mCamera->handle.move(rel_move);

    // Camera is smoothed
    mCamera->update(elapsedSeconds);
}
