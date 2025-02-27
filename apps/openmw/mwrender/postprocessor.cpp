#include "postprocessor.hpp"

#include <SDL_opengl_glext.h>

#include <osg/Group>
#include <osg/Camera>
#include <osg/Callback>
#include <osg/Texture2D>
#include <osg/FrameBufferObject>

#include <osgViewer/Viewer>

#include <components/settings/settings.hpp>
#include <components/sceneutil/depth.hpp>
#include <components/sceneutil/nodecallback.hpp>
#include <components/debug/debuglog.hpp>

#include "vismask.hpp"

namespace
{
    osg::ref_ptr<osg::Geometry> createFullScreenTri()
    {
        osg::ref_ptr<osg::Geometry> geom = new osg::Geometry;

        osg::ref_ptr<osg::Vec3Array> verts = new osg::Vec3Array;
        verts->push_back(osg::Vec3f(-1, -1, 0));
        verts->push_back(osg::Vec3f(-1, 3, 0));
        verts->push_back(osg::Vec3f(3, -1, 0));

        geom->setVertexArray(verts);

        geom->addPrimitiveSet(new osg::DrawArrays(osg::PrimitiveSet::TRIANGLES, 0, 3));

        return geom;
    }

    class CullCallback : public SceneUtil::NodeCallback<CullCallback, osg::Node*, osgUtil::CullVisitor*>
    {
    public:
        CullCallback()
            : mLastFrameNumber(0)
        {
        }

        void operator()(osg::Node* node, osgUtil::CullVisitor* cv)
        {
            osgUtil::RenderStage* renderStage = cv->getCurrentRenderStage();

            unsigned int frame = cv->getTraversalNumber();
            if (frame != mLastFrameNumber)
            {
                mLastFrameNumber = frame;

                MWRender::PostProcessor* postProcessor = dynamic_cast<MWRender::PostProcessor*>(cv->getCurrentCamera()->getUserData());

                if (!postProcessor)
                {
                    Log(Debug::Error) << "Failed retrieving user data for master camera: FBO setup failed";
                    traverse(node, cv);
                    return;
                }

                if (!postProcessor->getMsaaFbo())
                {
                    renderStage->setFrameBufferObject(postProcessor->getFbo());
                }
                else
                {
                    renderStage->setMultisampleResolveFramebufferObject(postProcessor->getFbo());
                    renderStage->setFrameBufferObject(postProcessor->getMsaaFbo());
                }
            }

            traverse(node, cv);
        }

    private:
        unsigned int mLastFrameNumber;
    };

    struct ResizedCallback : osg::GraphicsContext::ResizedCallback
    {
        ResizedCallback(MWRender::PostProcessor* postProcessor)
            : mPostProcessor(postProcessor)
        {
        }

        void resizedImplementation(osg::GraphicsContext* gc, int x, int y, int width, int height) override
        {
            gc->resizedImplementation(x, y, width, height);
            mPostProcessor->resize(width, height);
        }

        MWRender::PostProcessor* mPostProcessor;
    };

    // Copies the currently bound depth attachment to a new texture so drawables in transparent renderbin can safely sample from depth.
    class OpaqueDepthCopyCallback : public osgUtil::RenderBin::DrawCallback
    {
    public:
        OpaqueDepthCopyCallback(osg::ref_ptr<osg::Texture2D> opaqueDepthTex, osg::ref_ptr<osg::FrameBufferObject> sourceFbo)
            : mOpaqueDepthFbo(new osg::FrameBufferObject)
            , mSourceFbo(sourceFbo)
            , mOpaqueDepthTex(opaqueDepthTex)
            , mColorAttached(false)
        {
            mOpaqueDepthFbo->setAttachment(osg::FrameBufferObject::BufferComponent::DEPTH_BUFFER, osg::FrameBufferAttachment(opaqueDepthTex));

#ifdef __APPLE__
            // Mac OS drivers complain that a FBO is incomplete if it has no color attachment
            mOpaqueDepthFbo->setAttachment(osg::FrameBufferObject::BufferComponent::COLOR_BUFFER, osg::FrameBufferAttachment(new osg::RenderBuffer(mOpaqueDepthTex->getTextureWidth(), mOpaqueDepthTex->getTextureHeight(), GL_RGB)));
            mColorAttached = true;
#endif
        }

