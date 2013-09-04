/****************************************************************************
Copyright (c) 2010-2012 cocos2d-x.org
Copyright (c) 2008-2010 Ricardo Quesada
Copyright (c) 2011      Zynga Inc.

http://www.cocos2d-x.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/

// standard includes
#include <string>

// cocos2d includes
#include "CCDirector.h"
#include "ccFPSImages.h"
#include "draw_nodes/CCDrawingPrimitives.h"
#include "CCConfiguration.h"
#include "cocoa/CCNS.h"
#include "layers_scenes_transitions_nodes/CCScene.h"
#include "cocoa/CCArray.h"
#include "CCScheduler.h"
#include "ccMacros.h"
#include "touch_dispatcher/CCTouchDispatcher.h"
#include "support/CCPointExtension.h"
#include "support/CCNotificationCenter.h"
#include "layers_scenes_transitions_nodes/CCTransition.h"
#include "textures/CCTextureCache.h"
#include "sprite_nodes/CCSpriteFrameCache.h"
#include "cocoa/CCAutoreleasePool.h"
#include "platform/platform.h"
#include "platform/CCFileUtils.h"
#include "CCApplication.h"
#include "label_nodes/CCLabelBMFont.h"
#include "label_nodes/CCLabelAtlas.h"
#include "actions/CCActionManager.h"
#include "CCConfiguration.h"
#include "keypad_dispatcher/CCKeypadDispatcher.h"
#include "CCAccelerometer.h"
#include "sprite_nodes/CCAnimationCache.h"
#include "touch_dispatcher/CCTouch.h"
#include "support/user_default/CCUserDefault.h"
#include "shaders/ccGLStateCache.h"
#include "shaders/CCShaderCache.h"
#include "kazmath/kazmath.h"
#include "kazmath/GL/matrix.h"
#include "support/CCProfiling.h"
#include "platform/CCImage.h"
#include "CCEGLView.h"
#include "CCConfiguration.h"



/**
 Position of the FPS
 
 Default: 0,0 (bottom-left corner)
 */
#ifndef CC_DIRECTOR_STATS_POSITION
#define CC_DIRECTOR_STATS_POSITION CCDirector::sharedDirector()->getVisibleOrigin()
#endif

using namespace std;
//���ո���,�������Ҳ��ȫ�ֱ���......
unsigned int g_uNumberOfDraws = 0;

NS_CC_BEGIN
// XXX it should be a Director ivar. Move it there once support for multiple directors is added

// singleton stuff
static CCDisplayLinkDirector *s_SharedDirector = NULL;

#define kDefaultFPS        60  // 60 frames per second
//��ð汾
extern const char* cocos2dVersion(void);

CCDirector* CCDirector::sharedDirector(void)
{
    if (!s_SharedDirector)
    {
        s_SharedDirector = new CCDisplayLinkDirector();
        s_SharedDirector->init();
    }

    return s_SharedDirector;
}

CCDirector::CCDirector(void)
{

}

bool CCDirector::init(void)
{
	setDefaultValues();

    // scenes
    m_pRunningScene = NULL;
    m_pNextScene = NULL;

    m_pNotificationNode = NULL;

    m_pobScenesStack = new CCArray();
    m_pobScenesStack->init();

    // projection delegate if "Custom" projection is used
    m_pProjectionDelegate = NULL;

    // FPS
    m_fAccumDt = 0.0f;
    m_fFrameRate = 0.0f;
    m_pFPSLabel = NULL;
    m_pSPFLabel = NULL;
    m_pDrawsLabel = NULL;
    m_uTotalFrames = m_uFrames = 0;
    m_pszFPS = new char[10];
    m_pLastUpdate = new struct cc_timeval();

    // paused ?
    m_bPaused = false;
   
    // purge ?
    m_bPurgeDirecotorInNextLoop = false;

    m_obWinSizeInPoints = CCSizeZero;    

    m_pobOpenGLView = NULL;

    m_fContentScaleFactor = 1.0f;

    // scheduler
    m_pScheduler = new CCScheduler();
    // action manager
    m_pActionManager = new CCActionManager();
    m_pScheduler->scheduleUpdateForTarget(m_pActionManager, kCCPrioritySystem, false);
    // touchDispatcher
    m_pTouchDispatcher = new CCTouchDispatcher();
    m_pTouchDispatcher->init();

    // KeypadDispatcher
    m_pKeypadDispatcher = new CCKeypadDispatcher();

    // Accelerometer
    m_pAccelerometer = new CCAccelerometer();

    // create autorelease pool
    CCPoolManager::sharedPoolManager()->push();

    return true;
}
    
