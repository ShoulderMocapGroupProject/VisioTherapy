#include "QTOgreWindow.h"
#include "OIS\OIS.h"
#include "Client.h"
#include <windows.h>
#if OGRE_VERSION >= ((2 << 16) | (0 << 8) | 0)
#include <Compositor/OgreCompositorManager2.h>
#endif

std::string Adapt(const bool i_Value)
{
	return i_Value ? "True" : "False";
}

std::string Adapt(const ViconDataStreamSDK::CPP::Direction::Enum i_Direction)
{
	switch (i_Direction)
	{
	case ViconDataStreamSDK::CPP::Direction::Forward:
		return "Forward";
	case ViconDataStreamSDK::CPP::Direction::Backward:
		return "Backward";
	case ViconDataStreamSDK::CPP::Direction::Left:
		return "Left";
	case ViconDataStreamSDK::CPP::Direction::Right:
		return "Right";
	case ViconDataStreamSDK::CPP::Direction::Up:
		return "Up";
	case ViconDataStreamSDK::CPP::Direction::Down:
		return "Down";
	default:
		return "Unknown";
	}
}


/*
Note that we pass any supplied QWindow parent to the base QWindow class. This is necessary should we
need to use our class within a container.
*/
QTOgreWindow::QTOgreWindow(QWindow *parent)
	: QWindow(parent)
	, m_update_pending(false)
	, m_animating(false)
	, m_ogreRoot(NULL)
	, m_ogreWindow(NULL)
	, m_ogreCamera(NULL)
	, m_cameraMan(NULL)
	, mClient(NULL)
	, mResourcesCfg(Ogre::StringUtil::BLANK)
	, mPluginsCfg(Ogre::StringUtil::BLANK)
	, entmesh(NULL)
	, node(NULL)
	, wristnodes()
	, uarmnodes()
	, sternumnodes()
	, acromiumnodes()
	, stdsternumnodes()
	, stdacromiumnodes()
	, stduarmnodes()
	, stdwristnodes()
	, csvptr(NULL)
	, mAnimationState(NULL)
	, cameranode(NULL)
	, mViewport(NULL)
	, slViewport(NULL)
	, srViewport(NULL)
	, upperarm(NULL)
	, forearm(NULL)
	, moved(false)
{
	setAnimating(true);
	installEventFilter(this);
	m_ogreBackground = Ogre::ColourValue(0, 0, 0);
	animationscale = 1;
	offset = 0;
	meshnum = 0;
	csvcount = 0;
	rotation << 1, 0, 0, 0, 1, 0, 0, 0, 1;
	translation << 0, 0, 0;
	calibration = false;

}

/*
Upon destruction of the QWindow object we destroy the Ogre3D scene.
*/
QTOgreWindow::~QTOgreWindow()
{
	if (m_cameraMan) delete m_cameraMan;
	delete m_ogreRoot;
}

/*
In case any drawing surface backing stores (QRasterWindow or QOpenGLWindow) of Qt are supplied to this
class in any way we inform Qt that they will be unused.
*/
void QTOgreWindow::render(QPainter *painter)
{
	Q_UNUSED(painter);
}

/*
Our initialization function. Called by our renderNow() function once when the window is first exposed.
*/
void QTOgreWindow::initialize()
{
#ifdef _DEBUG
	mResourcesCfg = "resources_d.cfg";
	mPluginsCfg = "plugins_d.cfg";
#else
	mResourcesCfg = "resources.cfg";
	mPluginsCfg = "plugins.cfg";
#endif

	m_ogreRoot = new Ogre::Root(mPluginsCfg);;

	Ogre::ConfigFile cf;
	cf.load(mResourcesCfg);

	Ogre::ConfigFile::SectionIterator seci = cf.getSectionIterator();

	Ogre::String secName, typeName, archName;
	while (seci.hasMoreElements())
	{
		secName = seci.peekNextKey();
		Ogre::ConfigFile::SettingsMultiMap *settings = seci.getNext();
		Ogre::ConfigFile::SettingsMultiMap::iterator i;
		for (i = settings->begin(); i != settings->end(); ++i)
		{
			typeName = i->first;
			archName = i->second;
			Ogre::ResourceGroupManager::getSingleton().addResourceLocation(
				archName, typeName, secName);
		}
	}

	const Ogre::RenderSystemList& rsList = m_ogreRoot->getAvailableRenderers();
	Ogre::RenderSystem* rs = rsList[0];

	/*
	This list setup the search order for used render system.
	*/
	Ogre::StringVector renderOrder;
#if defined(Q_OS_WIN)
	renderOrder.push_back("Direct3D9");
	renderOrder.push_back("Direct3D11");
#endif
	renderOrder.push_back("OpenGL");
	renderOrder.push_back("OpenGL 3+");
	for (Ogre::StringVector::iterator iter = renderOrder.begin(); iter != renderOrder.end(); iter++)
	{
		for (Ogre::RenderSystemList::const_iterator it = rsList.begin(); it != rsList.end(); it++)
		{
			if ((*it)->getName().find(*iter) != Ogre::String::npos)
			{
				rs = *it;
				break;
			}
		}
		if (rs != NULL) break;
	}
	if (rs == NULL)
	{
		if (!m_ogreRoot->restoreConfig())
		{
			if (!m_ogreRoot->showConfigDialog())
				OGRE_EXCEPT(Ogre::Exception::ERR_INVALIDPARAMS,
				"Abort render system configuration",
				"QTOgreWindow::initialize");
		}
	}

	/*
	Setting size and VSync on windows will solve a lot of problems
	*/
	QString dimensions = QString("%1 x %2").arg(this->width()).arg(this->height());
	rs->setConfigOption("Video Mode", dimensions.toStdString());
	rs->setConfigOption("Full Screen", "No");
	rs->setConfigOption("VSync", "No");
	rs->setConfigOption("FSAA", "8");
	m_ogreRoot->setRenderSystem(rs);
	m_ogreRoot->initialise(false);

	Ogre::NameValuePairList parameters;
	/*
	Flag within the parameters set so that Ogre3D initializes an OpenGL context on it's own.
	*/
	if (rs->getName().find("GL") <= rs->getName().size())
		parameters["currentGLContext"] = Ogre::String("false");

	/*
	We need to supply the low level OS window handle to this QWindow so that Ogre3D knows where to draw
	the scene. Below is a cross-platform method on how to do this.
	If you set both options (externalWindowHandle and parentWindowHandle) this code will work with OpenGL
	and DirectX.
	*/
#if defined(Q_OS_MAC) || defined(Q_OS_WIN)
	parameters["externalWindowHandle"] = Ogre::StringConverter::toString((size_t)(this->winId()));
	parameters["parentWindowHandle"] = Ogre::StringConverter::toString((size_t)(this->winId()));
#else
	parameters["externalWindowHandle"] = Ogre::StringConverter::toString((unsigned long)(this->winId()));
	parameters["parentWindowHandle"] = Ogre::StringConverter::toString((unsigned long)(this->winId()));
#endif

#if defined(Q_OS_MAC)
	parameters["macAPI"] = "cocoa";
	parameters["macAPICocoaUseNSView"] = "true";
#endif

	/*
	Note below that we supply the creation function for the Ogre3D window the width and height
	from the current QWindow object using the "this" pointer.
	*/
	m_ogreWindow = m_ogreRoot->createRenderWindow("QT Window",
		this->width(),
		this->height(),
		false,
		&parameters);
	m_ogreWindow->setVisible(true);

	/*
	The rest of the code in the initialization function is standard Ogre3D scene code. Consult other
	tutorials for specifics.
	*/
#if OGRE_VERSION >= ((2 << 16) | (0 << 8) | 0)
	const size_t numThreads = std::max<int>(1, Ogre::PlatformInformation::getNumLogicalCores());
	Ogre::InstancingTheadedCullingMethod threadedCullingMethod = Ogre::INSTANCING_CULLING_SINGLETHREAD;
	if (numThreads > 1)threadedCullingMethod = Ogre::INSTANCING_CULLING_THREADED;
	m_ogreSceneMgr = m_ogreRoot->createSceneManager(Ogre::ST_GENERIC, numThreads, threadedCullingMethod);
#else
	mSceneMgr = m_ogreRoot->createSceneManager(Ogre::ST_GENERIC);
#endif

	mOverlaySystem = new Ogre::OverlaySystem();
	mSceneMgr->addRenderQueueListener(mOverlaySystem);

	m_ogreCamera = mSceneMgr->createCamera("MainCamera");
	m_ogreCamera->setPosition(Ogre::Vector3(0, 2500, -2500));
	m_ogreCamera->lookAt(Ogre::Vector3(0.0f, 0, 0));
	m_ogreCamera->setNearClipDistance(0.1f);
	m_ogreCamera->setFarClipDistance(2000.0f);
	m_cameraMan = new OgreQtBites::SdkQtCameraMan(m_ogreCamera);   // create a default camera controller
	cameranode = mSceneMgr->getRootSceneNode()->createChildSceneNode("Cameranode");
	cameranode->setPosition(Ogre::Vector3(0, 1000, 0 ));
	m_cameraMan->setStyle(OgreQtBites::CS_ORBIT);
	m_cameraMan->setTarget(cameranode);
	m_cameraMan->setYawPitchDist(Ogre::Radian(Ogre::Real(0)), Ogre::Radian(Ogre::Real(0.2)), 2500);

#if OGRE_VERSION >= ((2 << 16) | (0 << 8) | 0)
	createCompositor();
#else
	mViewport = m_ogreWindow->addViewport(m_ogreCamera);
	mViewport->setBackgroundColour(m_ogreBackground);
#endif

	m_ogreCamera->setAspectRatio(
		Ogre::Real(m_ogreWindow->getWidth()) / Ogre::Real(m_ogreWindow->getHeight()));
	m_ogreCamera->setAutoAspectRatio(true);

	Ogre::TextureManager::getSingleton().setDefaultNumMipmaps(5);
	Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups();
	Ogre::FontManager::getSingletonPtr()->load("MyFont","Popular");

	createScene();

	m_ogreRoot->addFrameListener(this);

}

