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
//我勒个草,面向对象也用全局变量......
unsigned int g_uNumberOfDraws = 0;

NS_CC_BEGIN
// XXX it should be a Director ivar. Move it there once support for multiple directors is added

// singleton stuff
static CCDisplayLinkDirector *s_SharedDirector = NULL;

#define kDefaultFPS        60  // 60 frames per second
//获得版本
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
// 描绘场景
// 每帧更新.....
void CCDirector::drawScene(void)
{
    // calculate "global" dt
	// 不知道干什么用
    calculateDeltaTime();

    //tick before glClear: issue #533
    if (! m_bPaused)
    {
        m_pScheduler->update(m_fDeltaTime);
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* to avoid flickr(闪烁), nextScene MUST be here: after tick and before draw.
     XXX: Which bug is this one. It seems that it can't be reproduced with v0.9 */
    if (m_pNextScene)
    {
        setNextScene();
    }
	//矩阵压栈?
	//为什么?
    kmGLPushMatrix();

    // draw the scene
	//
    if (m_pRunningScene)
    {
		//场景下面的Node都需要描绘
        m_pRunningScene->visit();
    }

    // draw the notifications node
    if (m_pNotificationNode)
    {
        m_pNotificationNode->visit();
    }
    
    if (m_bDisplayStats)
    {
		//显示状态==>就是计算各种
        showStats();
    }

    kmGLPopMatrix();
	//帧计数+1
    m_uTotalFrames++;

    // swap buffers
    if (m_pobOpenGLView)
    {
		//交换缓冲区
		//这个干到全局GDI===>亲,我真的看不下去了....
        m_pobOpenGLView->swapBuffers();
    }
    
    if (m_bDisplayStats)
    {
		//计算MPF
		//其实就是更新m_fSecondsPerFrame把当前帧获得时间-前一帧时间
        calculateMPF();
    }
}
//计算间隔时间
//每帧更新
void CCDirector::calculateDeltaTime(void)
{
	//获取当前时间
    struct cc_timeval now;

    if (CCTime::gettimeofdayCocos2d(&now, NULL) != 0)
    {
		//获取失败报错
        CCLOG("error in gettimeofday");
        m_fDeltaTime = 0;
        return;
    }

    // new delta time. Re-fixed issue #1277
	// 除了能够调用API设置外,都为false
    if (m_bNextDeltaTimeZero)
    {
        m_fDeltaTime = 0;
        m_bNextDeltaTimeZero = false;
    }
    else
    {
		//间隔时间
		//当前帧秒-前一帧秒
		//当前微秒-前一帧微秒
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
	//更新LastUpdate
    *m_pLastUpdate = now;
}

//获取间隔时间
float CCDirector::getDeltaTime()
{
	return m_fDeltaTime;
}

//设置OPENGL VIew
//APPDelegate调用
void CCDirector::setOpenGLView(CCEGLView *pobOpenGLView)
{
    CCAssert(pobOpenGLView, "opengl view should not be null");

	//如果GLView不想同
    if (m_pobOpenGLView != pobOpenGLView)
    {
		// Configuration. Gather GPU info
		CCConfiguration *conf = CCConfiguration::sharedConfiguration();
		conf->gatherGPUInfo();
		conf->dumpInfo();

        // EAGLView is not a CCObject
		//删除先前的GLVIew
        delete m_pobOpenGLView; // [openGLView_ release]
		//替换为当前设置的GLVIew
        m_pobOpenGLView = pobOpenGLView;

        // set size
        m_obWinSizeInPoints = m_pobOpenGLView->getDesignResolutionSize();
        
		// 就是将图像数据 变成CCImage 
		// 将CCImage 加入 纹理
		// 创建LabelAtlas 
		// 然后...你懂的
        createStatsLabel();
        
        if (m_pobOpenGLView)
        {
            setGLDefaultValues();
        }  
        
        CHECK_GL_ERROR_DEBUG();
		//设置Touch代理
        m_pobOpenGLView->setTouchDelegate(m_pTouchDispatcher);
		//设置是否派发touch事件===>这个你应该懂的
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
	//参数判读
    CCAssert(pScene != NULL, "This command can only be used to start the CCDirector. There is already a scene present.");
    CCAssert(m_pRunningScene == NULL, "m_pRunningScene should be null");
	//压入场景
    pushScene(pScene);
	//调用startAnimation
    startAnimation();//LinkedDisplay->setAnimationInterval
}

void CCDirector::replaceScene(CCScene *pScene)
{
	//参数检测
    CCAssert(m_pRunningScene, "Use runWithScene: instead to start the director");
    CCAssert(pScene != NULL, "the scene should not be null");
	//获取堆栈大小
    unsigned int index = m_pobScenesStack->count();
	//清除Scene
    m_bSendCleanupToScene = true;
	//替换===>CCArray就是爽啊
    m_pobScenesStack->replaceObjectAtIndex(index - 1, pScene);
	//下一帧running scene 为替换的 scene
    m_pNextScene = pScene;
}

void CCDirector::pushScene(CCScene *pScene)
{
    CCAssert(pScene, "the scene should not null");
	//是否清除标记为false
    m_bSendCleanupToScene = false;
	//压栈
    m_pobScenesStack->addObject(pScene);
	//更新下一个场景
    m_pNextScene = pScene;
}

void CCDirector::popScene(void)
{
	//参数检查
    CCAssert(m_pRunningScene != NULL, "running scene should not null");
	//弹出场景
    m_pobScenesStack->removeLastObject();
	//弹出后获取堆栈大小
    unsigned int c = m_pobScenesStack->count();

    if (c == 0)
    {
		//如果最后一个场景已经被弹出===>则调用end函数
        end();
    }
    else
    {
		//否则标记场景去需清理
        m_bSendCleanupToScene = true;
		//下一帧的running scene 为 栈顶场景
        m_pNextScene = (CCScene*)m_pobScenesStack->objectAtIndex(c - 1);
    }
}

void CCDirector::popToRootScene(void)
{
	//弹出到根场景
    popToSceneStackLevel(1);
}

void CCDirector::popToSceneStackLevel(int level)
{
	//参数检查
    CCAssert(m_pRunningScene != NULL, "A running Scene is needed");
	//获取场景堆栈大小
    int c = (int)m_pobScenesStack->count();

    // level 0? -> end
    if (level == 0)
    {
		//如果层为0 ===> 直接调用end函数
        end();
        return;
    }

    // current level or lower -> nothing
	// 如果level大于场景堆栈大小,直接返回
    if (level >= c)
        return;

	// pop stack until reaching desired(渴望的) level
	// 弹出场景
	while (c > level)
    {
		CCScene *current = (CCScene*)m_pobScenesStack->lastObject();
		//检查场景是否处于running状态
		if (current->isRunning())
        {
			//时间回调
            current->onExitTransitionDidStart();
            current->onExit();
		}
		//场景清除
        current->cleanup();
		//清除
        m_pobScenesStack->removeLastObject();
		c--;
	}
	//下一帧将为running scene
	m_pNextScene = (CCScene*)m_pobScenesStack->lastObject();
	m_bSendCleanupToScene = false;
}

void CCDirector::end()
{
	//下一次循环清除导演
    m_bPurgeDirecotorInNextLoop = true;
}

//清除导演
void CCDirector::purgeDirector()
{
    // cleanup scheduler
	// 停止所有schedule
    getScheduler()->unscheduleAll();
    
    // don't release the event handlers
    // They are needed in case the director is run again
	// Touch不再分发消息
    m_pTouchDispatcher->removeAllDelegates();

	//如果当前running scene 存在
    if (m_pRunningScene)
    {
		//退出且release
        m_pRunningScene->onExitTransitionDidStart();
        m_pRunningScene->onExit();
        m_pRunningScene->cleanup();
        m_pRunningScene->release();
    }
    //当前scene为空
    m_pRunningScene = NULL;
	//下一帧running scene场景为空
    m_pNextScene = NULL;

    // remove all objects, but don't release it.
    // runWithScene might be executed after 'end'.
	// 场景栈弹出所有堆栈
    m_pobScenesStack->removeAllObjects();

	//停止Animation 
	//仅仅设Invailid变量为true
    stopAnimation();

	//清除label
	//FPS ==> 帧率
	//Draw == > 绘制对象个数
	//SPF == > second pre frame
    CC_SAFE_RELEASE_NULL(m_pFPSLabel);
    CC_SAFE_RELEASE_NULL(m_pSPFLabel);
    CC_SAFE_RELEASE_NULL(m_pDrawsLabel);

    // purge bitmap cache
	//bmp cache 清除
    CCLabelBMFont::purgeCachedData();

    // purge all managed caches
	// 
	/** Frees allocated resources by the drawing primitives */
    ccDrawFree();
	//清除Animation 缓存
    CCAnimationCache::purgeSharedAnimationCache();
	//精灵帧缓存清理
    CCSpriteFrameCache::purgeSharedSpriteFrameCache();
	//纹理缓存清理
    CCTextureCache::purgeSharedTextureCache();
	//Shader缓存清理
    CCShaderCache::purgeSharedShaderCache();
	//文件缓存清理
    CCFileUtils::purgeFileUtils();
	//配置清理 ==> 里面含一些OPENGL配置
    CCConfiguration::purgeConfiguration();

    // cocos2d-x specific data structures
	//cocos2d-x 特殊的数据结构清理

	//用户配置配置清理
    CCUserDefault::purgeSharedUserDefault();
	//??????未知
    CCNotificationCenter::purgeNotificationCenter();
	//
    ccGLInvalidateStateCache();
    //输出GL调试信息
    CHECK_GL_ERROR_DEBUG();
    
    // OpenGL view
	//GLView关闭
    m_pobOpenGLView->end();
	//GLView赋空
    m_pobOpenGLView = NULL;

    // delete CCDirector
	//CCDirector 继承自CCObject
	//由于是单例并不存在多个引用
	//释放=====>release
	//==>相关于delete this
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
		//当前Running Scene释放
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
	//保存先前的时间间隔
    m_dOldAnimationInterval = m_dAnimationInterval;

    // when paused, don't consume(消耗) CPU
	// == > 最终是调用application 的 setAnimaitonInterval来实现的
    setAnimationInterval(1 / 4.0);
    m_bPaused = true;
}

//恢复
void CCDirector::resume(void)
{
    if (! m_bPaused)
    {
        return;
    }
	//设置动画间隔
    setAnimationInterval(m_dOldAnimationInterval);
	//获取
    if (CCTime::gettimeofdayCocos2d(m_pLastUpdate, NULL) != 0)
    {
        CCLOG("cocos2d: Director: Error in gettimeofday");
    }
	//暂停标记设置为false
    m_bPaused = false;
	//
    m_fDeltaTime = 0;
}

// display the FPS using a LabelAtlas
// updates the FPS every frame
// 每帧都更新
void CCDirector::showStats(void)
{
    m_uFrames++;
	//累计间隔时间+加上每帧间隔时间
    m_fAccumDt += m_fDeltaTime;
    
    if (m_bDisplayStats)
    {
        if (m_pFPSLabel && m_pSPFLabel && m_pDrawsLabel)
        {
			//如果累计间隔时间>信息更新间隔时间
            if (m_fAccumDt > CC_DIRECTOR_STATS_INTERVAL)
            {
                sprintf(m_pszFPS, "%.3f", m_fSecondsPerFrame);
                m_pSPFLabel->setString(m_pszFPS);
               
				//帧数除以间隔时间就是帧率啦....
                m_fFrameRate = m_uFrames / m_fAccumDt;
                m_uFrames = 0;//清零
                m_fAccumDt = 0;//间隔时间清0
                
				//更新帧率
                sprintf(m_pszFPS, "%.1f", m_fFrameRate);
                m_pFPSLabel->setString(m_pszFPS);
                
				//绘图的对象计数器
                sprintf(m_pszFPS, "%4lu", (unsigned long)g_uNumberOfDraws);
                m_pDrawsLabel->setString(m_pszFPS);
            }
            //visit你妹啊?什么东西???/
            m_pDrawsLabel->visit();
            m_pFPSLabel->visit();
            m_pSPFLabel->visit();
        }
    }    
    //更新哦==清0哦,你妈这个....
    g_uNumberOfDraws = 0;
}

void CCDirector::calculateMPF()
{
	//定义时间变量
    struct cc_timeval now;
	//获取当前时间
    CCTime::gettimeofdayCocos2d(&now, NULL);
    //当前秒-前一帧秒
	//当前毫秒-前一帧毫秒
    m_fSecondsPerFrame = (now.tv_sec - m_pLastUpdate->tv_sec) + (now.tv_usec - m_pLastUpdate->tv_usec) / 1000000.0f;
}

// returns the FPS image data pointer and len
// 其实这个函数很简单,文件数据指针和大小
void CCDirector::getFPSImageData(unsigned char** datapointer, unsigned int* length)
{
    // XXX fixed me if it should be used 
    *datapointer = cc_fps_images_png;
	*length = cc_fps_images_len();
}

//创建状态Label
void CCDirector::createStatsLabel()
{
    CCTexture2D *texture = NULL;
	//获得纹理Cache
    CCTextureCache *textureCache = CCTextureCache::sharedTextureCache();

    if( m_pFPSLabel && m_pSPFLabel )
    {
		//释放内存空间
		//他是NEW的==>所以需要手动释放
        CC_SAFE_RELEASE_NULL(m_pFPSLabel);
        CC_SAFE_RELEASE_NULL(m_pSPFLabel);
        CC_SAFE_RELEASE_NULL(m_pDrawsLabel);
		//从纹理缓存中清除
        textureCache->removeTextureForKey("cc_fps_images");
		//
        CCFileUtils::sharedFileUtils()->purgeCachedEntries();
    }

	//下面这段没懂===>怎么可能不懂呢???
	//定义2D纹理像素的格式
    CCTexture2DPixelFormat currentFormat = CCTexture2D::defaultAlphaPixelFormat();
	//设置2D纹理像素格式
    CCTexture2D::setDefaultAlphaPixelFormat(kCCTexture2DPixelFormat_RGBA4444);

	//以下为通过字库创建纹理
    unsigned char *data = NULL;//就是获得图像指针而已
    unsigned int data_len = 0;
    getFPSImageData(&data, &data_len);
	//将字库变成图像==>CCImage与平台相关
    CCImage* image = new CCImage();
    bool isOK = image->initWithImageData(data, data_len);
    if (!isOK) {
        CCLOGERROR("%s", "Fails: init fps_images");
        return;
    }

	//纹理添加图像
    texture = textureCache->addUIImage(image, "cc_fps_images");
	//释放图像数据
    CC_SAFE_RELEASE(image);
    /*
     We want to use an image which is stored in the file named ccFPSImage.c 
     for any design resolutions and all resource resolutions. 
     
     To achieve this,
     
     Firstly, we need to ignore 'contentScaleFactor' in 'CCAtlasNode' and 'CCLabelAtlas'.
     So I added a new method called 'setIgnoreContentScaleFactor' for 'CCAtlasNode',
     this is not exposed(暴露) to game developers, it's only used for displaying FPS now.
     
     Secondly, the size of this image is 480*320, to display the FPS label with correct size, 
     a factor of design resolution ratio(比例) of 480x320 is also needed.
     */
	//为什么要Height/320
	//按照比例放大????亲,这都不懂还怎么开发游戏?
    float factor = CCEGLView::sharedOpenGLView()->getDesignResolutionSize().height / 320.0f;

    m_pFPSLabel = new CCLabelAtlas();
    m_pFPSLabel->setIgnoreContentScaleFactor(true);
    m_pFPSLabel->initWithString("00.0", texture, 12, 32 , '.');
    m_pFPSLabel->setScale(factor);

    m_pSPFLabel = new CCLabelAtlas();
    m_pSPFLabel->setIgnoreContentScaleFactor(true);
    m_pSPFLabel->initWithString("0.000", texture, 12, 32, '.');
    m_pSPFLabel->setScale(factor);

	//NEW对象
    m_pDrawsLabel = new CCLabelAtlas();
	//忽略缩放
    m_pDrawsLabel->setIgnoreContentScaleFactor(true);
	//初始化字体
    m_pDrawsLabel->initWithString("000", texture, 12, 32, '.');
	//设置缩放
    m_pDrawsLabel->setScale(factor);

    CCTexture2D::setDefaultAlphaPixelFormat(currentFormat);
	//设置Draws	==> 00.0
	//设置SPF	==> 0.000
	//设置FPS	==> 000
    m_pDrawsLabel->setPosition(ccpAdd(ccp(0, 34*factor), CC_DIRECTOR_STATS_POSITION));
    m_pSPFLabel->setPosition(ccpAdd(ccp(0, 17*factor), CC_DIRECTOR_STATS_POSITION));
    m_pFPSLabel->setPosition(CC_DIRECTOR_STATS_POSITION);
}

//获取缩放因子
float CCDirector::getContentScaleFactor(void)
{
    return m_fContentScaleFactor;
}

//设置ContentScaleFactor
void CCDirector::setContentScaleFactor(float scaleFactor)
{
    if (scaleFactor != m_fContentScaleFactor)
    {
        m_fContentScaleFactor = scaleFactor;
		//每重新设置ScaleFactor,都得重新调用一次
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

//获取投影代理器
CCDirectorDelegate* CCDirector::getDelegate() const
{
    return m_pProjectionDelegate;
}
//设置代理器
void CCDirector::setDelegate(CCDirectorDelegate* pDelegate)
{
    m_pProjectionDelegate = pDelegate;
}
//设置Scheduler
void CCDirector::setScheduler(CCScheduler* pScheduler)
{
    if (m_pScheduler != pScheduler)
    {
        CC_SAFE_RETAIN(pScheduler);
        CC_SAFE_RELEASE(m_pScheduler);
        m_pScheduler = pScheduler;
    }
}
//得到Scheduler
CCScheduler* CCDirector::getScheduler()
{
    return m_pScheduler;
}
//设置ActionManager
void CCDirector::setActionManager(CCActionManager* pActionManager)
{
    if (m_pActionManager != pActionManager)
    {
        CC_SAFE_RETAIN(pActionManager);
        CC_SAFE_RELEASE(m_pActionManager);
        m_pActionManager = pActionManager;
    }    
}
//得到ActionManger
CCActionManager* CCDirector::getActionManager()
{
    return m_pActionManager;
}
//设置TouchDispatcher
void CCDirector::setTouchDispatcher(CCTouchDispatcher* pTouchDispatcher)
{
    if (m_pTouchDispatcher != pTouchDispatcher)
    {
        CC_SAFE_RETAIN(pTouchDispatcher);
        CC_SAFE_RELEASE(m_pTouchDispatcher);
        m_pTouchDispatcher = pTouchDispatcher;
    }    
}
//获取TouchDispatcher
CCTouchDispatcher* CCDirector::getTouchDispatcher()
{
    return m_pTouchDispatcher;
}
//设置KeypadDispathcer
void CCDirector::setKeypadDispatcher(CCKeypadDispatcher* pKeypadDispatcher)
{
    CC_SAFE_RETAIN(pKeypadDispatcher);
    CC_SAFE_RELEASE(m_pKeypadDispatcher);
    m_pKeypadDispatcher = pKeypadDispatcher;
}
//得到KeypadDispathcer
CCKeypadDispatcher* CCDirector::getKeypadDispatcher()
{
    return m_pKeypadDispatcher;
}
//设置重力加速
void CCDirector::setAccelerometer(CCAccelerometer* pAccelerometer)
{
    if (m_pAccelerometer != pAccelerometer)
    {
        CC_SAFE_DELETE(m_pAccelerometer);
        m_pAccelerometer = pAccelerometer;
    }
}
//得到重力加速
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
// 导演本来有4种,但是人家说这一种已经足够啦
void CCDisplayLinkDirector::startAnimation(void)
{
    if (CCTime::gettimeofdayCocos2d(m_pLastUpdate, NULL) != 0)
    {
        CCLOG("cocos2d: DisplayLinkDirector: Error on gettimeofday");
    }

    m_bInvalid = false;
#ifndef EMSCRIPTEN
	//最后还是Application来实现的
    CCApplication::sharedApplication()->setAnimationInterval(m_dAnimationInterval);
#endif // EMSCRIPTEN
}
//主循环
void CCDisplayLinkDirector::mainLoop(void)
{
	//还记得end()吗?
	//如果清除
    if (m_bPurgeDirecotorInNextLoop)
    {
        m_bPurgeDirecotorInNextLoop = false;
		//清除各种chace
        purgeDirector();
    }
    else if (! m_bInvalid)
     {
		 //绘制场景
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

