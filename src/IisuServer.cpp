#include "IisuServer.h"

enum POINTER_STATUS
{
	POINTER_STATUS_NOT_DETECTED = 0,
	POINTER_STATUS_STILL = 1,
	POINTER_STATUS_OUT_OF_BOX = 2,
	POINTER_STATUS_ACTIVE = 3
};


void IisuServer::setup( bool _bCloseInteraction ) 
{
	bCloseInteraction = _bCloseInteraction ;

	bConnected = false ; 
	// We need to specify where is located the iisu dll and it's configuration file.
	// in this sample we'll use the SDK's environment variable.
	string dllLocation = getenv("IISU_SDK_DIR") ;
	dllLocation+="/bin" ;

	// get the working context
	Context& context = Context::Instance();

	// create an iisu configuration
	IisuHandle::Configuration iisuConfiguration(dllLocation.c_str(),"iisu_config.xml");
	// you can customize the configuration here
	
	// create the handle according to the configuration structure
	Return<IisuHandle*> retHandle = context.createHandle(iisuConfiguration);
	if(retHandle.failed())
	{
		cerr << "Failed to get iisu handle!" << endl
			<< "Error " << retHandle.getErrorCode() << ": " << retHandle.getDescription().ptr() << endl;
		getchar();
		exit(0);
	}

	// get the iisu handle
	m_iisuHandle = retHandle.get();

	// create device configuration
	Device::Configuration deviceConfiguration ;
	// you can customize the configuration here

	// create the device according to the configuration structure
	Return<Device*> retDevice = m_iisuHandle->initializeDevice(deviceConfiguration);
	if(retDevice.failed())
	{
		cerr << "Failed to create device!" << endl
			<< "Error " << retDevice.getErrorCode() << ": " << retDevice.getDescription().ptr() << endl;
		getchar();
		exit(0);
	}
	// get the device
	m_device = retDevice.get();

	registerEvents() ; 
	initIisu() ; 
	m_skeletonStatus = 0 ; 
	numHands = 0 ; 
}

int IisuServer::addController( ) 
{
	int iisuIndex = controllerIsActiveData.size() + 1 ; 

	string pointerString = "UI.CONTROLLER" + ofToString( iisuIndex ) ;

	string activeString = pointerString +".IsActive"  ; 
	controllerIsActiveData.push_back(  m_device->registerDataHandle<bool>( activeString.c_str() ) ) ;
	controllerIsActive.push_back ( false ) ; 
	
	string normalizedString = pointerString + ".POINTER.NormalizedCoordinates" ; 
	pointerNormalizedCoordinatesData.push_back( m_device->registerDataHandle<Vector3>( normalizedString.c_str() ) );
	pointerNormalizedCoordinates.push_back( Vector3( ) ) ; 

	//UI.CONTROLLER#.POINTER.WorldCoordinates
	string statusString = pointerString + ".POINTER.Status" ; 
	pointerStatusData.push_back( m_device->registerDataHandle<int32_t>(statusString.c_str() ) );
	pointerStatus.push_back( 0 ) ;


	string globalString = pointerString + ".POINTER.WorldCoordinates" ; 
	pointerGlobalCoordinatesData.push_back( m_device->registerDataHandle<Vector3>( globalString.c_str() ) ); 
	pointerGlobalCoordinates.push_back( Vector3() ) ; 

	return ( iisuIndex - 1 ) ; 
	
}
int IisuServer::addCloseInteractionHand ( ) 
{
	int iisuIndex = numHands + 1 ; 
	string handString = "CI.HAND" + ofToString( iisuIndex ) + "." ;

	string handStatus = handString + "Status" ; 
	handStatusesHandle.push_back( m_device->registerDataHandle<int32_t>( handStatus.c_str() ) ) ; 
	handStatuses.push_back ( 0 ) ; 

	string handPalmPosition2D = handString + "PalmPosition2D" ; 
	handPalmPositions2DHandle.push_back( m_device->registerDataHandle<Vector2>( handPalmPosition2D.c_str() )) ; 
	handPalmPositions2D.push_back( Vector2() ) ;

	string handTipPosition2D = handString + "TipPosition2D" ;
	handTipPositions2DHandle.push_back( m_device->registerDataHandle<Vector2>( handTipPosition2D.c_str() )) ;
	handTipPositions2D.push_back( Vector2() ) ;

	string handFingerStatusString = handString + "FingerStatus" ; 
	handFingerTipsStatusHandle.push_back( m_device->registerDataHandle<SK::Array<int32_t>>( handFingerStatusString.c_str() )) ;
	SK::Array<int32_t> fingerArgs ;
	//for ( int i = 0 ; i < 5 ; i++ ) fingerArgs.pushBack( 0 ) ; 
	handFingerTipsStatus.push_back( fingerArgs ) ; 

	string handFingerTips2DString = handString + "FingerTipPositions2D" ; 
	handFingerTips2DHandle.push_back( m_device->registerDataHandle<SK::Array<Vector2>>( handFingerTips2DString.c_str() )) ;
	SK::Array<Vector2> fingerTipArgs ; 

	handFingerTips2D.push_back( fingerTipArgs ) ; 

	string handsOpenStatus = handString + "IsOpen" ; 
	handsOpenHandle.push_back( m_device->registerDataHandle<bool>( handsOpenStatus.c_str() ) ) ; 
	handsOpen.push_back( false ) ; 

	string handOpenAmount = handString + "Openness" ; 
	handsOpenAmountHandle.push_back( m_device->registerDataHandle<float>( handOpenAmount.c_str() ) ) ; 
	handsOpenAmount.push_back( 0.0f ) ; 


	//vector<bool>								handsOpen ; 
//		vector<float>								handsOpenAmount ; 	


	numHands++ ; 

	return ( iisuIndex - 1 ) ; 
}