void QTOgreWindow::createScene()
{
	/*Example scene
		Derive this class for your own purpose and overwite this function to have a working Ogre widget with
		your own content.
		*/
	mSceneMgr->setAmbientLight(Ogre::ColourValue(1, 1, 1));
	mSceneMgr->setShadowTechnique(Ogre::SHADOWTYPE_STENCIL_ADDITIVE);

	//Ogre::Entity* entNinja = mSceneMgr->createEntity("Ninja", "ninja.mesh");
	//entNinja->setCastShadows(true);
	//Ogre::SceneNode* node = mSceneMgr->getRootSceneNode()->createChildSceneNode("NinjaNode");
	//node->attachObject(entNinja);

	//mAnimationState = entNinja->getAnimationState("Backflip");
	//mAnimationState->setLoop(true);

	grid = new OgreGrid(mSceneMgr,"BaseWhiteAlphaBlended");
	grid->showPlanes(false, true, false);
	grid->setCellSize(500, 0, 500);
	grid->showDivisions = false;
	grid->xmin = -5;
	grid->xmax = 5;
	grid->zmin = -5;
	grid->zmax = 5;
	grid->colorMaster = Ogre::ColourValue(1, 1, 1);
	grid->colorOffset = Ogre::ColourValue(0.8f, 0, 0, 1.0f);
	grid->attachToNode(mSceneMgr->getRootSceneNode());
	grid->update();

	upperarm = mSceneMgr->createManualObject("Upperarm");
	upperarm->begin("BaseWhiteNoLighting", Ogre::RenderOperation::OT_LINE_STRIP);
	upperarm->position(0,0,0);
	upperarm->position(0,0,0);
	upperarm->end();
	forearm = mSceneMgr->createManualObject("Forearm");
	forearm->begin("BaseWhiteNoLighting", Ogre::RenderOperation::OT_LINE_STRIP);
	forearm->position(0,0,0);
	forearm->position(0,0,0);
	forearm->end();
	stdupperarm = mSceneMgr->createManualObject("UpperarmStd");
	stdupperarm->begin("BaseWhiteNoLighting", Ogre::RenderOperation::OT_LINE_STRIP);
	stdupperarm->position(0, 0, 0);
	stdupperarm->position(0, 0, 0);
	stdupperarm->end();
	stdforearm = mSceneMgr->createManualObject("ForearmStd");
	stdforearm->begin("BaseWhiteNoLighting", Ogre::RenderOperation::OT_LINE_STRIP);
	stdforearm->position(0, 0, 0);
	stdforearm->position(0, 0, 0);
	stdforearm->end();

	mClient = new ViconDataStreamSDK::CPP::Client();

	textitem = new TextRenderer();
	textitem->TextRenderer::getSingleton().addTextBox("txtGreeting1", "Calibration Disabled", 0, 0, 100, 20, Ogre::ColourValue::White);
	textitem->TextRenderer::getSingleton().addTextBox("ElbowWristAngleCaption", "Forearm angle from vertical: ", 0, 20, 220, 20, Ogre::ColourValue::White);
	textitem->TextRenderer::getSingleton().addTextBox("ShoulderElbowAngleCaption", "Upperarm angle from vertical: ", 0, 40, 220, 20, Ogre::ColourValue::White);
	textitem->TextRenderer::getSingleton().addTextBox("ElbowWristAngle", "No angles", 220, 20, 100, 20, Ogre::ColourValue::White);
	textitem->TextRenderer::getSingleton().addTextBox("ShoulderElbowAngle", "No angles", 220, 40, 100, 20, Ogre::ColourValue::White);
}

void QTOgreWindow::render()
{
	/*
	How we tied in the render function for OGre3D with QWindow's render function. This is what gets call
	repeatedly. Note that we don't call this function directly; rather we use the renderNow() function
	to call this method as we don't want to render the Ogre3D scene unless everything is set up first.
	That is what renderNow() does.

	Theoretically you can have one function that does this check but from my experience it seems better
	to keep things separate and keep the render function as simple as possible.
	*/
	Ogre::WindowEventUtilities::messagePump();
	m_ogreRoot->renderOneFrame();

}

void QTOgreWindow::renderLater()
{
	/*
	This function forces QWindow to keep rendering. Omitting this causes the renderNow() function to
	only get called when the window is resized, moved, etc. as opposed to all of the time; which is
	generally what we need.
	*/
	if (!m_update_pending)
	{
		m_update_pending = true;
		QApplication::postEvent(this, new QEvent(QEvent::UpdateRequest));
	}
}

bool QTOgreWindow::event(QEvent *event)
{
	/*
	QWindow's "message pump". The base method that handles all QWindow events. As you will see there
	are other methods that actually process the keyboard/other events of Qt and the underlying OS.

	Note that we call the renderNow() function which checks to see if everything is initialized, etc.
	before calling the render() function.
	*/

	switch (event->type())
	{
	case QEvent::UpdateRequest:
		m_update_pending = false;
		renderNow();
		return true;

	default:
		return QWindow::event(event);
	}
}

/*
Called after the QWindow is reopened or when the QWindow is first opened.
*/
void QTOgreWindow::exposeEvent(QExposeEvent *event)
{
	Q_UNUSED(event);

	if (isExposed())
		renderNow();
}

/*
The renderNow() function calls the initialize() function when needed and if the QWindow is already
initialized and prepped calls the render() method.
*/
void QTOgreWindow::renderNow()
{
	if (!isExposed())
		return;

	if (m_ogreRoot == NULL)
	{
		initialize();
	}

	render();

	if (m_animating)
		renderLater();
}