        void drawImplementation(osgUtil::RenderBin* bin, osg::RenderInfo& renderInfo, osgUtil::RenderLeaf*& previous) override
        {
            if (bin->getStage()->getFrameBufferObject() == mSourceFbo)
            {
                osg::State& state = *renderInfo.getState();
                osg::GLExtensions* ext = state.get<osg::GLExtensions>();

                mSourceFbo->apply(state, osg::FrameBufferObject::READ_FRAMEBUFFER);
                postBindOperation(state);

                mOpaqueDepthFbo->apply(state, osg::FrameBufferObject::DRAW_FRAMEBUFFER);
                postBindOperation(state);

                ext->glBlitFramebuffer(0, 0, mOpaqueDepthTex->getTextureWidth(), mOpaqueDepthTex->getTextureHeight(), 0, 0, mOpaqueDepthTex->getTextureWidth(), mOpaqueDepthTex->getTextureHeight(), GL_DEPTH_BUFFER_BIT, GL_NEAREST);

                mSourceFbo->apply(state);
            }

            bin->drawImplementation(renderInfo, previous);
        }
    private:
        void postBindOperation(osg::State& state)
        {
            if (mColorAttached)
                return;
            #if !defined(OSG_GLES1_AVAILABLE) && !defined(OSG_GLES2_AVAILABLE) && !defined(OSG_GLES3_AVAILABLE)
            state.glDrawBuffer(GL_NONE);
            state.glReadBuffer(GL_NONE);
            #endif
        }

        osg::ref_ptr<osg::FrameBufferObject> mOpaqueDepthFbo;
        osg::ref_ptr<osg::FrameBufferObject> mSourceFbo;
        osg::ref_ptr<osg::Texture2D> mOpaqueDepthTex;
        bool mColorAttached;
    };
}

namespace MWRender
{
    PostProcessor::PostProcessor(osgViewer::Viewer* viewer, osg::Group* rootNode)
        : mViewer(viewer)
        , mRootNode(new osg::Group)
        , mDepthFormat(GL_DEPTH24_STENCIL8_EXT)
    {
        bool softParticles = Settings::Manager::getBool("soft particles", "Shaders");

        if (!SceneUtil::AutoDepth::isReversed() && !softParticles)
            return;

        osg::GraphicsContext* gc = viewer->getCamera()->getGraphicsContext();
        unsigned int contextID = gc->getState()->getContextID();
        osg::GLExtensions* ext = gc->getState()->get<osg::GLExtensions>();

        constexpr char errPreamble[] = "Postprocessing and floating point depth buffers disabled: ";

        if (!ext->isFrameBufferObjectSupported)
        {
            Log(Debug::Warning) << errPreamble << "FrameBufferObject unsupported.";
            return;
        }

        if (Settings::Manager::getInt("antialiasing", "Video") > 1 && !ext->isRenderbufferMultisampleSupported())
        {
            Log(Debug::Warning) << errPreamble << "RenderBufferMultiSample unsupported. Disabling antialiasing will resolve this issue.";
            return;
        }

        if (SceneUtil::AutoDepth::isReversed())
        {
            if (osg::isGLExtensionSupported(contextID, "GL_ARB_depth_buffer_float"))
                mDepthFormat = GL_DEPTH32F_STENCIL8;
            else if (osg::isGLExtensionSupported(contextID, "GL_NV_depth_buffer_float"))
                mDepthFormat = GL_DEPTH32F_STENCIL8_NV;
            else
            {
                // TODO: Once we have post-processing implemented we want to skip this return and continue with setup.
                // Rendering to a FBO to fullscreen geometry has overhead (especially when MSAA is enabled) and there are no
                // benefits if no floating point depth formats are supported.
                Log(Debug::Warning) << errPreamble << "'GL_ARB_depth_buffer_float' and 'GL_NV_depth_buffer_float' unsupported.";

                if (!softParticles)
                    return;
            }
        }

        int width = viewer->getCamera()->getViewport()->width();
        int height = viewer->getCamera()->getViewport()->height();

        createTexturesAndCamera(width, height);
        resize(width, height);

        mRootNode->addChild(mHUDCamera);
        mRootNode->addChild(rootNode);
        mViewer->setSceneData(mRootNode);

        // We need to manually set the FBO and resolve FBO during the cull callback. If we were using a separate
        // RTT camera this would not be needed.
        mViewer->getCamera()->addCullCallback(new CullCallback);
        mViewer->getCamera()->setRenderTargetImplementation(osg::Camera::FRAME_BUFFER_OBJECT);
        mViewer->getCamera()->attach(osg::Camera::COLOR_BUFFER0, mSceneTex);
        mViewer->getCamera()->attach(osg::Camera::PACKED_DEPTH_STENCIL_BUFFER, mDepthTex);

        mViewer->getCamera()->getGraphicsContext()->setResizedCallback(new ResizedCallback(this));
        mViewer->getCamera()->setUserData(this);
    }

