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

        // create framebuffer (16bit color + 32bit depth)
        // size is 1x1 for now and is changed onResize
        mTargets.push_back(mTargetColor = glow::TextureRectangle::create(1, 1, GL_RGB16F));
        mTargets.push_back(mTargetDepth = glow::TextureRectangle::create(1, 1, GL_DEPTH_COMPONENT32F));
        mFramebuffer = glow::Framebuffer::create("fColor", mTargetColor, mTargetDepth);
    }

    // load gfx resources
    {
        // color textures are usually sRGB and data textures Linear
        mTexCubeAlbedo = glow::Texture2D::createFromFile("../data/textures/cube.albedo.png", glow::ColorSpace::sRGB);
        mTexCubeNormal = glow::Texture2D::createFromFile("../data/textures/cube.normal.png", glow::ColorSpace::Linear);

        // simple procedural quad with vec2 aPosition
        mMeshQuad = glow::geometry::make_quad();

        // UV sphere
        mMeshSphere = glow::geometry::UVSphere<>(glow::geometry::UVSphere<>::attributesOf(nullptr), 64, 32).generate();

        // cube.obj contains a cube with normals, tangents, and texture coordinates
        mMeshCube = load_mesh_from_obj("../data/meshes/cube.obj", false /* do not interpolate tangents for cubes */);

        // automatically takes .fsh and .vsh shaders and combines them into a program
        mShaderObject = glow::Program::createFromFile("../data/shaders/object");
        mShaderOutput = glow::Program::createFromFile("../data/shaders/output");

        // Models
        mTexMechAlbedo = glow::Texture2D::createFromFile("../data/textures/mech.albedo.png", glow::ColorSpace::sRGB);
        mTexMechNormal = glow::Texture2D::createFromFile("../data/textures/mech.normal.png", glow::ColorSpace::Linear);
        mechModel = AssimpModel::load("../data/models/mech/mech.fbx");
    }
}

void Game::update(float elapsedSeconds)
{
    // update game in 60 Hz fixed timestep
}

void Game::render(float elapsedSeconds)
{
    // render game variable timestep


    // camera update here because it should be coupled tightly to rendering!
    updateCamera(elapsedSeconds);


    // render everything into 16bit framebuffer
    {
        auto fb = mFramebuffer->bind(); // is bound until "fb" goes out-of-scope
        // glViewport is automatically set by framebuffer

        // enable depth test and backface culling for this scope
        GLOW_SCOPED(enable, GL_DEPTH_TEST);
        GLOW_SCOPED(enable, GL_CULL_FACE);

        GLOW_SCOPED(polygonMode, mShowWireframe ? GL_LINE : GL_FILL);

        // clear framebuffer with BG color
        // also clear depth
        // NOTE: depth test must be enable, otherwise glClear does not clear depth
        GLOW_SCOPED(clearColor, mBackgroundColor);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // get camera matrices
        auto proj = mCamera->getProjectionMatrix();
        auto view = mCamera->getViewMatrix();

        // render cube and sphere and
        {
            // build model matrix
            auto modelCube = glm::translate(mCubePosition) * glm::scale(glm::vec3(mCubeSize));
            auto modelSphere = glm::translate(mSpherePosition) * glm::scale(glm::vec3(mSphereSize));
            modelSphere = glm::rotate(modelSphere, glm::radians(-90.f), glm::vec3(0, 1, 0));
            // let light rotate around the objects
            auto lightDir = glm::vec3(glm::cos(getCurrentTime()), 0, glm::sin(getCurrentTime()));

            // set up shader
            auto shader = mShaderObject->use(); // shader is active until scope ends
            shader.setUniform("uProj", proj);
            shader.setUniform("uView", view);
            shader.setUniform("uLightDir", lightDir);

            shader.setTexture("uTexAlbedo", mTexCubeAlbedo);
            shader.setTexture("uTexNormal", mTexCubeNormal);

            // bind and render cube
            shader.setUniform("uModel", modelCube);
            mMeshCube->bind().draw();

            // bind and render sphere
            shader.setUniform("uModel", modelSphere);
            // mMeshSphere->bind().draw();
            shader.setTexture("uTexAlbedo", mTexMechAlbedo);
            shader.setTexture("uTexNormal", mTexMechNormal);
            mechModel->draw(shader, 5, "WalkInPlace");
        }
    }

    // render framebuffer content to output with small post-processing effect
    {
        // no framebuffer is bound, i.e. render-to-screen

        // post-process does not need depth test or culling
        GLOW_SCOPED(disable, GL_DEPTH_TEST);
        GLOW_SCOPED(disable, GL_CULL_FACE);

        // draw a fullscreen quad for outputting the framebuffer and applying a post-process
        auto shader = mShaderOutput->use();
        shader.setTexture("uTexColor", mTargetColor);
        shader.setTexture("uTexDepth", mTargetDepth);
        shader.setUniform("uShowPostProcess", mShowPostProcess);
        mMeshQuad->bind().draw();
    }
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
            ImGui::Checkbox("Show PostProcess", &mShowPostProcess);
            ImGui::ColorEdit3("Background Color", &mBackgroundColor.r);
            ImGui::Unindent();
        }
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
