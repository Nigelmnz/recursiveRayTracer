/** \file App.cpp */
#include "App.h"

// Tells C++ to invoke command-line main() function even on OS X and Win32.
G3D_START_AT_MAIN();

int main(int argc, const char* argv[]) {
	{
		G3DSpecification g3dSpec;
		g3dSpec.audio = false;
		initGLG3D(g3dSpec);
	}

    GApp::Settings settings(argc, argv);

    // Change the window and other startup parameters by modifying the
    // settings class.  For example:
    settings.window.caption			= argv[0];
    //settings.window.debugContext = true;
    // Some popular resolutions:
    // settings.window.width        = 640;  settings.window.height       = 400;
    // settings.window.width		= 1024; settings.window.height       = 768;
    settings.window.width         = 1280; settings.window.height       = 720;
    //settings.window.width        = 1920; settings.window.height       = 1080;
    // settings.window.width		= OSWindow::primaryDisplayWindowSize().x; settings.window.height = OSWindow::primaryDisplayWindowSize().y;

    // Set to true for a significant performance boost if your app can't render at 60fps,
    // or if you *want* to render faster than the display.
    settings.window.asynchronous	    = true;
    settings.depthGuardBandThickness    = Vector2int16(0, 0);
    settings.colorGuardBandThickness    = Vector2int16(0, 0);
    settings.dataDir			        = FileSystem::currentDirectory();
    settings.screenshotDirectory	    = "../journal/";
 
    return App(settings).run();
}


App::App(const GApp::Settings& settings) : GApp(settings) {
}


// Called before the application loop begins.  Load data here and
// not in the constructor so that common exceptions will be
// automatically caught.
void App::onInit() {
    GApp::onInit();
    takeScreenshot = false;
    setFrameDuration(1.0f / 60.0f);

    // Call setScene(shared_ptr<Scene>()) or setScene(MyScene::create()) to replace
    // the default scene here.
    
    showRenderingStats      = false;

    makeGUI();
    m_lastLightingChangeTime = 0.0;
    // For higher-quality screenshots:
    // developerWindow->videoRecordDialog->setScreenShotFormat("PNG");
    developerWindow->videoRecordDialog->setCaptureGui(false);
    developerWindow->cameraControlWindow->moveTo(Point2(developerWindow->cameraControlWindow->rect().x0(), 0));
    loadScene(
		//"Beethoven" // Load something simple
            "Mirrorbox"  // Load the first scene encountered 
        );
}

void App::onRenderButton() {

    Array<shared_ptr<Surface> > surfaces;
    Array<shared_ptr<Surface2D> > ignore;
    onPose(surfaces, ignore);
    //window()->setClientRect(Rect2D::xywh(50.0f, 50.0f, (float)m_rayTracerSettings.width, (float)m_rayTracerSettings.height));
    drawMessage("Ray Tracing...");
    m_rayTracerStats = RayTracer::Stats::Stats();
    shared_ptr<Image> im = RayTracer::create()->render(m_rayTracerSettings, surfaces, shared_ptr<LightingEnvironment>(new LightingEnvironment(scene()->lightingEnvironment())), activeCamera(), m_rayTracerStats);
    m_rayTracedImage = Texture::fromImage("rayTracedImage", im);
    m_textureBox->setTexture(m_rayTracedImage);
    m_textureBox->setWidth(min(150,m_rayTracedImage->width()));
    m_textureBox->setHeight(min(150,m_rayTracedImage->height()));
    rtPane->pack();
}