    void PostProcessor::resize(int width, int height)
    {
        mDepthTex->setTextureSize(width, height);
        mSceneTex->setTextureSize(width, height);
        mDepthTex->dirtyTextureObject();
        mSceneTex->dirtyTextureObject();

        if (mOpaqueDepthTex)
        {
            mOpaqueDepthTex->setTextureSize(width, height);
            mOpaqueDepthTex->dirtyTextureObject();
        }

        int samples = Settings::Manager::getInt("antialiasing", "Video");

        mFbo = new osg::FrameBufferObject;
        mFbo->setAttachment(osg::Camera::COLOR_BUFFER0, osg::FrameBufferAttachment(mSceneTex));
        mFbo->setAttachment(osg::Camera::PACKED_DEPTH_STENCIL_BUFFER, osg::FrameBufferAttachment(mDepthTex));

        // When MSAA is enabled we must first render to a render buffer, then
        // blit the result to the FBO which is either passed to the main frame
        // buffer for display or used as the entry point for a post process chain.
        if (samples > 1)
        {
            mMsaaFbo = new osg::FrameBufferObject;
            osg::ref_ptr<osg::RenderBuffer> colorRB = new osg::RenderBuffer(width, height, mSceneTex->getInternalFormat(), samples);
            osg::ref_ptr<osg::RenderBuffer> depthRB = new osg::RenderBuffer(width, height, mDepthTex->getInternalFormat(), samples);
            mMsaaFbo->setAttachment(osg::Camera::COLOR_BUFFER0, osg::FrameBufferAttachment(colorRB));
            mMsaaFbo->setAttachment(osg::Camera::PACKED_DEPTH_STENCIL_BUFFER, osg::FrameBufferAttachment(depthRB));
        }

        if (const auto depthProxy = std::getenv("OPENMW_ENABLE_DEPTH_CLEAR_PROXY"))
            mFirstPersonDepthRBProxy = new osg::RenderBuffer(width, height, mDepthTex->getInternalFormat(), samples);

        if (Settings::Manager::getBool("soft particles", "Shaders"))
            osgUtil::RenderBin::getRenderBinPrototype("DepthSortedBin")->setDrawCallback(new OpaqueDepthCopyCallback(mOpaqueDepthTex, mMsaaFbo ? mMsaaFbo : mFbo));

        mViewer->getCamera()->resize(width, height);
        mHUDCamera->resize(width, height);
    }