/*
Our event filter; handles the resizing of the QWindow. When the size of the QWindow changes note the
call to the Ogre3D window and camera. This keeps the Ogre3D scene looking correct.
*/
bool QTOgreWindow::eventFilter(QObject *target, QEvent *event)
{
	if (target == this)
	{
		if (event->type() == QEvent::Resize)
		{
			if (isExposed() && m_ogreWindow != NULL)
			{
				m_ogreWindow->resize(this->width(), this->height());
			}
		}
	}

	return false;
}

/*
How we handle keyboard and mouse events.
*/
void QTOgreWindow::keyPressEvent(QKeyEvent * ev)
{
	if (m_cameraMan)
		m_cameraMan->injectKeyDown(*ev);
	if (ev->key() == Qt::Key_PageUp)
	{
		Ogre::Vector3 cposition = cameranode->getPosition();
		Ogre::Vector3 camposition = m_ogreCamera->getRealPosition();
		cameranode->setPosition(Ogre::Vector3(cposition.x, cposition.y + 10, cposition.z));
		m_ogreCamera->setPosition(camposition.x, camposition.y + 10, camposition.z);
		m_cameraMan->setTarget(cameranode);
	}
	else if (ev->key() == Qt::Key_PageDown)
	{
		Ogre::Vector3 cposition = cameranode->getPosition();
		Ogre::Vector3 camposition = m_ogreCamera->getRealPosition();
		cameranode->setPosition(Ogre::Vector3(cposition.x, cposition.y - 10, cposition.z));
		m_ogreCamera->setPosition(camposition.x, camposition.y - 10, camposition.z);
		m_cameraMan->setTarget(cameranode);
	}

}

void QTOgreWindow::keyReleaseEvent(QKeyEvent * ev)
{
	if (m_cameraMan)
		m_cameraMan->injectKeyUp(*ev);
}

void QTOgreWindow::mouseMoveEvent(QMouseEvent* e)
{
	static int lastX = e->x();
	static int lastY = e->y();
	int relX = e->x() - lastX;
	int relY = e->y() - lastY;
	lastX = e->x();
	lastY = e->y();

	if (m_cameraMan && (e->buttons() & Qt::LeftButton))
		m_cameraMan->injectMouseMove(relX, relY);
}

void QTOgreWindow::mouseWheelEvent(QWheelEvent *e)
{
	if (m_cameraMan)
		m_cameraMan->injectWheelMove(*e);
}

void QTOgreWindow::mousePressEvent(QMouseEvent* e)
{
	if (m_cameraMan)
		m_cameraMan->injectMouseDown(*e);
}

void QTOgreWindow::mouseReleaseEvent(QMouseEvent* e)
{
	if (m_cameraMan)
		m_cameraMan->injectMouseUp(*e);

	QPoint pos = e->pos();
	Ogre::Ray mouseRay = m_ogreCamera->getCameraToViewportRay(
		(Ogre::Real)pos.x() / m_ogreWindow->getWidth(),
		(Ogre::Real)pos.y() / m_ogreWindow->getHeight());
	Ogre::RaySceneQuery* pSceneQuery = mSceneMgr->createRayQuery(mouseRay);
	pSceneQuery->setSortByDistance(true);
	Ogre::RaySceneQueryResult vResult = pSceneQuery->execute();
	for (size_t ui = 0; ui < vResult.size(); ui++)
	{
		if (vResult[ui].movable)
		{
			if (vResult[ui].movable->getMovableType().compare("Entity") == 0)
			{
				emit entitySelected((Ogre::Entity*)vResult[ui].movable);
			}
		}
	}
	mSceneMgr->destroyQuery(pSceneQuery);
}

/*
Function to keep track of when we should and shouldn't redraw the window; we wouldn't want to do
rendering when the QWindow is minimized. This takes care of those scenarios.
*/
void QTOgreWindow::setAnimating(bool animating)
{
	m_animating = animating;

	if (animating)
		renderLater();
}

bool QTOgreWindow::frameRenderingQueued(const Ogre::FrameEvent& evt)
{
	m_cameraMan->frameRenderingQueued(evt);
	if (mAnimationState != NULL)
		mAnimationState->addTime((evt.timeSinceLastFrame)/animationscale);
	asdupdate(evt.timeSinceLastFrame / animationscale);
	cluster_finder();
	if (calibration == true)
	{
		calibrate();
	}
	liveplot();
	//liveplottest();
	angleCalculate();

	return true;
}

void QTOgreWindow::log(Ogre::String msg)
{
	if (Ogre::LogManager::getSingletonPtr() != NULL) Ogre::LogManager::getSingletonPtr()->logMessage(msg);
}

void QTOgreWindow::log(QString msg)
{
	log(Ogre::String(msg.toStdString().c_str()));
}

void QTOgreWindow::animatestart() // function to start the animation
{
	if (mAnimationState != NULL)
	{
		mAnimationState->setEnabled(true);
		mAnimationState->setTimePosition(Ogre::Real(0));
		animationtime = 0;
	}
}

void QTOgreWindow::animatestop() // function to stop the animation
{
	if (mAnimationState != NULL)
		mAnimationState->setEnabled(false);
}

void QTOgreWindow::animatelength(int length) // function to change length of mesh animation
{
	animationscale=length;
}

void QTOgreWindow::viconconnect(std::string address) // function to connect to the vicon system if not already connected
{
	ViconDataStreamSDK::CPP::Output_IsConnected Output = mClient->IsConnected();
	if (Output.Connected == false)
	{
		mClient->Connect(address);
		mClient->SetStreamMode(ViconDataStreamSDK::CPP::StreamMode::ClientPull);
		mClient->EnableUnlabeledMarkerData();
		mClient->EnableMarkerData();
		mClient->SetAxisMapping(ViconDataStreamSDK::CPP::Direction::Forward,
			ViconDataStreamSDK::CPP::Direction::Up,
			ViconDataStreamSDK::CPP::Direction::Right); // Y-up
		for (int i = 0; i < 100; i++)
		{
			viconPoint[i] = mSceneMgr->createEntity("sphere.mesh");
			viconNode[i] = mSceneMgr->getRootSceneNode()->createChildSceneNode(Ogre::Vector3(0, 0, 0));
			viconNode[i]->attachObject(viconPoint[i]);
			viconNode[i]->setScale(0.08, 0.08, 0.08);
			viconNode[i]->setVisible(false);
		}
	}
}