CCDirector::~CCDirector(void)
{
    CCLOG("cocos2d: deallocing CCDirector %p", this);

    CC_SAFE_RELEASE(m_pFPSLabel);
    CC_SAFE_RELEASE(m_pSPFLabel);
    CC_SAFE_RELEASE(m_pDrawsLabel);
    
    CC_SAFE_RELEASE(m_pRunningScene);
    CC_SAFE_RELEASE(m_pNotificationNode);
    CC_SAFE_RELEASE(m_pobScenesStack);
    CC_SAFE_RELEASE(m_pScheduler);
    CC_SAFE_RELEASE(m_pActionManager);
    CC_SAFE_RELEASE(m_pTouchDispatcher);
    CC_SAFE_RELEASE(m_pKeypadDispatcher);
    CC_SAFE_DELETE(m_pAccelerometer);

    // pop the autorelease pool
    CCPoolManager::sharedPoolManager()->pop();
    CCPoolManager::purgePoolManager();

    // delete m_pLastUpdate
    CC_SAFE_DELETE(m_pLastUpdate);
    // delete fps string
    delete []m_pszFPS;

    s_SharedDirector = NULL;
}

void CCDirector::setDefaultValues(void)
{
	CCConfiguration *conf = CCConfiguration::sharedConfiguration();

	// default FPS
	double fps = conf->getNumber("cocos2d.x.fps", kDefaultFPS);
	m_dOldAnimationInterval = m_dAnimationInterval = 1.0 / fps;

	// Display FPS
	m_bDisplayStats = conf->getBool("cocos2d.x.display_fps", false);

	// GL projection
	const char *projection = conf->getCString("cocos2d.x.gl.projection", "3d");
	if( strcmp(projection, "3d") == 0 )
		m_eProjection = kCCDirectorProjection3D;
	else if (strcmp(projection, "2d") == 0)
		m_eProjection = kCCDirectorProjection2D;
	else if (strcmp(projection, "custom") == 0)
		m_eProjection = kCCDirectorProjectionCustom;
	else
		CCAssert(false, "Invalid projection value");

	// Default pixel format for PNG images with alpha
	const char *pixel_format = conf->getCString("cocos2d.x.texture.pixel_format_for_png", "rgba8888");
	if( strcmp(pixel_format, "rgba8888") == 0 )
		CCTexture2D::setDefaultAlphaPixelFormat(kCCTexture2DPixelFormat_RGBA8888);
	else if( strcmp(pixel_format, "rgba4444") == 0 )
		CCTexture2D::setDefaultAlphaPixelFormat(kCCTexture2DPixelFormat_RGBA4444);
	else if( strcmp(pixel_format, "rgba5551") == 0 )
		CCTexture2D::setDefaultAlphaPixelFormat(kCCTexture2DPixelFormat_RGB5A1);

	// PVR v2 has alpha premultiplied ?
	bool pvr_alpha_premultipled = conf->getBool("cocos2d.x.texture.pvrv2_has_alpha_premultiplied", false);
	CCTexture2D::PVRImagesHavePremultipliedAlpha(pvr_alpha_premultipled);
}