void IisuServer::initIisu() 
{
	//User
	m_user1SceneID = m_device->registerDataHandle<int32_t>("USER1.SceneObjectID") ; 
	m_userIsActiveData = m_device->registerDataHandle<bool>("USER.IsActive") ; 

	//Volume + Skeleton
	m_skeletonStatusData = m_device->registerDataHandle<int>("USER1.SKELETON.Status");
	m_keyPointsData = m_device->registerDataHandle<Array<Vector3> >("USER1.SKELETON.KeyPoints");
	m_keyPointsConfidenceData = m_device->registerDataHandle<Array<float> >("USER1.SKELETON.KeyPointsConfidence");
	m_centroidCountParameter = m_device->registerParameterHandle<int32_t>( "SHAPE.CENTROIDS.Count" ) ; 
	m_centroidPositionsData = m_device->registerDataHandle<SK::Array<SK::Vector3>>("USER1.SHAPE.CENTROIDS.Positions") ;
	m_user1MassCenterData = m_device->registerDataHandle<Vector3>("USER1.MassCenter") ; 
	m_centroidsJumpStatusHandle = m_device->registerDataHandle< Array<int> >("USER1.SHAPE.CENTROIDS.JumpStatus" ) ; 

	//Camera
	sceneImageHandle = m_device->registerDataHandle< SK::Image >("SCENE.LabelImage") ; 

	m_centroidCountParameter.set( 150 ) ; 


	
	cout << "IS close interaction enabled m_CI_Enabled ? " << m_CI_Enabled << " bCloseInteraction : " << bCloseInteraction << endl ;

	if ( bCloseInteraction == true ) 
	{
		m_CI_EnabledHandle = m_device->registerParameterHandle<bool>("CI.Enabled") ; 
		Result res = m_device->getEventManager().registerEventListener( "CI.HandActivated" , *this , &IisuServer::handActivatedHandler ) ; 
		if ( res.failed() )
			cout << "failed to regsiter CI.HandActivated!" << endl ; 
		else
			cout << "succesfully registered CI.HandActivat6ed! " << endl ; 

		res = m_device->getEventManager().registerEventListener( "CI.HandDeactivated" , *this , &IisuServer::handDeactivatedHandler ) ; 
		if ( res.failed() )
			cout << "failed to regsiter CI.HandDeactivated!" << endl ; 
		else
			cout << "succesfully registered CI.HandDeactivated! " << endl ; 
	}
	// we need it check if applicatin is set-up properly
	//m_uiEnabledParameter = m_device->registerParameterHandle<bool>("UI.Enabled");
	//m_controllersCount = m_device->registerParameterHandle<int32_t>("UI.ControllerCount");

	// we will use circle gestures to create controller and to start game, therefore...
	// ...set-up activation gesture (for simplicity we will use just circle gesture)
	ParameterHandle<bool> circleActivation = m_device->registerParameterHandle<bool>("UI.ACTIVATIONGESTURE.CIRCLE.Enabled");
	circleActivation.set(true);

	// ...make sure that gestures and circle gesture are enabled
	ParameterHandle<bool> gesturesEnabled = m_device->registerParameterHandle<bool>("UI.CONTROLLERS.GESTURES.Enabled");
	ParameterHandle<bool> circleGestureEnabled = m_device->registerParameterHandle<bool>("UI.CONTROLLERS.GESTURES.CIRCLE.Enabled");

	gesturesEnabled.set(true);
	circleGestureEnabled.set(true);

	// register this object to listen for UI.CONTROLLERS.Created event (we satrt game as soon as controller is created)
	Result res = m_device->getEventManager().registerEventListener("UI.CONTROLLERS.Created", *this, &IisuServer::onControllerCreated);
	if (res.failed()) 
	{
		cerr << "Failed to register in iisu for UI.CONTROLLERS.Created events!" << endl;
	}

	// register this object to listen for UI.CONTROLLERS.GESTURES.CIRCLE.Detected event (we might have controller already
	res = m_device->getEventManager().registerEventListener("UI.CONTROLLERS.GESTURES.CIRCLE.Detected", *this, &IisuServer::onCircleGesture);
	if (res.failed()) 
	{
		cerr << "Failed to register in iisu for UI.CONTROLLERS.GESTURES.CIRCLE.Detected events!" << endl;
	}

	SK::Result devStart = m_device->start();
	if(devStart.failed())
	{
		cerr << "Failed to start device!" << endl
			<< "Error " << devStart.getErrorCode() << ": " << devStart.getDescription().ptr() << endl;
		getchar();
		exit(0);
	}
}