void QTOgreWindow::liveplot() // function to plot marker data as spheres once they have been identified as clusters
{
	Eigen::Vector3d reference, translate;
	for (int i = 0; i <= 4; i++)
	{
		if (wristnodes[i] != NULL)
		{
			reference(0) = wristnodes[i]->getPosition().x;
			reference(1) = wristnodes[i]->getPosition().y;
			reference(2) = wristnodes[i]->getPosition().z;
			translate = (rotation*reference) + translation;
			wristnodes[i]->setPosition(Ogre::Vector3(translate(0), translate(1), translate(2)));
			wristnodes[i]->setVisible(true);
		}
		if (uarmnodes[i] != NULL)
		{
			reference(0) = uarmnodes[i]->getPosition().x;
			reference(1) = uarmnodes[i]->getPosition().y;
			reference(2) = uarmnodes[i]->getPosition().z;
			translate = (rotation*reference) + translation;
			uarmnodes[i]->setPosition(Ogre::Vector3(translate(0), translate(1), translate(2)));
			uarmnodes[i]->setVisible(true);
		}
		if (sternumnodes[i] != NULL)
		{
			reference(0) = sternumnodes[i]->getPosition().x;
			reference(1) = sternumnodes[i]->getPosition().y;
			reference(2) = sternumnodes[i]->getPosition().z;
			translate = (rotation*reference) + translation;
			sternumnodes[i]->setPosition(Ogre::Vector3(translate(0), translate(1), translate(2)));
			sternumnodes[i]->setVisible(true);
		}
		if (acromiumnodes[i] != NULL)
		{
			reference(0) = acromiumnodes[i]->getPosition().x;
			reference(1) = acromiumnodes[i]->getPosition().y;
			reference(2) = acromiumnodes[i]->getPosition().z;
			translate = (rotation*reference) + translation;
			acromiumnodes[i]->setPosition(Ogre::Vector3(translate(0), translate(1), translate(2)));
			acromiumnodes[i]->setVisible(true);
		}
	}
	if (acromiumnodes[0] != NULL && uarmnodes[0] != NULL)
	{
		mSceneMgr->destroyManualObject("Upperarm");
		upperarm = mSceneMgr->createManualObject("Upperarm");
		upperarm->begin("BaseWhiteNoLighting", Ogre::RenderOperation::OT_LINE_STRIP);
		upperarm->position(acromiumnodes[0]->getPosition().x, acromiumnodes[0]->getPosition().y, acromiumnodes[0]->getPosition().z);
		upperarm->position(uarmnodes[0]->getPosition().x, uarmnodes[0]->getPosition().y, uarmnodes[0]->getPosition().z);
		upperarm->end();
		mSceneMgr->getRootSceneNode()->attachObject(upperarm);
	}	
	if (uarmnodes[0] != NULL && wristnodes[0] != NULL)
	{
		mSceneMgr->destroyManualObject("Forearm");
		forearm = mSceneMgr->createManualObject("Forearm");
		forearm->begin("BaseWhiteNoLighting", Ogre::RenderOperation::OT_LINE_STRIP);
		forearm->position(wristnodes[0]->getPosition().x, wristnodes[0]->getPosition().y, wristnodes[0]->getPosition().z);
		forearm->position(uarmnodes[0]->getPosition().x, uarmnodes[0]->getPosition().y, uarmnodes[0]->getPosition().z);
		forearm->end();
		mSceneMgr->getRootSceneNode()->attachObject(forearm);
	}
}

void QTOgreWindow::camerareset()
{
	m_cameraMan->setYawPitchDist(Ogre::Radian(Ogre::Real(0)), Ogre::Radian(Ogre::Real(0.2)), 2500);
}

void QTOgreWindow::createmesh(std::string meshname)
{
	if (node == NULL)
	{
		std::stringstream ssEntityName, ssNodeName;
		ssEntityName << meshname << meshnum;
		ssNodeName << "EntityNode" << meshnum;
		try
		{
			entmesh = mSceneMgr->createEntity(ssEntityName.str(), meshname);
			node = mSceneMgr->getRootSceneNode()->createChildSceneNode(ssNodeName.str());
			node->attachObject(entmesh);
			Ogre::AxisAlignedBox box = entmesh->getBoundingBox();
			if (moved == true)
			{
				node->setPosition(Ogre::Real(box.getCenter().x), -Ogre::Real(box.getCorner(Ogre::AxisAlignedBox::FAR_LEFT_BOTTOM).y * 10),
					Ogre::Real(box.getCenter().z + 1500));
			}
			else
				node->setPosition(Ogre::Real(box.getCenter().x), -Ogre::Real(box.getCorner(Ogre::AxisAlignedBox::FAR_LEFT_BOTTOM).y * 10),
					Ogre::Real(box.getCenter().z));
			node->setScale(10, 10, 10);
			node->yaw(Ogre::Degree(-90));
		}
		catch (Ogre::Exception &e)
		{
			emit mesherror();
			entmesh = NULL;
		}
	}
	else emit dataloaderror();
}

void QTOgreWindow::setanimation(std::string animationname)
{
	if (entmesh != NULL)
	{
		try
		{
			mAnimationState = entmesh->getAnimationState(animationname);
			mAnimationState->setLoop(true);
		}
		catch (Ogre::Exception &e)
		{
			emit animationerror();
			mAnimationState = NULL;
		}
	}
}

void QTOgreWindow::cluster_finder()
{
	// NB this function must be called before liveplot
	ViconDataStreamSDK::CPP::Output_IsConnected Output = mClient->IsConnected();
	if (Output.Connected == true)
	{
		mClient->GetFrame();
		unsigned int numNodes = mClient->GetUnlabeledMarkerCount().MarkerCount;
		if (numNodes >= 1)
		{
			for (unsigned int UnlabeledMarkerIndex = 0; UnlabeledMarkerIndex < numNodes; UnlabeledMarkerIndex++)
			{
				ViconDataStreamSDK::CPP::Output_GetUnlabeledMarkerGlobalTranslation _Output_GetUnlabeledMarkerGlobalTranslation =
					mClient->GetUnlabeledMarkerGlobalTranslation(UnlabeledMarkerIndex);
				float x = -_Output_GetUnlabeledMarkerGlobalTranslation.Translation[2];
				float y = _Output_GetUnlabeledMarkerGlobalTranslation.Translation[1];
				float z = _Output_GetUnlabeledMarkerGlobalTranslation.Translation[0];
				viconNode[UnlabeledMarkerIndex]->setPosition(Ogre::Vector3(x,y,z));
				viconNode[UnlabeledMarkerIndex]->setVisible(false);
			}
		}
		for (int i = 0; i <= 4; i++)
		{
			wristnodes[i] = NULL;
			uarmnodes[i] = NULL;
			acromiumnodes[i] = NULL;
			sternumnodes[i] = NULL;
		}
		Ogre::Vector3 vector1, vector2, vector3;
		//compare input radius to possible radii
		//make target node depending on radius
		for (int i = 0; i < numNodes; i++)
		{
			int counter = 0; // counter for radius  < 30
			int counter1 = 0; // counter for radius >= 30 and <40
			int counter3 = 0; //counter for radius >=30 and <80
			Ogre::SceneNode *temp[5], *temp1[5];
			vector1 = viconNode[i]->getPosition();
			int x0 = vector1[0];
			int y0 = vector1[1];
			int z0 = vector1[2];
			temp[0] = viconNode[i];
			temp1[0] = viconNode[i];
			for (int j = 0; j < numNodes; j++)
			{
				vector2 = viconNode[j]->getPosition();
				int x = vector2[0];
				int y = vector2[1];
				int z = vector2[2];

				if ((std::pow((x - x0), 2) + std::pow((y - y0), 2) + std::pow((z - z0), 2) < std::pow(50, 2)) &&
					(std::pow((x - x0), 2) + std::pow((y - y0), 2) + std::pow((z - z0), 2) >= std::pow(10, 2)))
				{
					counter++;
					temp[counter] = viconNode[j];
				}
				if ((std::pow((x - x0), 2) + std::pow((y - y0), 2) + std::pow((z - z0), 2) >= std::pow(30, 2)) &&
					(std::pow((x - x0), 2) + std::pow((y - y0), 2) + std::pow((z - z0), 2) <= std::pow(40, 2)))
				{
					counter1++;
					temp[counter1] = viconNode[j];
				}
				if ((std::pow((x - x0), 2) + std::pow((y - y0), 2) + std::pow((z - z0), 2) >= std::pow(10, 2)) &&
					(std::pow((x - x0), 2) + std::pow((y - y0), 2) + std::pow((z - z0), 2) <= std::pow(120, 2)))
				{
					counter3++;
					temp1[counter3] = viconNode[j];
				}
			}
			if (counter == 2)  	//assign ith pointer to target pointer
			{
				for (int z = 0; z <= counter; z++)
					wristnodes[z] = temp[z];
				wristnodes[2] = NULL;
				wristnodes[3] = NULL;
				wristnodes[4] = NULL;
			}
			if (counter == 3)
			{
				for (int z = 0; z <= counter; z++)
					uarmnodes[z] = temp[z];
			}

			if (counter1 == 4)
			{
				for (int z = 0; z <= counter1; z++)
					sternumnodes[z] = temp[z];
			}
			if (counter3 == 1 || counter3 == 2)
			{
				int counter2 = 0;
				Ogre::SceneNode *temp2[5];
				for (int a = 0; a < numNodes; a++)
				{
					vector3 = viconNode[a]->getPosition();
					int x2 = vector3[0];
					int y2 = vector3[1];
					int z2 = vector3[2];
					vector2 = temp1[1]->getPosition();
					int x = vector2[0];
					int y = vector2[1];
					int z = vector2[2];
					if ((std::pow((x - x2), 2) + std::pow((y - y2), 2) + std::pow((z - z2), 2) >= std::pow(10, 2)) &&
						(std::pow((x - x2), 2) + std::pow((y - y2), 2) + std::pow((z - z2), 2) <= std::pow(30, 2)) /*50*/ &&
						std::pow((x0 - x2), 2) + std::pow((y0 - y2), 2) + std::pow((z0 - z2), 2) >= std::pow(10, 2))
					{
						temp2[counter2] = viconNode[a];
						counter2++;
					}
				}
				if (counter2 == 1)
				{
					temp1[(counter2 + 1)] = temp2[(counter2 - 1)];
					acromiumnodes[0] = temp1[0];
					acromiumnodes[1] = temp1[1];
					acromiumnodes[2] = temp1[2];
					acromiumnodes[3] = NULL;
					acromiumnodes[4] = NULL;
				}
			}
		}
	}
	if (Output.Connected == false)
	{
		for (int i = 0; i <= 4; i++)
		{
			wristnodes[i] = NULL;
			uarmnodes[i] = NULL;
			acromiumnodes[i] = NULL;
			sternumnodes[i] = NULL;
		}
	}
}