void App::makeGUI() {
    // Initialize the developer HUD (using the existing scene)
    createDeveloperHUD();
    debugWindow->setVisible(true);
    developerWindow->videoRecordDialog->setEnabled(true);
 
    //GuiPane* infoPane = debugPane->addPane("Info", GuiTheme::ORNATE_PANE_STYLE);

    // Example of how to add debugging controls
    //infoPane->addLabel("You can add GUI controls");
    //infoPane->addLabel("in App::onInit().");
    //infoPane->addButton("Exit G3D", this, &App::endProgram);

    shared_ptr<GFont> arialFont = GFont::fromFile(System::findDataFile("icon.fnt"));
    shared_ptr<GuiTheme> theme = GuiTheme::fromFile(System::findDataFile("osx-10.7.gtm"), arialFont);
    rtWindow = GuiWindow::create("Ray Trace", theme, Rect2D::xywh(0,0,100,100), GuiTheme::TOOL_WINDOW_STYLE, GuiWindow::HIDE_ON_CLOSE);
    addWidget(rtWindow);
    
    rtPane = rtWindow->pane();
    m_resolutionList = rtPane->addDropDownList("Resolution", Array<String>("640 x 480", "160 x 90", "320 x 180", "640 x 360", "1280 x 720", "2880 x 1800"), NULL, GuiControl::Callback(this, &App::onResolutionChange));
    m_resolutionList->setSelectedIndex(0);
    onResolutionChange();
    rtPane->addNumberBox("Tracing Depth ", &m_rayTracerSettings.traceDepth);
    rtPane->addNumberBox("Rays per Pixel ", &m_rayTracerSettings.raysPerPixel);

    rtPane->addCheckBox("Multithreaded", &m_rayTracerSettings.multithreaded);
    m_rayTracerSettings.useTree = true;
    rtPane->addCheckBox("Use TriTree as Tree", &m_rayTracerSettings.useTree);
    rtPane->addCheckBox("Use Shadows", &m_rayTracerSettings.useShadows);
    rtPane->addCheckBox("Path Trace", &m_rayTracerSettings.pathTrace);

    rtPane->addCheckBox("showImage", &showImage);
    rtPane->addNumberBox("Triangles ", &m_rayTracerStats.triangles);
    rtPane->addNumberBox("BT Time ", &m_rayTracerStats.buildTriTreeTimeMilliseconds);
    rtPane->addNumberBox("Trace Time ", &m_rayTracerStats.rayTraceTimeMilliseconds);
    rtPane->addNumberBox("Lights ", &m_rayTracerStats.lights);
    rtPane->addNumberBox("Pixels ", &m_rayTracerStats.pixels);
    rtPane->addNumberBox("Primary Rays ", &m_rayTracerStats.primaryRays);
    rtPane->addNumberBox("Impulse Rays ", &m_rayTracerStats.impulseRays);
    rtPane->addNumberBox("Shadow Rays ", &m_rayTracerStats.shadowRays);
    rtPane->addNumberBox("Total Rays ", &m_rayTracerStats.totalRays);
    rtPane->addButton("RayTrace Image", this, &App::onRenderButton);
    m_textureBox = rtPane->addTextureBox();
    rtPane->pack();
    
    shared_ptr<GuiWindow> timeWindow = GuiWindow::create("Make Movie", theme, Rect2D::xywh(230, 0, 100, 100));
    addWidget(timeWindow);
    GuiPane* windowPane = timeWindow->pane();\
    m_saveFilename = "image.jpg";
    windowPane->addTextBox("Filename: ", &m_saveFilename);
    windowPane->addButton("SaveImage", this, &App::savePostProcessed);
    windowPane->pack();

    // More examples of debugging GUI controls:
    // debugPane->addCheckBox("Use explicit checking", &explicitCheck);
    // debugPane->addTextBox("Name", &myName);
    // debugPane->addNumberBox("height", &height, "m", GuiTheme::LINEAR_SLIDER, 1.0f, 2.5f);
    // button = debugPane->addButton("Run Simulator");

    debugWindow->pack();
    debugWindow->setRect(Rect2D::xywh(0, 0, (float)window()->width(), debugWindow->rect().height()));
}

void App::savePostProcessed() {
    takeScreenshot = true;
}
void App::renderVideo() {
    //GEvent event;
    //event.type = GEventType::KEY_DOWN;
    //event.key.keysym.sym = GKey::F6;
    //window()->fireEvent(event);
    m_simulating = true;
    showImage = true;
    m_currentFrame = 0;
}

