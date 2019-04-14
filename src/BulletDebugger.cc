#include "BulletDebugger.hh"

#include <glow/common/log.hh>
#include <glow/common/scoped_gl.hh>
#include <glow/objects/ArrayBuffer.hh>
#include <glow/objects/Program.hh>
#include <glow/objects/VertexArray.hh>

using namespace glow;

BulletDebugger::BulletDebugger()
{
    mLineShader = Program::createFromFile("../data/shaders/line");
}

BulletDebugger::~BulletDebugger() {}

void BulletDebugger::draw(glm::mat4 pPVMatrix)
{
    GLOW_SCOPED(enable, GL_DEPTH_TEST);
    GLOW_SCOPED(disable, GL_CULL_FACE);

    auto shader = mLineShader->use();
    shader.setUniform("uProjView", pPVMatrix);

    // bad? should reuse maybe?
    // but only used for debugging so whatever
    auto ab = ArrayBuffer::create();
    ab->setObjectLabel("Bullet");
    ab->defineAttributes({{&Vertex::pos, "position"}, //
                          {&Vertex::color, "color"}});
    ab->bind().setData(lines);

    auto va = VertexArray::create(ab, GL_LINES);
    va->setObjectLabel("Bullet");
    va->bind().draw();
}

void BulletDebugger::drawLine(const btVector3& from, const btVector3& to, const btVector3& color)
{
    // cache lines
    lines.push_back({{from.getX(), from.getY(), from.getZ()}, {color.getX(), color.getY(), color.getZ()}});
    lines.push_back({{to.getX(), to.getY(), to.getZ()}, {color.getX(), color.getY(), color.getZ()}});
}

void BulletDebugger::reportErrorWarning(const char* str)
{
    glow::log(glow::LogLevel::Warning) << "Bullet: " << str << "\n";
}

void BulletDebugger::setDebugMode(int p)
{
    mode = p;
}

int BulletDebugger::getDebugMode(void) const
{
    return mode;
}

void BulletDebugger::clearLines()
{
    lines.clear();
}