void IisuServer::handActivatedHandler( SK::HandActivatedEvent ) 
{
	cout << "hand activated! " << endl ;
}
void IisuServer::handDeactivatedHandler( SK::HandDeactivatedEvent )
{
	cout << "hand deactivated! " << endl ; 

}


void IisuServer::onError(const ErrorEvent& event)
{
	cerr << "iisu error : " << event.getError().getDescription().ptr() << endl;
	getchar();
}

void IisuServer::onControllerCreated(ControllerCreationEvent event)
{
	//First controller created 
	cout << "IisuServer::onControllerCreated !! " << endl ; 
}

void IisuServer::onCircleGesture(CircleGestureEvent event)
{
	cout << "IisuServer::onCircleGesture !! " << endl ; 	
}


void IisuServer::onDataFrame(const DataFrameEvent& event)	
{
  	SK::Result resUpdate = m_device->updateFrame( false ) ; 
	if(resUpdate.failed())
	{
		cerr << "Failed to update data frame" << endl;

	}
	// the rest of the logic depends on iisu data, so we need to make sure that we have
	// already a new data frame
	const int32_t currentFrameID = m_device->getDataFrame().getFrameID();
	if (currentFrameID == m_lastFrameID)
	{
		cout << "Same Frame as before" << endl ; 
		bConnected = false ; 
		//return;
	}
	else
	{
		//cout << "frame# " << currentFrameID << endl ; 
		bConnected = true ; 
	}

	// remember current frame id
	m_lastFrameID = currentFrameID;

	//USER
	m_user1MassCenter = m_user1MassCenterData.get() ; 
	m_userIsActive = m_userIsActiveData.get() ; 
	sceneImage = sceneImageHandle.get() ; 
	user1SceneID = m_user1SceneID.get() ;

	//Look through all our cursor data
	for ( int i = 0 ; i < pointerStatusData.size() ; i++ ) 
	{
		pointerStatus[ i ]  = pointerStatusData[ i ].get() ; 
		pointerNormalizedCoordinates[ i ] = pointerNormalizedCoordinatesData[ i ].get() ; 
		controllerIsActive[ i ] = controllerIsActiveData[ i ].get( ) ; 
		pointerGlobalCoordinates[ i ] = pointerGlobalCoordinatesData[ i ].get() ; 
	}

	if ( bCloseInteraction ) 
	{
		for ( int i = 0 ; i < handStatuses.size() ; i++ ) 
		{
			handStatuses[i] = handStatusesHandle[i].get() ; 
			handPalmPositions2D[i] = handPalmPositions2DHandle[i].get() ; 
			handTipPositions2D[i] = handTipPositions2DHandle[i].get() ; 
			handsOpen[i] = handsOpenHandle[i].get() ; 
			handsOpenAmount[i] = handsOpenAmountHandle[i].get() ; 
			handFingerTipsStatus[i] = handFingerTipsStatusHandle[i].get() ; 
			handFingerTips2D[i] = handFingerTips2DHandle[i].get() ;
		}
	}
	
	//Skeleton + Volume
	m_centroidCount = m_centroidCountParameter.get() ; 
	m_centroidPositions = m_centroidPositionsData.get() ; 
	m_skeletonStatus = m_skeletonStatusData.get() ; 	

	if ( m_skeletonStatus != 0 ) 
	{
		m_keyPoints = m_keyPointsData.get() ; 
		m_keyPointsConfidence = m_keyPointsConfidenceData.get() ; 
		m_centroidJumpStatus = m_centroidsJumpStatusHandle.get( ) ; 
	}

	if ( m_skeletonStatus != last_skeletonStatus ) 
	{
		int args = 9 ; 
		if ( m_skeletonStatus != 0 ) 
			ofNotifyEvent( IisuEvents::Instance()->USER_DETECTED , args ) ; 
		else
			ofNotifyEvent( IisuEvents::Instance()->USER_LOST , args ) ; 
	}

	// tell iisu we finished using data.
	m_device->releaseFrame();

	last_skeletonStatus = m_skeletonStatus ;
}

