#ifndef QTOGREWINDOW_H
#define QTOGREWINDOW_H

#include <QtWidgets/QApplication>
#include <QtGui/QKeyEvent>
#include <QtGui/QWindow>
#include <Ogre.h>
#include <OgreOverlaySystem.h>
#include <Client.h>
#include <iostream>
#include <fstream>
#include <cassert>
#include <ctime>
#include <vector>
#include <string.h>
#include "TextRenderer.h"
#include <Qmessagebox.h>
#include "OgreGrid.h"
#include "CSVRead.h"
#include <OgreException.h>
#include <Eigen/Dense>
#include <Eigen/SVD>
#include <math.h>
#include "SdkQtCameraManHugo.h"

#define PI 3.14159265

// header for the class of the Ogre Render Window

class QTOgreWindow : public QWindow, public Ogre::FrameListener //inherits from QObject so signals can be sent to the OgreWindow
{
	Q_OBJECT

public:
	explicit QTOgreWindow(QWindow *parent = NULL);
	~QTOgreWindow();

	virtual void render(QPainter *painter);
	virtual void render();
	virtual void initialize();
	virtual void createScene();
#if OGRE_VERSION >= ((2 << 16) | (0 << 8) | 0)
	virtual void createCompositor();
#endif
	Ogre::AnimationState* mAnimationState;
	void setAnimating(bool animating);

	public slots:
	virtual void renderLater();
	virtual void renderNow();
	virtual void animatestart();
	virtual void animatestop();
	virtual void animatelength(int length);
	virtual void viconconnect(std::string address); // function to connect to the vicon server
	virtual void liveplot(); //function which plots marker clusters in real time
	virtual void camerareset();  // resets the camera view to that originally initialised
	virtual void createmesh(std::string meshname); //creates a mesh when a new animation is selected
	virtual void setanimation(std::string animationname); //sets the animation of the mesh when a new animation is selected
	virtual void cluster_finder(); //algorithm for finding the clusters
	virtual void liveplottest();  //test function which plots all the markers recieved in the vicon data
	virtual void loadcsv(std::string csvname, std::string fps); //loads a csv containing stored marker data for animations
	virtual void asdupdatetest(float time); //manual updating function for the stored marker data when animated
	virtual void asdupdate(float time); //manual updating function for the stored marker data when animated
	virtual void vicondisconnect(); //disconnects from the vicon server
	virtual void closedata(); //closes all the animation data (mesh and pre-recorded markers)
	virtual void calibrate(); //function for calibration
	virtual void enablecalibrate(); //toggles calibration
	virtual void angleCalculate(); //function to calculate angles
	virtual void skeletonmove(int state); //function for the split-scene option (simply translates skeleton and camera)
	virtual void writecsv(); //functions to write and save csvs, not implemented 
	virtual void savecsv();
	virtual void csvwriter(float time);


	virtual bool eventFilter(QObject *target, QEvent *event);

signals:
	void entitySelected(Ogre::Entity* entity);
	void amdcsverror(); //error signals to be sent if Ogre fails to find the data required for an animation
	void mesherror();
	void animationerror();
	void dataloaderror();

protected:
	Ogre::Root* m_ogreRoot; //Lots of useful ogre Pointers here, read Ogre tutorials to understand these
	Ogre::RenderWindow* m_ogreWindow;
	Ogre::SceneManager* mSceneMgr;
	Ogre::Camera* m_ogreCamera;
	Ogre::SceneNode* cameranode;
	Ogre::ColourValue m_ogreBackground;
	OgreQtBites::SdkQtCameraMan* m_cameraMan;
	Ogre::String mResourcesCfg;
	Ogre::String mPluginsCfg;
	Ogre::Viewport *mViewport;
	Ogre::Viewport *slViewport;
	Ogre::Viewport *srViewport;
	ViconDataStreamSDK::CPP::Client *mClient; //Pointer to vicon client object
	Ogre::OverlaySystem* mOverlaySystem; //Pointer to text overlay 
	TextRenderer *textitem;
	Ogre::Entity* entmesh;  //pointer for new entity
	Ogre::SceneNode* node; //pointer for scenenode of mesh used in animation
	Ogre::Entity* viconPoint[100];  //FIX THE ARRAY LENGTH DECLARATION - THIS IS NOT LEGIT
	Ogre::SceneNode* viconNode[100];
	Ogre::Entity* viconPointstored[100]; //array of pointers to nodes for stored data
	Ogre::SceneNode* viconNodestored[100];
	OgreGrid* grid;
	Ogre::SceneNode *wristnodes[5]; //pointers to shoulder node - 0 = centrenode, 1+ = surrounding nodes
	Ogre::SceneNode *uarmnodes[5]; //pointers to uarm node - 0 = centrenode, 1+ = surrounding nodes
	Ogre::SceneNode *sternumnodes[5]; //pointers to sternum node - 0 = centrenode, 1+ = surrounding nodes
	Ogre::SceneNode *acromiumnodes[5];
	Ogre::SceneNode *stdsternumnodes[5]; 
	Ogre::SceneNode *stdwristnodes[5];
	Ogre::SceneNode *stdacromiumnodes[5];
	Ogre::SceneNode *stduarmnodes[5];
	CSVRow *csvptr;
	Ogre::ManualObject *upperarm, *forearm, *stdupperarm, *stdforearm;

	Eigen::Matrix3d rotation;
	Eigen::Vector3d translation;
	bool m_update_pending;
	bool m_animating;
	bool calibration; // true = calibration occuring
	float animationlength;  // length of animation in frames
	float animationtime; // current animation time state
	int animationscale; // scaling factor to change speed of animation
	float framerate; // animation frame rate
	float offset; // x axis offset for animation data from skeleton mesh
	int meshnum; // incrementer for loading multiple meshes
	int csvcount; // incrementer for writing multiple csvs
	bool writing; // true = csv is being written, false = csv not being written
	std::ofstream *outputcsv;
	bool moved; //bool for if skelton has been moved
	std::vector<CSVRow> animationdata;


	virtual void keyPressEvent(QKeyEvent * ev);
	virtual void keyReleaseEvent(QKeyEvent * ev);
	virtual void mouseMoveEvent(QMouseEvent* e);
	virtual void mouseWheelEvent(QWheelEvent* e);
	virtual void mousePressEvent(QMouseEvent* e);
	virtual void mouseReleaseEvent(QMouseEvent* e);
	virtual void exposeEvent(QExposeEvent *event);
	virtual bool event(QEvent *event);


	virtual bool frameRenderingQueued(const Ogre::FrameEvent& evt);

	void log(Ogre::String msg);
	void log(QString msg);
private:
};

#endif // QTOGREWINDOW_H