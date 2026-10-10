// THE COUNTER-EXAMPLE. This is the same courtyard, written the way Days 1 to 3 wrote it: the
// game and the renderer welded together in one file.
//
// It is here to be COMPARED, not to be copied, and it is deliberately outside lanternkeep/ so
// the boundary grep never sees it. Nothing about it is wrong as a program -- it builds, it runs,
// and it writes a frame byte-identical to the one the engine writes. That is the uncomfortable
// part of today: the welded version is not broken, it is just not an engine, and the difference
// does not show up in the picture. It shows up the first time somebody wants a second world, a
// second renderer, or a test that runs without a GPU.
//
// Count what this file knows that the game next door does not. Measured, not estimated -- a
// grep for `Ogre::[A-Z]` over this file returns THIRTY-TWO distinct names:
//
//   Camera ColourValue Degree Entity Light LogManager MSN_DEFAULT MSN_SHADERGEN Material
//   MaterialManager NameValuePairList Node PF_B8G8R8A8 RGN_DEFAULT RGN_INTERNAL RTShader
//   RenderSystem RenderTexture RenderWindow Renderable ResourceGroupManager Root SceneManager
//   SceneNode String TEX_TYPE_2D TU_RENDERTARGET Technique TextureManager TexturePtr Vector3
//   Viewport
//
// Thirty-two, in a program whose job is to stand five blocks in a row.
//
// The same grep over the game next door returns ONE name: `keepstone::Renderer`. Not three --
// one. `Vec3` and `Colour` are never spelled there at all, because a braced initialiser like
// {0.25f, 0.25f, 0.30f} reaches them without naming them. The game's entire vocabulary for
// talking to the renderer is a single type and the shapes of its arguments.

#include <Ogre.h>
#include <OgreRTShaderSystem.h>

#include <cstdlib>
#include <iostream>
#include <string>

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

int main()
{
    Ogre::LogManager* logMgr = new Ogre::LogManager();
    logMgr->createLog("welded.log", true, false, false);

    Ogre::Root* root = new Ogre::Root("", "", "");
    root->loadPlugin(std::string(LK_OGRE_PLUGINS) + "/RenderSystem_Direct3D11");
    root->loadPlugin(std::string(LK_OGRE_PLUGINS) + "/Codec_STBI");

    Ogre::RenderSystem* rs = root->getRenderSystemByName("Direct3D11 Rendering Subsystem");
    rs->setConfigOption("Video Mode", "640 x 480 @ 32-bit colour");
    root->setRenderSystem(rs);
    root->initialise(false, "Lantern Keep");

    Ogre::NameValuePairList params;
    params["hidden"] = "true";
    Ogre::RenderWindow* hidden = root->createRenderWindow("hidden", 64, 64, false, &params);
    hidden->setActive(false);

    Ogre::ResourceGroupManager& rgm = Ogre::ResourceGroupManager::getSingleton();
    const std::string media = LK_OGRE_MEDIA;
    rgm.addResourceLocation(media + "/Main",        "FileSystem", Ogre::RGN_INTERNAL);
    rgm.addResourceLocation(media + "/RTShaderLib", "FileSystem", Ogre::RGN_INTERNAL);
    rgm.addResourceLocation(LK_BOOK_MEDIA,          "FileSystem", Ogre::RGN_DEFAULT);
    rgm.initialiseAllResourceGroups();

    Ogre::SceneManager* scene = root->createSceneManager();

    Ogre::RTShader::ShaderGenerator::initialize();
    Ogre::RTShader::ShaderGenerator* gen = Ogre::RTShader::ShaderGenerator::getSingletonPtr();
    gen->addSceneManager(scene);
    Ogre::MaterialManager::getSingleton().addListener(new ShaderResolver(gen));

    // -- and only HERE does the file start talking about a courtyard ---------------------
    scene->setAmbientLight(Ogre::ColourValue(0.25f, 0.25f, 0.30f));

    Ogre::Light* lantern = scene->createLight();
    lantern->setType(Ogre::Light::LT_POINT);
    lantern->setDiffuseColour(Ogre::ColourValue(1.0f, 0.88f, 0.66f));
    lantern->setSpecularColour(Ogre::ColourValue(1.0f, 0.95f, 0.80f));
    Ogre::SceneNode* lightNode = scene->getRootSceneNode()->createChildSceneNode();
    lightNode->setPosition(140, 180, 190);
    lightNode->attachObject(lantern);

    for (int i = 0; i < 5; ++i)
    {
        Ogre::Entity* e = scene->createEntity(Ogre::SceneManager::PT_CUBE);
        Ogre::SceneNode* n = scene->getRootSceneNode()->createChildSceneNode();
        n->attachObject(e);
        n->setPosition(-140.0f + 70.0f * i, 40, 0);
        n->setScale(0.32f, 0.55f + 0.14f * (i % 3), 0.32f);
        n->yaw(Ogre::Degree(13.0f * i));
    }

    Ogre::Camera* camera = scene->createCamera("view");
    camera->setNearClipDistance(0.5f);
    camera->setAutoAspectRatio(true);
    Ogre::SceneNode* camNode = scene->getRootSceneNode()->createChildSceneNode();
    camNode->setPosition(0, 120, 300);
    camNode->lookAt(Ogre::Vector3(0, 30, 0), Ogre::Node::TS_WORLD);
    camNode->attachObject(camera);

    Ogre::TexturePtr tex = Ogre::TextureManager::getSingleton().createManual(
        "keepstone/shot", Ogre::RGN_DEFAULT, Ogre::TEX_TYPE_2D,
        2400, 1800, 0, Ogre::PF_B8G8R8A8, Ogre::TU_RENDERTARGET);
    Ogre::RenderTexture* target = tex->getBuffer()->getRenderTarget();
    Ogre::Viewport* vp = target->addViewport(camera);
    vp->setBackgroundColour(Ogre::ColourValue(0.05f, 0.06f, 0.09f));
    vp->setMaterialScheme(Ogre::MSN_SHADERGEN);

    target->update();
    target->writeContentsToFile("shots/keep_welded.png");
    std::cout << "wrote shots/keep_welded.png\n";

    root->shutdown();
    std::cout.flush();
    std::_Exit(0);
}