void IisuServer::registerEvents ( ) 
{
	if (m_device==NULL || m_iisuHandle==NULL)
	{
		cerr << "Iisu is not initialized" <<endl;
		getchar();
		exit();
	}
	// system errors events
	Result ret = m_iisuHandle->getEventManager().registerEventListener("SYSTEM.Error", *this, &IisuServer::onError);
	if (ret.failed()) 
	{
		cerr << "Failed to register for system error events!" << endl
			<< "Error " << ret.getErrorCode() << ": " << ret.getDescription().ptr() << endl;
		getchar();
		exit();
	}

	// a new dataframe has been computed by iisu
	ret = m_iisuHandle->getEventManager().registerEventListener("DEVICE.DataFrame", *this, &IisuServer::onDataFrame);
	if (ret.failed()) 
	{
		cerr << "Failed to register for data frame events!" << endl
			<< "Error " << ret.getErrorCode() << ": " << ret.getDescription().ptr() << endl;
		getchar();
		exit();
	}

	if ( bCloseInteraction )
	{
		ret = m_iisuHandle->getEventManager().registerEventListener("CI.HandPosingGesture", *this, &IisuServer::handPoseGestureHandler );
		if (ret.failed()) 
		{
			cerr << "Failed to register for CI.HandPosing Gesture events!" << endl
				<< "Error " << ret.getErrorCode() << ": " << ret.getDescription().ptr() << endl;
			getchar();
			exit();
		}
	}
	/*
	// users activation events 
	ret = m_iisuHandle->getEventManager().registerEventListener("UM.UserActivated", *this, &IisuServer::onUserActivation);
	if (ret.failed()) 
	{
		cerr << "Failed to register for user activation events!" << endl
			<< "Error " << ret.getErrorCode() << ": " << ret.getDescription().ptr() << endl;
		getchar();
		exit();
	}

	// users deactivation events 
	ret = m_iisuHandle->getEventManager().registerEventListener("UM.UserDeactivated", *this, &IisuServer::onUserDeactivation);
	if (ret.failed()) 
	{
		cerr << "Failed to register for user deactivation events!" << endl
			<< "Error " << ret.getErrorCode() << ": " << ret.getDescription().ptr() << endl;
		getchar();
		exit();
	}
	
	*/
}

void IisuServer::handPoseGestureHandler ( SK::HandPosingGestureEvent e ) 
{
	//Get posing metaInfo
	SK::Return<SK::MetaInfo<SK::HandPosingGestureEvent> > retMetaInfo = m_device->getEventManager().getMetaInfo<SK::HandPosingGestureEvent>("CI.HandPosingGesture") ; 

	if ( retMetaInfo.succeeded() ) 
	{
		SK::MetaInfo<SK::HandPosingGestureEvent> poseMetaInfo = retMetaInfo.get() ; 
		const SK::EnumMapper &enumMapper = poseMetaInfo.getEnumMapper() ; 

		int gestureID = e.getGestureTypeID() ; 
		SK::Return<SK::String> retPosingGestureName = enumMapper[gestureID] ; 
		if ( retPosingGestureName.succeeded() ) 
		{
			SK::String gestureName = retPosingGestureName.get() ; 
			cout << "pose: " << gestureName << "   _ gestureTypeID : " << e.getGestureTypeID() << "   _ firstHand ID: " << e.getFirstHandID() << endl ; 
			int args =  e.getGestureTypeID() ; 
			ofNotifyEvent( IisuEvents::Instance()->POSE_GESTURE , args ) ; 
		}
	}
	else
	{
		cout << "meta Info did not succeed! " << endl ; 
	}
}