void CCDirector::setGLDefaultValues(void)
{
    // This method SHOULD be called only after openGLView_ was initialized
    CCAssert(m_pobOpenGLView, "opengl view should not be null");

    setAlphaBlending(true);
    // XXX: Fix me, should enable/disable depth test according the depth format as cocos2d-iphone did
    // [self setDepthTest: view_.depthFormat];
    setDepthTest(false);
    setProjection(m_eProjection);

    // set other opengl default values
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

// Draw the Scene
// ��泡��
// ÿ֡����.....
void CCDirector::drawScene(void)
{
    // calculate "global" dt
	// ��֪����ʲô��
    calculateDeltaTime();

    //tick before glClear: issue #533
    if (! m_bPaused)
    {
        m_pScheduler->update(m_fDeltaTime);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* to avoid flickr(��˸), nextScene MUST be here: after tick and before draw.
     XXX: Which bug is this one. It seems that it can't be reproduced with v0.9 */
    if (m_pNextScene)
    {
        setNextScene();
    }
	//����ѹջ?
	//Ϊʲô?
    kmGLPushMatrix();

    // draw the scene
	//
    if (m_pRunningScene)
    {
		//���������Node����Ҫ���
        m_pRunningScene->visit();
    }

    // draw the notifications node
    if (m_pNotificationNode)
    {
        m_pNotificationNode->visit();
    }
    
    if (m_bDisplayStats)
    {
		//��ʾ״̬==>���Ǽ������
        showStats();
    }

    kmGLPopMatrix();
	//֡����+1
    m_uTotalFrames++;

    // swap buffers
    if (m_pobOpenGLView)
    {
		//����������
		//����ɵ�ȫ��GDI===>��,����Ŀ�����ȥ��....
        m_pobOpenGLView->swapBuffers();
    }
    
    if (m_bDisplayStats)
    {
		//����MPF
		//��ʵ���Ǹ���m_fSecondsPerFrame�ѵ�ǰ֡���ʱ��-ǰһ֡ʱ��
        calculateMPF();
    }
}
//������ʱ��
//ÿ֡����
void CCDirector::calculateDeltaTime(void)
{
	//��ȡ��ǰʱ��
    struct cc_timeval now;

    if (CCTime::gettimeofdayCocos2d(&now, NULL) != 0)
    {
		//��ȡʧ�ܱ���
        CCLOG("error in gettimeofday");
        m_fDeltaTime = 0;
        return;
    }

    // new delta time. Re-fixed issue #1277
	// �����ܹ�����API������,��Ϊfalse
    if (m_bNextDeltaTimeZero)
    {
        m_fDeltaTime = 0;
        m_bNextDeltaTimeZero = false;
    }
    else
    {
		//���ʱ��
		//��ǰ֡��-ǰһ֡��
		//��ǰ΢��-ǰһ֡΢��
        m_fDeltaTime = (now.tv_sec - m_pLastUpdate->tv_sec) + (now.tv_usec - m_pLastUpdate->tv_usec) / 1000000.0f;
        m_fDeltaTime = MAX(0, m_fDeltaTime);
    }

#ifdef DEBUG
    // If we are debugging our code, prevent big delta time
    if(m_fDeltaTime > 0.2f)
    {
        m_fDeltaTime = 1 / 60.0f;
    }
#endif
	//����LastUpdate
    *m_pLastUpdate = now;
}

//��ȡ���ʱ��
float CCDirector::getDeltaTime()
{
	return m_fDeltaTime;
}

//����OPENGL VIew
//APPDelegate����
void CCDirector::setOpenGLView(CCEGLView *pobOpenGLView)
{
    CCAssert(pobOpenGLView, "opengl view should not be null");

	//���GLView����ͬ
    if (m_pobOpenGLView != pobOpenGLView)
    {
		// Configuration. Gather GPU info
		CCConfiguration *conf = CCConfiguration::sharedConfiguration();
		conf->gatherGPUInfo();
		conf->dumpInfo();

        // EAGLView is not a CCObject
		//ɾ����ǰ��GLVIew
        delete m_pobOpenGLView; // [openGLView_ release]
		//�滻Ϊ��ǰ���õ�GLVIew
        m_pobOpenGLView = pobOpenGLView;

        // set size
        m_obWinSizeInPoints = m_pobOpenGLView->getDesignResolutionSize();
        
		// ���ǽ�ͼ������ ���CCImage 
		// ��CCImage ���� ����
		// ����LabelAtlas 
		// Ȼ��...�㶮��
        createStatsLabel();
        
        if (m_pobOpenGLView)
        {
            setGLDefaultValues();
        }  
        
        CHECK_GL_ERROR_DEBUG();
		//����Touch����
        m_pobOpenGLView->setTouchDelegate(m_pTouchDispatcher);
		//�����Ƿ��ɷ�touch�¼�===>�����Ӧ�ö���
        m_pTouchDispatcher->setDispatchEvents(true);
    }
}

void CCDirector::setViewport()
{
    if (m_pobOpenGLView)
    {
        m_pobOpenGLView->setViewPortInPoints(0, 0, m_obWinSizeInPoints.width, m_obWinSizeInPoints.height);
    }
}

void CCDirector::setNextDeltaTimeZero(bool bNextDeltaTimeZero)
{
    m_bNextDeltaTimeZero = bNextDeltaTimeZero;
}

void CCDirector::setProjection(ccDirectorProjection kProjection)
{
    CCSize size = m_obWinSizeInPoints;

    setViewport();

    switch (kProjection)
    {
    case kCCDirectorProjection2D:
        {
            kmGLMatrixMode(KM_GL_PROJECTION);
            kmGLLoadIdentity();
            kmMat4 orthoMatrix;
            kmMat4OrthographicProjection(&orthoMatrix, 0, size.width, 0, size.height, -1024, 1024 );
            kmGLMultMatrix(&orthoMatrix);
            kmGLMatrixMode(KM_GL_MODELVIEW);
            kmGLLoadIdentity();
        }
        break;

    case kCCDirectorProjection3D:
        {
            float zeye = this->getZEye();

            kmMat4 matrixPerspective, matrixLookup;

            kmGLMatrixMode(KM_GL_PROJECTION);
            kmGLLoadIdentity();

            // issue #1334
            kmMat4PerspectiveProjection( &matrixPerspective, 60, (GLfloat)size.width/size.height, 0.1f, zeye*2);
            // kmMat4PerspectiveProjection( &matrixPerspective, 60, (GLfloat)size.width/size.height, 0.1f, 1500);

            kmGLMultMatrix(&matrixPerspective);

            kmGLMatrixMode(KM_GL_MODELVIEW);
            kmGLLoadIdentity();
            kmVec3 eye, center, up;
            kmVec3Fill( &eye, size.width/2, size.height/2, zeye );
            kmVec3Fill( &center, size.width/2, size.height/2, 0.0f );
            kmVec3Fill( &up, 0.0f, 1.0f, 0.0f);
            kmMat4LookAt(&matrixLookup, &eye, &center, &up);
            kmGLMultMatrix(&matrixLookup);
        }
        break;
            
    case kCCDirectorProjectionCustom:
        if (m_pProjectionDelegate)
        {
            m_pProjectionDelegate->updateProjection();
        }
        break;
            
    default:
        CCLOG("cocos2d: Director: unrecognized projection");
        break;
    }

    m_eProjection = kProjection;
    ccSetProjectionMatrixDirty();
}

void CCDirector::purgeCachedData(void)
{
    CCLabelBMFont::purgeCachedData();
    if (s_SharedDirector->getOpenGLView())
    {
        CCTextureCache::sharedTextureCache()->removeUnusedTextures();
    }
    CCFileUtils::sharedFileUtils()->purgeCachedEntries();
}

float CCDirector::getZEye(void)
{
    return (m_obWinSizeInPoints.height / 1.1566f);
}

void CCDirector::setAlphaBlending(bool bOn)
{
    if (bOn)
    {
        ccGLBlendFunc(CC_BLEND_SRC, CC_BLEND_DST);
    }
    else
    {
        ccGLBlendFunc(GL_ONE, GL_ZERO);
    }

    CHECK_GL_ERROR_DEBUG();
}

void CCDirector::setDepthTest(bool bOn)
{
    if (bOn)
    {
        glClearDepth(1.0f);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
//        glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    }
    else
    {
        glDisable(GL_DEPTH_TEST);
    }
    CHECK_GL_ERROR_DEBUG();
}

static void
GLToClipTransform(kmMat4 *transformOut)
{
	kmMat4 projection;
	kmGLGetMatrix(KM_GL_PROJECTION, &projection);
	
	kmMat4 modelview;
	kmGLGetMatrix(KM_GL_MODELVIEW, &modelview);
	
	kmMat4Multiply(transformOut, &projection, &modelview);
}

CCPoint CCDirector::convertToGL(const CCPoint& uiPoint)
{
    kmMat4 transform;
	GLToClipTransform(&transform);
	
	kmMat4 transformInv;
	kmMat4Inverse(&transformInv, &transform);
	
	// Calculate z=0 using -> transform*[0, 0, 0, 1]/w
	kmScalar zClip = transform.mat[14]/transform.mat[15];
	
    CCSize glSize = m_pobOpenGLView->getDesignResolutionSize();
	kmVec3 clipCoord = {2.0f*uiPoint.x/glSize.width - 1.0f, 1.0f - 2.0f*uiPoint.y/glSize.height, zClip};
	
	kmVec3 glCoord;
	kmVec3TransformCoord(&glCoord, &clipCoord, &transformInv);
	
	return ccp(glCoord.x, glCoord.y);
}

CCPoint CCDirector::convertToUI(const CCPoint& glPoint)
{
    kmMat4 transform;
	GLToClipTransform(&transform);
    
	kmVec3 clipCoord;
	// Need to calculate the zero depth from the transform.
	kmVec3 glCoord = {glPoint.x, glPoint.y, 0.0};
	kmVec3TransformCoord(&clipCoord, &glCoord, &transform);
	
	CCSize glSize = m_pobOpenGLView->getDesignResolutionSize();
	return ccp(glSize.width*(clipCoord.x*0.5 + 0.5), glSize.height*(-clipCoord.y*0.5 + 0.5));
}

CCSize CCDirector::getWinSize(void)
{
    return m_obWinSizeInPoints;
}

CCSize CCDirector::getWinSizeInPixels()
{
    return CCSizeMake(m_obWinSizeInPoints.width * m_fContentScaleFactor, m_obWinSizeInPoints.height * m_fContentScaleFactor);
}

CCSize CCDirector::getVisibleSize()
{
    if (m_pobOpenGLView)
    {
        return m_pobOpenGLView->getVisibleSize();
    }
    else 
    {
        return CCSizeZero;
    }
}

CCPoint CCDirector::getVisibleOrigin()
{
    if (m_pobOpenGLView)
    {
        return m_pobOpenGLView->getVisibleOrigin();
    }
    else 
    {
        return CCPointZero;
    }
}

// scene management

void CCDirector::runWithScene(CCScene *pScene)
{
	//�����ж�
    CCAssert(pScene != NULL, "This command can only be used to start the CCDirector. There is already a scene present.");
    CCAssert(m_pRunningScene == NULL, "m_pRunningScene should be null");
	//ѹ�볡��
    pushScene(pScene);
	//����startAnimation
    startAnimation();//LinkedDisplay->setAnimationInterval
}

void CCDirector::replaceScene(CCScene *pScene)
{
	//�������
    CCAssert(m_pRunningScene, "Use runWithScene: instead to start the director");
    CCAssert(pScene != NULL, "the scene should not be null");
	//��ȡ��ջ��С
    unsigned int index = m_pobScenesStack->count();
	//���Scene
    m_bSendCleanupToScene = true;
	//�滻===>CCArray����ˬ��
    m_pobScenesStack->replaceObjectAtIndex(index - 1, pScene);
	//��һ֡running scene Ϊ�滻�� scene
    m_pNextScene = pScene;
}

void CCDirector::pushScene(CCScene *pScene)
{
    CCAssert(pScene, "the scene should not null");
	//�Ƿ�������Ϊfalse
    m_bSendCleanupToScene = false;
	//ѹջ
    m_pobScenesStack->addObject(pScene);
	//������һ������
    m_pNextScene = pScene;
}

void CCDirector::popScene(void)
{
	//�������
    CCAssert(m_pRunningScene != NULL, "running scene should not null");
	//��������
    m_pobScenesStack->removeLastObject();
	//�������ȡ��ջ��С
    unsigned int c = m_pobScenesStack->count();

    if (c == 0)
    {
		//������һ�������Ѿ�������===>�����end����
        end();
    }
    else
    {
		//�����ǳ���ȥ������
        m_bSendCleanupToScene = true;
		//��һ֡��running scene Ϊ ջ������
        m_pNextScene = (CCScene*)m_pobScenesStack->objectAtIndex(c - 1);
    }
}

void CCDirector::popToRootScene(void)
{
	//������������
    popToSceneStackLevel(1);
}

void CCDirector::popToSceneStackLevel(int level)
{
	//�������
    CCAssert(m_pRunningScene != NULL, "A running Scene is needed");
	//��ȡ������ջ��С
    int c = (int)m_pobScenesStack->count();

    // level 0? -> end
    if (level == 0)
    {
		//�����Ϊ0 ===> ֱ�ӵ���end����
        end();
        return;
    }

    // current level or lower -> nothing
	// ���level���ڳ�����ջ��С,ֱ�ӷ���
    if (level >= c)
        return;

	// pop stack until reaching desired(������) level
	// ��������
	while (c > level)
    {
		CCScene *current = (CCScene*)m_pobScenesStack->lastObject();
		//��鳡���Ƿ���running״̬
		if (current->isRunning())
        {
			//ʱ��ص�
            current->onExitTransitionDidStart();
            current->onExit();
		}
		//�������
        current->cleanup();
		//���
        m_pobScenesStack->removeLastObject();
		c--;
	}
	//��һ֡��Ϊrunning scene
	m_pNextScene = (CCScene*)m_pobScenesStack->lastObject();
	m_bSendCleanupToScene = false;
}

void CCDirector::end()
{
	//��һ��ѭ���������
    m_bPurgeDirecotorInNextLoop = true;
}

//�������
void CCDirector::purgeDirector()
{
    // cleanup scheduler
	// ֹͣ����schedule
    getScheduler()->unscheduleAll();
    
    // don't release the event handlers
    // They are needed in case the director is run again
	// Touch���ٷַ���Ϣ
    m_pTouchDispatcher->removeAllDelegates();

	//�����ǰrunning scene ����
    if (m_pRunningScene)
    {
		//�˳���release
        m_pRunningScene->onExitTransitionDidStart();
        m_pRunningScene->onExit();
        m_pRunningScene->cleanup();
        m_pRunningScene->release();
    }
    //��ǰsceneΪ��
    m_pRunningScene = NULL;
	//��һ֡running scene����Ϊ��
    m_pNextScene = NULL;

    // remove all objects, but don't release it.
    // runWithScene might be executed after 'end'.
	// ����ջ�������ж�ջ
    m_pobScenesStack->removeAllObjects();

	//ֹͣAnimation 
	//������Invailid����Ϊtrue
    stopAnimation();

	//���label
	//FPS ==> ֡��
	//Draw == > ���ƶ������
	//SPF == > second pre frame
    CC_SAFE_RELEASE_NULL(m_pFPSLabel);
    CC_SAFE_RELEASE_NULL(m_pSPFLabel);
    CC_SAFE_RELEASE_NULL(m_pDrawsLabel);

    // purge bitmap cache
	//bmp cache ���
    CCLabelBMFont::purgeCachedData();

    // purge all managed caches
	// 
	/** Frees allocated resources by the drawing primitives */
    ccDrawFree();
	//���Animation ����
    CCAnimationCache::purgeSharedAnimationCache();
	//����֡��������
    CCSpriteFrameCache::purgeSharedSpriteFrameCache();
	//����������
    CCTextureCache::purgeSharedTextureCache();
	//Shader��������
    CCShaderCache::purgeSharedShaderCache();
	//�ļ���������
    CCFileUtils::purgeFileUtils();
	//�������� ==> ���溬һЩOPENGL����
    CCConfiguration::purgeConfiguration();

    // cocos2d-x specific data structures
	//cocos2d-x ��������ݽṹ����

	//�û�������������
    CCUserDefault::purgeSharedUserDefault();
	//??????δ֪
    CCNotificationCenter::purgeNotificationCenter();
	//
    ccGLInvalidateStateCache();
    //���GL������Ϣ
    CHECK_GL_ERROR_DEBUG();
    
    // OpenGL view
	//GLView�ر�
    m_pobOpenGLView->end();
	//GLView����
    m_pobOpenGLView = NULL;

    // delete CCDirector
	//CCDirector �̳���CCObject
	//�����ǵ����������ڶ������
	//�ͷ�=====>release
	//==>�����delete this
    release();
}

void CCDirector::setNextScene(void)
{
	//
    bool runningIsTransition = dynamic_cast<CCTransitionScene*>(m_pRunningScene) != NULL;
    bool newIsTransition = dynamic_cast<CCTransitionScene*>(m_pNextScene) != NULL;

    // If it is not a transition, call onExit/cleanup
     if (! newIsTransition)
     {
         if (m_pRunningScene)
         {
             m_pRunningScene->onExitTransitionDidStart();
             m_pRunningScene->onExit();
         }
 
         // issue #709. the root node (scene) should receive the cleanup message too
         // otherwise it might be leaked.
         if (m_bSendCleanupToScene && m_pRunningScene)
         {
             m_pRunningScene->cleanup();
         }
     }

    if (m_pRunningScene)
    {
		//��ǰRunning Scene�ͷ�
        m_pRunningScene->release();
    }
    m_pRunningScene = m_pNextScene;
    m_pNextScene->retain();
    m_pNextScene = NULL;

    if ((! runningIsTransition) && m_pRunningScene)
    {
        m_pRunningScene->onEnter();
        m_pRunningScene->onEnterTransitionDidFinish();
    }
}

void CCDirector::pause(void)
{
    if (m_bPaused)
    {
        return;
    }
	//������ǰ��ʱ����
    m_dOldAnimationInterval = m_dAnimationInterval;

    // when paused, don't consume(����) CPU
	// == > �����ǵ���application �� setAnimaitonInterval��ʵ�ֵ�
    setAnimationInterval(1 / 4.0);
    m_bPaused = true;
}

//�ָ�
void CCDirector::resume(void)
{
    if (! m_bPaused)
    {
        return;
    }
	//���ö������
    setAnimationInterval(m_dOldAnimationInterval);
	//��ȡ
    if (CCTime::gettimeofdayCocos2d(m_pLastUpdate, NULL) != 0)
    {
        CCLOG("cocos2d: Director: Error in gettimeofday");
    }
	//��ͣ�������Ϊfalse
    m_bPaused = false;
	//
    m_fDeltaTime = 0;
}

// display the FPS using a LabelAtlas
// updates the FPS every frame
// ÿ֡������
void CCDirector::showStats(void)
{
    m_uFrames++;
	//�ۼƼ��ʱ��+����ÿ֡���ʱ��
    m_fAccumDt += m_fDeltaTime;
    
    if (m_bDisplayStats)
    {
        if (m_pFPSLabel && m_pSPFLabel && m_pDrawsLabel)
        {
			//����ۼƼ��ʱ��>��Ϣ���¼��ʱ��
            if (m_fAccumDt > CC_DIRECTOR_STATS_INTERVAL)
            {
                sprintf(m_pszFPS, "%.3f", m_fSecondsPerFrame);
                m_pSPFLabel->setString(m_pszFPS);
               
				//֡�����Լ��ʱ�����֡����....
                m_fFrameRate = m_uFrames / m_fAccumDt;
                m_uFrames = 0;//����
                m_fAccumDt = 0;//���ʱ����0
                
				//����֡��
                sprintf(m_pszFPS, "%.1f", m_fFrameRate);
                m_pFPSLabel->setString(m_pszFPS);
                
				//��ͼ�Ķ��������
                sprintf(m_pszFPS, "%4lu", (unsigned long)g_uNumberOfDraws);
                m_pDrawsLabel->setString(m_pszFPS);
            }
            //visit���ð�?ʲô����???/
            m_pDrawsLabel->visit();
            m_pFPSLabel->visit();
            m_pSPFLabel->visit();
        }
    }    
    //����Ŷ==��0Ŷ,�������....
    g_uNumberOfDraws = 0;
}

void CCDirector::calculateMPF()
{
	//����ʱ�����
    struct cc_timeval now;
	//��ȡ��ǰʱ��
    CCTime::gettimeofdayCocos2d(&now, NULL);
    //��ǰ��-ǰһ֡��
	//��ǰ����-ǰһ֡����
    m_fSecondsPerFrame = (now.tv_sec - m_pLastUpdate->tv_sec) + (now.tv_usec - m_pLastUpdate->tv_usec) / 1000000.0f;
}

// returns the FPS image data pointer and len
// ��ʵ��������ܼ�,�ļ�����ָ��ʹ�С
void CCDirector::getFPSImageData(unsigned char** datapointer, unsigned int* length)
{
    // XXX fixed me if it should be used 
    *datapointer = cc_fps_images_png;
	*length = cc_fps_images_len();
}

//����״̬Label
void CCDirector::createStatsLabel()
{
    CCTexture2D *texture = NULL;
	//�������Cache
    CCTextureCache *textureCache = CCTextureCache::sharedTextureCache();

    if( m_pFPSLabel && m_pSPFLabel )
    {
		//�ͷ��ڴ�ռ�
		//����NEW��==>������Ҫ�ֶ��ͷ�
        CC_SAFE_RELEASE_NULL(m_pFPSLabel);
        CC_SAFE_RELEASE_NULL(m_pSPFLabel);
        CC_SAFE_RELEASE_NULL(m_pDrawsLabel);
		//�������������
        textureCache->removeTextureForKey("cc_fps_images");
		//
        CCFileUtils::sharedFileUtils()->purgeCachedEntries();
    }

	//�������û��===>��ô���ܲ�����???
	//����2D�������صĸ�ʽ
    CCTexture2DPixelFormat currentFormat = CCTexture2D::defaultAlphaPixelFormat();
	//����2D�������ظ�ʽ
    CCTexture2D::setDefaultAlphaPixelFormat(kCCTexture2DPixelFormat_RGBA4444);

	//����Ϊͨ���ֿⴴ������
    unsigned char *data = NULL;//���ǻ��ͼ��ָ�����
    unsigned int data_len = 0;
    getFPSImageData(&data, &data_len);
	//���ֿ���ͼ��==>CCImage��ƽ̨���
    CCImage* image = new CCImage();
    bool isOK = image->initWithImageData(data, data_len);
    if (!isOK) {
        CCLOGERROR("%s", "Fails: init fps_images");
        return;
    }

	//�������ͼ��
    texture = textureCache->addUIImage(image, "cc_fps_images");
	//�ͷ�ͼ������
    CC_SAFE_RELEASE(image);
    /*
     We want to use an image which is stored in the file named ccFPSImage.c 
     for any design resolutions and all resource resolutions. 
     
     To achieve this,
     
     Firstly, we need to ignore 'contentScaleFactor' in 'CCAtlasNode' and 'CCLabelAtlas'.
     So I added a new method called 'setIgnoreContentScaleFactor' for 'CCAtlasNode',
     this is not exposed(��¶) to game developers, it's only used for displaying FPS now.
     
     Secondly, the size of this image is 480*320, to display the FPS label with correct size, 
     a factor of design resolution ratio(����) of 480x320 is also needed.
     */
	//ΪʲôҪHeight/320
	//���ձ����Ŵ�????��,�ⶼ��������ô������Ϸ?
    float factor = CCEGLView::sharedOpenGLView()->getDesignResolutionSize().height / 320.0f;

    m_pFPSLabel = new CCLabelAtlas();
    m_pFPSLabel->setIgnoreContentScaleFactor(true);
    m_pFPSLabel->initWithString("00.0", texture, 12, 32 , '.');
    m_pFPSLabel->setScale(factor);

    m_pSPFLabel = new CCLabelAtlas();
    m_pSPFLabel->setIgnoreContentScaleFactor(true);
    m_pSPFLabel->initWithString("0.000", texture, 12, 32, '.');
    m_pSPFLabel->setScale(factor);

	//NEW����
    m_pDrawsLabel = new CCLabelAtlas();
	//��������
    m_pDrawsLabel->setIgnoreContentScaleFactor(true);
	//��ʼ������
    m_pDrawsLabel->initWithString("000", texture, 12, 32, '.');
	//��������
    m_pDrawsLabel->setScale(factor);

    CCTexture2D::setDefaultAlphaPixelFormat(currentFormat);
	//����Draws	==> 00.0
	//����SPF	==> 0.000
	//����FPS	==> 000
    m_pDrawsLabel->setPosition(ccpAdd(ccp(0, 34*factor), CC_DIRECTOR_STATS_POSITION));
    m_pSPFLabel->setPosition(ccpAdd(ccp(0, 17*factor), CC_DIRECTOR_STATS_POSITION));
    m_pFPSLabel->setPosition(CC_DIRECTOR_STATS_POSITION);
}

//��ȡ��������
float CCDirector::getContentScaleFactor(void)
{
    return m_fContentScaleFactor;
}

//����ContentScaleFactor
void CCDirector::setContentScaleFactor(float scaleFactor)
{
    if (scaleFactor != m_fContentScaleFactor)
    {
        m_fContentScaleFactor = scaleFactor;
		//ÿ��������ScaleFactor,�������µ���һ��
        createStatsLabel();
    }
}

CCNode* CCDirector::getNotificationNode() 
{ 
    return m_pNotificationNode; 
}

void CCDirector::setNotificationNode(CCNode *node)
{
    CC_SAFE_RELEASE(m_pNotificationNode);
    m_pNotificationNode = node;
    CC_SAFE_RETAIN(m_pNotificationNode);
}

//��ȡͶӰ������
CCDirectorDelegate* CCDirector::getDelegate() const
{
    return m_pProjectionDelegate;
}
//���ô�����
void CCDirector::setDelegate(CCDirectorDelegate* pDelegate)
{
    m_pProjectionDelegate = pDelegate;
}
//����Scheduler
void CCDirector::setScheduler(CCScheduler* pScheduler)
{
    if (m_pScheduler != pScheduler)
    {
        CC_SAFE_RETAIN(pScheduler);
        CC_SAFE_RELEASE(m_pScheduler);
        m_pScheduler = pScheduler;
    }
}
//�õ�Scheduler
CCScheduler* CCDirector::getScheduler()
{
    return m_pScheduler;
}
//����ActionManager
void CCDirector::setActionManager(CCActionManager* pActionManager)
{
    if (m_pActionManager != pActionManager)
    {
        CC_SAFE_RETAIN(pActionManager);
        CC_SAFE_RELEASE(m_pActionManager);
        m_pActionManager = pActionManager;
    }    
}
//�õ�ActionManger
CCActionManager* CCDirector::getActionManager()
{
    return m_pActionManager;
}
//����TouchDispatcher
void CCDirector::setTouchDispatcher(CCTouchDispatcher* pTouchDispatcher)
{
    if (m_pTouchDispatcher != pTouchDispatcher)
    {
        CC_SAFE_RETAIN(pTouchDispatcher);
        CC_SAFE_RELEASE(m_pTouchDispatcher);
        m_pTouchDispatcher = pTouchDispatcher;
    }    
}
//��ȡTouchDispatcher
CCTouchDispatcher* CCDirector::getTouchDispatcher()
{
    return m_pTouchDispatcher;
}
//����KeypadDispathcer
void CCDirector::setKeypadDispatcher(CCKeypadDispatcher* pKeypadDispatcher)
{
    CC_SAFE_RETAIN(pKeypadDispatcher);
    CC_SAFE_RELEASE(m_pKeypadDispatcher);
    m_pKeypadDispatcher = pKeypadDispatcher;
}
//�õ�KeypadDispathcer
CCKeypadDispatcher* CCDirector::getKeypadDispatcher()
{
    return m_pKeypadDispatcher;
}
//������������
void CCDirector::setAccelerometer(CCAccelerometer* pAccelerometer)
{
    if (m_pAccelerometer != pAccelerometer)
    {
        CC_SAFE_DELETE(m_pAccelerometer);
        m_pAccelerometer = pAccelerometer;
    }
}
//�õ���������
CCAccelerometer* CCDirector::getAccelerometer()
{
    return m_pAccelerometer;
}

/***************************************************
* implementation of DisplayLinkDirector
**************************************************/

// should we implement 4 types of director ??
// I think DisplayLinkDirector is enough
// so we now only support DisplayLinkDirector
// ���ݱ�����4��,�����˼�˵��һ���Ѿ��㹻��
void CCDisplayLinkDirector::startAnimation(void)
{
    if (CCTime::gettimeofdayCocos2d(m_pLastUpdate, NULL) != 0)
    {
        CCLOG("cocos2d: DisplayLinkDirector: Error on gettimeofday");
    }

    m_bInvalid = false;
#ifndef EMSCRIPTEN
	//�����Application��ʵ�ֵ�
    CCApplication::sharedApplication()->setAnimationInterval(m_dAnimationInterval);
#endif // EMSCRIPTEN
}
//��ѭ��
void CCDisplayLinkDirector::mainLoop(void)
{
	//���ǵ�end()��?
	//������
    if (m_bPurgeDirecotorInNextLoop)
    {
        m_bPurgeDirecotorInNextLoop = false;
		//�������chace
        purgeDirector();
    }
    else if (! m_bInvalid)
     {
		 //���Ƴ���
         drawScene();
     
         // release the objects
         CCPoolManager::sharedPoolManager()->pop();        
     }
}

void CCDisplayLinkDirector::stopAnimation(void)
{
    m_bInvalid = true;
}

void CCDisplayLinkDirector::setAnimationInterval(double dValue)
{
    m_dAnimationInterval = dValue;
    if (! m_bInvalid)
    {
        stopAnimation();
        startAnimation();
    }    
}

NS_CC_END