void QTOgreWindow::liveplottest()
{
	ViconDataStreamSDK::CPP::Output_IsConnected Output = mClient->IsConnected();
	if (Output.Connected == true)
	{
		mClient->GetFrame();
		unsigned int numNodes = mClient->GetUnlabeledMarkerCount().MarkerCount;
		if (numNodes >= 1)
		{
			for (unsigned int UnlabeledMarkerIndex = 0; UnlabeledMarkerIndex < numNodes; UnlabeledMarkerIndex++)
			{
				Eigen::Vector3d reference, translate;
				
				ViconDataStreamSDK::CPP::Output_GetUnlabeledMarkerGlobalTranslation _Output_GetUnlabeledMarkerGlobalTranslation =
					mClient->GetUnlabeledMarkerGlobalTranslation(UnlabeledMarkerIndex);
				reference(0) = _Output_GetUnlabeledMarkerGlobalTranslation.Translation[2];
				reference(1) = _Output_GetUnlabeledMarkerGlobalTranslation.Translation[1];
				reference(2) = _Output_GetUnlabeledMarkerGlobalTranslation.Translation[0];
				translate = (rotation*reference) + translation;
				viconNode[UnlabeledMarkerIndex]->setPosition(Ogre::Vector3(translate(0), translate(1), translate(2)));
				viconNode[UnlabeledMarkerIndex]->setVisible(true);
			}
		}
	}
}

void QTOgreWindow::loadcsv(std::string address, std::string fps)
{
	if (animationdata.size()==0)
	{
		animationdata.clear();
		std::ifstream file(address);
		if (!file.is_open())
		{
			emit amdcsverror();
		}
		else
		{
			CSVRow row;
			while (file >> row)
			{
				animationdata.push_back(row);
			}
			framerate = std::stoi(fps);

			animationlength = animationdata.size();

			for (int i = 0; i < 100; i++)
			{
				viconPointstored[i] = mSceneMgr->createEntity("sphere2.mesh");
				Ogre::MaterialPtr m_Mat, m_NewMat;
				m_Mat = viconPointstored[i]->getSubEntity(0)->getMaterial();
				m_NewMat = m_Mat->clone("RedMaterial");
				m_NewMat->getTechnique(0)->getPass(0)->setAmbient(Ogre::ColourValue::Red);
				viconPointstored[i]->setMaterial(m_NewMat);
				viconNodestored[i] = mSceneMgr->getRootSceneNode()->createChildSceneNode(Ogre::Vector3(0, 0, 0));
				viconNodestored[i]->attachObject(viconPointstored[i]);
				viconNodestored[i]->setScale(0.08, 0.08, 0.08);
				viconNodestored[i]->setVisible(false);
			}
			csvptr = &animationdata.at(0);
			int j = 0;
			for (int i = 1; i < csvptr->size(); i += 3)
			{
				int x = std::stof(csvptr->m_data[i + 1]);
				int y = std::stof(csvptr->m_data[i + 2]);
				int z = std::stof(csvptr->m_data[i]);
				viconNodestored[j]->setPosition(Ogre::Vector3(x, y, z));
				j++;
			}
			for (int i = 0; i <= 4; i++)
			{
				wristnodes[i] = NULL;
				uarmnodes[i] = NULL;
				acromiumnodes[i] = NULL;
				sternumnodes[i] = NULL;
			}
			Ogre::Vector3 vector1, vector2, vector3;
			//compare input radius to possible radii
			//make target node depending on radius
			int numNodes = (csvptr->size() / 3);
			for (int i = 0; i < numNodes; i++)
			{
				int counter = 0; // counter for radius  < 30
				int counter1 = 0; // counter for radius >= 30 and <40
				int counter3 = 0; //counter for radius >=30 and <80
				Ogre::SceneNode *temp[5];
				vector1 = viconNodestored[i]->getPosition();
				int x0 = vector1[0];
				int y0 = vector1[1];
				int z0 = vector1[2];
				temp[0] = viconNodestored[i];
				for (int j = 0; j < numNodes; j++)
				{
					vector2 = viconNodestored[j]->getPosition();
					int x = vector2[0];
					int y = vector2[1];
					int z = vector2[2];

					if ((std::pow((x - x0), 2) + std::pow((y - y0), 2) + std::pow((z - z0), 2) < std::pow(30, 2)) &&
						(std::pow((x - x0), 2) + std::pow((y - y0), 2) + std::pow((z - z0), 2) >= std::pow(10, 2)))
					{
						counter++;
						temp[counter] = viconNodestored[j];
					}
					if ((std::pow((x - x0), 2) + std::pow((y - y0), 2) + std::pow((z - z0), 2) >= std::pow(30, 2)) &&
						(std::pow((x - x0), 2) + std::pow((y - y0), 2) + std::pow((z - z0), 2) <= std::pow(40, 2)))
					{
						counter1++;
						temp[counter1] = viconNodestored[j];
					}
					if ((std::pow((x - x0), 2) + std::pow((y - y0), 2) + std::pow((z - z0), 2) >= std::pow(40, 2)) &&
						(std::pow((x - x0), 2) + std::pow((y - y0), 2) + std::pow((z - z0), 2) <= std::pow(80, 2)))
					{
						counter3++;
						temp[counter3] = viconNodestored[j];
					}
				}
				if (counter == 4)  	//assign ith pointer to target pointer
				{
					for (int z = 0; z <= counter; z++)
						stdwristnodes[z] = temp[z];
					stdwristnodes[4] = NULL;
				}
				if (counter == 3)
				{
					for (int z = 0; z <= counter; z++)
						stduarmnodes[z] = temp[z];
				}

				if (counter1 == 4)
				{
					for (int z = 0; z <= counter1; z++)
						stdsternumnodes[z] = temp[z];
				}
				if (counter3 == 1 || counter3 == 2)
				{
					int counter2 = 0;
					Ogre::SceneNode *temp1[5];
					for (int a = 0; a < numNodes; a++)
					{
						vector3 = viconNodestored[a]->getPosition();
						int x2 = vector3[0];
						int y2 = vector3[1];
						int z2 = vector3[2];
						vector2 = temp[1]->getPosition();
						int x = vector2[0];
						int y = vector2[1];
						int z = vector2[2];
						if ((std::pow((x - x2), 2) + std::pow((y - y2), 2) + std::pow((z - z2), 2) >= std::pow(10, 2)) &&
							(std::pow((x - x2), 2) + std::pow((y - y2), 2) + std::pow((z - z2), 2) <= std::pow(50, 2)) &&
							std::pow((x0 - x2), 2) + std::pow((y0 - y2), 2) + std::pow((z0 - z2), 2) >= std::pow(10, 2))
						{
							temp1[counter2] = viconNodestored[a];
							counter2++;
						}
					}
					if (counter2 == 1)
					{
						temp[(counter2 + 1)] = temp1[(counter2 - 1)];
						stdacromiumnodes[0] = temp[0];
						stdacromiumnodes[1] = temp[1];
						stdacromiumnodes[2] = temp[2];
						stdacromiumnodes[3] = NULL;
						stdacromiumnodes[4] = NULL;
					}
				}
			}
			animationtime = 0;
			if (stdacromiumnodes[0] != NULL && stduarmnodes[0] != NULL && stduarmnodes[0] != NULL)
			{
				mSceneMgr->destroyManualObject("UpperarmStd");
				mSceneMgr->destroyManualObject("ForearmStd");
				stdupperarm = mSceneMgr->createManualObject("UpperarmStd");
				stdupperarm->begin("RedMaterial", Ogre::RenderOperation::OT_LINE_STRIP);
				stdupperarm->position(stdacromiumnodes[0]->getPosition().x, stdacromiumnodes[0]->getPosition().y, stdacromiumnodes[0]->getPosition().z);
				stdupperarm->position(stduarmnodes[0]->getPosition().x, stduarmnodes[0]->getPosition().y, stduarmnodes[0]->getPosition().z);
				stdupperarm->end();
				mSceneMgr->getRootSceneNode()->attachObject(stdupperarm);
				stdforearm = mSceneMgr->createManualObject("ForearmStd");
				stdforearm->begin("RedMaterial", Ogre::RenderOperation::OT_LINE_STRIP);
				stdforearm->position(stdwristnodes[0]->getPosition().x, stdwristnodes[0]->getPosition().y, stdwristnodes[0]->getPosition().z);
				stdforearm->position(stduarmnodes[0]->getPosition().x, stduarmnodes[0]->getPosition().y, stduarmnodes[0]->getPosition().z);
				stdforearm->end();
				mSceneMgr->getRootSceneNode()->attachObject(stdforearm);
			}
		}
	}
}