    void PostProcessor::createTexturesAndCamera(int width, int height)
    {
        mDepthTex = new osg::Texture2D;
        mDepthTex->setTextureSize(width, height);
        mDepthTex->setSourceFormat(GL_DEPTH_STENCIL_EXT);
        mDepthTex->setSourceType(SceneUtil::isFloatingPointDepthFormat(getDepthFormat()) ? GL_FLOAT_32_UNSIGNED_INT_24_8_REV : GL_UNSIGNED_INT_24_8_EXT);
        mDepthTex->setInternalFormat(mDepthFormat);
        mDepthTex->setFilter(osg::Texture2D::MIN_FILTER, osg::Texture2D::NEAREST);
        mDepthTex->setFilter(osg::Texture2D::MAG_FILTER, osg::Texture2D::NEAREST);
        mDepthTex->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        mDepthTex->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
        mDepthTex->setResizeNonPowerOfTwoHint(false);

        if (Settings::Manager::getBool("soft particles", "Shaders"))
        {
            mOpaqueDepthTex = new osg::Texture2D(*mDepthTex);
            mOpaqueDepthTex->setName("opaqueTexMap");
        }

        mSceneTex = new osg::Texture2D;
        mSceneTex->setTextureSize(width, height);
        mSceneTex->setSourceFormat(GL_RGB);
        mSceneTex->setSourceType(GL_UNSIGNED_BYTE);
        mSceneTex->setInternalFormat(GL_RGB);
        mSceneTex->setFilter(osg::Texture2D::MIN_FILTER, osg::Texture2D::NEAREST);
        mSceneTex->setFilter(osg::Texture2D::MAG_FILTER, osg::Texture2D::NEAREST);
        mSceneTex->setWrap(osg::Texture::WRAP_S, osg::Texture::CLAMP_TO_EDGE);
        mSceneTex->setWrap(osg::Texture::WRAP_T, osg::Texture::CLAMP_TO_EDGE);
        mSceneTex->setResizeNonPowerOfTwoHint(false);

        mHUDCamera = new osg::Camera;
        mHUDCamera->setReferenceFrame(osg::Camera::ABSOLUTE_RF);
        mHUDCamera->setRenderOrder(osg::Camera::POST_RENDER);
        mHUDCamera->setClearColor(osg::Vec4(0.45, 0.45, 0.14, 1.0));
        mHUDCamera->setProjectionMatrix(osg::Matrix::ortho2D(0, 1, 0, 1));
        mHUDCamera->setAllowEventFocus(false);
        mHUDCamera->setViewport(0, 0, width, height);

        // Shaders calculate correct UV coordinates for our fullscreen triangle
        constexpr char vertSrc[] = R"GLSL(
            #version 120

            varying vec2 uv;

            void main()
            {
                gl_Position = vec4(gl_Vertex.xy, 0.0, 1.0);
                uv = gl_Position.xy * 0.5 + 0.5;
            }
        )GLSL";

        constexpr char fragSrc[] = R"GLSL(
            #version 120

            varying vec2 uv;
            uniform sampler2D sceneTex;

            void main()
            {
                gl_FragData[0] = texture2D(sceneTex, uv);
            }
        )GLSL";

        osg::ref_ptr<osg::Shader> vertShader = new osg::Shader(osg::Shader::VERTEX, vertSrc);
        osg::ref_ptr<osg::Shader> fragShader = new osg::Shader(osg::Shader::FRAGMENT, fragSrc);

        osg::ref_ptr<osg::Program> program = new osg::Program;
        program->addShader(vertShader);
        program->addShader(fragShader);

        mHUDCamera->addChild(createFullScreenTri());
        mHUDCamera->setNodeMask(Mask_RenderToTexture);

        auto* stateset = mHUDCamera->getOrCreateStateSet();
        stateset->setTextureAttributeAndModes(0, mSceneTex, osg::StateAttribute::ON);
        stateset->setAttributeAndModes(program, osg::StateAttribute::ON);
        stateset->addUniform(new osg::Uniform("sceneTex", 0));
        stateset->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
        stateset->setMode(GL_DEPTH_TEST, osg::StateAttribute::OFF);
    }

}