int IisuServer::getCursorStatus ( int cursorID ) 
{
	if ( cursorID < pointerStatus.size() ) 
		return pointerStatus[ cursorID ] ; 
	else
		cout << "IisuServer::getCursorStatus :: INVALID INDEX" << endl ; 
	

	return -1 ; 
}

Vector3 IisuServer::getNormalizedCursorCoordinates ( int cursorID ) 
{	
	if ( cursorID < pointerStatus.size() ) 
		return pointerNormalizedCoordinates[ cursorID ] ;
	else
		cout << "IisuServer::getNormalizedCursorCoordinates :: INVALID INDEX" << endl ;

	return Vector3( ) ; 
}

Vector3 IisuServer::getWorldCursorPosition( int cursorID ) 
{
	if ( cursorID < pointerStatus.size() ) 
		return pointerGlobalCoordinates[ cursorID ] ;
	else
		cout << "IisuServer::getNormalizedCursorCoordinates :: INVALID INDEX" << endl ;

	return Vector3( ) ; 
}

void IisuServer::exit ( int exitCode ) 
{
	m_iisuHandle->getEventManager().unregisterEventListener( "SYSTEM.Error" , *this , &IisuServer::onError ) ; 
	m_iisuHandle->getEventManager().unregisterEventListener( "DEVICE.DataFrame" , *this , &IisuServer::onDataFrame );
	
	cout << "IISU ERROR ! Exit Code of : " << exitCode << endl ; 

	ofNotifyEvent( IisuEvents::Instance()->exitApplication , exitCode , this ) ; 
}

int IisuServer::getHandStatus ( int handID ) 
{
	if ( handID < handStatuses.size() ) 
		return handStatuses[ handID ] ; 
	else
		cout << "IisuServer::getHandStatus :: INVALID INDEX" << endl ;

	return -1 ; 
}
	
Vector2 IisuServer::getHandPalmPosition2D ( int handID ) 
{
	if ( handID < handPalmPositions2D.size() ) 
		return handPalmPositions2D[ handID ] ; 
	else
		cout << "IisuServer::getHandPalmPosition2D :: INVALID INDEX" << endl ;

	return Vector2() ; 
}

Vector2 IisuServer::getHandTipPosition2D( int handID ) 
{
	if ( handID < handTipPositions2D.size() ) 
		return handTipPositions2D[ handID ] ; 
	else
		cout << "IisuServer::getHandTipPosition2D :: INVALID INDEX" << endl ;

	return Vector2() ; 
}

bool IisuServer::getHandsOpen ( int handID ) 
{
	if ( handID < handsOpen.size() ) 
		return handsOpen[ handID ] ; 
	else
		cout << "IisuServer::getHandsOpen :: INVALID INDEX" << endl ;

	return false ;
}

float IisuServer::getHandsOpenAmount ( int handID ) 
{
	if ( handID < handsOpenAmount.size() ) 
		return handsOpenAmount[ handID ] ; 
	else
		cout << "IisuServer::getHandsOpenAmount :: INVALID INDEX" << endl ;

	return 0.0f ; 
}

SK::Array<int32_t> IisuServer::getHandsFingerTipsStatus ( int handID ) 
{
	if ( handID < handFingerTipsStatus.size() ) 
		return handFingerTipsStatus[handID] ; 
	else
		cout << "IisuServer::getHandsOpenAmount :: INVALID INDEX" << endl ;

	SK::Array<int32_t> args ; 
	return args;  
}

SK::Array<Vector2> IisuServer::getHandsFingerTips2D ( int handID ) 
{
	if ( handID < handFingerTips2D.size() ) 
		return handFingerTips2D[ handID ] ; 
	else
		cout << "IisuServer::getHandsOpenAmount :: INVALID INDEX" << endl ;

	SK::Array<Vector2> args ; 
	return args ; 
}