void QTOgreWindow::asdupdatetest(float time)
{
	if (animationdata.size() > 0)
	{
		if (mAnimationState != NULL)
		{
			if (mAnimationState->getEnabled() == true)
			{
				for (int i = 0; i < 100; i++)
					viconNodestored[i]->setVisible(false);
				int frame = 0;
				float ctime = time + animationtime;
				if (ctime >= (animationlength / framerate))
					ctime = ctime - (animationlength / framerate);
				frame = (int)(ctime * framerate);
				animationtime = ctime;
				csvptr = &animationdata.at(frame);
				int j = 0;
				for (int i = 1; i < csvptr->size(); i += 3)
				{
					int x = std::stoi(csvptr->m_data[i]);
					int y = std::stoi(csvptr->m_data[i + 2]);
					int z = std::stoi(csvptr->m_data[i + 1]);
					viconNodestored[j]->setPosition(Ogre::Vector3(x, y, z));
					viconNodestored[j]->setVisible(true);
					j++;
				}
			}
		}
	}
}

void QTOgreWindow::asdupdate(float time)
{
	if (animationdata.size() > 0)
	{
		if (mAnimationState != NULL)
		{
			if (mAnimationState->getEnabled() == true)
			{
				for (int i = 0; i < 100; i++)
					viconNodestored[i]->setVisible(false);
				int frame = 0;
				float ctime = time + animationtime;
				if (ctime >= (animationlength / framerate))
					ctime = ctime - (animationlength / framerate);
				frame = (int)(ctime * framerate);
				animationtime = ctime;
				csvptr = &animationdata.at(frame);
				int j = 0;
				for (int i = 1; i < csvptr->size(); i += 3)
				{
					float x = std::stof(csvptr->m_data[i+1]);
					float y = std::stof(csvptr->m_data[i + 2]);
					float z = std::stof(csvptr->m_data[i]);
					viconNodestored[j]->setPosition(Ogre::Vector3(x, y, z));
					viconNodestored[j]->setVisible(true);
					j++;
				}
			}
			else
			{
				csvptr = &animationdata.at(0);
				int j = 0;
				for (int i = 1; i < csvptr->size(); i += 3)
				{
					float x = std::stof(csvptr->m_data[i + 1]);
					float y = std::stof(csvptr->m_data[i + 2]);
					float z = std::stof(csvptr->m_data[i]);
					viconNodestored[j]->setPosition(Ogre::Vector3(x, y, z));
					viconNodestored[j]->setVisible(true);
					j++;
				}
			}
		}
		for (int i = 0; i <= 4; i++)
		{
			wristnodes[i] = NULL;
			uarmnodes[i] = NULL;
			acromiumnodes[i] = NULL;
			sternumnodes[i] = NULL;
		}
		Ogre::Vector3 vector1, vector2, vector3;
		//compare input radius to possible radii
		//make target node depending on radius
		int numNodes = (csvptr->size() / 3);
		for (int i = 0; i < numNodes; i++)
		{
			int counter = 0; // counter for radius  < 30
			int counter1 = 0; // counter for radius >= 30 and <40
			int counter3 = 0; //counter for radius >=30 and <80
			Ogre::SceneNode *temp[5];
			vector1 = viconNodestored[i]->getPosition();
			int x0 = vector1[0];
			int y0 = vector1[1];
			int z0 = vector1[2];
			temp[0] = viconNodestored[i];
			for (int j = 0; j < numNodes; j++)
			{
				vector2 = viconNodestored[j]->getPosition();
				int x = vector2[0];
				int y = vector2[1];
				int z = vector2[2];

				if ((std::pow((x - x0), 2) + std::pow((y - y0), 2) + std::pow((z - z0), 2) < std::pow(30, 2)) &&
					(std::pow((x - x0), 2) + std::pow((y - y0), 2) + std::pow((z - z0), 2) >= std::pow(10, 2)))
				{
					counter++;
					temp[counter] = viconNodestored[j];
				}
				if ((std::pow((x - x0), 2) + std::pow((y - y0), 2) + std::pow((z - z0), 2) >= std::pow(30, 2)) &&
					(std::pow((x - x0), 2) + std::pow((y - y0), 2) + std::pow((z - z0), 2) <= std::pow(40, 2)))
				{
					counter1++;
					temp[counter1] = viconNodestored[j];
				}
				if ((std::pow((x - x0), 2) + std::pow((y - y0), 2) + std::pow((z - z0), 2) >= std::pow(40, 2)) &&
					(std::pow((x - x0), 2) + std::pow((y - y0), 2) + std::pow((z - z0), 2) <= std::pow(80, 2)))
				{
					counter3++;
					temp[counter3] = viconNodestored[j];
				}
			}
			if (counter == 4)  	//assign ith pointer to target pointer
			{
				for (int z = 0; z <= counter; z++)
					stdwristnodes[z] = temp[z];
				stdwristnodes[4] = NULL;
			}
			if (counter == 3)
			{
				for (int z = 0; z <= counter; z++)
					stduarmnodes[z] = temp[z];
			}

			if (counter1 == 4)
			{
				for (int z = 0; z <= counter1; z++)
					stdsternumnodes[z] = temp[z];
			}
			if (counter3 == 1 || counter3 == 2)
			{
				int counter2 = 0;
				Ogre::SceneNode *temp1[5];
				for (int a = 0; a < numNodes; a++)
				{
					vector3 = viconNodestored[a]->getPosition();
					int x2 = vector3[0];
					int y2 = vector3[1];
					int z2 = vector3[2];
					vector2 = temp[1]->getPosition();
					int x = vector2[0];
					int y = vector2[1];
					int z = vector2[2];
					if ((std::pow((x - x2), 2) + std::pow((y - y2), 2) + std::pow((z - z2), 2) >= std::pow(10, 2)) &&
						(std::pow((x - x2), 2) + std::pow((y - y2), 2) + std::pow((z - z2), 2) <= std::pow(50, 2)) &&
						std::pow((x0 - x2), 2) + std::pow((y0 - y2), 2) + std::pow((z0 - z2), 2) >= std::pow(10, 2))
					{
						temp1[counter2] = viconNodestored[a];
						counter2++;
					}
				}
				if (counter2 == 1)
				{
					temp[(counter2 + 1)] = temp1[(counter2 - 1)];
					stdacromiumnodes[0] = temp[0];
					stdacromiumnodes[1] = temp[1];
					stdacromiumnodes[2] = temp[2];
					stdacromiumnodes[3] = NULL;
					stdacromiumnodes[4] = NULL;
				}
			}
		}
	}
	if (stdacromiumnodes[0] != NULL && stduarmnodes[0] != NULL && stduarmnodes[0] != NULL)
	{
		mSceneMgr->destroyManualObject("UpperarmStd");
		mSceneMgr->destroyManualObject("ForearmStd");
		stdupperarm = mSceneMgr->createManualObject("UpperarmStd");
		stdupperarm->begin("RedMaterial", Ogre::RenderOperation::OT_LINE_STRIP);
		stdupperarm->position(stdacromiumnodes[0]->getPosition().x, stdacromiumnodes[0]->getPosition().y, stdacromiumnodes[0]->getPosition().z);
		stdupperarm->position(stduarmnodes[0]->getPosition().x, stduarmnodes[0]->getPosition().y, stduarmnodes[0]->getPosition().z);
		stdupperarm->end();
		mSceneMgr->getRootSceneNode()->attachObject(stdupperarm);
		stdforearm = mSceneMgr->createManualObject("ForearmStd");
		stdforearm->begin("RedMaterial", Ogre::RenderOperation::OT_LINE_STRIP);
		stdforearm->position(stdwristnodes[0]->getPosition().x, stdwristnodes[0]->getPosition().y, stdwristnodes[0]->getPosition().z);
		stdforearm->position(stduarmnodes[0]->getPosition().x, stduarmnodes[0]->getPosition().y, stduarmnodes[0]->getPosition().z);
		stdforearm->end();
		mSceneMgr->getRootSceneNode()->attachObject(stdforearm);
	}
}

