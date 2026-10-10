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
    Ogre::Root* root = nullptr;
    Ogre::SceneManager* scene = nullptr;
    Ogre::Camera* camera = nullptr;
    Ogre::RenderTexture* target = nullptr;
    Ogre::SceneNode* camNode = nullptr;
    std::vector<Ogre::SceneNode*> nodes;   // ObjectId is an index into this
};

Renderer::Renderer() : mImpl(new Impl) {}

Renderer::~Renderer() { delete mImpl; }

void Renderer::startUp(unsigned width, unsigned height)
{
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
    vp->setBackgroundColour(Ogre::ColourValue(0.05f, 0.06f, 0.09f));
    vp->setMaterialScheme(Ogre::MSN_SHADERGEN);
}

void Renderer::shutDown()
{
    if (mImpl->root) mImpl->root->shutdown();
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

void Renderer::renderTo(const std::string& pngPath)
{
    mImpl->target->update();
    mImpl->target->writeContentsToFile(pngPath);
}

}  // namespace keepstone
