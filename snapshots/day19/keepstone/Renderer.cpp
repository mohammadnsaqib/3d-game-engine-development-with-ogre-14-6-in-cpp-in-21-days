// Keepstone -- Renderer.cpp
//
// THE ONLY FILE IN THE ENGINE THAT INCLUDES OGRE. Everything Days 1 to 3 wrote in one main()
// lives here now, behind the interface in Renderer.h.
//
// Nothing about the renderer got better by moving. The same Root is created, the same hidden
// window, the same resource groups, the same RTSS wiring, and the frame that comes out is the
// same frame -- proved, byte for byte, by the two programs in this snapshot. What changed is
// who has to know about it.

#include "Renderer.h"

#include <Ogre.h>
#include <OgreRTShaderSystem.h>
// OGRE 14.6's gizmos. Included HERE and nowhere else: see Renderer.h on why the
// game never learns that this type exists.
#include <OgreGizmos.h>

#include <memory>
#include <vector>

namespace keepstone {

namespace {

// The RTSS resolver from Day 1, unchanged. It is engine furniture now rather than something the
// reader wires up in main(), which is exactly the point of today.
class ShaderResolver : public Ogre::MaterialManager::Listener
{
public:
    explicit ShaderResolver(Ogre::RTShader::ShaderGenerator* gen) : mGen(gen) {}