void App::onGraphics3D(RenderDevice* rd, Array<shared_ptr<Surface> >& allSurfaces) {
    // This implementation is equivalent to the default GApp's. It is repeated here to make it
    // easy to modify rendering. If you don't require custom rendering, just delete this
    // method from your application and rely on the base class.

    // screenPrintf("Print to the screen from anywhere in a G3D program with this command.");
    if (! scene()) {
        return;
    }
    if (isNull(m_rayTracedImage) || !showImage) {
        m_gbuffer->setSpecification(m_gbufferSpecification);
        m_gbuffer->resize(m_framebuffer->width(), m_framebuffer->height());
     
    // Share the depth buffer with the forward-rendering pipeline
    m_framebuffer->set(Framebuffer::DEPTH, m_gbuffer->texture(GBuffer::Field::DEPTH_AND_STENCIL));
    m_depthPeelFramebuffer->resize(m_framebuffer->width(), m_framebuffer->height());

    Surface::AlphaMode coverageMode = Surface::ALPHA_BLEND;

    // Bind the main framebuffer
    rd->pushState(m_framebuffer); {
        rd->clear();
        rd->setProjectionAndCameraMatrix(activeCamera()->projection(), activeCamera()->frame());
        
        m_gbuffer->prepare(rd, activeCamera(), 0, -(float)previousSimTimeStep(), m_settings.depthGuardBandThickness, m_settings.colorGuardBandThickness);
        
        // Cull and sort
        Array<shared_ptr<Surface> > sortedVisibleSurfaces;
        Surface::cull(activeCamera()->frame(), activeCamera()->projection(), rd->viewport(), allSurfaces, sortedVisibleSurfaces);
        Surface::sortBackToFront(sortedVisibleSurfaces, activeCamera()->frame().lookVector());
        
        const bool renderTransmissiveSurfaces = false;

        // Intentionally copy the lighting environment for mutation
        LightingEnvironment environment = scene()->lightingEnvironment();
        environment.ambientOcclusion = m_ambientOcclusion;
       
        // Render z-prepass and G-buffer.
        Surface::renderIntoGBuffer(rd, sortedVisibleSurfaces, m_gbuffer, activeCamera()->previousFrame(), activeCamera()->expressivePreviousFrame(), renderTransmissiveSurfaces, coverageMode);

        // This could be the OR of several flags; the starter begins with only one motivating algorithm for depth peel
        const bool needDepthPeel = environment.ambientOcclusionSettings.useDepthPeelBuffer;
        if (needDepthPeel) {
            rd->pushState(m_depthPeelFramebuffer); {
                rd->clear();
                rd->setProjectionAndCameraMatrix(activeCamera()->projection(), activeCamera()->frame());
                Surface::renderDepthOnly(rd, sortedVisibleSurfaces, CullFace::BACK, renderTransmissiveSurfaces, coverageMode, m_framebuffer->texture(Framebuffer::DEPTH), environment.ambientOcclusionSettings.depthPeelSeparationHint);
            } rd->popState();
        }
        
        if (! m_settings.colorGuardBandThickness.isZero()) {
            rd->setGuardBandClip2D(m_settings.colorGuardBandThickness);
        }        

        // Compute AO
        m_ambientOcclusion->update(rd, environment.ambientOcclusionSettings, activeCamera(), m_framebuffer->texture(Framebuffer::DEPTH), m_depthPeelFramebuffer->texture(Framebuffer::DEPTH), m_gbuffer->texture(GBuffer::Field::CS_NORMAL), m_gbuffer->texture(GBuffer::Field::SS_POSITION_CHANGE), m_settings.depthGuardBandThickness - m_settings.colorGuardBandThickness);

        const RealTime lightingChangeTime = max(scene()->lastEditingTime(), max(scene()->lastLightChangeTime(), scene()->lastVisibleChangeTime()));
        bool updateShadowMaps = false;
        if (lightingChangeTime > m_lastLightingChangeTime) {
            m_lastLightingChangeTime = lightingChangeTime;
            updateShadowMaps = true;
        }
        // No need to write depth, since it was covered by the gbuffer pass
        rd->setDepthWrite(false);
        // Compute shadow maps and forward-render visible surfaces
        Surface::render(rd, activeCamera()->frame(), activeCamera()->projection(), sortedVisibleSurfaces, allSurfaces, environment, coverageMode, updateShadowMaps, m_settings.depthGuardBandThickness - m_settings.colorGuardBandThickness, sceneVisualizationSettings());      
                
        // Call to make the App show the output of debugDraw(...)
        drawDebugShapes();
        const shared_ptr<Entity>& selectedEntity = (notNull(developerWindow) && notNull(developerWindow->sceneEditorWindow)) ? developerWindow->sceneEditorWindow->selectedEntity() : shared_ptr<Entity>();
        scene()->visualize(rd, selectedEntity, sceneVisualizationSettings());

        // Post-process special effects
        m_depthOfField->apply(rd, m_framebuffer->texture(0), m_framebuffer->texture(Framebuffer::DEPTH), activeCamera(), m_settings.depthGuardBandThickness - m_settings.colorGuardBandThickness);
        
        m_motionBlur->apply(rd, m_framebuffer->texture(0), m_gbuffer->texture(GBuffer::Field::SS_EXPRESSIVE_MOTION), 
                            m_framebuffer->texture(Framebuffer::DEPTH), activeCamera(), 
                            m_settings.depthGuardBandThickness - m_settings.colorGuardBandThickness);

    } rd->popState();
    swapBuffers();
    rd->clear();
    // Perform gamma correction, bloom, and SSAA, and write to the native window frame buffer
    m_film->exposeAndRender(rd, activeCamera()->filmSettings(), m_framebuffer->texture(0));
    } else {
        rd->pushState(m_framebuffer); {
            m_framebuffer->set(Framebuffer::COLOR0, m_rayTracedImage);
        } rd->popState();
        swapBuffers();
        rd->clear();
        shared_ptr<Texture> tex;
        m_film->exposeAndRender(rd, activeCamera()->filmSettings(), m_framebuffer->texture(0), tex);
        m_film->exposeAndRender(rd, activeCamera()->filmSettings(), m_framebuffer->texture(0));
       if (takeScreenshot) {
           
           m_textureBox->setTexture(tex);
           m_textureBox->setCaption("___" + m_saveFilename);
           m_textureBox->save();
           rd->screenshotPic()->save(m_saveFilename);
           takeScreenshot = false;
       }
        
    }
}