void QTOgreWindow::vicondisconnect()
{
	ViconDataStreamSDK::CPP::Output_IsConnected Output = mClient->IsConnected();
	if (Output.Connected == true)
	{
		mClient->Disconnect();
		for (int i = 0; i <= 4; i++)
		{
			if (wristnodes[i]!=NULL)
				mSceneMgr->destroySceneNode(wristnodes[i]);
			if (uarmnodes[i]!=NULL)
				mSceneMgr->destroySceneNode(uarmnodes[i]);
			if (sternumnodes[i]!=NULL)
				mSceneMgr->destroySceneNode(sternumnodes[i]);
			if (acromiumnodes[i]!=NULL)
				mSceneMgr->destroySceneNode(acromiumnodes[i]);
			wristnodes[i] = NULL;
			uarmnodes[i] = NULL;
			acromiumnodes[i] = NULL;
			sternumnodes[i] = NULL;
		}
	}
	mSceneMgr->destroyManualObject("Upperarm");
	mSceneMgr->destroyManualObject("Forearm");
	textitem->setText("ShoulderElbowAngle", "No angles");
	textitem->setText("ElbowWristAngle", "No angles");
}

void QTOgreWindow::closedata()
{
	//function that closes all animation data
	meshnum++;
	if (node != NULL)
	{
		node->setVisible(false);
	}
	if (mAnimationState!=NULL)
		mAnimationState = NULL;
	if (entmesh != NULL)
	{
		mSceneMgr->destroyEntity(entmesh);
		entmesh = NULL;
	}
	if (animationdata.size() > 0)
	{
		animationdata.clear();
		for (int i = 0; i <= 4; i++)
		{
			if (stdwristnodes[i] != NULL)
				mSceneMgr->destroySceneNode(stdwristnodes[i]);
			if (stduarmnodes[i] != NULL)
				mSceneMgr->destroySceneNode(stduarmnodes[i]);
			if (stdsternumnodes[i] != NULL)
				mSceneMgr->destroySceneNode(stdsternumnodes[i]);
			if (stdacromiumnodes[i] != NULL)
				mSceneMgr->destroySceneNode(stdacromiumnodes[i]);
			stdwristnodes[i] = NULL;
			stduarmnodes[i] = NULL;
			stdacromiumnodes[i] = NULL;
			stdsternumnodes[i] = NULL;
		}
	}

	rotation << 1, 0, 0, 0, 1, 0, 0, 0, 1;
	translation << 0, 0, 0;
	mSceneMgr->destroyManualObject("UpperarmStd");
	mSceneMgr->destroyManualObject("ForearmStd");
	node = NULL;
}