    Ogre::Technique* handleSchemeNotFound(unsigned short, const Ogre::String& schemeName,
                                          Ogre::Material* mat, unsigned short,
                                          const Ogre::Renderable*) override
    {
        if (schemeName != Ogre::MSN_SHADERGEN) return nullptr;
        if (!mGen->createShaderBasedTechnique(*mat, Ogre::MSN_DEFAULT, schemeName))
            return nullptr;
        mGen->validateMaterial(schemeName, mat->getName(), mat->getGroup());
        return mat->getTechnique(1);
    }

private:
    Ogre::RTShader::ShaderGenerator* mGen;
};

Ogre::Vector3 toOgre(Vec3 v) { return Ogre::Vector3(v.x, v.y, v.z); }
Ogre::ColourValue toOgre(Colour c) { return Ogre::ColourValue(c.r, c.g, c.b); }

}  // namespace

// Every OGRE member the engine owns. This struct is declared in the header as an incomplete
// type, so a gameplay file can hold a Renderer without OGRE existing as far as it is concerned.
struct Renderer::Impl
{
    unsigned width = 1200;
    unsigned height = 900;
    bool up = false;
    Ogre::Root* root = nullptr;
    Ogre::SceneManager* scene = nullptr;
    Ogre::Camera* camera = nullptr;
    Ogre::RenderTexture* target = nullptr;
    Ogre::SceneNode* camNode = nullptr;
    std::vector<Ogre::SceneNode*> nodes;   // ObjectId is an index into this
    Ogre::Viewport* viewport = nullptr;    // the gizmos need it to turn pixels into rays
    std::unique_ptr<OgreBites::Gizmo> gizmo;
    std::unique_ptr<OgreBites::CameraGizmo> cameraGizmo;
};

Renderer::Renderer() : mImpl(new Impl) {}

Renderer::~Renderer() { delete mImpl; }

void Renderer::setSize(unsigned width, unsigned height)
{
    mImpl->width = width;
    mImpl->height = height;
}

bool Renderer::isUp() const { return mImpl->up; }

bool Renderer::startUp()
{
    const unsigned width = mImpl->width;
    const unsigned height = mImpl->height;
    Ogre::LogManager* logMgr = new Ogre::LogManager();
    logMgr->createLog("lanternkeep.log", true, false, false);

    mImpl->root = new Ogre::Root("", "", "");
    mImpl->root->loadPlugin(std::string(LK_OGRE_PLUGINS) + "/RenderSystem_Direct3D11");
    mImpl->root->loadPlugin(std::string(LK_OGRE_PLUGINS) + "/Codec_STBI");

    Ogre::RenderSystem* rs =
        mImpl->root->getRenderSystemByName("Direct3D11 Rendering Subsystem");
    rs->setConfigOption("Video Mode", "640 x 480 @ 32-bit colour");
    mImpl->root->setRenderSystem(rs);
    mImpl->root->initialise(false, "Lantern Keep");

    Ogre::NameValuePairList params;
    params["hidden"] = "true";
    Ogre::RenderWindow* hidden =
        mImpl->root->createRenderWindow("hidden", 64, 64, false, &params);
    hidden->setActive(false);

    Ogre::ResourceGroupManager& rgm = Ogre::ResourceGroupManager::getSingleton();
    const std::string media = LK_OGRE_MEDIA;
    rgm.addResourceLocation(media + "/Main",        "FileSystem", Ogre::RGN_INTERNAL);
    rgm.addResourceLocation(media + "/RTShaderLib", "FileSystem", Ogre::RGN_INTERNAL);
    rgm.addResourceLocation(LK_BOOK_MEDIA,          "FileSystem", Ogre::RGN_DEFAULT);
    rgm.initialiseAllResourceGroups();

    mImpl->scene = mImpl->root->createSceneManager();

    Ogre::RTShader::ShaderGenerator::initialize();
    Ogre::RTShader::ShaderGenerator* gen =
        Ogre::RTShader::ShaderGenerator::getSingletonPtr();
    gen->addSceneManager(mImpl->scene);
    Ogre::MaterialManager::getSingleton().addListener(new ShaderResolver(gen));

    mImpl->camera = mImpl->scene->createCamera("view");
    mImpl->camera->setNearClipDistance(0.5f);
    mImpl->camera->setAutoAspectRatio(true);
    mImpl->camNode = mImpl->scene->getRootSceneNode()->createChildSceneNode();
    mImpl->camNode->attachObject(mImpl->camera);

    Ogre::TexturePtr tex = Ogre::TextureManager::getSingleton().createManual(
        "keepstone/shot", Ogre::RGN_DEFAULT, Ogre::TEX_TYPE_2D,
        width, height, 0, Ogre::PF_B8G8R8A8, Ogre::TU_RENDERTARGET);
    mImpl->target = tex->getBuffer()->getRenderTarget();
    Ogre::Viewport* vp = mImpl->target->addViewport(mImpl->camera);
    mImpl->viewport = vp;
    vp->setBackgroundColour(Ogre::ColourValue(0.05f, 0.06f, 0.09f));
    vp->setMaterialScheme(Ogre::MSN_SHADERGEN);

    mImpl->up = true;
    return true;
}

void Renderer::shutDown()
{
    if (mImpl->root) mImpl->root->shutdown();
    mImpl->up = false;
}

void Renderer::setAmbient(Colour light)
{
    mImpl->scene->setAmbientLight(toOgre(light));
}

void Renderer::setCamera(Vec3 eye, Vec3 target)
{
    mImpl->camNode->setPosition(toOgre(eye));
    mImpl->camNode->lookAt(toOgre(target), Ogre::Node::TS_WORLD);
}

ObjectId Renderer::addBlock(Vec3 position, Vec3 scale, float yawDegrees)
{
    Ogre::Entity* e = mImpl->scene->createEntity(Ogre::SceneManager::PT_CUBE);
    Ogre::SceneNode* n = mImpl->scene->getRootSceneNode()->createChildSceneNode();
    n->attachObject(e);
    n->setPosition(toOgre(position));
    n->setScale(toOgre(scale));
    n->yaw(Ogre::Degree(yawDegrees));
    mImpl->nodes.push_back(n);
    return static_cast<ObjectId>(mImpl->nodes.size() - 1);
}

ObjectId Renderer::addLantern(Vec3 position, Colour light)
{
    Ogre::Light* l = mImpl->scene->createLight();
    l->setType(Ogre::Light::LT_POINT);
    l->setDiffuseColour(toOgre(light));
    l->setSpecularColour(Ogre::ColourValue(1.0f, 0.95f, 0.80f));
    Ogre::SceneNode* n = mImpl->scene->getRootSceneNode()->createChildSceneNode();
    n->setPosition(toOgre(position));
    n->attachObject(l);
    mImpl->nodes.push_back(n);
    return static_cast<ObjectId>(mImpl->nodes.size() - 1);
}

void Renderer::setPosition(ObjectId id, Vec3 position)
{
    if (id < mImpl->nodes.size()) mImpl->nodes[id]->setPosition(toOgre(position));
}

Vec3 Renderer::positionOf(ObjectId id) const
{
    if (id >= mImpl->nodes.size()) return Vec3();
    const Ogre::Vector3 p = mImpl->nodes[id]->getPosition();
    return Vec3{ p.x, p.y, p.z };
}

void Renderer::showGizmo(ObjectId id, GizmoKind kind)
{
    if (kind == GizmoKind::None || id >= mImpl->nodes.size())
    {
        mImpl->gizmo.reset();
        return;
    }
    OgreBites::GizmoOperation op = OgreBites::GO_TRANSLATE;
    if (kind == GizmoKind::Rotate) op = OgreBites::GO_ROTATE;
    if (kind == GizmoKind::Scale)  op = OgreBites::GO_SCALE;

    // Rebuilt rather than retargeted, because a Gizmo is cheap and this keeps the
    // one place that knows about OGRE's construction order down to a single line.
    mImpl->gizmo.reset(new OgreBites::Gizmo(mImpl->nodes[id], op, mImpl->viewport));
}

void Renderer::showCameraGizmo(bool on)
{
    if (!on) { mImpl->cameraGizmo.reset(); return; }
    mImpl->cameraGizmo.reset(new OgreBites::CameraGizmo(mImpl->viewport));
}

bool Renderer::dragGizmo(float fromX, float fromY, float toX, float toY)
{
    if (!mImpl->gizmo) return false;

    // A DRAG IS THREE EVENTS, and all three have to arrive for the gizmo to do
    // anything: the press decides which axis was grabbed, the motion moves along it,
    // the release lets go. Sending only the motion -- which is what a first attempt at
    // this did -- produces a perfectly valid frame in which nothing has moved, because
    // nothing was ever grabbed.
    OgreBites::MouseButtonEvent down{};
    down.type = OgreBites::MOUSEBUTTONDOWN;
    down.button = OgreBites::BUTTON_LEFT;
    down.x = static_cast<int>(fromX);
    down.y = static_cast<int>(fromY);
    down.clicks = 1;
    const bool grabbed = mImpl->gizmo->mousePressed(down);

    OgreBites::MouseMotionEvent move{};
    move.type = OgreBites::MOUSEMOTION;
    move.x = static_cast<int>(toX);
    move.y = static_cast<int>(toY);
    move.xrel = static_cast<int>(toX - fromX);
    move.yrel = static_cast<int>(toY - fromY);
    mImpl->gizmo->mouseMoved(move);

    OgreBites::MouseButtonEvent up{};
    up.type = OgreBites::MOUSEBUTTONUP;
    up.button = OgreBites::BUTTON_LEFT;
    up.x = static_cast<int>(toX);
    up.y = static_cast<int>(toY);
    up.clicks = 1;
    mImpl->gizmo->mouseReleased(up);

    return grabbed;
}

void Renderer::renderTo(const std::string& pngPath)
{
    mImpl->target->update();
    mImpl->target->writeContentsToFile(pngPath);
}

}  // namespace keepstone