void App::onAI() {
    GApp::onAI();
    // Add non-simulation game logic and AI code here
}


void App::onNetwork() {
    GApp::onNetwork();
    // Poll net messages here
}


void App::onSimulation(RealTime rdt, SimTime sdt, SimTime idt) {
    //if (m_simulating || m_fakeTime) {
    GApp::onSimulation(rdt, sdt, idt);
    /*    
        //Ray motion
        Array<shared_ptr<Entity> > entities;
        scene()->getEntityArray(entities);
        
        for (int e = 0; e < entities.size(); ++e) {
            shared_ptr<Entity> entity = entities[e];
            if (beginsWith(entity->name(), "cylinder")) {
                CFrame frame = entity->frame();
                float speed = 0.01f;
                frame = frame * CFrame::fromXYZYPRDegrees(0, speed, 0);
                entity->setFrame(frame);
            }
            if (endsWith(entity->name(), "Light")) {
                Any a(entity->toAny());
                a["bulbPower"] = Any::parse(format("Power3(%f)", 100 + log(1.0f + m_currentFrame * 50.0f)));
                scene()->remove(entity);
                scene()->createEntity("Light", "newLight", a); 
            }

            if (beginsWith(entity->name(), "camera") && endsWith(entity->name(), format("%d", m_cameraNum))) {
                setActiveCamera( dynamic_pointer_cast<Camera>(entity));
            }
        }
        //Video Rendering Logic:
        ++m_currentFrame;
    }

    if (m_simulating) {
        

        // Example GUI dynamic layout code.  Resize the debugWindow to fill
        // the screen horizontally.
       // debugWindow->setRect(Rect2D::xywh(0, 0, (float)window()->width(), debugWindow->rect().height()));
        
        if ((m_currentFrame >= m_startFrame) && !m_renderingVideo) {
            m_renderingVideo = true;
            TextInput ti(TextInput::FROM_STRING, m_resolutionList->selectedValue().text());
            int width = (int)ti.readNumber();
            ti.readSymbol("x");
            int height = (int)ti.readNumber();
            //m_activeVideoRecordDialog->startRecording();
            window()->setClientRect(Rect2D::xywh(100, 100, width, height));
            developerWindow->videoRecordDialog->startRecording();
            //onEvent(event);
        }
        if (m_currentFrame >= m_endFrame) {
            window()->setClientRect(Rect2D::xywh(100, 100, m_settings.window.width, m_settings.window.height)); // scary breaking ??
            m_renderingVideo = false;
            m_simulating = false;
            m_currentFrame = 0;
            //GEvent event;
            //event.type = GEventType::KEY_DOWN;
            //event.key.keysym.sym = GKey::F6;
            //window()->fireEvent(event);
            developerWindow->videoRecordDialog->stopRecording();
            //m_activeVideoRecordDialog->stopRecording();
            //onEvent(event);
        }
        }*/
}


bool App::onEvent(const GEvent& event) {
    // Handle super-class events
    if (event.type == GEventType::KEY_DOWN && event.key.keysym.sym == 'f') {
        renderVideo();
    }
    if (GApp::onEvent(event)) { return true; }

    // If you need to track individual UI events, manage them here.
    // Return true if you want to prevent other parts of the system
    // from observing this specific event.
    //
    // For example,
    // if ((event.type == GEventType::GUI_ACTION) && (event.gui.control == m_button)) { ... return true; }
    // if ((event.type == GEventType::KEY_DOWN) && (event.key.keysym.sym == GKey::TAB)) { ... return true; }

    return false;
}


void App::onUserInput(UserInput* ui) {
    GApp::onUserInput(ui);
    (void)ui;
    // Add key handling here based on the keys currently held or
    // ones that changed in the last frame.
}


void App::onPose(Array<shared_ptr<Surface> >& surface, Array<shared_ptr<Surface2D> >& surface2D) {
    GApp::onPose(surface, surface2D);

    // Append any models to the arrays that you want to later be rendered by onGraphics()
}


void App::onGraphics2D(RenderDevice* rd, Array<shared_ptr<Surface2D> >& posed2D) {
    // Render 2D objects like Widgets.  These do not receive tone mapping or gamma correction.
    Surface2D::sortAndRender(rd, posed2D);
}


void App::onCleanup() {
    // Called after the application loop ends.  Place a majority of cleanup code
    // here instead of in the constructor so that exceptions can be caught.
}


void App::endProgram() {
    m_endProgram = true;
}

void App::onResolutionChange() {
    TextInput ti(TextInput::FROM_STRING, m_resolutionList->selectedValue().text());
    m_rayTracerSettings.width = (int)ti.readNumber();
    ti.readSymbol("x");
    m_rayTracerSettings.height = (int)ti.readNumber();
}