void QTOgreWindow::calibrate()
{
	if (stdsternumnodes[0]!=NULL && sternumnodes[0]!=NULL)
	{
		Eigen::MatrixXd A(5, 3);
		Eigen::MatrixXd B(5, 3);

		for (int i = 0; i <= 4; i++)
		{
			A(i, 0) = stdsternumnodes[i]->getPosition().x;
			A(i, 1) = stdsternumnodes[i]->getPosition().y;
			A(i, 2) = stdsternumnodes[i]->getPosition().z;
			B(i, 0) = sternumnodes[i]->getPosition().x;
			B(i, 1) = sternumnodes[i]->getPosition().y;
			B(i, 2) = sternumnodes[i]->getPosition().z;
		}

		int ia[4] = { 0 };
		int ib[6] = { 0 };

		float maxstdy, minstdy, maxstdz, minstdz;
		maxstdy = A.col(1).maxCoeff();
		minstdy = A.col(1).minCoeff();
		maxstdz = A.col(2).maxCoeff();
		minstdz = A.col(2).minCoeff();

		float maxy, miny, maxz, minz, maxx, minx;
		maxy = B.col(1).maxCoeff();
		miny = B.col(1).minCoeff();
		maxz = B.col(2).maxCoeff();
		minz = B.col(2).minCoeff();
		maxx = B.col(0).maxCoeff();
		minx = B.col(0).minCoeff();

		int mxstdy, mnstdy, mxstdz, mnstdz, mxy, mny, mxz, mnz, mxx, mnx;
		for (int i = 0; i < 5; i++)
		{
			if (A(i,1) == maxstdy)
				mxstdy = i;
			if (A(i,1) == minstdy)
				mnstdy = i;
			if (A(i,2) == maxstdz)
				mxstdz = i;
			if (A(i,2) == minstdz)
				mnstdz = i;
			
			if (B(i,0) == maxx)
				mxx = i;
			if (B(i,0) == minx)
				mnx = i;
			if (B(i,1) == maxy)
				mxy = i;
			if (B(i,1) == miny)
				mny = i;
			if (B(i,2) == maxz)
				mxz = i;
			if (B(i,2) == minz)
				mnz = i;
		}

		ia[0] = mxstdy;
		ia[1] = mnstdy;
		ia[2] = mxstdz;
		ia[3] = mnstdz;

		ib[0] = mxy;
		ib[1] = mny;
		ib[2] = mxz;
		ib[3] = mnz;
		ib[4] = mxx;
		ib[5] = mnx;

		if (abs(B(ib[4], 0) - B(0, 0)) > (abs(B(ib[2], 2) - B(0, 2))))
		{
			ib[2] = ib[4];
			ib[3] = ib[5];
		}

		int row = 5;
		int col = 3;

		Eigen::MatrixXd newA(5, 3), newB(5,3);

		for (int i = 1; i<5; i++)
		{
			for (int j = 0; j<col; j++)
			{
				newA(i, j) = A(ia[i - 1], j);
				newB(i, j) = B(ib[i - 1], j);
			}
		}
		for (int i = 0; i<3; i++)
		{
			newA(0, i) = A(0, i);
			newB(0, i) = B(0, i);
		}

		double acentroid[3] = { 0, 0, 0 };
		double bcentroid[3] = { 0, 0, 0 };
		for (int i = 0; i<col; i++)
		{
			for (int j = 0; j<row; j++)
			{
				acentroid[i] = acentroid[i] + newA(j, i);
				bcentroid[i] = bcentroid[i] + newB(j, i);
			}
		}

		for (int i = 0; i<col; i++)
		{
			acentroid[i] = acentroid[i] / row;
			bcentroid[i] = bcentroid[i] / row;
		}
		for (int i = 0; i<row; i++)
		{
			for (int j = 0; j<col; j++)
			{
				newA(i, j) = newA(i, j) - acentroid[j];
				newB(i, j) = newB(i, j) - bcentroid[j];
			}
		}
		Eigen::Matrix3d H;

		H = ((newB.transpose())*newA);

		Eigen::JacobiSVD<Eigen::Matrix3d> svd(H, Eigen::ComputeFullU | Eigen::ComputeFullV);
		Eigen::Matrix3d U;
		Eigen::Matrix3d V;
		U = svd.matrixU();
		V = svd.matrixV();
		
		rotation = V*(U.transpose());
		if (rotation.determinant() < 0)
		{
			V.col(2)*=-1;
			rotation = V*(U.transpose());
		}
		Eigen::Vector3d acentroidobj, bcentroidobj;
		for (int i = 0; i<3; i++)
		{
			acentroidobj(i) = acentroid[i];
			bcentroidobj(i) = bcentroid[i];
		}
		translation = -rotation*bcentroidobj + acentroidobj;
	}
}

void QTOgreWindow::enablecalibrate()
{
	if (calibration == false)
	{
		textitem->setText("txtGreeting1", "Calibration Enabled");
		calibration = true;
	}
	else
	{
		translation << 0, 0, 0;
		rotation << 1, 0, 0, 0, 1, 0, 0, 0, 1;
		calibration = false;
		textitem->setText("txtGreeting1", "Calibration Disabled");
	}
}

void QTOgreWindow::angleCalculate()
{
	if (sternumnodes[0] != NULL && acromiumnodes[0]!=NULL && uarmnodes[0] != NULL && wristnodes[0] != NULL)
	{
		Eigen::MatrixXd B(5, 3);

		for (int i = 0; i <= 4; i++)
		{
			B(i, 0) = sternumnodes[i]->getPosition().x;
			B(i, 1) = sternumnodes[i]->getPosition().y;
			B(i, 2) = sternumnodes[i]->getPosition().z;
		}

		int ib[6] = { 0 };

		float maxy, miny, maxz, minz, maxx, minx;
		maxy = B.col(1).maxCoeff();
		miny = B.col(1).minCoeff();
		maxz = B.col(2).maxCoeff();
		minz = B.col(2).minCoeff();
		maxx = B.col(0).maxCoeff();
		minx = B.col(0).minCoeff();

		int mxy, mny, mxz, mnz, mxx, mnx;
		for (int i = 0; i < 5; i++)
		{
			if (B(i, 0) == maxx)
				mxx = i;
			if (B(i, 0) == minx)
				mnx = i;
			if (B(i, 1) == maxy)
				mxy = i;
			if (B(i, 1) == miny)
				mny = i;
			if (B(i, 2) == maxz)
				mxz = i;
			if (B(i, 2) == minz)
				mnz = i;
		}

		ib[0] = mxy;
		ib[1] = mny;
		ib[2] = mxz;
		ib[3] = mnz;
		ib[4] = mxx;
		ib[5] = mnx;

		if (abs(B(ib[4], 0) - B(0, 0)) > (abs(B(ib[2], 2) - B(0, 0))))
		{
			ib[2] = ib[4];
			ib[3] = ib[5];
		}

		Eigen::Vector3d vertical, horizontal, wristelbow, elbowacromium;
		for (int i = 0; i < 3; i++)
		{
			vertical(i) = B(ib[0], i) - B(ib[1], i);
			horizontal(i) = B(ib[2], i) - B(ib[3], i);
		}
		wristelbow(0) = uarmnodes[0]->getPosition().x - wristnodes[0]->getPosition().x;
		wristelbow(1) = uarmnodes[0]->getPosition().y - wristnodes[0]->getPosition().y;
		wristelbow(2) = uarmnodes[0]->getPosition().z - wristnodes[0]->getPosition().z;

		elbowacromium(0) = acromiumnodes[0]->getPosition().x - uarmnodes[0]->getPosition().x;
		elbowacromium(1) = acromiumnodes[0]->getPosition().y - uarmnodes[0]->getPosition().y;
		elbowacromium(2) = acromiumnodes[0]->getPosition().z - uarmnodes[0]->getPosition().z;
		Eigen::Vector3d tempwe;
		int anglewe = atan2((wristelbow.cross(vertical)).norm(), wristelbow.dot(vertical)) * 180 / PI;
		int angleea = atan2((elbowacromium.cross(vertical)).norm(), elbowacromium.dot(vertical)) * 180 / PI;
		std::stringstream weangle, eaangle;
		textitem->setText("ShoulderElbowAngle", std::to_string(angleea));
		textitem->setText("ElbowWristAngle", std::to_string(anglewe));
	}
	else
	{
		/*textitem->setText("ShoulderElbowAngle", "No angles");
		textitem->setText("ElbowWristAngle", "No angles");*/
	}
}

void QTOgreWindow::skeletonmove(int state)
{
	Ogre::Vector3 cposition = node->getPosition();
	Ogre::Vector3 camposition = cameranode->getPosition();
	if(state == 2 && node !=NULL)
	{
		node->setPosition(cposition.x, cposition.y, cposition.z+1500);
		cameranode->setPosition(camposition.x, camposition.y, camposition.z+750);
		moved = true;
		m_cameraMan->setTarget(cameranode);
	}
	else if (state == 0 && node != NULL)
	{
		node->setPosition(cposition.x, cposition.y, cposition.z-1500);
		cameranode->setPosition(camposition.x, camposition.y, camposition.z-700);
		moved = false;
		m_cameraMan->setTarget(cameranode);
	}
}

void QTOgreWindow::writecsv()
{
	/*std::stringstream name;
	name << "Log_" << csvcount << ".csv";
	outputcsv = new std::ofstream(name.str());
	outputcsv->operator<< "Frame Interval,x,y,z\n";*/

}

void QTOgreWindow::savecsv()
{

}

void QTOgreWindow::csvwriter(float time)
{
	/*outputcsv->operator<<"time";
	for (int i = 0; i <= 4; i++)
	{
		if (sternumnodes[i] != NULL)
		{
			float x = sternumnodes[i]->getPosition().x;
			float y = sternumnodes[i]->getPosition().y;
			float z = sternumnodes[i]->getPosition().z;
			outputcsv->operator<<x, y, z;
		}
	}*/
}