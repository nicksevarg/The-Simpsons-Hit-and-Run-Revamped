//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================

#include <pddi/gles/gl.hpp>
#include <pddi/gles/glcon.hpp>
#include <pddi/gles/gldisplay.hpp>
#include <pddi/base/debug.hpp>
#include <SDL.h>
#if defined(RAD_ANDROID)
#include <SDL_system.h>
#endif

#include<stdio.h>
#include<string.h>
#include<math.h>
#include<stdlib.h>
#include<vector>
#include<string>

#if defined(RAD_ANDROID)
#include <jni.h>
#include <dlfcn.h>
#include <EGL/egl.h>
#include <sys/stat.h>

// Keep GLES context-version state in this renderer translation unit.
// Android helper executables link libpddi without linking Win32Platform.cpp.
int gSHARRequestedOpenGLESVersionMajor = 3;
int gSHARActiveOpenGLESVersionMajor = 3;

bool IsCelShadingEnabled();
bool IsSHARFrontendMenuCelPassActive();
bool IsMotionBlurEnabled();
void SetSHARCharacterMaskWorldEnabled(bool enabled);
void SetSHARCharacterMaskModelPass(bool enabled);
void SetSHARTrackedColorWriteMask(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
void InvalidateSHARTrackedColorWriteMaskCache();
bool GetSHARMotionBlurCameraData(float* output);
bool IsSHARWorldCloudShadowLevelAllowed();
bool IsFrameSharpeningEnabled();
float GetFrameSharpenStrength();


void GetCRTFilterConfiguration(bool* enabled, float* scanlineStrength, float* curvature, float* glowStrength, float* cornerDarkening, float* overscan, float* brightness, float* contrast);

static int gSHARAndroidTargetFPS = 60;
static bool gSHARDynamicResolutionEnabled = false;
static int gSHARDynamicResolutionMaxHeight = 0;
static int gSHARDynamicResolutionMinHeight = 0;
static int gSHARDynamicResolutionCurrentHeight = 0;
static int gSHARDynamicResolutionHighSamples = 0;
static int gSHARDynamicResolutionLowSamples = 0;
static int gSHARDynamicResolutionLastTargetFPS = 0;
static Uint64 gSHARDynamicResolutionLastResizeTicks = 0;

static bool gSHARDynamicResolutionCpuFrameSampleActive = false;
static Uint64 gSHARDynamicResolutionCpuFrameStartTicks = 0;
static void SHARResetDynamicResolutionController();
static int SHARGetDynamicResolutionMinimumHeight(int maxHeight);

// Render-target size snapshots are shared between Android's UI callback and the GL thread.
static SDL_SpinLock gSHARAndroidRenderResolutionLock = 0;
static int gSHARAndroidRenderWidth = 0;
static int gSHARAndroidRenderHeight = 0;
static int gSHARAndroidRenderResizeSerial = 0;

bool RequestSHARAndroidRenderResolutionHeight(int targetHeight);

// Tracks the complete iris transition, including the black-screen scene handoff
// between the closing and opening halves.
static bool gSHARIrisTransitionActive = false;

static bool gSHARBloomEnabled = true;
static bool gSHARCloudShadowsEnabled = false;
static bool gSHARGraphicsEffectTogglesLoaded = false;

static bool ReadSHARGraphicsToggleFile(const char* filename, bool defaultValue, bool* fileExists)
{
    if( fileExists != NULL )
    {
        *fileExists = false;
    }

    const char* basePath = SDL_AndroidGetApplicationExternalFilesPath();
    if( basePath == NULL || basePath[0] == '\0' )
    {
        return defaultValue;
    }

    char path[512];
    const int length = snprintf(
        path, sizeof(path), "%s/Graphics/%s", basePath, filename );
    if( length < 0 || length >= static_cast<int>( sizeof(path) ) )
    {
        return defaultValue;
    }

    FILE* file = fopen( path, "r" );
    if( file == NULL )
    {
        return defaultValue;
    }
    if( fileExists != NULL )
    {
        *fileExists = true;
    }

    bool result = defaultValue;
    char line[256];
    while( fgets( line, sizeof(line), file ) != NULL )
    {
        char* value = strchr( line, '=' );
        if( value == NULL )
        {
            continue;
        }
        ++value;
        while( *value == ' ' || *value == '\t' )
        {
            ++value;
        }

        if( strncmp( value, "true", 4 ) == 0 ||
            strncmp( value, "TRUE", 4 ) == 0 ||
            *value == '1' || strncmp( value, "on", 2 ) == 0 ||
            strncmp( value, "ON", 2 ) == 0 ||
            strncmp( value, "yes", 3 ) == 0 )
        {
            result = true;
        }
        else if( strncmp( value, "false", 5 ) == 0 ||
                 strncmp( value, "FALSE", 5 ) == 0 ||
                 *value == '0' || strncmp( value, "off", 3 ) == 0 ||
                 strncmp( value, "OFF", 3 ) == 0 ||
                 strncmp( value, "no", 2 ) == 0 )
        {
            result = false;
        }
        break;
    }

    fclose( file );
    return result;
}

static void WriteSHARGraphicsToggleFile(const char* filename,
                                        const char* title,
                                        const char* description,
                                        const char* key,
                                        bool enabled)
{
    const char* basePath = SDL_AndroidGetApplicationExternalFilesPath();
    if( basePath == NULL || basePath[0] == '\0' )
    {
        return;
    }

    char directory[512];
    int length = snprintf(
        directory, sizeof(directory), "%s/Graphics", basePath );
    if( length < 0 || length >= static_cast<int>( sizeof(directory) ) )
    {
        return;
    }

    struct stat directoryInfo;
    if( stat( directory, &directoryInfo ) != 0 )
    {
        mkdir( directory, 0777 );
    }
    if( stat( directory, &directoryInfo ) != 0 || !S_ISDIR( directoryInfo.st_mode ) )
    {
        return;
    }

    char path[512];
    length = snprintf( path, sizeof(path), "%s/%s", directory, filename );
    if( length < 0 || length >= static_cast<int>( sizeof(path) ) )
    {
        return;
    }

    FILE* file = fopen( path, "w" );
    if( file == NULL )
    {
        return;
    }

    fprintf( file, "%s\n", title );
    fprintf( file, "%s\n", description );
    fprintf( file, "# Change with true/false or 1/0 values.\n" );
    fprintf( file, "# You can also use the in-game settings toggle.\n" );
    fprintf( file, "# Changes to this file will be reflected upon restart.\n" );
    fprintf( file, "\n" );
    fprintf( file, "%s=%s\n", key, enabled ? "true" : "false" );
    fclose( file );
}
static void EnsureSHARGraphicsEffectTogglesLoaded()
{
    if( gSHARGraphicsEffectTogglesLoaded )
    {
        return;
    }
    gSHARGraphicsEffectTogglesLoaded = true;

    bool bloomFileExists = false;
    gSHARBloomEnabled =
        ReadSHARGraphicsToggleFile( "Simpsons_bloom.txt", true, &bloomFileExists );
    if( !bloomFileExists )
    {
        WriteSHARGraphicsToggleFile(
            "Simpsons_bloom.txt",
            "# Simpsons Hit & Run - Revamped! [Bloom Settings]",
            "# Makes lights and special effects glow more intensely.",
            "bloom",
            gSHARBloomEnabled );
    }

    bool cloudShadowsFileExists = false;
    gSHARCloudShadowsEnabled =
        ReadSHARGraphicsToggleFile( "Simpsons_cloud_shadows.txt", false, &cloudShadowsFileExists );
    if( !cloudShadowsFileExists )
    {
        WriteSHARGraphicsToggleFile(
            "Simpsons_cloud_shadows.txt",
            "# Simpsons Hit & Run - Revamped! [Cloud Shadows Setting]",
            "# Adds drifting cloud shadows (Only used on daytime levels)",
            "cloud_shadows",
            gSHARCloudShadowsEnabled );
    }

    // DRS remains opt-in; write its default without changing existing installations.
    bool dynamicResolutionFileExists = false;
    gSHARDynamicResolutionEnabled =
        ReadSHARGraphicsToggleFile(
            "Simpsons_dynamic_resolution.txt", false, &dynamicResolutionFileExists );
    if( !dynamicResolutionFileExists )
    {
        WriteSHARGraphicsToggleFile(
            "Simpsons_dynamic_resolution.txt",
            "# Simpsons Hit & Run - Revamped! [Dynamic Resolution]",
            "# Automatically scales internal render resolution to help meet the selected FPS target.",
            "dynamic_resolution",
            gSHARDynamicResolutionEnabled );
    }
}

void InitializeSHARGraphicsEffectConfigurations()
{
    EnsureSHARGraphicsEffectTogglesLoaded();
}

bool IsSHARBloomEnabled()
{
    EnsureSHARGraphicsEffectTogglesLoaded();
    return gSHARBloomEnabled;
}

void SetSHARBloomEnabled(bool enabled)
{
    EnsureSHARGraphicsEffectTogglesLoaded();
    if( gSHARBloomEnabled == enabled )
    {
        return;
    }
    gSHARBloomEnabled = enabled;
    WriteSHARGraphicsToggleFile(
        "Simpsons_bloom.txt",
        "# Simpsons Hit & Run - Revamped! [Bloom Settings]",
        "# Makes lights and special effects glow more intensely.",
        "bloom",
        enabled );
}

bool IsSHARCloudShadowsEnabled()
{
    EnsureSHARGraphicsEffectTogglesLoaded();
    return gSHARCloudShadowsEnabled;
}

void SetSHARCloudShadowsEnabled(bool enabled)
{
    EnsureSHARGraphicsEffectTogglesLoaded();
    if( gSHARCloudShadowsEnabled == enabled )
    {
        return;
    }
    gSHARCloudShadowsEnabled = enabled;
    WriteSHARGraphicsToggleFile(
        "Simpsons_cloud_shadows.txt",
        "# Simpsons Hit & Run - Revamped! [Cloud Shadows Setting]",
        "# Adds drifting cloud shadows (Only used on daytime levels)",
        "cloud_shadows",
        enabled );
}

static int SHARGetDynamicResolutionMinimumHeight(int maxHeight)
{
    if (maxHeight >= 2160) return 1080;
    if (maxHeight >= 1440) return 1080;
    if (maxHeight >= 1080) return 720;
    if (maxHeight >= 720) return 480;
    return 360;
}

bool IsSHARDynamicResolutionEnabled()
{
    EnsureSHARGraphicsEffectTogglesLoaded();
    return gSHARDynamicResolutionEnabled;
}

void SetSHARAndroidResolutionProfile(int maxHeight)
{
    if (maxHeight != 480 && maxHeight != 720 && maxHeight != 1080 &&
        maxHeight != 1440 && maxHeight != 2160)
    {
        return;
    }

    if (gSHARDynamicResolutionMaxHeight == maxHeight)
        return;

    SDL_AtomicLock(&gSHARAndroidRenderResolutionLock);
    gSHARDynamicResolutionMaxHeight = maxHeight;
    gSHARDynamicResolutionMinHeight = SHARGetDynamicResolutionMinimumHeight(maxHeight);
    gSHARDynamicResolutionCurrentHeight = maxHeight;
    SDL_AtomicUnlock(&gSHARAndroidRenderResolutionLock);
    SHARResetDynamicResolutionController();
}

void SetSHARDynamicResolutionEnabled(bool enabled)
{
    EnsureSHARGraphicsEffectTogglesLoaded();
    if (gSHARDynamicResolutionEnabled == enabled)
        return;

    gSHARDynamicResolutionEnabled = enabled;
    WriteSHARGraphicsToggleFile(
        "Simpsons_dynamic_resolution.txt",
        "# Simpsons Hit & Run - Revamped! [Dynamic Resolution]",
        "# Automatically scales internal render resolution to help meet the selected FPS target.",
        "dynamic_resolution",
        enabled);

    SHARResetDynamicResolutionController();

    // Enabling starts at the selected maximum; disabling immediately restores it.
    const int restoreHeight = gSHARDynamicResolutionMaxHeight;
    if (restoreHeight > 0 && RequestSHARAndroidRenderResolutionHeight(restoreHeight))
    {
        SDL_AtomicLock(&gSHARAndroidRenderResolutionLock);
        gSHARDynamicResolutionCurrentHeight = restoreHeight;
        SDL_AtomicUnlock(&gSHARAndroidRenderResolutionLock);
    }
}

void SetSHARIrisTransitionActive(bool active)
{
    gSHARIrisTransitionActive = active;
}

// The GUI lives in libmain.so and the renderer is built as libpddi.a. Keep
// this setter at global scope so the main library can link to it.
void SetSHARAndroidTargetFPS(int targetFPS)
{
    if (targetFPS == 30 || targetFPS == 60 || targetFPS == 90 ||
        targetFPS == 120 || targetFPS == 144)
    {
        if (gSHARAndroidTargetFPS != targetFPS)
        {
            gSHARAndroidTargetFPS = targetFPS;
            SHARResetDynamicResolutionController();
        }
    }
}

namespace
{

struct SHARMotionBlurCameraState
{
    float position[3];
    float right[3];
    float up[3];
    float forward[3];
    float tanHalfFovX;
    float tanHalfFovY;
    float nearPlane;
    float farPlane;
    bool valid;
};

static SHARMotionBlurCameraState gSHARPreviousMotionBlurCamera = {};
static int gSHARMotionBlurHistoryWidth = 0;
static int gSHARMotionBlurHistoryHeight = 0;
static Uint64 gSHARPreviousMotionBlurSampleTime = 0;

struct SHARMotionBlurSubjectBounds
{
    float position[3];
    float halfWidth;
    float halfHeight;
    float motionIntensity;
    bool valid;
};

static SHARMotionBlurSubjectBounds gSHARMotionBlurCharacterSubject = {};
static SHARMotionBlurSubjectBounds gSHARMotionBlurVehicleSubject = {};

static bool CaptureSHARMotionBlurCamera(SHARMotionBlurCameraState* state)
{
    if (state == NULL)
        return false;

    // Keep camera acquisition inside the PDDI context implementation so the
    // display backend does not depend on Pure3D headers with broader includes.
    float cameraData[16] = {};
    if (!GetSHARMotionBlurCameraData(cameraData))
        return false;

    state->position[0] = cameraData[0];
    state->position[1] = cameraData[1];
    state->position[2] = cameraData[2];
    state->right[0] = cameraData[3];
    state->right[1] = cameraData[4];
    state->right[2] = cameraData[5];
    state->up[0] = cameraData[6];
    state->up[1] = cameraData[7];
    state->up[2] = cameraData[8];
    state->forward[0] = cameraData[9];
    state->forward[1] = cameraData[10];
    state->forward[2] = cameraData[11];
    state->tanHalfFovX = cameraData[12];
    state->tanHalfFovY = cameraData[13];
    state->nearPlane = cameraData[14];
    state->farPlane = cameraData[15];
    state->valid = true;
    return true;
}

static bool IsSHARMotionBlurHistoryUsable(const SHARMotionBlurCameraState& current, int width, int height)
{
    if (!current.valid || !gSHARPreviousMotionBlurCamera.valid ||
        width != gSHARMotionBlurHistoryWidth || height != gSHARMotionBlurHistoryHeight)
        return false;

    const float dx = current.position[0] - gSHARPreviousMotionBlurCamera.position[0];
    const float dy = current.position[1] - gSHARPreviousMotionBlurCamera.position[1];
    const float dz = current.position[2] - gSHARPreviousMotionBlurCamera.position[2];
    if (dx * dx + dy * dy + dz * dz > 400.0f)
        return false; // Avoid streaks across teleports and camera cuts.

    const float forwardDot =
        current.forward[0] * gSHARPreviousMotionBlurCamera.forward[0] +
        current.forward[1] * gSHARPreviousMotionBlurCamera.forward[1] +
        current.forward[2] * gSHARPreviousMotionBlurCamera.forward[2];
    return forwardDot > 0.60f;
}

static void CommitSHARMotionBlurCameraHistory(
    const SHARMotionBlurCameraState& current, bool captured, int width, int height)
{
    if (captured && current.valid && width > 0 && height > 0)
    {
        gSHARPreviousMotionBlurCamera = current;
        gSHARMotionBlurHistoryWidth = width;
        gSHARMotionBlurHistoryHeight = height;
    }
    else
    {
        gSHARPreviousMotionBlurCamera.valid = false;
        gSHARMotionBlurHistoryWidth = 0;
        gSHARMotionBlurHistoryHeight = 0;
    }
}

static float GetSHARMotionBlurTimeScale()
{
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    const Uint64 now = SDL_GetPerformanceCounter();
    float scale = 1.0f;

    if( frequency > 0 && gSHARPreviousMotionBlurSampleTime > 0 &&
        now > gSHARPreviousMotionBlurSampleTime )
    {
        const double elapsedTicks =
            (double)(now - gSHARPreviousMotionBlurSampleTime);
        const double referenceTicks = (double)frequency / 60.0;
        const double ratio = referenceTicks / elapsedTicks;

        // Preserve roughly the same screen-space blur duration as FPS changes,
        // while preventing unusually long frames or timer anomalies from causing streaks.
        if( ratio < 0.25 )
        {
            scale = 0.25f;
        }
        else if( ratio > 2.4 )
        {
            scale = 2.4f;
        }
        else
        {
            scale = (float)ratio;
        }
    }

    gSHARPreviousMotionBlurSampleTime = now;
    return scale;
}

GLuint gCelPostProgram = 0;
GLuint gCelBloomOnlyPostProgram = 0;
bool gCelBloomOnlyPostProgramAttempted = false;
GLint gCelBloomOnlySceneLocation = -1;
GLint gCelBloomOnlyTextureLocation = -1;
GLint gCelBloomOnlyStrengthLocation = -1;
GLuint gCelPostTexture = 0;
GLuint gCelPostVbo = 0;
GLuint gCelFallbackProgram = 0;
GLuint gCelFallbackVbo = 0;
GLint gCelFallbackTextureLocation = -1;

// A frame uses one committed world-render-size snapshot. PDDI dimensions are
// restored to that target at frame start and temporarily switch to output size
// after scene composition for GUI rendering.
static int gSHARFrameWidth = 0;
static int gSHARFrameHeight = 0;
static bool gCelWorldInFbo = false;

// Optional post-processing uses VAOs. Resolve GLES 3.0 core functions or the
// GLES 2.0 OES extension rather than assuming OES entry points always exist.
static bool gSHARVertexArrayObjectsSupported = false;
static bool gSHARVertexArrayOESAdvertised = false;
static bool gSHARDepthTextureOESAdvertised = false;
static bool gSHARFramebufferSupportAvailable = false;
static const char* gSHARVertexArrayProvider = "unavailable";
static PFNGLGENVERTEXARRAYSOESPROC gSHARGenVertexArraysProc = NULL;
static PFNGLBINDVERTEXARRAYOESPROC gSHARBindVertexArrayProc = NULL;
static PFNGLDELETEVERTEXARRAYSOESPROC gSHARDeleteVertexArraysProc = NULL;
// Tracks whether full-frame filters have already run before touch controls.
static bool gSHARPostProcessAppliedBeforeTouchHud = false;
// Set at RenderManager::ContextUpdate's end; prevents applying post-processing
// to stale contents when SwapBuffers runs without a newly rendered frame.
static bool gSHARFrameReadyForPresentation = false;
// pglDisplay keeps the internal render size for the world. During the GUI phase,
// briefly expose the actual Surface dimensions, then restore the render size at
// the next frame start in CommitPendingResize().
static int* gSHARActiveDisplayWidthForGui = NULL;
static int* gSHARActiveDisplayHeightForGui = NULL;
GLuint gCelRenderFbo = 0;
GLuint gCelRenderDepth = 0;
GLuint gCelRenderDepthTexture = 0;
int gCelDepthTextureWidth = 0;
int gCelDepthTextureHeight = 0;
bool gCelDepthTextureActive = false;
bool gCelDepthTextureFailed = false;
bool gCelDepthExtensionChecked = false;
bool gCelDepthTextureExtensionAvailable = false;
bool gCelRenderFboReady = false;
GLint gCelPostSceneLocation = -1;
GLint gCelPostTexelLocation = -1;
GLint gCelPostBloomLocation = -1;
GLint gCelPostBloomStrengthLocation = -1;
GLint gCelPostDepthLocation = -1;
GLint gCelPostDepthAvailableLocation = -1;
// Depth-aware, world-anchored moving cloud shadows.
GLint gCelPostCloudShadowEnabledLocation = -1;
GLint gCelPostCloudShadowTimeLocation = -1;
GLint gCelPostCloudShadowStrengthLocation = -1;
GLint gCelPostCloudShadowMaskLocation = -1;
GLint gCelPostCloudShadowMaskTexelSizeLocation = -1;
GLint gCelPostCloudShadowMaskAvailableLocation = -1;
GLuint gCelCloudShadowProgram = 0;
GLuint gCelCloudShadowFbo = 0;
GLuint gCelCloudShadowTexture = 0;
GLint gCelCloudMaskDepthLocation = -1;
GLint gCelCloudMaskTimeLocation = -1;
GLint gCelCloudMaskCameraPositionLocation = -1;
GLint gCelCloudMaskCameraRightLocation = -1;
GLint gCelCloudMaskCameraUpLocation = -1;
GLint gCelCloudMaskCameraForwardLocation = -1;
GLint gCelCloudMaskTanHalfFovXLocation = -1;
GLint gCelCloudMaskTanHalfFovYLocation = -1;
GLint gCelCloudMaskNearPlaneLocation = -1;
GLint gCelCloudMaskFarPlaneLocation = -1;
int gCelCloudShadowWidth = 0;
int gCelCloudShadowHeight = 0;
int gCelCloudShadowFailedWidth = 0;
int gCelCloudShadowFailedHeight = 0;
bool gCelCloudShadowReady = false;
GLint gCelPostMotionBlurEnabledLocation = -1;
GLint gCelPostMotionBlurValidLocation = -1;
GLint gCelPostMotionBlurScaleLocation = -1;
GLint gCelPostCurrentCameraPositionLocation = -1;
GLint gCelPostCurrentCameraRightLocation = -1;
GLint gCelPostCurrentCameraUpLocation = -1;
GLint gCelPostCurrentCameraForwardLocation = -1;
GLint gCelPostPreviousCameraPositionLocation = -1;
GLint gCelPostPreviousCameraRightLocation = -1;
GLint gCelPostPreviousCameraUpLocation = -1;
GLint gCelPostPreviousCameraForwardLocation = -1;
GLint gCelPostCurrentTanHalfFovXLocation = -1;
GLint gCelPostCurrentTanHalfFovYLocation = -1;
GLint gCelPostPreviousTanHalfFovXLocation = -1;
GLint gCelPostPreviousTanHalfFovYLocation = -1;
GLint gCelPostCurrentNearPlaneLocation = -1;
GLint gCelPostCurrentFarPlaneLocation = -1;
GLint gCelPostPreviousNearPlaneLocation = -1;
GLint gCelPostCharacterMaskEnabledLocation = -1;
GLint gCelPostCharacterMaskPositionLocation = -1;
GLint gCelPostCharacterMaskSizeLocation = -1;
GLint gCelPostVehicleMaskEnabledLocation = -1;
GLint gCelPostVehicleMaskPositionLocation = -1;
GLint gCelPostVehicleMaskSizeLocation = -1;
GLint gCelPostVehicleMotionIntensityLocation = -1;
GLuint gCelBloomExtractProgram = 0;
GLuint gCelBloomBlurProgram = 0;

// One captured frame texture feeds the combined sharpening/saturation/CRT pass.
GLuint gFrameSharpenTexture = 0;
int gFrameSharpenWidth = 0;
int gFrameSharpenHeight = 0;

GLuint gCRTProgram = 0;
GLint gCRTSharpenLocation = -1;
GLuint gSHARSaturationFetchProgram = 0;
GLint gSHARSaturationFetchLocation = -1;
int gSHARSaturationFetchMode = 0; // 0=unavailable, 1=EXT, 2=NV, 3=ARM
bool gSHARSaturationFetchChecked = false;
GLuint gCRTVao = 0;
GLuint gCRTVbo = 0;
bool gCRTVaoReady = false;
GLint gCRTSceneLocation = -1;
GLint gCRTTexelLocation = -1;
GLint gCRTScanlineLocation = -1;
GLint gCRTCurvatureLocation = -1;
GLint gCRTGlowLocation = -1;
GLint gCRTCornersLocation = -1;
GLint gCRTOverscanLocation = -1;
GLint gCRTBrightnessLocation = -1;
GLint gCRTContrastLocation = -1;
GLint gCRTCrtEnabledLocation = -1;
GLint gCRTSaturationLocation = -1;


GLuint gCelBloomFbo = 0;
GLuint gCelBloomTexture = 0;
GLuint gCelBloomScratchTexture = 0;
GLint gCelBloomExtractSceneLocation = -1;
GLint gCelBloomExtractTexelLocation = -1;
GLint gCelBloomBlurSourceLocation = -1;
GLint gCelBloomBlurStepLocation = -1;
int gCelBloomWidth = 0;
int gCelBloomHeight = 0;
int gCelBloomFailedWidth = 0;
int gCelBloomFailedHeight = 0;
bool gCelBloomReady = false;

int gCelPostWidth = 0;
int gCelPostHeight = 0;
int gCelPostFailedWidth = 0;
int gCelPostFailedHeight = 0;
int gCelRenderbufferWidth = 0;
int gCelRenderbufferHeight = 0;
int gCelRenderbufferFailedWidth = 0;
int gCelRenderbufferFailedHeight = 0;
int gCelRenderFboFailedWidth = 0;
int gCelRenderFboFailedHeight = 0;

// Successful scene-FBO validation is reusable while its size, attachments and
// resource-budget inputs remain unchanged. ResetAndroidPostProcess() invalidates
// this cache whenever the GL resources are recreated or the context is reset.
static bool gCelRenderTargetValidationCached = false;
static int gCelRenderTargetValidatedWidth = 0;
static int gCelRenderTargetValidatedHeight = 0;
static bool gCelRenderTargetValidatedBloomEnabled = false;
static bool gCelRenderTargetValidatedUsesDepthTexture = false;
static bool gCelRenderTargetValidatedDepthTextureRequested = false;
static GLuint gCelRenderTargetValidatedFbo = 0;
static GLuint gCelRenderTargetValidatedColourTexture = 0;
static GLuint gCelRenderTargetValidatedDepthObject = 0;

int gFrameSharpenFailedWidth = 0;
int gFrameSharpenFailedHeight = 0;


// Budget for auxiliary full-screen post-processing targets. This deliberately
// excludes game assets and the Android default framebuffer, so it is only a
// conservative guard—not a claim about total free GPU memory. At 2160p, the
// estimated targets for cel shading, bloom, and one shared sharpening/CRT
// frame snapshot fit within the 128 MiB test budget; allocations remain checked.
static const size_t SHAR_POSTPROCESS_BUDGET_BYTES =
    (size_t)128 * 1024 * 1024;

static void SHARConfigureVertexArrayFunctions(int actualMajor)
{
    gSHARGenVertexArraysProc = NULL;
    gSHARBindVertexArrayProc = NULL;
    gSHARDeleteVertexArraysProc = NULL;
    gSHARVertexArrayProvider = "unavailable";

    // Vertex array objects are core in GLES 3.0; GLES 2.0 devices may expose
    // the same API through GL_OES_vertex_array_object.
    if (actualMajor >= 3)
    {
        gSHARGenVertexArraysProc =
            reinterpret_cast<PFNGLGENVERTEXARRAYSOESPROC>(SDL_GL_GetProcAddress("glGenVertexArrays"));
        gSHARBindVertexArrayProc =
            reinterpret_cast<PFNGLBINDVERTEXARRAYOESPROC>(SDL_GL_GetProcAddress("glBindVertexArray"));
        gSHARDeleteVertexArraysProc =
            reinterpret_cast<PFNGLDELETEVERTEXARRAYSOESPROC>(SDL_GL_GetProcAddress("glDeleteVertexArrays"));

        if (gSHARGenVertexArraysProc != NULL &&
            gSHARBindVertexArrayProc != NULL &&
            gSHARDeleteVertexArraysProc != NULL)
        {
            gSHARVertexArrayProvider = "OpenGL ES 3.x core";
        }
    }

    gSHARVertexArrayOESAdvertised =
        SDL_GL_ExtensionSupported("GL_OES_vertex_array_object") == SDL_TRUE ||
        GLAD_GL_OES_vertex_array_object != 0;

    if (gSHARGenVertexArraysProc == NULL ||
        gSHARBindVertexArrayProc == NULL ||
        gSHARDeleteVertexArraysProc == NULL)
    {
        if (gSHARVertexArrayOESAdvertised)
        {
            gSHARGenVertexArraysProc = glad_glGenVertexArraysOES != NULL
                ? glad_glGenVertexArraysOES
                : reinterpret_cast<PFNGLGENVERTEXARRAYSOESPROC>(SDL_GL_GetProcAddress("glGenVertexArraysOES"));
            gSHARBindVertexArrayProc = glad_glBindVertexArrayOES != NULL
                ? glad_glBindVertexArrayOES
                : reinterpret_cast<PFNGLBINDVERTEXARRAYOESPROC>(SDL_GL_GetProcAddress("glBindVertexArrayOES"));
            gSHARDeleteVertexArraysProc = glad_glDeleteVertexArraysOES != NULL
                ? glad_glDeleteVertexArraysOES
                : reinterpret_cast<PFNGLDELETEVERTEXARRAYSOESPROC>(SDL_GL_GetProcAddress("glDeleteVertexArraysOES"));

            if (gSHARGenVertexArraysProc != NULL &&
                gSHARBindVertexArrayProc != NULL &&
                gSHARDeleteVertexArraysProc != NULL)
            {
                gSHARVertexArrayProvider = "GL_OES_vertex_array_object";
            }
        }
    }

    gSHARVertexArrayObjectsSupported =
        gSHARGenVertexArraysProc != NULL &&
        gSHARBindVertexArrayProc != NULL &&
        gSHARDeleteVertexArraysProc != NULL;
}

static void SHARBindVertexArray(GLuint array)
{
    if (gSHARVertexArrayObjectsSupported && gSHARBindVertexArrayProc != NULL)
        gSHARBindVertexArrayProc(array);
}

static void SHARGenVertexArrays(GLsizei count, GLuint* arrays)
{
    if (arrays == NULL)
        return;

    if (gSHARVertexArrayObjectsSupported && gSHARGenVertexArraysProc != NULL)
    {
        gSHARGenVertexArraysProc(count, arrays);
        return;
    }

    for (GLsizei i = 0; i < count; ++i)
        arrays[i] = 0;
}

static GLint SHARGetBoundVertexArray()
{
    GLint array = 0;
    if (gSHARVertexArrayObjectsSupported)
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING_OES, &array);
    return array;
}

static bool SHARValidateFramebufferSupport()
{
    if (glad_glGenFramebuffers == NULL ||
        glad_glBindFramebuffer == NULL ||
        glad_glFramebufferTexture2D == NULL ||
        glad_glCheckFramebufferStatus == NULL ||
        glad_glDeleteFramebuffers == NULL ||
        glad_glGenRenderbuffers == NULL ||
        glad_glBindRenderbuffer == NULL ||
        glad_glRenderbufferStorage == NULL ||
        glad_glFramebufferRenderbuffer == NULL ||
        glad_glDeleteRenderbuffers == NULL ||
        glad_glGenTextures == NULL ||
        glad_glBindTexture == NULL ||
        glad_glTexParameteri == NULL ||
        glad_glTexImage2D == NULL ||
        glad_glDeleteTextures == NULL ||
        glad_glGetIntegerv == NULL ||
        glad_glActiveTexture == NULL ||
        glad_glGetError == NULL)
    {
        return false;
    }

    // Isolate this capability probe from earlier startup GL errors.
    while (glGetError() != GL_NO_ERROR)
    {
    }

    GLint previousFramebuffer = 0;
    GLint previousRenderbuffer = 0;
    GLint previousActiveTexture = GL_TEXTURE0;
    GLint previousTexture = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &previousRenderbuffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);

    GLuint framebuffer = 0;
    GLuint colourTexture = 0;
    GLuint depthRenderbuffer = 0;
    glGenFramebuffers(1, &framebuffer);
    glGenTextures(1, &colourTexture);
    glGenRenderbuffers(1, &depthRenderbuffer);

    GLenum status = 0;
    if (framebuffer != 0 && colourTexture != 0 && depthRenderbuffer != 0)
    {
        glBindTexture(GL_TEXTURE_2D, colourTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 4, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

        glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colourTexture, 0);

        glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, 4, 4);
        glFramebufferRenderbuffer(
            GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderbuffer);

        status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    }

    const GLenum error = glGetError();

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)previousFramebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, (GLuint)previousRenderbuffer);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, (GLuint)previousTexture);
    glActiveTexture((GLenum)previousActiveTexture);

    if (framebuffer != 0)
        glDeleteFramebuffers(1, &framebuffer);
    if (depthRenderbuffer != 0)
        glDeleteRenderbuffers(1, &depthRenderbuffer);
    if (colourTexture != 0)
        glDeleteTextures(1, &colourTexture);

    return framebuffer != 0 &&
        colourTexture != 0 &&
        depthRenderbuffer != 0 &&
        status == GL_FRAMEBUFFER_COMPLETE &&
        error == GL_NO_ERROR;
}

static void SHARInitializeGraphicsCapabilities()
{
    const GLubyte* versionString = glGetString(GL_VERSION);

    int actualMajor = 0;
    if (versionString != NULL)
    {
        const char* versionNumbers = reinterpret_cast<const char*>(versionString);
        const char* esPrefix = strstr(versionNumbers, "OpenGL ES");
        if (esPrefix != NULL)
            versionNumbers = esPrefix + strlen("OpenGL ES");
        while (*versionNumbers != '\0' &&
               (*versionNumbers < '0' || *versionNumbers > '9'))
        {
            ++versionNumbers;
        }
        if (sscanf(versionNumbers, "%d", &actualMajor) != 1)
            actualMajor = 0;
    }

    gSHARDepthTextureOESAdvertised =
        SDL_GL_ExtensionSupported("GL_OES_depth_texture") == SDL_TRUE;
    // GLES 3.0 includes depth textures in core; GLES 2.0 requires the OES extension.
    gCelDepthTextureExtensionAvailable =
        actualMajor >= 3 || gSHARDepthTextureOESAdvertised;
    gCelDepthExtensionChecked = true;

    SHARConfigureVertexArrayFunctions(actualMajor);
    gSHARFramebufferSupportAvailable = SHARValidateFramebufferSupport();

    if (!gSHARVertexArrayObjectsSupported)
        SDL_LogError(SDL_LOG_CATEGORY_RENDER,
            "[GRAPHICS] Vertex array objects: UNAVAILABLE (optional fullscreen post-processing will be bypassed)");

    if (!gSHARFramebufferSupportAvailable)
        SDL_LogError(SDL_LOG_CATEGORY_RENDER,
            "[GRAPHICS] Cel/bloom offscreen rendering will be bypassed");
}

// Driver limits are constant for the lifetime of a GL context. Cache them so
// repeated target checks avoid redundant glGetIntegerv calls.
static GLint gSHARCachedMaxTextureSize = 0;
static GLint gSHARCachedMaxRenderbufferSize = 0;
static int gSHARLastRejectedWidth = 0;
static int gSHARLastRejectedHeight = 0;
static int gSHARLastRejectedReason = 0;


static bool QuerySDLDrawableSize(int* width, int* height)
{
    if (width == NULL || height == NULL)
        return false;

    // Query the drawable through SDL rather than calling EGL directly. This
    // keeps the Pure3D static library linkable by Android's small SDL test
    // targets as well as the main game library.
    SDL_Window* currentWindow = SDL_GL_GetCurrentWindow();
    if (currentWindow == NULL)
        return false;

    int drawableWidth = 0;
    int drawableHeight = 0;
#if SDL_MAJOR_VERSION < 3
    SDL_GL_GetDrawableSize(currentWindow, &drawableWidth, &drawableHeight);
#else
    SDL_GetWindowSizeInPixels(currentWindow, &drawableWidth, &drawableHeight);
#endif
    if (drawableWidth <= 0 || drawableHeight <= 0)
        return false;

    *width = drawableWidth;
    *height = drawableHeight;
    return true;
}

static bool QueryEglSurfaceSize(int* width, int* height)
{
    if (width == NULL || height == NULL)
        return false;

    typedef EGLDisplay (*GetCurrentDisplayFn)(void);
    typedef EGLSurface (*GetCurrentSurfaceFn)(EGLint);
    typedef EGLBoolean (*QuerySurfaceFn)(EGLDisplay, EGLSurface, EGLint, EGLint*);

    static void* eglLibrary = dlopen("libEGL.so", RTLD_NOW);
    if (eglLibrary == NULL)
        return false;
    static GetCurrentDisplayFn getCurrentDisplay =
        reinterpret_cast<GetCurrentDisplayFn>(dlsym(eglLibrary, "eglGetCurrentDisplay"));
    static GetCurrentSurfaceFn getCurrentSurface =
        reinterpret_cast<GetCurrentSurfaceFn>(dlsym(eglLibrary, "eglGetCurrentSurface"));
    static QuerySurfaceFn querySurface =
        reinterpret_cast<QuerySurfaceFn>(dlsym(eglLibrary, "eglQuerySurface"));
    if (getCurrentDisplay == NULL || getCurrentSurface == NULL || querySurface == NULL)
        return false;

    EGLDisplay display = getCurrentDisplay();
    EGLSurface surface = getCurrentSurface(EGL_DRAW);
    if (display == EGL_NO_DISPLAY || surface == EGL_NO_SURFACE)
        return false;

    EGLint surfaceWidth = 0;
    EGLint surfaceHeight = 0;
    if (!querySurface(display, surface, EGL_WIDTH, &surfaceWidth) ||
        !querySurface(display, surface, EGL_HEIGHT, &surfaceHeight) ||
        surfaceWidth <= 0 || surfaceHeight <= 0)
    {
        return false;
    }

    *width = (int)surfaceWidth;
    *height = (int)surfaceHeight;
    return true;
}

// The EGL/SDL drawable dimensions remain stable throughout one render frame.
// Refresh once at BeginFrame and reuse the result in GUI, filter and presentation paths.
static int gSHARCachedOutputFramebufferWidth = 0;
static int gSHARCachedOutputFramebufferHeight = 0;
static bool gSHAROutputFramebufferSizeCached = false;

static void InvalidateSHARDefaultFramebufferSizeCache()
{
    gSHAROutputFramebufferSizeCached = false;
}

static bool QueryDefaultFramebufferSize(int* width, int* height)
{
    if (width == NULL || height == NULL)
        return false;

    if (!gSHAROutputFramebufferSizeCached)
    {
        int queriedWidth = 0;
        int queriedHeight = 0;
        const bool queried = QueryEglSurfaceSize(&queriedWidth, &queriedHeight) ||
            QuerySDLDrawableSize(&queriedWidth, &queriedHeight);
        if (!queried)
            return false;

        gSHARCachedOutputFramebufferWidth = queriedWidth;
        gSHARCachedOutputFramebufferHeight = queriedHeight;
        gSHAROutputFramebufferSizeCached = true;
    }

    *width = gSHARCachedOutputFramebufferWidth;
    *height = gSHARCachedOutputFramebufferHeight;
    return true;
}

static void LogSHARRenderTargetRejectedOnce(
    int width, int height, int reason, const char* message)
{
    if (gSHARLastRejectedWidth == width &&
        gSHARLastRejectedHeight == height &&
        gSHARLastRejectedReason == reason)
    {
        return;
    }

    gSHARLastRejectedWidth = width;
    gSHARLastRejectedHeight = height;
    gSHARLastRejectedReason = reason;
    SDL_LogWarn(SDL_LOG_CATEGORY_RENDER,
        "SHAR Android render target %dx%d skipped: %s",
        width, height, message ? message : "unsupported size");
}


static bool CheckSHARRenderTargetSize(
    int width, int height, const char* label,
    int reasonCode, bool needsRenderbuffer, bool checkBudget)
{
    if (width <= 0 || height <= 0)
        return false;

    if (gSHARCachedMaxTextureSize <= 0)
        glGetIntegerv(GL_MAX_TEXTURE_SIZE, &gSHARCachedMaxTextureSize);
    const GLint maxTextureSize = gSHARCachedMaxTextureSize;

    if (maxTextureSize <= 0 ||
        width > maxTextureSize || height > maxTextureSize)
    {
        char message[192];
        snprintf(message, sizeof(message),
            "%s exceeds GL_MAX_TEXTURE_SIZE=%d",
            label ? label : "target", (int)maxTextureSize);
        LogSHARRenderTargetRejectedOnce(
            width, height, reasonCode, message);
        return false;
    }

    if (needsRenderbuffer)
    {
        if (gSHARCachedMaxRenderbufferSize <= 0)
            glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &gSHARCachedMaxRenderbufferSize);
        const GLint maxRenderbufferSize = gSHARCachedMaxRenderbufferSize;
        if (maxRenderbufferSize <= 0 ||
            width > maxRenderbufferSize || height > maxRenderbufferSize)
        {
            char message[192];
            snprintf(message, sizeof(message),
                "%s exceeds GL_MAX_RENDERBUFFER_SIZE=%d",
                label ? label : "target", (int)maxRenderbufferSize);
            LogSHARRenderTargetRejectedOnce(
                width, height, reasonCode, message);
            return false;
        }
    }

    if (checkBudget)
    {
        const size_t pixelWidth = (size_t)width;
        const size_t pixelHeight = (size_t)height;
        if (pixelHeight > ((size_t)-1) / pixelWidth)
        {
            LogSHARRenderTargetRejectedOnce(
                width, height, reasonCode,
                "pixel-count calculation overflowed");
            return false;
        }

        const size_t pixels = pixelWidth * pixelHeight;
        if (pixels > ((size_t)-1) / 14)
        {
            LogSHARRenderTargetRejectedOnce(
                width, height, reasonCode,
                "post-process memory estimate overflowed");
            return false;
        }

        size_t estimatedBytes = 0;
        const bool bloomEnabled = IsSHARBloomEnabled();
        const bool sharpeningEnabled = IsFrameSharpeningEnabled();

        // The offscreen scene target (RGBA color + 16-bit depth) is used
        // regardless of Cel Shading; bloom targets are budgeted independently.
        estimatedBytes += pixels * 6;
        if (bloomEnabled)
        {
            const size_t bloomWidth = width > 3 ? (size_t)(width / 4) : 1;
            const size_t bloomHeight = height > 3 ? (size_t)(height / 4) : 1;
            estimatedBytes += bloomWidth * bloomHeight * 8;
        }

        // Count full-screen copies as four bytes per pixel to allow for drivers
        // that pad RGB allocations internally. The global saturation pass shares
        // its texture with CRT, so reserve one copy even when CRT is disabled.
        // If the budget is tight, sharpening is bypassed first.
        if (reasonCode == 5 && sharpeningEnabled)
            estimatedBytes += pixels * 4;

        if (reasonCode == 5 || reasonCode == 6)
            estimatedBytes += pixels * 4;

        if (estimatedBytes > SHAR_POSTPROCESS_BUDGET_BYTES)
        {
            char message[192];
            snprintf(message, sizeof(message),
                "%s would exceed the %u MiB auxiliary post-process budget (estimate=%u MiB)",
                label ? label : "target",
                (unsigned)(SHAR_POSTPROCESS_BUDGET_BYTES / (1024 * 1024)),
                (unsigned)(estimatedBytes / (1024 * 1024)));
            LogSHARRenderTargetRejectedOnce(
                width, height, reasonCode, message);
            return false;
        }
    }

    return true;
}

static bool CheckSHARAllocationError(
    const char* label, int width, int height)
{
    const GLenum error = glGetError();
    if (error == GL_NO_ERROR)
        return true;

    // Drain the remaining error flags after recording the first failure so
    // subsequent GL work can proceed on the direct/default framebuffer.
    GLenum pendingError = GL_NO_ERROR;
    while ((pendingError = glGetError()) != GL_NO_ERROR)
    {
    }

    SDL_LogWarn(SDL_LOG_CATEGORY_RENDER,
        "SHAR Android allocation failed for %s (%dx%d), GL error=0x%04x; bypassing that effect",
        label ? label : "post-process target",
        width, height, (unsigned)error);
    return false;
}

static GLuint CompileCelPostShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, 0);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE)
    {
        GLint length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        if (length > 0)
        {
            std::vector<char> log((size_t)length + 1, 0);
            glGetShaderInfoLog(shader, length, NULL, log.data());
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Cel post-process shader compile failed: %s", log.data());
        }
        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

static GLuint CreateCelPostProgram(const char* vertexSource, const char* fragmentSource, const char* label)
{
    GLuint vs = CompileCelPostShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fs = CompileCelPostShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (vs == 0 || fs == 0)
    {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "[GRAPHICS] %s shader compilation: FAILED",
            label ? label : "Optional post-process");
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }

    GLuint program = glCreateProgram();
    glBindAttribLocation(program, 0, "position");
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint linked = GL_FALSE;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (linked == GL_FALSE)
    {
        GLint length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        if (length > 0)
        {
            std::vector<char> log((size_t)length + 1, 0);
            glGetProgramInfoLog(program, length, NULL, log.data());
            SDL_LogError(SDL_LOG_CATEGORY_RENDER, "%s shader program link failed: %s",
                label ? label : "Cel post-process", log.data());
        }
        SDL_LogError(SDL_LOG_CATEGORY_RENDER, "[GRAPHICS] %s shader program link: FAILED",
            label ? label : "Optional post-process");
        glDeleteProgram(program);
        program = 0;
    }
    else
    {
        }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

static bool EnsureCelPostProcessResources(int width, int height);

static void InvalidateCelRenderTargetValidationCache()
{
    gCelRenderTargetValidationCached = false;
}

typedef void (*SHARDiscardFramebufferProc)(GLenum, GLsizei, const GLenum*);

static void DiscardCelDepthAttachmentIfSupported()
{
    // GLES 3 exposes glInvalidateFramebuffer. GLES 2 devices can provide the
    // equivalent GL_EXT_discard_framebuffer entry point. Both are optional:
    // skipping the hint is safe when neither entry point is available.
    static bool checked = false;
    static SHARDiscardFramebufferProc discardFramebuffer = NULL;
    if (!checked)
    {
        checked = true;
        if (gSHARActiveOpenGLESVersionMajor >= 3)
        {
            discardFramebuffer = (SHARDiscardFramebufferProc)
                SDL_GL_GetProcAddress("glInvalidateFramebuffer");
        }
        if (discardFramebuffer == NULL &&
            SDL_GL_ExtensionSupported("GL_EXT_discard_framebuffer") == SDL_TRUE)
        {
            discardFramebuffer = (SHARDiscardFramebufferProc)
                SDL_GL_GetProcAddress("glDiscardFramebufferEXT");
        }
    }

    if (discardFramebuffer != NULL)
    {
        const GLenum attachment = GL_DEPTH_ATTACHMENT;
        discardFramebuffer(GL_FRAMEBUFFER, 1, &attachment);
    }
}

static bool EnsureCelRenderTarget(int width, int height)
{
    // The budget check depends on whether bloom is enabled, so a runtime toggle
    // invalidates this cache entry even if the render dimensions are unchanged.
    const bool bloomEnabled =
        (width > 0 && height > 0) ? IsSHARBloomEnabled() : false;
    // Only effects that sample scene depth need a texture attachment. Bloom
    // uses scene colour alone and remains on the cheaper depth-renderbuffer path.
    const bool depthTextureRequested =
        width > 0 && height > 0 &&
        (IsMotionBlurEnabled() || IsSHARCloudShadowsEnabled());
    const GLuint depthObject =
        gCelDepthTextureActive ? gCelRenderDepthTexture : gCelRenderDepth;
    const bool depthStorageMatches =
        gCelDepthTextureActive
            ? (gCelRenderDepthTexture != 0 &&
               gCelDepthTextureWidth == width &&
               gCelDepthTextureHeight == height)
            : (gCelRenderDepth != 0 &&
               gCelRenderbufferWidth == width &&
               gCelRenderbufferHeight == height);

#if !defined(RAD_DEBUG)
    // Release fast path: reuse known-good attachments while the render target
    // configuration remains unchanged. Debug builds retain full validation.
    if (width > 0 && height > 0 &&
        gCelRenderTargetValidationCached &&
        gCelRenderFboReady &&
        gCelRenderTargetValidatedWidth == width &&
        gCelRenderTargetValidatedHeight == height &&
        gCelPostWidth == width && gCelPostHeight == height &&
        gCelRenderTargetValidatedBloomEnabled == bloomEnabled &&
        gCelRenderTargetValidatedDepthTextureRequested == depthTextureRequested &&
        gCelRenderTargetValidatedUsesDepthTexture == gCelDepthTextureActive &&
        gCelRenderTargetValidatedFbo == gCelRenderFbo &&
        gCelRenderTargetValidatedColourTexture == gCelPostTexture &&
        gCelRenderTargetValidatedDepthObject == depthObject &&
        depthStorageMatches &&
        !(gCelPostFailedWidth == width && gCelPostFailedHeight == height) &&
        !(gCelRenderbufferFailedWidth == width && gCelRenderbufferFailedHeight == height) &&
        !(gCelRenderFboFailedWidth == width && gCelRenderFboFailedHeight == height))
    {
        return true;
    }
#endif

    // Any non-fast-path call performs the original size, allocation, attachment
    // and framebuffer validation below before it can repopulate the cache.
    InvalidateCelRenderTargetValidationCache();

    if (!CheckSHARRenderTargetSize(
            width, height, "cel render target", 2, true, true))
    {
        gCelRenderFboReady = false;
        return false;
    }

    if ((gCelRenderbufferFailedWidth == width &&
         gCelRenderbufferFailedHeight == height) ||
        (gCelRenderFboFailedWidth == width &&
         gCelRenderFboFailedHeight == height))
    {
        gCelRenderFboReady = false;
        return false;
    }

    if (gCelRenderFbo == 0)
        glGenFramebuffers(1, &gCelRenderFbo);
    if (gCelRenderDepth == 0)
        glGenRenderbuffers(1, &gCelRenderDepth);
    if (gCelRenderFbo == 0 || gCelRenderDepth == 0)
        return false;

    if (!EnsureCelPostProcessResources(width, height))
        return false;

    if (!gCelDepthExtensionChecked)
    {
        // GLES 3.0 includes depth textures in core; GLES 2.0 requires OES_depth_texture.
        gCelDepthTextureExtensionAvailable =
            gSHARActiveOpenGLESVersionMajor >= 3 ||
            SDL_GL_ExtensionSupported("GL_OES_depth_texture") == SDL_TRUE;
        gCelDepthExtensionChecked = true;
    }

    GLint previousFramebuffer = 0;
    GLint previousTexture2D = 0;
    GLint previousRenderbuffer = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture2D);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &previousRenderbuffer);

    glBindTexture(GL_TEXTURE_2D, gCelPostTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindFramebuffer(GL_FRAMEBUFFER, gCelRenderFbo);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        gCelPostTexture,
        0
    );

    // Allocate a sampleable depth texture only while motion blur or supported-
    // level cloud shadows need it. Otherwise use ordinary depth testing with
    // the existing 16-bit renderbuffer.
    const GLenum depthTextureType = GL_UNSIGNED_INT;
    gCelDepthTextureActive = false;
    bool depthTextureReady = false;
    if (depthTextureRequested && gCelDepthTextureExtensionAvailable &&
        !gCelDepthTextureFailed)
    {
        if (gCelRenderDepthTexture == 0)
            glGenTextures(1, &gCelRenderDepthTexture);

        if (gCelRenderDepthTexture != 0)
        {
            glBindTexture(GL_TEXTURE_2D, gCelRenderDepthTexture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            bool depthStorageReady = true;
            if (gCelDepthTextureWidth != width || gCelDepthTextureHeight != height)
            {
                glTexImage2D(
                    GL_TEXTURE_2D,
                    0,
                    GL_DEPTH_COMPONENT,
                    width,
                    height,
                    0,
                    GL_DEPTH_COMPONENT,
                    depthTextureType,
                    NULL
                );
                depthStorageReady = CheckSHARAllocationError(
                    "cel depth texture", width, height);
                if (depthStorageReady)
                {
                    gCelDepthTextureWidth = width;
                    gCelDepthTextureHeight = height;
                }
                else
                {
                    gCelDepthTextureFailed = true;
                }
            }

            if (depthStorageReady)
            {
                glFramebufferTexture2D(
                    GL_FRAMEBUFFER,
                    GL_DEPTH_ATTACHMENT,
                    GL_TEXTURE_2D,
                    gCelRenderDepthTexture,
                    0
                );
                GLenum depthTextureStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
                depthTextureReady = (depthTextureStatus == GL_FRAMEBUFFER_COMPLETE);
                if (!depthTextureReady)
                {
                    SDL_LogWarn(
                        SDL_LOG_CATEGORY_RENDER,
                        "SHAR Android depth texture unsupported by framebuffer (0x%04x); using depth renderbuffer",
                        (unsigned)depthTextureStatus
                    );
                    gCelDepthTextureFailed = true;
                }
            }
        }
        else
        {
            gCelDepthTextureFailed = true;
        }
    }

    if (depthTextureReady)
    {
        gCelDepthTextureActive = true;
        gCelRenderFboReady = true;
    }
    else
    {
        bool depthRenderbufferReady = true;
        glBindRenderbuffer(GL_RENDERBUFFER, gCelRenderDepth);
        if (gCelRenderbufferWidth != width || gCelRenderbufferHeight != height)
        {
            bool depthStorageReady = false;
            const bool depth24Supported =
                gSHARActiveOpenGLESVersionMajor >= 3 ||
                SDL_GL_ExtensionSupported("GL_OES_depth24") == SDL_TRUE;

            // GLES 3.0 supports 24-bit depth renderbuffers in core. GLES 2.0
            // devices may use the same format when GL_OES_depth24 is present.
            if (depth24Supported)
            {
                glRenderbufferStorage(
                    GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
                depthStorageReady = CheckSHARAllocationError(
                    "cel 24-bit depth renderbuffer", width, height);

                if (depthStorageReady)
                {
                    glFramebufferRenderbuffer(
                        GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                        GL_RENDERBUFFER, gCelRenderDepth);
                    const GLenum depth24Status =
                        glCheckFramebufferStatus(GL_FRAMEBUFFER);
                    depthStorageReady =
                        depth24Status == GL_FRAMEBUFFER_COMPLETE;
                    if (!depthStorageReady)
                    {
                        SDL_LogWarn(
                            SDL_LOG_CATEGORY_RENDER,
                            "SHAR Android 24-bit depth framebuffer incomplete (0x%04x); retrying with 16-bit depth",
                            (unsigned)depth24Status);
                    }
                }
                else
                {
                    SDL_LogWarn(
                        SDL_LOG_CATEGORY_RENDER,
                        "SHAR Android 24-bit depth allocation failed; retrying with 16-bit depth");
                }

            }

            // Keep the universally supported GLES depth format as a safe fallback.
            if (!depthStorageReady)
            {
                glRenderbufferStorage(
                    GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, width, height);
                depthStorageReady = CheckSHARAllocationError(
                    "cel 16-bit depth renderbuffer", width, height);
            }

            depthRenderbufferReady = depthStorageReady;
            if (depthRenderbufferReady)
            {
                gCelRenderbufferWidth = width;
                gCelRenderbufferHeight = height;
                gCelRenderbufferFailedWidth = 0;
                gCelRenderbufferFailedHeight = 0;

            }
            else
            {
                gCelRenderbufferWidth = 0;
                gCelRenderbufferHeight = 0;
                gCelRenderbufferFailedWidth = width;
                gCelRenderbufferFailedHeight = height;
            }
        }

        if (depthRenderbufferReady)
        {
            glFramebufferRenderbuffer(
                GL_FRAMEBUFFER,
                GL_DEPTH_ATTACHMENT,
                GL_RENDERBUFFER,
                gCelRenderDepth
            );

            GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
            gCelRenderFboReady = (status == GL_FRAMEBUFFER_COMPLETE);
            if (!gCelRenderFboReady)
            {
                SDL_LogError(
                    SDL_LOG_CATEGORY_RENDER,
                    "SHAR Android cel render FBO incomplete: 0x%04x",
                    (unsigned)status
                );
            }
        }
        else
        {
            gCelRenderFboReady = false;
        }
    }

    // Validate the final attachment combination after all size-dependent
    // resources have been attached. Earlier branch-specific checks do not show
    // which objects are attached when scene rendering begins.
    const GLenum finalFramebufferStatus = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    gCelRenderFboReady =
        gCelRenderFboReady &&
        finalFramebufferStatus == GL_FRAMEBUFFER_COMPLETE;

    GLint colorAttachmentType = 0;
    GLint colorAttachmentName = 0;
    GLint depthAttachmentType = 0;
    GLint depthAttachmentName = 0;
    glGetFramebufferAttachmentParameteriv(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &colorAttachmentType);
    glGetFramebufferAttachmentParameteriv(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &colorAttachmentName);
    glGetFramebufferAttachmentParameteriv(
        GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &depthAttachmentType);
    glGetFramebufferAttachmentParameteriv(
        GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
        GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME, &depthAttachmentName);

    GLint maxTextureSize = 0;
    GLint maxRenderbufferSize = 0;
    GLint currentViewport[4] = {0, 0, 0, 0};
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maxRenderbufferSize);
    glGetIntegerv(GL_VIEWPORT, currentViewport);
    int onscreenWidth = 0;
    int onscreenHeight = 0;
    const bool onscreenSizeKnown =
        QueryDefaultFramebufferSize(&onscreenWidth, &onscreenHeight);

    GLenum targetError = glGetError();
    GLenum pendingTargetError = GL_NO_ERROR;
    while ((pendingTargetError = glGetError()) != GL_NO_ERROR)
    {
        if (targetError == GL_NO_ERROR)
            targetError = pendingTargetError;
    }


    if (gCelRenderFboReady)
    {
        gCelRenderFboFailedWidth = 0;
        gCelRenderFboFailedHeight = 0;
    }
    else
    {
        gCelRenderFboFailedWidth = width;
        gCelRenderFboFailedHeight = height;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)previousFramebuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, (GLuint)previousRenderbuffer);
    glBindTexture(GL_TEXTURE_2D, (GLuint)previousTexture2D);

    if (gCelRenderFboReady)
    {
        gCelRenderTargetValidationCached = true;
        gCelRenderTargetValidatedWidth = width;
        gCelRenderTargetValidatedHeight = height;
        gCelRenderTargetValidatedBloomEnabled = bloomEnabled;
        gCelRenderTargetValidatedDepthTextureRequested = depthTextureRequested;
        gCelRenderTargetValidatedUsesDepthTexture = gCelDepthTextureActive;
        gCelRenderTargetValidatedFbo = gCelRenderFbo;
        gCelRenderTargetValidatedColourTexture = gCelPostTexture;
        gCelRenderTargetValidatedDepthObject =
            gCelDepthTextureActive ? gCelRenderDepthTexture : gCelRenderDepth;
    }
    else
    {
        InvalidateCelRenderTargetValidationCache();
    }

    return gCelRenderFboReady;
}

static bool BindCelRenderTarget(int width, int height)
{
    if (!EnsureCelRenderTarget(width, height))
        return false;

    glBindFramebuffer(GL_FRAMEBUFFER, gCelRenderFbo);
    glViewport(0, 0, width, height);
    return true;
}

static bool EnsureCelPostProcessResources(int width, int height)
{
    if (width <= 0 || height <= 0)
        return false;

    if (gCelPostFailedWidth == width && gCelPostFailedHeight == height)
        return false;

    if (!CheckSHARRenderTargetSize(
            width, height, "cel color texture", 1, false, true))
        return false;

    if (gCelPostProgram == 0)
    {
        const char* vertexSource =
            "attribute vec2 position;\n"
            "varying vec2 texcoord;\n"
            "void main() {\n"
            "    texcoord = position * 0.5 + 0.5;\n"
            "    gl_Position = vec4(position, 0.0, 1.0);\n"
            "}\n";

        const char* fragmentSource =
            "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
            "precision highp float;\n"
            "#define SHAR_DEPTH_SAMPLER_PRECISION highp\n"
            "#define SHAR_CLOUD_SHADOW_EDGE_MIN 0.62\n"
            "#define SHAR_CLOUD_SHADOW_EDGE_MAX 0.78\n"
            "#else\n"
            "precision mediump float;\n"
            "#define SHAR_DEPTH_SAMPLER_PRECISION mediump\n"
            "#define SHAR_CLOUD_SHADOW_EDGE_MIN 0.62\n"
            "#define SHAR_CLOUD_SHADOW_EDGE_MAX 0.78\n"
            "#endif\n"
            "uniform sampler2D sceneTex;\n"
            "uniform sampler2D bloomTex;\n"
            "uniform float bloomStrength;\n"
            "// Depth reconstruction is precision-sensitive; qualify the sample, not only the math.\n"
            "uniform SHAR_DEPTH_SAMPLER_PRECISION sampler2D depthTex;\n"
            "uniform float depthTextureAvailable;\n"
            "uniform float cloudShadowEnabled;\n"
            "uniform float cloudShadowTime;\n"
            "uniform float cloudShadowStrengthMultiplier;\n"
            "uniform sampler2D cloudShadowMaskTex;\n"
            "uniform vec2 cloudShadowMaskTexelSize;\n"
            "uniform float cloudShadowMaskAvailable;\n"
            "uniform vec2 texelSize;\n"
            "uniform float motionBlurEnabled;\n"
            "uniform float motionBlurCameraValid;\n"
            "uniform float motionBlurTimeScale;\n"
            "uniform vec3 currentCameraPosition;\n"
            "uniform vec3 currentCameraRight;\n"
            "uniform vec3 currentCameraUp;\n"
            "uniform vec3 currentCameraForward;\n"
            "uniform vec3 previousCameraPosition;\n"
            "uniform vec3 previousCameraRight;\n"
            "uniform vec3 previousCameraUp;\n"
            "uniform vec3 previousCameraForward;\n"
            "uniform float currentTanHalfFovX;\n"
            "uniform float currentTanHalfFovY;\n"
            "uniform float previousTanHalfFovX;\n"
            "uniform float previousTanHalfFovY;\n"
            "uniform float currentNearPlane;\n"
            "uniform float currentFarPlane;\n"
            "uniform float previousNearPlane;\n"
            "uniform float characterMaskEnabled;\n"
            "uniform vec3 characterMaskPosition;\n"
            "uniform vec2 characterMaskSize;\n"
            "uniform float vehicleMaskEnabled;\n"
            "uniform vec3 vehicleMaskPosition;\n"
            "uniform vec2 vehicleMaskSize;\n"
            "uniform float vehicleMotionIntensity;\n"
            "varying vec2 texcoord;\n"
            "\n"
            "// Cheap interpolated value noise; coordinates are reconstructed in world space,\n"
            "// so the shadows move over the level instead of sliding with the camera.\n"
            "float cloudHash(vec2 p) {\n"
            "    vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));\n"
            "    p3 += dot(p3, p3.yxz + 33.33);\n"
            "    return fract((p3.x + p3.y) * p3.z);\n"
            "}\n"
            "float cloudNoise(vec2 p) {\n"
            "    vec2 cell = floor(p);\n"
            "    vec2 local = fract(p);\n"
            "    local = local * local * (3.0 - 2.0 * local);\n"
            "    float a = cloudHash(cell);\n"
            "    float b = cloudHash(cell + vec2(1.0, 0.0));\n"
            "    float c = cloudHash(cell + vec2(0.0, 1.0));\n"
            "    float d = cloudHash(cell + vec2(1.0, 1.0));\n"
            "    return mix(mix(a, b, local.x), mix(c, d, local.x), local.y);\n"
            "}\n"
            "float decodeCloudMaskDepth(vec4 packedMask) {\n"
            "    vec2 bytes = floor(packedMask.gb * 255.0 + 0.5);\n"
            "    // Decode in staged divisions so mediump fragments never need\n"
            "    // to represent a 16-bit integer-sized intermediate.\n"
            "    return bytes.x / 255.0 + (bytes.y / 255.0) / 255.0;\n"
            "}\n"
            "vec3 applyWorldCloudShadow(vec2 uv, vec3 baseColour) {\n"
            "    if (cloudShadowEnabled < 0.5 || depthTextureAvailable < 0.5) return baseColour;\n"
            "    float depthValue = texture2D(depthTex, uv).r;\n"
            "    if (depthValue >= 0.999999) return baseColour;\n"
            "    float shadowCoverage = 0.0;\n"
            "    if (cloudShadowMaskAvailable > 0.5) {\n"
            "        // Depth-aware 2x2 upsampling limits mask bleed across silhouettes.\n"
            "        vec2 samplePosition = uv / cloudShadowMaskTexelSize - vec2(0.5);\n"
            "        vec2 base = floor(samplePosition);\n"
            "        vec2 blend = fract(samplePosition);\n"
            "        vec2 minUv = cloudShadowMaskTexelSize * 0.5;\n"
            "        vec2 maxUv = vec2(1.0) - minUv;\n"
            "        vec2 uv00 = clamp((base + vec2(0.5, 0.5)) * cloudShadowMaskTexelSize, minUv, maxUv);\n"
            "        vec2 uv10 = clamp((base + vec2(1.5, 0.5)) * cloudShadowMaskTexelSize, minUv, maxUv);\n"
            "        vec2 uv01 = clamp((base + vec2(0.5, 1.5)) * cloudShadowMaskTexelSize, minUv, maxUv);\n"
            "        vec2 uv11 = clamp((base + vec2(1.5, 1.5)) * cloudShadowMaskTexelSize, minUv, maxUv);\n"
            "        vec4 m00 = texture2D(cloudShadowMaskTex, uv00);\n"
            "        vec4 m10 = texture2D(cloudShadowMaskTex, uv10);\n"
            "        vec4 m01 = texture2D(cloudShadowMaskTex, uv01);\n"
            "        vec4 m11 = texture2D(cloudShadowMaskTex, uv11);\n"
            "        float w00 = (1.0 - blend.x) * (1.0 - blend.y) * m00.a * (1.0 - smoothstep(0.002, 0.025, abs(depthValue - decodeCloudMaskDepth(m00))));\n"
            "        float w10 = blend.x * (1.0 - blend.y) * m10.a * (1.0 - smoothstep(0.002, 0.025, abs(depthValue - decodeCloudMaskDepth(m10))));\n"
            "        float w01 = (1.0 - blend.x) * blend.y * m01.a * (1.0 - smoothstep(0.002, 0.025, abs(depthValue - decodeCloudMaskDepth(m01))));\n"
            "        float w11 = blend.x * blend.y * m11.a * (1.0 - smoothstep(0.002, 0.025, abs(depthValue - decodeCloudMaskDepth(m11))));\n"
            "        float totalWeight = w00 + w10 + w01 + w11;\n"
            "        if (totalWeight > 0.0001)\n"
            "            shadowCoverage = (m00.r * w00 + m10.r * w10 + m01.r * w01 + m11.r * w11) / totalWeight;\n"
            "    } else {\n"
            "        // Retain the previous full-resolution path if the optional mask\n"
            "        // resources cannot be created on this device or at this resolution.\n"
            "        if (currentNearPlane <= 0.0 || currentFarPlane <= currentNearPlane) return baseColour;\n"
            "        float depthNdc = depthValue * 2.0 - 1.0;\n"
            "        float depthDenominator = currentFarPlane + currentNearPlane - depthNdc * (currentFarPlane - currentNearPlane);\n"
            "        float viewZ = (2.0 * currentNearPlane * currentFarPlane) / max(depthDenominator, 0.0001);\n"
            "        vec2 ndc = uv * 2.0 - 1.0;\n"
            "        vec3 viewPosition = vec3(ndc.x * viewZ * currentTanHalfFovX, ndc.y * viewZ * currentTanHalfFovY, viewZ);\n"
            "        vec3 worldPosition = currentCameraPosition + currentCameraRight * viewPosition.x + currentCameraUp * viewPosition.y + currentCameraForward * viewPosition.z;\n"
            "        vec2 cloudCoordinates = worldPosition.xz * 0.012 + cloudShadowTime * vec2(0.0750, -0.0555);\n"
            "        shadowCoverage = smoothstep(SHAR_CLOUD_SHADOW_EDGE_MIN, SHAR_CLOUD_SHADOW_EDGE_MAX, cloudNoise(cloudCoordinates));\n"
            "    }\n"
            "    return baseColour * (1.0 - shadowCoverage * 0.28 * cloudShadowStrengthMultiplier);\n"
            "}\n"
            "\n"
            "float protectMotionBlurSubject(vec2 uv, float sceneViewZ, vec3 subjectPosition, vec2 halfSize, float enabled) {\n"
            "    if (enabled < 0.5) return 0.0;\n"
            "    vec3 fromCamera = subjectPosition - currentCameraPosition;\n"
            "    float subjectZ = dot(fromCamera, currentCameraForward);\n"
            "    if (subjectZ <= currentNearPlane) return 0.0;\n"
            "    // Bound depth protection to the registered subject's projected world-space extent.\n"
            "    // This prevents foreground road/scenery inside the vehicle ellipse from being kept sharp.\n"
            "    float depthExtent = halfSize.x * sqrt(currentCameraForward.x * currentCameraForward.x + currentCameraForward.z * currentCameraForward.z) + halfSize.y * abs(currentCameraForward.y);\n"
            "    float nearDepthLimit = subjectZ - depthExtent - 0.45;\n"
            "    float farDepthLimit = subjectZ + depthExtent + 0.55;\n"
            "    float depthFadeIn = smoothstep(nearDepthLimit - 0.35, nearDepthLimit + 0.35, sceneViewZ);\n"
            "    float depthFadeOut = 1.0 - smoothstep(farDepthLimit - 0.35, farDepthLimit + 0.35, sceneViewZ);\n"
            "    float depthProtection = depthFadeIn * depthFadeOut;\n"
            "    vec2 subjectNdc = vec2(dot(fromCamera, currentCameraRight) / (subjectZ * currentTanHalfFovX), dot(fromCamera, currentCameraUp) / (subjectZ * currentTanHalfFovY));\n"
            "    vec2 subjectUv = subjectNdc * 0.5 + 0.5;\n"
            "    float halfWidthInCameraRight = sqrt(halfSize.x * halfSize.x * (currentCameraRight.x * currentCameraRight.x + currentCameraRight.z * currentCameraRight.z) + halfSize.y * halfSize.y * currentCameraRight.y * currentCameraRight.y);\n"
            "    float halfHeightInCameraUp = sqrt(halfSize.x * halfSize.x * (currentCameraUp.x * currentCameraUp.x + currentCameraUp.z * currentCameraUp.z) + halfSize.y * halfSize.y * currentCameraUp.y * currentCameraUp.y);\n"
            "    vec2 subjectRadius = vec2(halfWidthInCameraRight / (subjectZ * currentTanHalfFovX), halfHeightInCameraUp / (subjectZ * currentTanHalfFovY)) * 0.5;\n"
            "    subjectRadius = max(subjectRadius, texelSize * 3.0);\n"
            "    // Pad the projected bounds slightly so the outline and extremities stay sharp too.\n"
            "    subjectRadius += texelSize * 5.0;\n"
            "    float ellipseDistance = length((uv - subjectUv) / subjectRadius);\n"
            "    float projectedMask = 1.0 - smoothstep(0.90, 1.30, ellipseDistance);\n"
            "    return projectedMask * depthProtection;\n"
            "}\n"
            "\n"
            "float rawMotionBlurSubjectMask(vec2 uv, vec3 subjectPosition, vec2 halfSize, float enabled) {\n"
            "    if (enabled < 0.5) return 0.0;\n"
            "    vec3 fromCamera = subjectPosition - currentCameraPosition;\n"
            "    float subjectZ = dot(fromCamera, currentCameraForward);\n"
            "    if (subjectZ <= currentNearPlane) return 0.0;\n"
            "    vec2 subjectNdc = vec2(dot(fromCamera, currentCameraRight) / (subjectZ * currentTanHalfFovX), dot(fromCamera, currentCameraUp) / (subjectZ * currentTanHalfFovY));\n"
            "    vec2 subjectUv = subjectNdc * 0.5 + 0.5;\n"
            "    float halfWidthInCameraRight = sqrt(halfSize.x * halfSize.x * (currentCameraRight.x * currentCameraRight.x + currentCameraRight.z * currentCameraRight.z) + halfSize.y * halfSize.y * currentCameraRight.y * currentCameraRight.y);\n"
            "    float halfHeightInCameraUp = sqrt(halfSize.x * halfSize.x * (currentCameraUp.x * currentCameraUp.x + currentCameraUp.z * currentCameraUp.z) + halfSize.y * halfSize.y * currentCameraUp.y * currentCameraUp.y);\n"
            "    vec2 subjectRadius = vec2(halfWidthInCameraRight / (subjectZ * currentTanHalfFovX), halfHeightInCameraUp / (subjectZ * currentTanHalfFovY)) * 0.5;\n"
            "    subjectRadius = max(subjectRadius, texelSize * 3.0);\n"
            "    subjectRadius += texelSize * 5.0;\n"
            "    float ellipseDistance = length((uv - subjectUv) / subjectRadius);\n"
            "    return 1.0 - smoothstep(0.90, 1.30, ellipseDistance);\n"
            "}\n"
            "float vehicleRoadTrailMask(vec2 uv) {\n"
            "    if (vehicleMaskEnabled < 0.5 || vehicleMotionIntensity < 0.01) return 0.0;\n"
            "    vec2 vehicleSize = vehicleMaskSize * vec2(1.20, 1.15);\n"
            "    vec3 fromCamera = vehicleMaskPosition - currentCameraPosition;\n"
            "    float vehicleZ = dot(fromCamera, currentCameraForward);\n"
            "    if (vehicleZ <= currentNearPlane) return 0.0;\n"
            "    vec2 vehicleNdc = vec2(dot(fromCamera, currentCameraRight) / (vehicleZ * currentTanHalfFovX), dot(fromCamera, currentCameraUp) / (vehicleZ * currentTanHalfFovY));\n"
            "    vec2 vehicleUv = vehicleNdc * 0.5 + 0.5;\n"
            "    float halfWidthInCameraRight = sqrt(vehicleSize.x * vehicleSize.x * (currentCameraRight.x * currentCameraRight.x + currentCameraRight.z * currentCameraRight.z) + vehicleSize.y * vehicleSize.y * currentCameraRight.y * currentCameraRight.y);\n"
            "    float halfHeightInCameraUp = sqrt(vehicleSize.x * vehicleSize.x * (currentCameraUp.x * currentCameraUp.x + currentCameraUp.z * currentCameraUp.z) + vehicleSize.y * vehicleSize.y * currentCameraUp.y * currentCameraUp.y);\n"
            "    vec2 vehicleRadius = vec2(halfWidthInCameraRight / (vehicleZ * currentTanHalfFovX), halfHeightInCameraUp / (vehicleZ * currentTanHalfFovY)) * 0.5;\n"
            "    vehicleRadius = max(vehicleRadius, texelSize * 3.0) + texelSize * 5.0;\n"
            "    // Trail starts at the rear of the car, widens toward both lower screen edges,\n"
            "    // and softly loses strength across the fan instead of using a flat-edged band.\n"
            "    float distanceBelowCar = vehicleUv.y - uv.y;\n"
            "    float trailStart = vehicleRadius.y * 2.25;\n"
            "    float trailLength = max(vehicleUv.y - trailStart, texelSize.y * 12.0);\n"
            "    float trailProgress = clamp((distanceBelowCar - trailStart) / trailLength, 0.0, 1.0);\n"
            "    float verticalFade = smoothstep(trailStart - texelSize.y * 5.0, trailStart + texelSize.y * 5.0, distanceBelowCar);\n"
            "    float curvedProgress = smoothstep(0.0, 0.95, trailProgress);\n"
            "    float baseSideWidth = max(vehicleRadius.x * 0.72, texelSize.x * 6.0);\n"
            "    float leftWidth = mix(baseSideWidth, max(vehicleUv.x, texelSize.x * 4.0), curvedProgress);\n"
            "    float rightWidth = mix(baseSideWidth, max(1.0 - vehicleUv.x, texelSize.x * 4.0), curvedProgress);\n"
            "    float sideDistance = uv.x < vehicleUv.x ? (vehicleUv.x - uv.x) / leftWidth : (uv.x - vehicleUv.x) / rightWidth;\n"
            "    float sideFalloff = 1.0 - smoothstep(0.0, 1.65, sideDistance);\n"
            "    float lowerScreenFade = 1.0 - 0.12 * smoothstep(0.0, 1.0, trailProgress);\n"
            "    float trailShape = verticalFade * sideFalloff * lowerScreenFade;\n"
            "    float vehicleBodyMask = rawMotionBlurSubjectMask(uv, vehicleMaskPosition, vehicleSize, vehicleMaskEnabled);\n"
            "    return trailShape * (1.0 - vehicleBodyMask);\n"
            "}\n"
            "\n"            "vec3 applyVehicleRoadTrail(vec2 uv, vec3 baseColour) {\n"
            "    if (vehicleMaskEnabled < 0.5 || vehicleMotionIntensity < 0.01) return baseColour;\n"
            "    vec3 fromCamera = vehicleMaskPosition - currentCameraPosition;\n"
            "    float vehicleZ = dot(fromCamera, currentCameraForward);\n"
            "    if (vehicleZ <= currentNearPlane) return baseColour;\n"
            "    vec2 vehicleNdc = vec2(dot(fromCamera, currentCameraRight) / (vehicleZ * currentTanHalfFovX), dot(fromCamera, currentCameraUp) / (vehicleZ * currentTanHalfFovY));\n"
            "    vec2 vehicleUv = vehicleNdc * 0.5 + 0.5;\n"
            "    vec2 towardVehiclePixels = (vehicleUv - uv) / texelSize;\n"
            "    float towardVehicleLength = max(length(towardVehiclePixels), 1.0);\n"
            "    vec2 trailStep = (towardVehiclePixels / towardVehicleLength) * texelSize * mix(10.0, 34.0, vehicleMotionIntensity);\n"
            "    float celAwareTrailStrength = 0.90;\n"
            "    float roadTrailWeight = vehicleRoadTrailMask(uv) * vehicleMotionIntensity * celAwareTrailStrength;\n"
            "    if (roadTrailWeight <= 0.001) return baseColour;\n"
            "    // Sample back toward the car along the fan, creating a visible directional road streak.\n"
            "    vec3 roadTrailColour = baseColour * 0.28;\n"
            "    roadTrailColour += texture2D(sceneTex, uv + trailStep * 0.25).rgb * 0.20;\n"
            "    roadTrailColour += texture2D(sceneTex, uv + trailStep * 0.50).rgb * 0.20;\n"
            "    roadTrailColour += texture2D(sceneTex, uv + trailStep * 0.75).rgb * 0.18;\n"
            "    roadTrailColour += texture2D(sceneTex, uv + trailStep).rgb * 0.14;\n"
            "    return mix(baseColour, roadTrailColour, roadTrailWeight);\n"
            "}\n"
            "\n"
            "vec3 applyCameraMotionBlur(vec2 uv, vec3 centreColour) {\n"
            "    if (motionBlurEnabled < 0.5 || depthTextureAvailable < 0.5) return centreColour;\n"
            "    // The vehicle subject is only registered while the player is driving.\n"
            "    if (vehicleMaskEnabled < 0.5) return centreColour;\n"
            "    float depthValue = texture2D(depthTex, uv).r;\n"
            "    if (depthValue >= 0.9995) return centreColour;\n"
            "    if (motionBlurCameraValid < 0.5) return applyVehicleRoadTrail(uv, centreColour);\n"
            "    float depthNdc = depthValue * 2.0 - 1.0;\n"
            "    float depthDenominator = currentFarPlane + currentNearPlane - depthNdc * (currentFarPlane - currentNearPlane);\n"
            "    float viewZ = (2.0 * currentNearPlane * currentFarPlane) / max(depthDenominator, 0.0001);\n"
            "    vec2 ndc = uv * 2.0 - 1.0;\n"
            "    vec3 currentViewPosition = vec3(ndc.x * viewZ * currentTanHalfFovX, ndc.y * viewZ * currentTanHalfFovY, viewZ);\n"
            "    vec3 worldPosition = currentCameraPosition + currentCameraRight * currentViewPosition.x + currentCameraUp * currentViewPosition.y + currentCameraForward * currentViewPosition.z;\n"
            "    vec3 fromPreviousCamera = worldPosition - previousCameraPosition;\n"
            "    float previousZ = dot(fromPreviousCamera, previousCameraForward);\n"
            "    if (previousZ <= previousNearPlane) return applyVehicleRoadTrail(uv, centreColour);\n"
            "    vec2 previousNdc = vec2(dot(fromPreviousCamera, previousCameraRight) / (previousZ * previousTanHalfFovX), dot(fromPreviousCamera, previousCameraUp) / (previousZ * previousTanHalfFovY));\n"
            "    vec2 previousUv = previousNdc * 0.5 + 0.5;\n"
            "    if (previousUv.x < -0.12 || previousUv.x > 1.12 || previousUv.y < -0.12 || previousUv.y > 1.12) return applyVehicleRoadTrail(uv, centreColour);\n"
            "    vec2 motion = uv - previousUv;\n"
            "    float motionPixels = length(motion / texelSize) * motionBlurTimeScale;\n"
            "    float edgeDistance = min(min(uv.x, 1.0 - uv.x), min(uv.y, 1.0 - uv.y));\n"
            "    float edgeProximity = 1.0 - smoothstep(0.05, 0.30, edgeDistance);\n"
            "    float edgeBlurBoost = 1.0 + 0.80 * edgeProximity;\n"
            "    float motionAmount = smoothstep(0.10, 0.90, motionPixels);\n"
            "    if (motionAmount <= 0.001 && vehicleMotionIntensity <= 0.01) return centreColour;\n"
            "    vec2 maxOffset = texelSize * mix(12.0, 20.0, edgeProximity);\n"
            "    vec2 blurOffset = clamp(motion * motionBlurTimeScale * edgeBlurBoost, -maxOffset, maxOffset);\n"
            "    vec3 blurred = centreColour * 0.40;\n"
            "    // Bias the same blur weight toward the previous screen position so passing scenery gets a visible directional trail.\n"
            "    blurred += texture2D(sceneTex, clamp(uv - blurOffset * 0.25, texelSize * 0.5, vec2(1.0) - texelSize * 0.5)).rgb * 0.20;\n"
            "    blurred += texture2D(sceneTex, clamp(uv - blurOffset * 0.50, texelSize * 0.5, vec2(1.0) - texelSize * 0.5)).rgb * 0.20;\n"
            "    blurred += texture2D(sceneTex, clamp(uv - blurOffset * 0.75, texelSize * 0.5, vec2(1.0) - texelSize * 0.5)).rgb * 0.10;\n"
            "    blurred += texture2D(sceneTex, clamp(uv - blurOffset, texelSize * 0.5, vec2(1.0) - texelSize * 0.5)).rgb * 0.10;\n"
            "    // Character materials do not consistently contribute depth, so use the projected player bounds directly.\n"
            "    float characterProtection = rawMotionBlurSubjectMask(uv, characterMaskPosition, characterMaskSize, characterMaskEnabled);\n"
            "    // Add a small horizontal allowance so the tail stays mostly protected without greatly widening the mask.\n"
            "    float vehicleProtection = protectMotionBlurSubject(uv, viewZ, vehicleMaskPosition, vehicleMaskSize * vec2(1.20, 1.15), vehicleMaskEnabled);\n"
            "    float subjectProtection = max(characterProtection, vehicleProtection);\n"
            "    // Fade camera-panning blur out before the screen midpoint; the road-trail fan is separate.\n"
            "    float lowerScreenMotionBlur = 1.0 - smoothstep(0.36, 0.50, uv.y);\n"

            "    vec3 baseColour = mix(centreColour, blurred, clamp(motionAmount * edgeBlurBoost * lowerScreenMotionBlur, 0.0, 1.0) * (1.0 - subjectProtection));\n"
            "    return applyVehicleRoadTrail(uv, baseColour);\n"
            "}\n"
            "\n"
            "void main() {\n"
            "    vec4 scene = texture2D(sceneTex, texcoord);\n"
            "    float bloomCharacterProtection = smoothstep(0.0, 0.5, scene.a);\n"
            "    scene.rgb = applyCameraMotionBlur(texcoord, scene.rgb);\n"
            "    vec3 finalColour = scene.rgb;\n"
                        "    // Optional effects are independent of object-based cel shading.\n"
            "    if (bloomStrength > 0.0)\n"
            "        finalColour += texture2D(bloomTex, texcoord).rgb * bloomStrength * (1.0 - bloomCharacterProtection);\n"
            "    if (cloudShadowEnabled > 0.5)\n"
            "        finalColour = applyWorldCloudShadow(texcoord, finalColour);\n"
            "    gl_FragColor = vec4(clamp(finalColour, 0.0, 1.0), 1.0);\n"
            "}\n";

        GLuint vs = CompileCelPostShader(GL_VERTEX_SHADER, vertexSource);
        GLuint fs = CompileCelPostShader(GL_FRAGMENT_SHADER, fragmentSource);
        if (vs == 0 || fs == 0)
        {
            SDL_LogError(SDL_LOG_CATEGORY_RENDER, "[GRAPHICS] Cel shader compilation: FAILED");
            if (vs) glDeleteShader(vs);
            if (fs) glDeleteShader(fs);
            gCelPostFailedWidth = width;
            gCelPostFailedHeight = height;
            return false;
        }

        gCelPostProgram = glCreateProgram();
        glBindAttribLocation(gCelPostProgram, 0, "position");
        glAttachShader(gCelPostProgram, vs);
        glAttachShader(gCelPostProgram, fs);
        glLinkProgram(gCelPostProgram);

        GLint linked = GL_FALSE;
        glGetProgramiv(gCelPostProgram, GL_LINK_STATUS, &linked);
        if (linked == GL_FALSE)
        {
            GLint length = 0;
            glGetProgramiv(gCelPostProgram, GL_INFO_LOG_LENGTH, &length);
            if (length > 0)
            {
                std::vector<char> log((size_t)length + 1, 0);
                glGetProgramInfoLog(gCelPostProgram, length, NULL, log.data());
                SDL_LogError(SDL_LOG_CATEGORY_RENDER, "Cel post-process program link failed: %s", log.data());
            }

            SDL_LogError(SDL_LOG_CATEGORY_RENDER, "[GRAPHICS] Cel shader program link: FAILED");
            glDeleteProgram(gCelPostProgram);
            gCelPostProgram = 0;
            glDeleteShader(vs);
            glDeleteShader(fs);
            gCelPostFailedWidth = width;
            gCelPostFailedHeight = height;
            return false;
        }

            glDeleteShader(vs);
        glDeleteShader(fs);

        gCelPostSceneLocation = glGetUniformLocation(gCelPostProgram, "sceneTex");
        gCelPostTexelLocation = glGetUniformLocation(gCelPostProgram, "texelSize");
        gCelPostBloomLocation = glGetUniformLocation(gCelPostProgram, "bloomTex");
        gCelPostBloomStrengthLocation = glGetUniformLocation(gCelPostProgram, "bloomStrength");
        gCelPostDepthLocation = glGetUniformLocation(gCelPostProgram, "depthTex");
        gCelPostDepthAvailableLocation = glGetUniformLocation(gCelPostProgram, "depthTextureAvailable");
        gCelPostCloudShadowEnabledLocation = glGetUniformLocation(gCelPostProgram, "cloudShadowEnabled");
        gCelPostCloudShadowTimeLocation = glGetUniformLocation(gCelPostProgram, "cloudShadowTime");
        gCelPostCloudShadowStrengthLocation = glGetUniformLocation(gCelPostProgram, "cloudShadowStrengthMultiplier");
        gCelPostCloudShadowMaskLocation = glGetUniformLocation(gCelPostProgram, "cloudShadowMaskTex");
        gCelPostCloudShadowMaskTexelSizeLocation = glGetUniformLocation(gCelPostProgram, "cloudShadowMaskTexelSize");
        gCelPostCloudShadowMaskAvailableLocation = glGetUniformLocation(gCelPostProgram, "cloudShadowMaskAvailable");
        gCelPostMotionBlurEnabledLocation = glGetUniformLocation(gCelPostProgram, "motionBlurEnabled");
        gCelPostMotionBlurValidLocation = glGetUniformLocation(gCelPostProgram, "motionBlurCameraValid");
        gCelPostMotionBlurScaleLocation = glGetUniformLocation(gCelPostProgram, "motionBlurTimeScale");
        gCelPostCurrentCameraPositionLocation = glGetUniformLocation(gCelPostProgram, "currentCameraPosition");
        gCelPostCurrentCameraRightLocation = glGetUniformLocation(gCelPostProgram, "currentCameraRight");
        gCelPostCurrentCameraUpLocation = glGetUniformLocation(gCelPostProgram, "currentCameraUp");
        gCelPostCurrentCameraForwardLocation = glGetUniformLocation(gCelPostProgram, "currentCameraForward");
        gCelPostPreviousCameraPositionLocation = glGetUniformLocation(gCelPostProgram, "previousCameraPosition");
        gCelPostPreviousCameraRightLocation = glGetUniformLocation(gCelPostProgram, "previousCameraRight");
        gCelPostPreviousCameraUpLocation = glGetUniformLocation(gCelPostProgram, "previousCameraUp");
        gCelPostPreviousCameraForwardLocation = glGetUniformLocation(gCelPostProgram, "previousCameraForward");
        gCelPostCurrentTanHalfFovXLocation = glGetUniformLocation(gCelPostProgram, "currentTanHalfFovX");
        gCelPostCurrentTanHalfFovYLocation = glGetUniformLocation(gCelPostProgram, "currentTanHalfFovY");
        gCelPostPreviousTanHalfFovXLocation = glGetUniformLocation(gCelPostProgram, "previousTanHalfFovX");
        gCelPostPreviousTanHalfFovYLocation = glGetUniformLocation(gCelPostProgram, "previousTanHalfFovY");
        gCelPostCurrentNearPlaneLocation = glGetUniformLocation(gCelPostProgram, "currentNearPlane");
        gCelPostCurrentFarPlaneLocation = glGetUniformLocation(gCelPostProgram, "currentFarPlane");
        gCelPostPreviousNearPlaneLocation = glGetUniformLocation(gCelPostProgram, "previousNearPlane");
        gCelPostCharacterMaskEnabledLocation = glGetUniformLocation(gCelPostProgram, "characterMaskEnabled");
        gCelPostCharacterMaskPositionLocation = glGetUniformLocation(gCelPostProgram, "characterMaskPosition");
        gCelPostCharacterMaskSizeLocation = glGetUniformLocation(gCelPostProgram, "characterMaskSize");
        gCelPostVehicleMaskEnabledLocation = glGetUniformLocation(gCelPostProgram, "vehicleMaskEnabled");
        gCelPostVehicleMaskPositionLocation = glGetUniformLocation(gCelPostProgram, "vehicleMaskPosition");
        gCelPostVehicleMaskSizeLocation = glGetUniformLocation(gCelPostProgram, "vehicleMaskSize");
        gCelPostVehicleMotionIntensityLocation = glGetUniformLocation(gCelPostProgram, "vehicleMotionIntensity");


        glGenTextures(1, &gCelPostTexture);

        static const GLfloat quad[] =
        {
            -1.0f, -1.0f,
             1.0f, -1.0f,
            -1.0f,  1.0f,
             1.0f,  1.0f
        };
        glGenBuffers(1, &gCelPostVbo);
        glBindBuffer(GL_ARRAY_BUFFER, gCelPostVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    if (!gCelBloomOnlyPostProgramAttempted)
    {
        gCelBloomOnlyPostProgramAttempted = true;
        const char* bloomOnlyVertexSource =
            "attribute vec2 position;\n"
            "varying vec2 texcoord;\n"
            "void main() { texcoord = position * 0.5 + 0.5; gl_Position = vec4(position, 0.0, 1.0); }\n";
        const char* bloomOnlyFragmentSource =
            "precision mediump float;\n"
            "uniform sampler2D sceneTex;\n"
            "uniform sampler2D bloomTex;\n"
            "uniform float bloomStrength;\n"
            "varying vec2 texcoord;\n"
            "void main() {\n"
            "    vec4 scene = texture2D(sceneTex, texcoord);\n"
            "    float characterProtection = smoothstep(0.0, 0.5, scene.a);\n"
            "    vec3 finalColour = scene.rgb;\n"
            "    if (bloomStrength > 0.0)\n"
            "        finalColour += texture2D(bloomTex, texcoord).rgb * bloomStrength * (1.0 - characterProtection);\n"
            "    gl_FragColor = vec4(clamp(finalColour, 0.0, 1.0), 1.0);\n"
            "}\n";
        gCelBloomOnlyPostProgram = CreateCelPostProgram(
            bloomOnlyVertexSource, bloomOnlyFragmentSource,
            "Bloom-only lightweight composite");
        if (gCelBloomOnlyPostProgram != 0)
        {
            gCelBloomOnlySceneLocation = glGetUniformLocation(gCelBloomOnlyPostProgram, "sceneTex");
            gCelBloomOnlyTextureLocation = glGetUniformLocation(gCelBloomOnlyPostProgram, "bloomTex");
            gCelBloomOnlyStrengthLocation = glGetUniformLocation(gCelBloomOnlyPostProgram, "bloomStrength");
        }
        else
        {
            SDL_LogWarn(SDL_LOG_CATEGORY_RENDER,
                "SHAR Bloom-only shader unavailable; using the full composite shader");
        }
    }

    if (gCelPostWidth != width || gCelPostHeight != height)
    {
        GLint previousTexture = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
        glBindTexture(GL_TEXTURE_2D, gCelPostTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA,
            width,
            height,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            NULL
        );

        if (!CheckSHARAllocationError("cel color texture", width, height))
        {
            gCelPostFailedWidth = width;
            gCelPostFailedHeight = height;
            glBindTexture(GL_TEXTURE_2D, (GLuint)previousTexture);
            return false;
        }

        gCelPostWidth = width;
        gCelPostHeight = height;
        gCelPostFailedWidth = 0;
        gCelPostFailedHeight = 0;
        glBindTexture(GL_TEXTURE_2D, (GLuint)previousTexture);
    }

    return gCelPostProgram != 0 && gCelPostTexture != 0;
}

static bool EnsureCelCloudShadowResources(int width, int height)
{
    if (width <= 0 || height <= 0)
        return false;

    // Half width/height requires one quarter of the full-resolution mask work.
    const int maskWidth = width > 1 ? (width + 1) / 2 : 1;
    const int maskHeight = height > 1 ? (height + 1) / 2 : 1;
    if (gCelCloudShadowFailedWidth == maskWidth &&
        gCelCloudShadowFailedHeight == maskHeight)
        return false;

    if (!CheckSHARRenderTargetSize(
            maskWidth, maskHeight, "cloud shadow mask", 1, false, false))
        return false;

    if (gCelCloudShadowProgram == 0)
    {
        const char* vertexSource =
            "attribute vec2 position;\n"
            "varying vec2 texcoord;\n"
            "void main() { texcoord = position * 0.5 + 0.5; gl_Position = vec4(position, 0.0, 1.0); }\n";
        const char* fragmentSource =
            "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
            "precision highp float;\n"
            "#define SHAR_DEPTH_SAMPLER_PRECISION highp\n"
            "#define SHAR_CLOUD_SHADOW_EDGE_MIN 0.62\n"
            "#define SHAR_CLOUD_SHADOW_EDGE_MAX 0.78\n"
            "#else\n"
            "precision mediump float;\n"
            "#define SHAR_DEPTH_SAMPLER_PRECISION mediump\n"
            "#define SHAR_CLOUD_SHADOW_EDGE_MIN 0.62\n"
            "#define SHAR_CLOUD_SHADOW_EDGE_MAX 0.78\n"
            "#endif\n"
            "// Depth reconstruction is precision-sensitive; qualify the sample, not only the math.\n"
            "uniform SHAR_DEPTH_SAMPLER_PRECISION sampler2D depthTex;\n"
            "uniform float cloudShadowTime;\n"
            "uniform vec3 currentCameraPosition;\n"
            "uniform vec3 currentCameraRight;\n"
            "uniform vec3 currentCameraUp;\n"
            "uniform vec3 currentCameraForward;\n"
            "uniform float currentTanHalfFovX;\n"
            "uniform float currentTanHalfFovY;\n"
            "uniform float currentNearPlane;\n"
            "uniform float currentFarPlane;\n"
            "varying vec2 texcoord;\n"
            "float cloudHash(vec2 p) {\n"
            "    vec3 p3 = fract(vec3(p.xyx) * vec3(0.1031, 0.1030, 0.0973));\n"
            "    p3 += dot(p3, p3.yxz + 33.33);\n"
            "    return fract((p3.x + p3.y) * p3.z);\n"
            "}\n"
            "float cloudNoise(vec2 p) {\n"
            "    vec2 cell = floor(p);\n"
            "    vec2 local = fract(p);\n"
            "    local = local * local * (3.0 - 2.0 * local);\n"
            "    float a = cloudHash(cell);\n"
            "    float b = cloudHash(cell + vec2(1.0, 0.0));\n"
            "    float c = cloudHash(cell + vec2(0.0, 1.0));\n"
            "    float d = cloudHash(cell + vec2(1.0, 1.0));\n"
            "    return mix(mix(a, b, local.x), mix(c, d, local.x), local.y);\n"
            "}\n"
            "void main() {\n"
            "    float depthValue = texture2D(depthTex, texcoord).r;\n"
            "    if (depthValue >= 0.999999 || currentNearPlane <= 0.0 || currentFarPlane <= currentNearPlane) {\n"
            "        gl_FragColor = vec4(0.0, 1.0, 1.0, 0.0);\n"
            "        return;\n"
            "    }\n"
            "    float depthNdc = depthValue * 2.0 - 1.0;\n"
            "    float depthDenominator = currentFarPlane + currentNearPlane - depthNdc * (currentFarPlane - currentNearPlane);\n"
            "    float viewZ = (2.0 * currentNearPlane * currentFarPlane) / max(depthDenominator, 0.0001);\n"
            "    vec2 ndc = texcoord * 2.0 - 1.0;\n"
            "    vec3 viewPosition = vec3(ndc.x * viewZ * currentTanHalfFovX, ndc.y * viewZ * currentTanHalfFovY, viewZ);\n"
            "    vec3 worldPosition = currentCameraPosition + currentCameraRight * viewPosition.x + currentCameraUp * viewPosition.y + currentCameraForward * viewPosition.z;\n"
            "    vec2 cloudCoordinates = worldPosition.xz * 0.012 + cloudShadowTime * vec2(0.0750, -0.0555);\n"
            "    float shadowCoverage = smoothstep(SHAR_CLOUD_SHADOW_EDGE_MIN, SHAR_CLOUD_SHADOW_EDGE_MAX, cloudNoise(cloudCoordinates));\n"
            "    // Base-255 byte packing keeps all intermediates in mediump range.\n"
            "    float depthScaled = clamp(depthValue, 0.0, 1.0) * 255.0;\n"
            "    float depthHigh = floor(depthScaled);\n"
            "    float depthLow = floor((depthScaled - depthHigh) * 255.0 + 0.5);\n"
            "    gl_FragColor = vec4(shadowCoverage, depthHigh / 255.0, depthLow / 255.0, 1.0);\n"
            "}\n";
        gCelCloudShadowProgram = CreateCelPostProgram(
            vertexSource, fragmentSource, "Half-resolution cloud-shadow mask");
        if (gCelCloudShadowProgram == 0)
        {
            gCelCloudShadowFailedWidth = maskWidth;
            gCelCloudShadowFailedHeight = maskHeight;
            return false;
        }

        gCelCloudMaskDepthLocation = glGetUniformLocation(gCelCloudShadowProgram, "depthTex");
        gCelCloudMaskTimeLocation = glGetUniformLocation(gCelCloudShadowProgram, "cloudShadowTime");
        gCelCloudMaskCameraPositionLocation = glGetUniformLocation(gCelCloudShadowProgram, "currentCameraPosition");
        gCelCloudMaskCameraRightLocation = glGetUniformLocation(gCelCloudShadowProgram, "currentCameraRight");
        gCelCloudMaskCameraUpLocation = glGetUniformLocation(gCelCloudShadowProgram, "currentCameraUp");
        gCelCloudMaskCameraForwardLocation = glGetUniformLocation(gCelCloudShadowProgram, "currentCameraForward");
        gCelCloudMaskTanHalfFovXLocation = glGetUniformLocation(gCelCloudShadowProgram, "currentTanHalfFovX");
        gCelCloudMaskTanHalfFovYLocation = glGetUniformLocation(gCelCloudShadowProgram, "currentTanHalfFovY");
        gCelCloudMaskNearPlaneLocation = glGetUniformLocation(gCelCloudShadowProgram, "currentNearPlane");
        gCelCloudMaskFarPlaneLocation = glGetUniformLocation(gCelCloudShadowProgram, "currentFarPlane");
    }

    if (gCelCloudShadowFbo == 0)
        glGenFramebuffers(1, &gCelCloudShadowFbo);
    if (gCelCloudShadowTexture == 0)
        glGenTextures(1, &gCelCloudShadowTexture);
    if (gCelCloudShadowFbo == 0 || gCelCloudShadowTexture == 0)
    {
        gCelCloudShadowFailedWidth = maskWidth;
        gCelCloudShadowFailedHeight = maskHeight;
        return false;
    }

    GLint previousFramebuffer = 0;
    GLint previousActiveTexture = GL_TEXTURE0;
    GLint previousTexture = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);

    bool textureReady = true;
    glBindTexture(GL_TEXTURE_2D, gCelCloudShadowTexture);
    if (gCelCloudShadowWidth != maskWidth || gCelCloudShadowHeight != maskHeight)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, maskWidth, maskHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        textureReady = CheckSHARAllocationError(
            "cloud shadow mask texture", maskWidth, maskHeight);
        if (textureReady)
        {
            gCelCloudShadowWidth = maskWidth;
            gCelCloudShadowHeight = maskHeight;
            gCelCloudShadowFailedWidth = 0;
            gCelCloudShadowFailedHeight = 0;
        }
    }

    if (textureReady)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, gCelCloudShadowFbo);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
            gCelCloudShadowTexture, 0);
        const GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        gCelCloudShadowReady = status == GL_FRAMEBUFFER_COMPLETE;
        if (!gCelCloudShadowReady)
        {
            gCelCloudShadowFailedWidth = maskWidth;
            gCelCloudShadowFailedHeight = maskHeight;
        }
    }
    else
    {
        gCelCloudShadowFailedWidth = maskWidth;
        gCelCloudShadowFailedHeight = maskHeight;
        gCelCloudShadowReady = false;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)previousFramebuffer);
    glBindTexture(GL_TEXTURE_2D, (GLuint)previousTexture);
    glActiveTexture((GLenum)previousActiveTexture);
    return gCelCloudShadowReady;
}

static bool EnsureCelBloomResources(int width, int height)
{
    if (width <= 0 || height <= 0)
        return false;

    const int bloomWidth = width > 3 ? width / 4 : 1;
    const int bloomHeight = height > 3 ? height / 4 : 1;

    if (!CheckSHARRenderTargetSize(
            bloomWidth, bloomHeight, "cel bloom textures", 4, false, false))
        return false;

    if (gCelBloomReady && gCelBloomWidth == bloomWidth && gCelBloomHeight == bloomHeight)
        return true;
    if (gCelBloomFailedWidth == bloomWidth && gCelBloomFailedHeight == bloomHeight)
        return false;

    static const char* vertexSource =
        "attribute vec2 position;\n"
        "varying vec2 texcoord;\n"
        "void main() {\n"
        "    texcoord = position * 0.5 + 0.5;\n"
        "    gl_Position = vec4(position, 0.0, 1.0);\n"
        "}\n";

    if (gCelBloomExtractProgram == 0)
    {
        const char* extractFragment =
            "precision mediump float;\n"
            "uniform sampler2D sceneTex;\n"
            "uniform vec2 sourceTexelSize;\n"
            "varying vec2 texcoord;\n"
            "float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }\n"
            "void main() {\n"
            "    vec2 d = sourceTexelSize * 2.0;\n"
            "    vec4 s0 = texture2D(sceneTex, texcoord + vec2(-d.x, -d.y));\n"
            "    vec4 s1 = texture2D(sceneTex, texcoord + vec2( d.x, -d.y));\n"
            "    vec4 s2 = texture2D(sceneTex, texcoord + vec2(-d.x,  d.y));\n"
            "    vec4 s3 = texture2D(sceneTex, texcoord + vec2( d.x,  d.y));\n"
            "    float m0 = smoothstep(0.0, 0.5, s0.a);\n"
            "    float m1 = smoothstep(0.0, 0.5, s1.a);\n"
            "    float m2 = smoothstep(0.0, 0.5, s2.a);\n"
            "    float m3 = smoothstep(0.0, 0.5, s3.a);\n"
            "    vec3 c0 = s0.rgb * (1.0 - m0);\n"
            "    vec3 c1 = s1.rgb * (1.0 - m1);\n"
            "    vec3 c2 = s2.rgb * (1.0 - m2);\n"
            "    vec3 c3 = s3.rgb * (1.0 - m3);\n"
            "    vec3 c = (c0 + c1 + c2 + c3) * 0.25;\n"
            "    float brightness = luma(c);\n"
            "    float contribution = clamp((brightness - 0.68) / max(brightness, 0.001), 0.0, 1.0);\n"
            "    gl_FragColor = vec4(c * contribution, 1.0);\n"
            "}\n";

        gCelBloomExtractProgram = CreateCelPostProgram(vertexSource, extractFragment, "Cel bloom bright-pass");
        if (gCelBloomExtractProgram == 0)
        {
            gCelBloomFailedWidth = bloomWidth;
            gCelBloomFailedHeight = bloomHeight;
            return false;
        }

        gCelBloomExtractSceneLocation = glGetUniformLocation(gCelBloomExtractProgram, "sceneTex");
        gCelBloomExtractTexelLocation = glGetUniformLocation(gCelBloomExtractProgram, "sourceTexelSize");
    }

    if (gCelBloomBlurProgram == 0)
    {
        const char* blurFragment =
            "precision mediump float;\n"
            "uniform sampler2D sourceTex;\n"
            "uniform vec2 blurStep;\n"
            "varying vec2 texcoord;\n"
            "void main() {\n"
            "    vec3 c = texture2D(sourceTex, texcoord).rgb * 0.19648255;\n"
            // Pair adjacent +/-1 and +/-2 taps with the texture's GL_LINEAR filtering.
            // Combined weight is 0.39137736 at weighted offset 1.2413793.
            "    c += texture2D(sourceTex, texcoord + blurStep * 1.2413793).rgb * 0.39137736;\n"
            "    c += texture2D(sourceTex, texcoord - blurStep * 1.2413793).rgb * 0.39137736;\n"
            "    c += texture2D(sourceTex, texcoord + blurStep * 3.0).rgb * 0.01038136;\n"
            "    c += texture2D(sourceTex, texcoord - blurStep * 3.0).rgb * 0.01038136;\n"
            "    gl_FragColor = vec4(c, 1.0);\n"
            "}\n";

        gCelBloomBlurProgram = CreateCelPostProgram(vertexSource, blurFragment, "Cel bloom Gaussian blur");
        if (gCelBloomBlurProgram == 0)
        {
            gCelBloomFailedWidth = bloomWidth;
            gCelBloomFailedHeight = bloomHeight;
            return false;
        }

        gCelBloomBlurSourceLocation = glGetUniformLocation(gCelBloomBlurProgram, "sourceTex");
        gCelBloomBlurStepLocation = glGetUniformLocation(gCelBloomBlurProgram, "blurStep");
    }

    if (gCelBloomFbo == 0)
        glGenFramebuffers(1, &gCelBloomFbo);
    if (gCelBloomTexture == 0)
        glGenTextures(1, &gCelBloomTexture);
    if (gCelBloomScratchTexture == 0)
        glGenTextures(1, &gCelBloomScratchTexture);

    if (gCelBloomFbo == 0 || gCelBloomTexture == 0 || gCelBloomScratchTexture == 0)
        return false;

    GLint previousFramebuffer = 0;
    GLint previousActiveTexture = GL_TEXTURE0;
    GLint previousTexture = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);

    if (gCelBloomWidth != bloomWidth || gCelBloomHeight != bloomHeight)
    {
        glBindTexture(GL_TEXTURE_2D, gCelBloomTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bloomWidth, bloomHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        if (!CheckSHARAllocationError("cel bloom texture", bloomWidth, bloomHeight))
        {
            gCelBloomFailedWidth = bloomWidth;
            gCelBloomFailedHeight = bloomHeight;
            glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)previousFramebuffer);
            glBindTexture(GL_TEXTURE_2D, (GLuint)previousTexture);
            glActiveTexture((GLenum)previousActiveTexture);
            return false;
        }

        glBindTexture(GL_TEXTURE_2D, gCelBloomScratchTexture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bloomWidth, bloomHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        if (!CheckSHARAllocationError("cel bloom scratch texture", bloomWidth, bloomHeight))
        {
            gCelBloomFailedWidth = bloomWidth;
            gCelBloomFailedHeight = bloomHeight;
            glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)previousFramebuffer);
            glBindTexture(GL_TEXTURE_2D, (GLuint)previousTexture);
            glActiveTexture((GLenum)previousActiveTexture);
            return false;
        }

        gCelBloomWidth = bloomWidth;
        gCelBloomHeight = bloomHeight;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, gCelBloomFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gCelBloomTexture, 0);
    GLenum statusA = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gCelBloomScratchTexture, 0);
    GLenum statusB = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    gCelBloomReady = (statusA == GL_FRAMEBUFFER_COMPLETE && statusB == GL_FRAMEBUFFER_COMPLETE);
    if (!gCelBloomReady)
    {
        gCelBloomFailedWidth = bloomWidth;
        gCelBloomFailedHeight = bloomHeight;
        SDL_LogError(SDL_LOG_CATEGORY_RENDER,
            "SHAR Android bloom framebuffer incomplete: textureA=0x%04x textureB=0x%04x",
            (unsigned)statusA, (unsigned)statusB);
    }
    else
    {
        gCelBloomFailedWidth = 0;
        gCelBloomFailedHeight = 0;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)previousFramebuffer);
    glBindTexture(GL_TEXTURE_2D, (GLuint)previousTexture);
    glActiveTexture((GLenum)previousActiveTexture);
    return gCelBloomReady;
}

static void DrawCelPostFullscreenQuad()
{
    SHARBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, gCelPostVbo);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (const void*)0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

static bool EnsureCelFallbackResolveResources()
{
    if (gCelFallbackProgram != 0 && gCelFallbackVbo != 0)
        return true;

    const char* vertexSource =
        "attribute vec2 position;\n"
        "varying vec2 texcoord;\n"
        "void main() {\n"
        "    texcoord = position * 0.5 + 0.5;\n"
        "    gl_Position = vec4(position, 0.0, 1.0);\n"
        "}\n";
    const char* fragmentSource =
        "precision mediump float;\n"
        "uniform sampler2D sceneTex;\n"
        "varying vec2 texcoord;\n"
        "void main() { gl_FragColor = vec4(texture2D(sceneTex, texcoord).rgb, 1.0); }\n";

    GLuint vertexShader = CompileCelPostShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = CompileCelPostShader(GL_FRAGMENT_SHADER, fragmentSource);
    if (vertexShader == 0 || fragmentShader == 0)
    {
        if (vertexShader) glDeleteShader(vertexShader);
        if (fragmentShader) glDeleteShader(fragmentShader);
        return false;
    }

    gCelFallbackProgram = glCreateProgram();
    glBindAttribLocation(gCelFallbackProgram, 0, "position");
    glAttachShader(gCelFallbackProgram, vertexShader);
    glAttachShader(gCelFallbackProgram, fragmentShader);
    glLinkProgram(gCelFallbackProgram);

    GLint linked = GL_FALSE;
    glGetProgramiv(gCelFallbackProgram, GL_LINK_STATUS, &linked);
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    if (linked == GL_FALSE)
    {
        GLint length = 0;
        glGetProgramiv(gCelFallbackProgram, GL_INFO_LOG_LENGTH, &length);
        if (length > 0)
        {
            std::vector<char> info((size_t)length + 1, 0);
            glGetProgramInfoLog(gCelFallbackProgram, length, NULL, info.data());
            SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                "SHAR cel fallback resolve program link failed: %s", info.data());
        }
        glDeleteProgram(gCelFallbackProgram);
        gCelFallbackProgram = 0;
        return false;
    }

    gCelFallbackTextureLocation = glGetUniformLocation(gCelFallbackProgram, "sceneTex");
    static const GLfloat quad[] =
    {
        -1.0f, -1.0f,
         1.0f, -1.0f,
        -1.0f,  1.0f,
         1.0f,  1.0f
    };
    glGenBuffers(1, &gCelFallbackVbo);
    glBindBuffer(GL_ARRAY_BUFFER, gCelFallbackVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return gCelFallbackVbo != 0;
}

static void ResolveSceneToScreen(int width, int height)
{
    if (!gCelWorldInFbo)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    GLint oldProgram = 0;
    GLint oldActiveTexture = GL_TEXTURE0;
    GLint oldTexture0 = 0;
    GLint oldArrayBuffer = 0;
    GLint oldVao = 0;
    GLint oldViewport[4] = {0, 0, width, height};
    GLint oldAttrib0Enabled = GL_FALSE;
    glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgram);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &oldActiveTexture);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &oldArrayBuffer);
    oldVao = SHARGetBoundVertexArray();
    glGetIntegerv(GL_VIEWPORT, oldViewport);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &oldTexture0);
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &oldAttrib0Enabled);
    glActiveTexture((GLenum)oldActiveTexture);

    const GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    const GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
    const GLboolean scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    const GLboolean stencilEnabled = glIsEnabled(GL_STENCIL_TEST);

    int outputWidth = width;
    int outputHeight = height;
    QueryDefaultFramebufferSize(&outputWidth, &outputHeight);

    if (gCelPostTexture != 0 && EnsureCelFallbackResolveResources())
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, outputWidth, outputHeight);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glDisable(GL_CULL_FACE);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_STENCIL_TEST);
        glUseProgram(gCelFallbackProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gCelPostTexture);
        if (gCelFallbackTextureLocation >= 0)
            glUniform1i(gCelFallbackTextureLocation, 0);
        // Keep the game-owned VAO untouched while configuring the fallback quad.
        SHARBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, gCelFallbackVbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (const void*)0);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    else
    {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER,
            "Unable to create direct cel resolve fallback at %dx%d",
            width, height);
    }

    // Restore the original VAO's attribute-enable state, without having
    // changed its vertex pointer or buffer association during fallback drawing.
    SHARBindVertexArray((GLuint)oldVao);
    if (oldAttrib0Enabled)
        glEnableVertexAttribArray(0);
    else
        glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)oldArrayBuffer);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, (GLuint)oldTexture0);
    glActiveTexture((GLenum)oldActiveTexture);
    glUseProgram((GLuint)oldProgram);
    if (depthEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (blendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (cullEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (scissorEnabled) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    if (stencilEnabled) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);

    // This fallback must leave the default framebuffer selected for GUI rendering.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, outputWidth, outputHeight);
    gCelWorldInFbo = false;

}

static void RecreateSHARTexture(GLuint& texture)
{
    if (texture == 0)
        return;

    glDeleteTextures(1, &texture);
    texture = 0;
    glGenTextures(1, &texture);
}

static void ResetAndroidPostProcess()
{
    InvalidateSHARTrackedColorWriteMaskCache();
    InvalidateCelRenderTargetValidationCache();
    // Detach the old texture storage from FBOs before replacing texture names.
    if (gCelRenderFbo != 0)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, gCelRenderFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
    }
    if (gCelBloomFbo != 0)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, gCelBloomFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    }
    if (gCelCloudShadowFbo != 0)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, gCelCloudShadowFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glActiveTexture(GL_TEXTURE0);
    RecreateSHARTexture(gCelPostTexture);
    RecreateSHARTexture(gCelRenderDepthTexture);
    RecreateSHARTexture(gCelBloomTexture);
    RecreateSHARTexture(gCelBloomScratchTexture);
    RecreateSHARTexture(gCelCloudShadowTexture);
    RecreateSHARTexture(gFrameSharpenTexture);

    if (gCelRenderDepth != 0)
    {
        glDeleteRenderbuffers(1, &gCelRenderDepth);
        gCelRenderDepth = 0;
        glGenRenderbuffers(1, &gCelRenderDepth);
    }

    gCelDepthTextureWidth = gCelDepthTextureHeight = 0;
    gCelDepthTextureActive = false;
    gCelDepthTextureFailed = false;
    gCelPostWidth = gCelPostHeight = 0;
    gCelRenderbufferWidth = gCelRenderbufferHeight = 0;
    gCelBloomWidth = gCelBloomHeight = 0;
    gCelCloudShadowWidth = gCelCloudShadowHeight = 0;
    gCelCloudShadowReady = false;
    gFrameSharpenWidth = gFrameSharpenHeight = 0;
    gCelRenderFboReady = false;
    gCelBloomReady = false;

    // Allocation failure latches are per-size. A successful new resize commit
    // explicitly allows each target to retry its allocation for the new surface.
    gCelPostFailedWidth = gCelPostFailedHeight = 0;
    gCelRenderbufferFailedWidth = gCelRenderbufferFailedHeight = 0;
    gCelRenderFboFailedWidth = gCelRenderFboFailedHeight = 0;
    gCelBloomFailedWidth = gCelBloomFailedHeight = 0;
    gCelCloudShadowFailedWidth = gCelCloudShadowFailedHeight = 0;
    gFrameSharpenFailedWidth = gFrameSharpenFailedHeight = 0;
    gSHARLastRejectedWidth = gSHARLastRejectedHeight = gSHARLastRejectedReason = 0;
    gSHARCachedMaxTextureSize = 0;
    gSHARCachedMaxRenderbufferSize = 0;
    gCelWorldInFbo = false;
    gSHARPreviousMotionBlurCamera.valid = false;
    gSHARMotionBlurHistoryWidth = gSHARMotionBlurHistoryHeight = 0;
    gSHARPreviousMotionBlurSampleTime = 0;
    // Normal frames remain on the offscreen path immediately after a resize.

}

static void ApplyCelPostProcess(int width, int height)
{
    // World geometry is finished. Re-enable ordinary alpha writes before the
    // fullscreen post-process and GUI/output rendering passes.
    SetSHARCharacterMaskModelPass(false);
    SetSHARCharacterMaskWorldEnabled(false);

    if (gSHARFrameWidth > 0 && gSHARFrameHeight > 0)
    {
        width = gSHARFrameWidth;
        height = gSHARFrameHeight;
    }

    const bool motionBlurRequested = IsMotionBlurEnabled();
    const bool bloomRequested = IsSHARBloomEnabled();
    const bool cloudShadowsUserEnabled = IsSHARCloudShadowsEnabled();
    // Limit cloud shadows to the supported exterior gameplay levels.
    // Do not key this effect off the iris-transition flag: the world behind the expanding
    // iris is already the exterior again, and toggling shadows here caused a visible blink.
    const bool cloudShadowLevelAllowed = IsSHARWorldCloudShadowLevelAllowed();
    // Camera reconstruction is required only by motion blur or active cloud shadows.
    // This leaves the no-effects LITE route free of unnecessary camera queries.
    const bool needMotionBlurCamera =
        motionBlurRequested ||
        (cloudShadowsUserEnabled && cloudShadowLevelAllowed && gCelDepthTextureActive);
    const float motionBlurTimeScale =
        motionBlurRequested ? GetSHARMotionBlurTimeScale() : 1.0f;
    SHARMotionBlurCameraState currentMotionBlurCamera = {};
    const bool haveCurrentMotionBlurCamera =
        needMotionBlurCamera && CaptureSHARMotionBlurCamera(&currentMotionBlurCamera);

    // Use full-strength cloud shadows on supported levels.
    const float cloudShadowStrengthMultiplier = 1.0f;
    static Uint32 cloudShadowStartTicks = SDL_GetTicks();
    const float cloudShadowTime = (float)(SDL_GetTicks() - cloudShadowStartTicks) * 0.001f;
    const bool cloudShadowsActive =
        cloudShadowsUserEnabled && cloudShadowLevelAllowed &&
        gCelDepthTextureActive && haveCurrentMotionBlurCamera;

    if (!gCelWorldInFbo)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        CommitSHARMotionBlurCameraHistory(currentMotionBlurCamera, false, width, height);
        return;
    }

    // Confirm the scene framebuffer is still bound before touching its depth
    // attachment. This also covers the direct-resolve path with no post effects.
    GLint currentFramebuffer = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFramebuffer);
    if ((GLuint)currentFramebuffer != gCelRenderFbo || !gCelRenderFboReady)
    {
        ResolveSceneToScreen(width, height);
        CommitSHARMotionBlurCameraHistory(currentMotionBlurCamera, haveCurrentMotionBlurCamera, width, height);
        return;
    }

    // World rendering is complete. When no depth-sampling effect is active,
    // discard the transient depth attachment before switching to colour passes.
    if (!gCelDepthTextureActive)
        DiscardCelDepthAttachmentIfSupported();

    const bool motionBlurCameraValid =
        motionBlurRequested && gCelDepthTextureActive && haveCurrentMotionBlurCamera &&
        IsSHARMotionBlurHistoryUsable(currentMotionBlurCamera, width, height);

    // Resolve the scene directly when no independent post effect is requested.
    if (!bloomRequested && !cloudShadowsActive && !motionBlurRequested)
    {
        ResolveSceneToScreen(width, height);
        CommitSHARMotionBlurCameraHistory(currentMotionBlurCamera,
            haveCurrentMotionBlurCamera, width, height);
        return;
    }

    if (!EnsureCelPostProcessResources(width, height))
    {
        ResolveSceneToScreen(width, height);
        CommitSHARMotionBlurCameraHistory(currentMotionBlurCamera, haveCurrentMotionBlurCamera, width, height);
        return;
    }

    const bool bloomReady = bloomRequested ? EnsureCelBloomResources(width, height) : false;
    const bool cloudShadowMaskReady = cloudShadowsActive && EnsureCelCloudShadowResources(width, height);

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    const GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    const GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
    const GLboolean scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    const GLboolean stencilEnabled = glIsEnabled(GL_STENCIL_TEST);
    GLint previousProgram = 0;
    GLint previousActiveTexture = GL_TEXTURE0;
    GLint previousTextureUnit0 = 0;
    GLint previousTextureUnit1 = 0;
    GLint previousTextureUnit2 = 0;
    GLint previousTextureUnit3 = 0;
    GLint previousArrayBuffer = 0;
    GLint previousVao = 0;
    GLint previousAttrib0Enabled = GL_FALSE;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTextureUnit0);
    glActiveTexture(GL_TEXTURE1);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTextureUnit1);
    glActiveTexture(GL_TEXTURE2);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTextureUnit2);
    glActiveTexture(GL_TEXTURE3);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTextureUnit3);
    glActiveTexture((GLenum)previousActiveTexture);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer);
    previousVao = SHARGetBoundVertexArray();
    glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &previousAttrib0Enabled);

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);

    if (bloomReady)
    {
        // Pass 1: downsample the scene and extract only pixels above the bright threshold.
        glBindFramebuffer(GL_FRAMEBUFFER, gCelBloomFbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gCelBloomTexture, 0);
        glViewport(0, 0, gCelBloomWidth, gCelBloomHeight);
        glUseProgram(gCelBloomExtractProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gCelPostTexture);
        glUniform1i(gCelBloomExtractSceneLocation, 0);
        if (gCelBloomExtractTexelLocation >= 0)
            glUniform2f(gCelBloomExtractTexelLocation, 1.0f / (float)width, 1.0f / (float)height);
        DrawCelPostFullscreenQuad();

        // Pass 2: horizontal Gaussian blur into a separate reduced-resolution texture.
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gCelBloomScratchTexture, 0);
        glUseProgram(gCelBloomBlurProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gCelBloomTexture);
        glUniform1i(gCelBloomBlurSourceLocation, 0);
        if (gCelBloomBlurStepLocation >= 0)
            glUniform2f(gCelBloomBlurStepLocation, 1.0f / (float)gCelBloomWidth, 0.0f);
        DrawCelPostFullscreenQuad();

        // Pass 3: vertical Gaussian blur back into the bloom texture.
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gCelBloomTexture, 0);
        glUseProgram(gCelBloomBlurProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gCelBloomScratchTexture);
        glUniform1i(gCelBloomBlurSourceLocation, 0);
        if (gCelBloomBlurStepLocation >= 0)
            glUniform2f(gCelBloomBlurStepLocation, 0.0f, 1.0f / (float)gCelBloomHeight);
        DrawCelPostFullscreenQuad();
    }

    if (cloudShadowMaskReady)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, gCelCloudShadowFbo);
        glFramebufferTexture2D(
            GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
            gCelCloudShadowTexture, 0);
        glViewport(0, 0, gCelCloudShadowWidth, gCelCloudShadowHeight);
        glUseProgram(gCelCloudShadowProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gCelRenderDepthTexture);
        if (gCelCloudMaskDepthLocation >= 0)
            glUniform1i(gCelCloudMaskDepthLocation, 0);
        if (gCelCloudMaskTimeLocation >= 0)
            glUniform1f(gCelCloudMaskTimeLocation, cloudShadowTime);
        if (gCelCloudMaskCameraPositionLocation >= 0)
            glUniform3f(gCelCloudMaskCameraPositionLocation,
                currentMotionBlurCamera.position[0], currentMotionBlurCamera.position[1], currentMotionBlurCamera.position[2]);
        if (gCelCloudMaskCameraRightLocation >= 0)
            glUniform3f(gCelCloudMaskCameraRightLocation,
                currentMotionBlurCamera.right[0], currentMotionBlurCamera.right[1], currentMotionBlurCamera.right[2]);
        if (gCelCloudMaskCameraUpLocation >= 0)
            glUniform3f(gCelCloudMaskCameraUpLocation,
                currentMotionBlurCamera.up[0], currentMotionBlurCamera.up[1], currentMotionBlurCamera.up[2]);
        if (gCelCloudMaskCameraForwardLocation >= 0)
            glUniform3f(gCelCloudMaskCameraForwardLocation,
                currentMotionBlurCamera.forward[0], currentMotionBlurCamera.forward[1], currentMotionBlurCamera.forward[2]);
        if (gCelCloudMaskTanHalfFovXLocation >= 0)
            glUniform1f(gCelCloudMaskTanHalfFovXLocation, currentMotionBlurCamera.tanHalfFovX);
        if (gCelCloudMaskTanHalfFovYLocation >= 0)
            glUniform1f(gCelCloudMaskTanHalfFovYLocation, currentMotionBlurCamera.tanHalfFovY);
        if (gCelCloudMaskNearPlaneLocation >= 0)
            glUniform1f(gCelCloudMaskNearPlaneLocation, currentMotionBlurCamera.nearPlane);
        if (gCelCloudMaskFarPlaneLocation >= 0)
            glUniform1f(gCelCloudMaskFarPlaneLocation, currentMotionBlurCamera.farPlane);
        DrawCelPostFullscreenQuad();
    }

    // The scene texture uses the internal render-size snapshot, while the
    // final fullscreen pass covers the actual default framebuffer.
    int outputWidth = width;
    int outputHeight = height;
    QueryDefaultFramebufferSize(&outputWidth, &outputHeight);

    // A single optional-effects composite remains for bloom, cloud shadows and motion blur.
    const GLint postSceneLocation = gCelPostSceneLocation;
    const GLint postTexelLocation = gCelPostTexelLocation;
    const GLint postBloomLocation = gCelPostBloomLocation;
    const GLint postBloomStrengthLocation = gCelPostBloomStrengthLocation;
    const GLint postDepthLocation = gCelPostDepthLocation;
    const GLint postDepthAvailableLocation = gCelPostDepthAvailableLocation;
    const GLint postCloudShadowEnabledLocation = gCelPostCloudShadowEnabledLocation;
    const GLint postCloudShadowTimeLocation = gCelPostCloudShadowTimeLocation;
    const GLint postCloudShadowStrengthLocation = gCelPostCloudShadowStrengthLocation;
    const GLint postCloudShadowMaskLocation = gCelPostCloudShadowMaskLocation;
    const GLint postCloudShadowMaskTexelSizeLocation = gCelPostCloudShadowMaskTexelSizeLocation;
    const GLint postCloudShadowMaskAvailableLocation = gCelPostCloudShadowMaskAvailableLocation;
    const GLint postMotionBlurEnabledLocation = gCelPostMotionBlurEnabledLocation;
    const GLint postMotionBlurValidLocation = gCelPostMotionBlurValidLocation;
    const GLint postMotionBlurScaleLocation = gCelPostMotionBlurScaleLocation;
    const GLint postCurrentCameraPositionLocation = gCelPostCurrentCameraPositionLocation;
    const GLint postCurrentCameraRightLocation = gCelPostCurrentCameraRightLocation;
    const GLint postCurrentCameraUpLocation = gCelPostCurrentCameraUpLocation;
    const GLint postCurrentCameraForwardLocation = gCelPostCurrentCameraForwardLocation;
    const GLint postPreviousCameraPositionLocation = gCelPostPreviousCameraPositionLocation;
    const GLint postPreviousCameraRightLocation = gCelPostPreviousCameraRightLocation;
    const GLint postPreviousCameraUpLocation = gCelPostPreviousCameraUpLocation;
    const GLint postPreviousCameraForwardLocation = gCelPostPreviousCameraForwardLocation;
    const GLint postCurrentTanHalfFovXLocation = gCelPostCurrentTanHalfFovXLocation;
    const GLint postCurrentTanHalfFovYLocation = gCelPostCurrentTanHalfFovYLocation;
    const GLint postPreviousTanHalfFovXLocation = gCelPostPreviousTanHalfFovXLocation;
    const GLint postPreviousTanHalfFovYLocation = gCelPostPreviousTanHalfFovYLocation;
    const GLint postCurrentNearPlaneLocation = gCelPostCurrentNearPlaneLocation;
    const GLint postCurrentFarPlaneLocation = gCelPostCurrentFarPlaneLocation;
    const GLint postPreviousNearPlaneLocation = gCelPostPreviousNearPlaneLocation;
    const GLint postCharacterMaskEnabledLocation = gCelPostCharacterMaskEnabledLocation;
    const GLint postCharacterMaskPositionLocation = gCelPostCharacterMaskPositionLocation;
    const GLint postCharacterMaskSizeLocation = gCelPostCharacterMaskSizeLocation;
    const GLint postVehicleMaskEnabledLocation = gCelPostVehicleMaskEnabledLocation;
    const GLint postVehicleMaskPositionLocation = gCelPostVehicleMaskPositionLocation;
    const GLint postVehicleMaskSizeLocation = gCelPostVehicleMaskSizeLocation;
    const GLint postVehicleMotionIntensityLocation = gCelPostVehicleMotionIntensityLocation;
    const bool useBloomOnlyComposite =
        bloomRequested && !cloudShadowsActive && !motionBlurRequested &&
        gCelBloomOnlyPostProgram != 0;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, outputWidth, outputHeight);
    if (useBloomOnlyComposite)
    {
        glUseProgram(gCelBloomOnlyPostProgram);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gCelPostTexture);
        if (gCelBloomOnlySceneLocation >= 0)
            glUniform1i(gCelBloomOnlySceneLocation, 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, bloomReady ? gCelBloomTexture : 0);
        if (gCelBloomOnlyTextureLocation >= 0)
            glUniform1i(gCelBloomOnlyTextureLocation, 1);
        if (gCelBloomOnlyStrengthLocation >= 0)
            glUniform1f(gCelBloomOnlyStrengthLocation, bloomReady ? 1.30f : 0.0f);
    }
    else
    {
        glUseProgram(gCelPostProgram);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gCelPostTexture);
    glUniform1i(postSceneLocation, 0);
    if (postTexelLocation >= 0)
        glUniform2f(postTexelLocation, 1.0f / (float)width, 1.0f / (float)height);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, bloomReady ? gCelBloomTexture : 0);
    if (postBloomLocation >= 0)
        glUniform1i(postBloomLocation, 1);
    if (postBloomStrengthLocation >= 0)
        glUniform1f(postBloomStrengthLocation, bloomReady ? 1.30f : 0.0f);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, gCelDepthTextureActive ? gCelRenderDepthTexture : gCelPostTexture);
    if (postDepthLocation >= 0)
        glUniform1i(postDepthLocation, 2);
    if (postDepthAvailableLocation >= 0)
        glUniform1f(postDepthAvailableLocation, gCelDepthTextureActive ? 1.0f : 0.0f);
    if (postCloudShadowEnabledLocation >= 0)
        glUniform1f(postCloudShadowEnabledLocation, cloudShadowsActive ? 1.0f : 0.0f);
    if (postCloudShadowTimeLocation >= 0)
        glUniform1f(postCloudShadowTimeLocation, cloudShadowTime);
    if (postCloudShadowStrengthLocation >= 0)
        glUniform1f(postCloudShadowStrengthLocation, cloudShadowStrengthMultiplier);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, cloudShadowMaskReady ? gCelCloudShadowTexture : 0);
    if (postCloudShadowMaskLocation >= 0)
        glUniform1i(postCloudShadowMaskLocation, 3);
    if (postCloudShadowMaskAvailableLocation >= 0)
        glUniform1f(postCloudShadowMaskAvailableLocation, cloudShadowMaskReady ? 1.0f : 0.0f);
    if (postCloudShadowMaskTexelSizeLocation >= 0)
        glUniform2f(postCloudShadowMaskTexelSizeLocation,
            cloudShadowMaskReady ? 1.0f / (float)gCelCloudShadowWidth : 1.0f,
            cloudShadowMaskReady ? 1.0f / (float)gCelCloudShadowHeight : 1.0f);

    if (postMotionBlurEnabledLocation >= 0)
        glUniform1f(postMotionBlurEnabledLocation, motionBlurRequested ? 1.0f : 0.0f);
    if (postMotionBlurValidLocation >= 0)
        glUniform1f(postMotionBlurValidLocation, motionBlurCameraValid ? 1.0f : 0.0f);
    if (postMotionBlurScaleLocation >= 0)
        glUniform1f(postMotionBlurScaleLocation, motionBlurTimeScale);

    if (haveCurrentMotionBlurCamera)
    {
        if (postCurrentCameraPositionLocation >= 0)
            glUniform3f(postCurrentCameraPositionLocation, currentMotionBlurCamera.position[0], currentMotionBlurCamera.position[1], currentMotionBlurCamera.position[2]);
        if (postCurrentCameraRightLocation >= 0)
            glUniform3f(postCurrentCameraRightLocation, currentMotionBlurCamera.right[0], currentMotionBlurCamera.right[1], currentMotionBlurCamera.right[2]);
        if (postCurrentCameraUpLocation >= 0)
            glUniform3f(postCurrentCameraUpLocation, currentMotionBlurCamera.up[0], currentMotionBlurCamera.up[1], currentMotionBlurCamera.up[2]);
        if (postCurrentCameraForwardLocation >= 0)
            glUniform3f(postCurrentCameraForwardLocation, currentMotionBlurCamera.forward[0], currentMotionBlurCamera.forward[1], currentMotionBlurCamera.forward[2]);
        if (postCurrentTanHalfFovXLocation >= 0)
            glUniform1f(postCurrentTanHalfFovXLocation, currentMotionBlurCamera.tanHalfFovX);
        if (postCurrentTanHalfFovYLocation >= 0)
            glUniform1f(postCurrentTanHalfFovYLocation, currentMotionBlurCamera.tanHalfFovY);
        if (postCurrentNearPlaneLocation >= 0)
            glUniform1f(postCurrentNearPlaneLocation, currentMotionBlurCamera.nearPlane);
        if (postCurrentFarPlaneLocation >= 0)
            glUniform1f(postCurrentFarPlaneLocation, currentMotionBlurCamera.farPlane);
    }

    if (postCharacterMaskEnabledLocation >= 0)
        glUniform1f(postCharacterMaskEnabledLocation, gSHARMotionBlurCharacterSubject.valid ? 1.0f : 0.0f);
    if (postCharacterMaskPositionLocation >= 0 && gSHARMotionBlurCharacterSubject.valid)
        glUniform3f(postCharacterMaskPositionLocation, gSHARMotionBlurCharacterSubject.position[0], gSHARMotionBlurCharacterSubject.position[1], gSHARMotionBlurCharacterSubject.position[2]);
    if (postCharacterMaskSizeLocation >= 0 && gSHARMotionBlurCharacterSubject.valid)
        glUniform2f(postCharacterMaskSizeLocation, gSHARMotionBlurCharacterSubject.halfWidth, gSHARMotionBlurCharacterSubject.halfHeight);
    if (postVehicleMaskEnabledLocation >= 0)
        glUniform1f(postVehicleMaskEnabledLocation, gSHARMotionBlurVehicleSubject.valid ? 1.0f : 0.0f);
    if (postVehicleMaskPositionLocation >= 0 && gSHARMotionBlurVehicleSubject.valid)
        glUniform3f(postVehicleMaskPositionLocation, gSHARMotionBlurVehicleSubject.position[0], gSHARMotionBlurVehicleSubject.position[1], gSHARMotionBlurVehicleSubject.position[2]);
    if (postVehicleMaskSizeLocation >= 0 && gSHARMotionBlurVehicleSubject.valid)
        glUniform2f(postVehicleMaskSizeLocation, gSHARMotionBlurVehicleSubject.halfWidth, gSHARMotionBlurVehicleSubject.halfHeight);
    if (postVehicleMotionIntensityLocation >= 0)
        glUniform1f(postVehicleMotionIntensityLocation,
            gSHARMotionBlurVehicleSubject.valid ? gSHARMotionBlurVehicleSubject.motionIntensity : 0.0f);

    if (motionBlurCameraValid)
    {
        if (postPreviousCameraPositionLocation >= 0)
            glUniform3f(postPreviousCameraPositionLocation, gSHARPreviousMotionBlurCamera.position[0], gSHARPreviousMotionBlurCamera.position[1], gSHARPreviousMotionBlurCamera.position[2]);
        if (postPreviousCameraRightLocation >= 0)
            glUniform3f(postPreviousCameraRightLocation, gSHARPreviousMotionBlurCamera.right[0], gSHARPreviousMotionBlurCamera.right[1], gSHARPreviousMotionBlurCamera.right[2]);
        if (postPreviousCameraUpLocation >= 0)
            glUniform3f(postPreviousCameraUpLocation, gSHARPreviousMotionBlurCamera.up[0], gSHARPreviousMotionBlurCamera.up[1], gSHARPreviousMotionBlurCamera.up[2]);
        if (postPreviousCameraForwardLocation >= 0)
            glUniform3f(postPreviousCameraForwardLocation, gSHARPreviousMotionBlurCamera.forward[0], gSHARPreviousMotionBlurCamera.forward[1], gSHARPreviousMotionBlurCamera.forward[2]);
        if (postPreviousTanHalfFovXLocation >= 0)
            glUniform1f(postPreviousTanHalfFovXLocation, gSHARPreviousMotionBlurCamera.tanHalfFovX);
        if (postPreviousTanHalfFovYLocation >= 0)
            glUniform1f(postPreviousTanHalfFovYLocation, gSHARPreviousMotionBlurCamera.tanHalfFovY);
        if (postPreviousNearPlaneLocation >= 0)
            glUniform1f(postPreviousNearPlaneLocation, gSHARPreviousMotionBlurCamera.nearPlane);
    }

    }

    glActiveTexture(GL_TEXTURE0);
    DrawCelPostFullscreenQuad();


    if (previousAttrib0Enabled)
        glEnableVertexAttribArray(0);
    else
        glDisableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, (GLuint)previousArrayBuffer);
    SHARBindVertexArray((GLuint)previousVao);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, (GLuint)previousTextureUnit0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, (GLuint)previousTextureUnit1);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, (GLuint)previousTextureUnit2);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, (GLuint)previousTextureUnit3);
    glActiveTexture((GLenum)previousActiveTexture);

    glUseProgram((GLuint)previousProgram);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);

    if (depthEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (blendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (cullEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (scissorEnabled) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    if (stencilEnabled) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);

    // Keep the GUI and all later passes on the onscreen framebuffer.
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, outputWidth, outputHeight);
    gCelWorldInFbo = false;
    CommitSHARMotionBlurCameraHistory(currentMotionBlurCamera, haveCurrentMotionBlurCamera, width, height);
}

}

static float GetSHARGlobalSaturationFactor()
{
    // Both object-based modes retain the same established +35% saturation.
    return IsCelShadingEnabled() ? 1.35f : 1.20f;
}

static void ApplyCombinedFrameFilter(int width, int height);

void RegisterSHARMotionBlurSubjectBounds(int subjectType, float x, float y, float z, float halfWidth, float halfHeight, float speedKmh)
{
    SHARMotionBlurSubjectBounds* subject = NULL;
    if (subjectType == 0)
        subject = &gSHARMotionBlurCharacterSubject;
    else if (subjectType == 1)
        subject = &gSHARMotionBlurVehicleSubject;
    if (subject == NULL)
        return;

    if (subjectType == 1)
    {
        // Use actual vehicle speed so repeated render passes cannot cancel the trail strength.
        float speed = speedKmh < 0.0f ? -speedKmh : speedKmh;
        float t = (speed - 12.0f) / (50.0f - 12.0f);
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        subject->motionIntensity = t * t * (3.0f - 2.0f * t);
    }

    subject->position[0] = x;
    subject->position[1] = y;
    subject->position[2] = z;
    subject->halfWidth = halfWidth > 0.05f ? halfWidth : 0.05f;
    subject->halfHeight = halfHeight > 0.05f ? halfHeight : 0.05f;
    subject->valid = true;
}

#ifdef RAD_ANDROID
static void SHARResetDynamicResolutionController()
{
    gSHARDynamicResolutionHighSamples = 0;
    gSHARDynamicResolutionLowSamples = 0;
    gSHARDynamicResolutionCpuFrameSampleActive = false;
    gSHARDynamicResolutionCpuFrameStartTicks = 0;
    gSHARDynamicResolutionLastTargetFPS = gSHARAndroidTargetFPS;
}

static void SHARApplyDynamicResolutionSample(unsigned long long sampleNanoseconds)
{
    if (!gSHARDynamicResolutionEnabled ||
        gSHARDynamicResolutionMaxHeight <= gSHARDynamicResolutionMinHeight)
        return;

    const int targetFPS = (gSHARAndroidTargetFPS >= 30 && gSHARAndroidTargetFPS <= 144)
        ? gSHARAndroidTargetFPS : 60;
    if (gSHARDynamicResolutionLastTargetFPS != targetFPS)
        SHARResetDynamicResolutionController();

    const double targetBudgetNs = 1000000000.0 / (double)targetFPS;
    if ((double)sampleNanoseconds > targetBudgetNs * 1.08)
    {
        ++gSHARDynamicResolutionHighSamples;
        gSHARDynamicResolutionLowSamples = 0;
    }
    else if ((double)sampleNanoseconds < targetBudgetNs * 0.88)
    {
        ++gSHARDynamicResolutionLowSamples;
        gSHARDynamicResolutionHighSamples = 0;
    }
    else
    {
        gSHARDynamicResolutionHighSamples = 0;
        gSHARDynamicResolutionLowSamples = 0;
    }

    SDL_AtomicLock(&gSHARAndroidRenderResolutionLock);
    const int currentHeight = gSHARDynamicResolutionCurrentHeight > 0
        ? gSHARDynamicResolutionCurrentHeight : gSHARDynamicResolutionMaxHeight;
    const int minHeight = gSHARDynamicResolutionMinHeight;
    const int maxHeight = gSHARDynamicResolutionMaxHeight;
    SDL_AtomicUnlock(&gSHARAndroidRenderResolutionLock);

    int requestedHeight = currentHeight;
    if (gSHARDynamicResolutionHighSamples >= 8 && currentHeight > minHeight)
    {
        requestedHeight = (currentHeight * 9) / 10;
        if (requestedHeight < minHeight)
            requestedHeight = minHeight;
    }
    else if (gSHARDynamicResolutionLowSamples >= 90 && currentHeight < maxHeight)
    {
        requestedHeight = (currentHeight * 11) / 10;
        if (requestedHeight > maxHeight)
            requestedHeight = maxHeight;
    }

    if (requestedHeight == currentHeight)
        return;

    const Uint64 now = SDL_GetPerformanceCounter();
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    if (gSHARDynamicResolutionLastResizeTicks != 0 && frequency > 0 &&
        now - gSHARDynamicResolutionLastResizeTicks < (frequency * 3) / 2)
    {
        return;
    }

    if (RequestSHARAndroidRenderResolutionHeight(requestedHeight))
    {
        SDL_AtomicLock(&gSHARAndroidRenderResolutionLock);
        gSHARDynamicResolutionCurrentHeight = requestedHeight;
        SDL_AtomicUnlock(&gSHARAndroidRenderResolutionLock);
        gSHARDynamicResolutionLastResizeTicks = now;
        SHARResetDynamicResolutionController();
    }
}

static void SHARBeginDynamicResolutionSample()
{
    if (!IsSHARDynamicResolutionEnabled() ||
        gSHARDynamicResolutionMaxHeight <= gSHARDynamicResolutionMinHeight)
        return;

    gSHARDynamicResolutionCpuFrameStartTicks = SDL_GetPerformanceCounter();
    gSHARDynamicResolutionCpuFrameSampleActive =
        gSHARDynamicResolutionCpuFrameStartTicks != 0;
}

static void SHAREndDynamicResolutionCpuFrameSample(bool completedFrame)
{
    if (!gSHARDynamicResolutionCpuFrameSampleActive)
        return;

    const Uint64 startTicks = gSHARDynamicResolutionCpuFrameStartTicks;
    gSHARDynamicResolutionCpuFrameSampleActive = false;
    gSHARDynamicResolutionCpuFrameStartTicks = 0;

    if (!completedFrame || !IsSHARDynamicResolutionEnabled() || startTicks == 0)
        return;

    const Uint64 now = SDL_GetPerformanceCounter();
    const Uint64 frequency = SDL_GetPerformanceFrequency();
    if (frequency == 0 || now <= startTicks)
        return;

    // Measure render submission and any GPU back-pressure in SwapWindow.
    // This excludes the explicit FPS-cap sleep and is not a GPU-time measurement.
    const double elapsedNanoseconds =
        (double)(now - startTicks) * 1000000000.0 / (double)frequency;
    if (elapsedNanoseconds <= 0.0)
        return;

    SHARApplyDynamicResolutionSample(
        (unsigned long long)(elapsedNanoseconds + 0.5));
}

#endif

bool BeginCelPostProcessFrame(int width, int height)
{
    gSHARMotionBlurCharacterSubject.valid = false;
    gSHARMotionBlurVehicleSubject.valid = false;
    gSHARFrameWidth = width;
    gSHARFrameHeight = height;
    gCelWorldInFbo = false;
    gSHARPostProcessAppliedBeforeTouchHud = false;
    SetSHARCharacterMaskModelPass(false);
    SetSHARCharacterMaskWorldEnabled(false);

    if (!gSHARVertexArrayObjectsSupported || !gSHARFramebufferSupportAvailable)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

    if (!BindCelRenderTarget(width, height))
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        return false;
    }

#ifdef RAD_ANDROID
    SHARBeginDynamicResolutionSample();
#endif

    // Clear alpha exactly once at the start of each world frame. Scissor and
    // RGB writes are disabled so the existing colour image is left untouched.
    GLboolean previousColourMask[4] = { GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE };
    GLboolean previousScissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    GLfloat previousClearColour[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    glGetBooleanv(GL_COLOR_WRITEMASK, previousColourMask);
    // Synchronize tracking with the queried state before the temporary clear.
    SetSHARTrackedColorWriteMask(
        previousColourMask[0], previousColourMask[1],
        previousColourMask[2], previousColourMask[3]);
    glGetFloatv(GL_COLOR_CLEAR_VALUE, previousClearColour);
    glDisable(GL_SCISSOR_TEST);
    SetSHARTrackedColorWriteMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glClearColor(previousClearColour[0], previousClearColour[1],
                 previousClearColour[2], previousClearColour[3]);
    SetSHARTrackedColorWriteMask(
        previousColourMask[0], previousColourMask[1],
        previousColourMask[2], previousColourMask[3]);
    if (previousScissorEnabled)
        glEnable(GL_SCISSOR_TEST);

    // From this point until composition, only CharacterRenderable model draws
    // may write scene alpha. Blob and static shadows remain unmarked.
    SetSHARCharacterMaskWorldEnabled(true);
    gCelWorldInFbo = true;
    return true;
}

void ApplyCelPostProcessBeforeGui(int width, int height)
{
    /*
     * Use the frame-begin snapshot for the scene target. After compositing, switch
     * the PDDI display dimensions to the physical output size so menus/HUD render
     * at output resolution while remaining on framebuffer 0.
     */
    (void)width;
    (void)height;
    ApplyCelPostProcess(gSHARFrameWidth, gSHARFrameHeight);

#ifdef RAD_ANDROID
    int outputWidth = gSHARFrameWidth;
    int outputHeight = gSHARFrameHeight;
    QueryDefaultFramebufferSize(&outputWidth, &outputHeight);
    if (gSHARActiveDisplayWidthForGui != NULL &&
        gSHARActiveDisplayHeightForGui != NULL &&
        outputWidth > 0 && outputHeight > 0)
    {
        *gSHARActiveDisplayWidthForGui = outputWidth;
        *gSHARActiveDisplayHeightForGui = outputHeight;
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, outputWidth, outputHeight);

        // The iris is a 3D GUI object and uses the default framebuffer's depth
        // test. The world is rendered into a separate FBO, so stale/default-FB
        // depth can incorrectly reject parts of the iris and produce thin strips.
        // Clear only during the iris wipe and preserve the caller's GL state.
        if (gSHARIrisTransitionActive)
        {
            GLboolean oldDepthMask = GL_TRUE;
            const GLboolean oldScissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
            GLfloat oldDepthClearValue = 1.0f;
            glGetBooleanv(GL_DEPTH_WRITEMASK, &oldDepthMask);
            glGetFloatv(GL_DEPTH_CLEAR_VALUE, &oldDepthClearValue);

            glDisable(GL_SCISSOR_TEST);
            glDepthMask(GL_TRUE);
            glClearDepthf(1.0f);
            glClear(GL_DEPTH_BUFFER_BIT);

            glClearDepthf(oldDepthClearValue);
            glDepthMask(oldDepthMask);
            if (oldScissorEnabled)
                glEnable(GL_SCISSOR_TEST);
        }
    }
    InvalidateSHARRenderStateCache();
#endif
}


#ifdef RAD_ANDROID
// Movies are presentation content, not part of the adaptive-resolution world
// target. Render them directly to the real Android output Surface so the movie
// quad's device-pixel projection and viewport use one consistent coordinate
// system at every selected internal render resolution.
void BeginSHARAndroidFMVOutputRender()
{
    // FMV output bypasses the world FBO, so restore normal alpha writes before
    // drawing directly to the Android Surface.
    SetSHARCharacterMaskModelPass(false);
    SetSHARCharacterMaskWorldEnabled(false);

    // Direct-output FMV frames must each receive the final global colour grade.
    gSHARPostProcessAppliedBeforeTouchHud = false;

    int outputWidth = 0;
    int outputHeight = 0;
    if (!QueryDefaultFramebufferSize(&outputWidth, &outputHeight) ||
        outputWidth <= 0 || outputHeight <= 0)
    {
        return;
    }

    if (gSHARActiveDisplayWidthForGui != NULL)
        *gSHARActiveDisplayWidthForGui = outputWidth;
    if (gSHARActiveDisplayHeightForGui != NULL)
        *gSHARActiveDisplayHeightForGui = outputHeight;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, outputWidth, outputHeight);

    // Do not let scissor or write masks preserve pixels from the previous
    // open-world frame in the letterbox area surrounding the movie.
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_BLEND);
    SetSHARTrackedColorWriteMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // The movie is now the frame being presented. The next BeginFrame will
    // restore the internal world target and bind its FBO as usual.
    gCelWorldInFbo = false;
    InvalidateSHARRenderStateCache();

}
#endif

static bool EnsureSHARCombinedPostProcessQuad()
{
    if (gCRTVbo == 0)
        glGenBuffers(1, &gCRTVbo);
    if (gCRTVao == 0)
        SHARGenVertexArrays(1, &gCRTVao);
    if (gCRTVbo == 0 || gCRTVao == 0)
        return false;

    if (!gCRTVaoReady)
    {
        const GLfloat quad[] = {
            -1.0f, -1.0f, 1.0f, -1.0f,
            -1.0f,  1.0f, 1.0f,  1.0f
        };
        GLint previousArrayBuffer = 0;
        const GLint previousVao = SHARGetBoundVertexArray();
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer);
        SHARBindVertexArray(gCRTVao);
        glBindBuffer(GL_ARRAY_BUFFER, gCRTVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)0);
        SHARBindVertexArray((GLuint)previousVao);
        glBindBuffer(GL_ARRAY_BUFFER, (GLuint)previousArrayBuffer);
        gCRTVaoReady = true;
    }

    return true;
}

static bool ApplySHARSaturationFramebufferFetch(int width, int height, float saturation)
{
    if (width <= 0 || height <= 0)
        return false;

    if (!gSHARSaturationFetchChecked)
    {
        gSHARSaturationFetchChecked = true;
        if (SDL_GL_ExtensionSupported("GL_EXT_shader_framebuffer_fetch") == SDL_TRUE)
        {
            gSHARSaturationFetchMode = 1;
        }
        else if (SDL_GL_ExtensionSupported("GL_NV_shader_framebuffer_fetch") == SDL_TRUE)
        {
            gSHARSaturationFetchMode = 2;
        }
        else if (SDL_GL_ExtensionSupported("GL_ARM_shader_framebuffer_fetch") == SDL_TRUE)
        {
            gSHARSaturationFetchMode = 3;
        }

    }

    if (gSHARSaturationFetchMode == 0)
        return false;

    if (gSHARSaturationFetchProgram == 0)
    {
        const char* vertexSource =
            "attribute vec2 position; varying vec2 texcoord;\n"
            "void main() { texcoord = position * 0.5 + 0.5; gl_Position = vec4(position, 0.0, 1.0); }\n";

        const char* extensionSource = gSHARSaturationFetchMode == 1
            ? "#extension GL_EXT_shader_framebuffer_fetch : require\n"
            : gSHARSaturationFetchMode == 2
                ? "#extension GL_NV_shader_framebuffer_fetch : require\n"
                : "#extension GL_ARM_shader_framebuffer_fetch : require\n";
        const char* fetchExpression = gSHARSaturationFetchMode == 3
            ? "gl_LastFragColorARM"
            : "gl_LastFragData[0]";

        std::string fragmentSource(extensionSource);
        fragmentSource +=
            "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
            "precision highp float;\n"
            "#else\n"
            "precision mediump float;\n"
            "#endif\n"
            "uniform float saturation;\n"
            "void main() {\n"
            "    vec4 scene = ";
        fragmentSource += fetchExpression;
        fragmentSource +=
            ";\n"
            "    float luma = dot(scene.rgb, vec3(0.299, 0.587, 0.114));\n"
            "    vec3 colour = mix(vec3(luma), scene.rgb, clamp(saturation, 1.0, 1.5));\n"
            "    gl_FragColor = vec4(clamp(colour, 0.0, 1.0), 1.0);\n"
            "}\n";

        gSHARSaturationFetchProgram = CreateCelPostProgram(
            vertexSource, fragmentSource.c_str(), "Saturation-only framebuffer fetch");
        if (gSHARSaturationFetchProgram != 0)
        {
            gSHARSaturationFetchLocation =
                glGetUniformLocation(gSHARSaturationFetchProgram, "saturation");
        }
        else
        {
            // Shader compilation failure is permanent for this GL context; retain
            // the established texture-copy implementation as a safe fallback.
            gSHARSaturationFetchMode = 0;
            return false;
        }
    }

    if (!EnsureSHARCombinedPostProcessQuad())
        return false;

    GLint previousProgram = 0;
    const GLint previousVao = SHARGetBoundVertexArray();
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);

    const GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    const GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
    const GLboolean scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    const GLboolean stencilEnabled = glIsEnabled(GL_STENCIL_TEST);

    // The fetch shader reads the current framebuffer pixel itself, so it needs
    // no snapshot texture, glCopyTexSubImage2D, or texture sampling.
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glUseProgram(gSHARSaturationFetchProgram);
    if (gSHARSaturationFetchLocation >= 0)
        glUniform1f(gSHARSaturationFetchLocation, saturation);
    SHARBindVertexArray(gCRTVao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    SHARBindVertexArray((GLuint)previousVao);

    glUseProgram((GLuint)previousProgram);
    if (depthEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (blendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (cullEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (scissorEnabled) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    if (stencilEnabled) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);

    return true;
}

static void ApplyCombinedFrameFilter(int width, int height)
{
    if (width <= 0 || height <= 0)
        return;

    bool crtEnabled = false;
    float scanlineStrength = 0.35f, curvature = 0.15f, glowStrength = 0.10f;
    float cornerDarkening = 0.05f, overscan = 0.0f, brightness = 1.0f, contrast = 1.0f;
    GetCRTFilterConfiguration(&crtEnabled, &scanlineStrength, &curvature, &glowStrength,
                              &cornerDarkening, &overscan, &brightness, &contrast);
    const float sharpen = IsFrameSharpeningEnabled() ? GetFrameSharpenStrength() : 0.0f;
    const float saturation = GetSHARGlobalSaturationFactor();

    // When no CRT or sharpening kernel is active, update the current pixel
    // in-place through framebuffer fetch instead of copying the whole frame.
    if (!crtEnabled && sharpen <= 0.0f &&
        ApplySHARSaturationFramebufferFetch(width, height, saturation))
    {
        return;
    }

    if (gFrameSharpenFailedWidth == width && gFrameSharpenFailedHeight)
        return;

    if (!CheckSHARRenderTargetSize(
            width, height, "combined sharpening/CRT texture", 6, false, true))
    {
        gFrameSharpenFailedWidth = width;
        gFrameSharpenFailedHeight = height;
        return;
    }

    if (gCRTProgram == 0)
    {
        const char* vertexSource =
            "attribute vec2 position; varying vec2 texcoord;\n"
            "void main() { texcoord = position * 0.5 + 0.5; gl_Position = vec4(position, 0.0, 1.0); }\n";
        const char* fragmentSource =
            "#ifdef GL_FRAGMENT_PRECISION_HIGH\n"
            "precision highp float;\n"
            "#else\n"
            "precision mediump float;\n"
            "#endif\n"
            "uniform sampler2D frameTex; uniform vec2 texelSize;\n"
            "uniform float sharpen; uniform float saturation;\n"
            "uniform float scanlineStrength; uniform float curvature; uniform float glowStrength;\n"
            "uniform float cornerDarkening; uniform float overscan; uniform float brightness; uniform float contrast;\n"
            "uniform float crtEnabled;\n"
            "varying vec2 texcoord;\n"
            "vec3 applySaturation(vec3 color) {\n"
            "    float luma = dot(color, vec3(0.299, 0.587, 0.114));\n"
            "    return mix(vec3(luma), color, clamp(saturation, 1.0, 1.5));\n"
            "}\n"
            "vec3 sampleSharpened(vec2 uv) {\n"
            "    vec3 centre = texture2D(frameTex, uv).rgb;\n"
            "    float amount = clamp(sharpen, 0.0, 1.0) * 0.9;\n"
            "    if (amount > 0.0) {\n"
            "        vec3 north = texture2D(frameTex, uv + vec2(0.0, -texelSize.y)).rgb;\n"
            "        vec3 west  = texture2D(frameTex, uv + vec2(-texelSize.x, 0.0)).rgb;\n"
            "        vec3 east  = texture2D(frameTex, uv + vec2(texelSize.x, 0.0)).rgb;\n"
            "        vec3 south = texture2D(frameTex, uv + vec2(0.0, texelSize.y)).rgb;\n"
            "        vec3 average = (north + west + east + south) * 0.25;\n"
            "        centre = clamp(centre + (centre - average) * amount, 0.0, 1.0);\n"
            "    }\n"
            "    return applySaturation(centre);\n"
            "}\n"
            "void main() {\n"
            "    vec2 uv = texcoord;\n"
            "    if (crtEnabled > 0.5) {\n"
            "        uv = (texcoord - 0.5) * (1.0 + clamp(overscan, 0.0, 0.05) * 2.0) + 0.5;\n"
            "        vec2 p = uv * 2.0 - 1.0;\n"
            "        float curve = clamp(curvature, 0.0, 0.5);\n"
            "        uv = (p * (1.0 + curve * vec2(p.y * p.y, p.x * p.x))) * 0.5 + 0.5;\n"
            "        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) {\n"
            "            gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
            "            return;\n"
            "        }\n"
            "    }\n"
            "    vec3 color = sampleSharpened(uv);\n"
            "    if (crtEnabled > 0.5) {\n"
            "        // Direct source taps avoid running the sharpening kernel four more times.\n"
            "        if (glowStrength > 0.0) {\n"
            "            vec3 glow = (texture2D(frameTex, uv + vec2(texelSize.x * 1.5, 0.0)).rgb +\n"
            "                         texture2D(frameTex, uv - vec2(texelSize.x * 1.5, 0.0)).rgb +\n"
            "                         texture2D(frameTex, uv + vec2(0.0, texelSize.y * 1.5)).rgb +\n"
            "                         texture2D(frameTex, uv - vec2(0.0, texelSize.y * 1.5)).rgb) * 0.25;\n"
            "            glow = applySaturation(glow);\n"
            "            color += max(glow - vec3(0.68), vec3(0.0)) * clamp(glowStrength, 0.0, 1.0) * 0.35;\n"
            "        }\n"
            "        float scan = 1.0 - mod(floor(gl_FragCoord.y), 2.0);\n"
            "        color *= 1.0 - clamp(scanlineStrength, 0.0, 1.0) * (0.22 * (1.0 - scan));\n"
            "        float edge = clamp(length((uv * 2.0 - 1.0) * vec2(0.82, 1.0)), 0.0, 1.4);\n"
            "        color *= 1.0 - clamp(cornerDarkening, 0.0, 1.0) * smoothstep(0.55, 1.15, edge);\n"
            "        color = (color - vec3(0.5)) * clamp(contrast, 0.5, 1.5) + vec3(0.5);\n"
            "        color *= clamp(brightness, 0.5, 1.5);\n"
            "    }\n"
            "    gl_FragColor = vec4(clamp(color, 0.0, 1.0), 1.0);\n"
            "}\n";

        gCRTProgram = CreateCelPostProgram(
            vertexSource, fragmentSource, "Combined sharpening, saturation and CRT filter");
        if (gCRTProgram != 0)
        {
            gCRTSceneLocation = glGetUniformLocation(gCRTProgram, "frameTex");
            gCRTTexelLocation = glGetUniformLocation(gCRTProgram, "texelSize");
            gCRTScanlineLocation = glGetUniformLocation(gCRTProgram, "scanlineStrength");
            gCRTCurvatureLocation = glGetUniformLocation(gCRTProgram, "curvature");
            gCRTGlowLocation = glGetUniformLocation(gCRTProgram, "glowStrength");
            gCRTCornersLocation = glGetUniformLocation(gCRTProgram, "cornerDarkening");
            gCRTOverscanLocation = glGetUniformLocation(gCRTProgram, "overscan");
            gCRTBrightnessLocation = glGetUniformLocation(gCRTProgram, "brightness");
            gCRTContrastLocation = glGetUniformLocation(gCRTProgram, "contrast");
            gCRTCrtEnabledLocation = glGetUniformLocation(gCRTProgram, "crtEnabled");
            gCRTSaturationLocation = glGetUniformLocation(gCRTProgram, "saturation");
            gCRTSharpenLocation = glGetUniformLocation(gCRTProgram, "sharpen");
        }
    }

    if (gCRTProgram == 0)
    {
        gFrameSharpenFailedWidth = width;
        gFrameSharpenFailedHeight = height;
        return;
    }

    if (gCRTVbo == 0)
        glGenBuffers(1, &gCRTVbo);
    if (gCRTVao == 0)
        SHARGenVertexArrays(1, &gCRTVao);
    if (gFrameSharpenTexture == 0)
        glGenTextures(1, &gFrameSharpenTexture);

    if (gCRTVbo == 0 || gCRTVao == 0 || gFrameSharpenTexture == 0)
    {
        gFrameSharpenFailedWidth = width;
        gFrameSharpenFailedHeight = height;
        return;
    }

    const GLint previousVao = SHARGetBoundVertexArray();
    if (!gCRTVaoReady)
    {
        const GLfloat quad[] = {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f};
        GLint previousArrayBuffer = 0;
        glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &previousArrayBuffer);
        SHARBindVertexArray(gCRTVao);
        glBindBuffer(GL_ARRAY_BUFFER, gCRTVbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (const GLvoid*)0);
        SHARBindVertexArray((GLuint)previousVao);
        glBindBuffer(GL_ARRAY_BUFFER, (GLuint)previousArrayBuffer);
        gCRTVaoReady = true;
    }

    // Caller already selected framebuffer 0 and the output viewport. This pass
    // changes only texture unit 0, its private VAO and the active program.
    GLint previousProgram = 0;
    GLint previousActiveTexture = GL_TEXTURE0;
    GLint previousTexture0 = 0;
    glGetIntegerv(GL_CURRENT_PROGRAM, &previousProgram);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &previousActiveTexture);
    glActiveTexture(GL_TEXTURE0);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture0);

    glBindTexture(GL_TEXTURE_2D, gFrameSharpenTexture);
    if (gFrameSharpenWidth != width || gFrameSharpenHeight != height)
    {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
        if (!CheckSHARAllocationError("combined sharpening/CRT texture", width, height))
        {
            glBindTexture(GL_TEXTURE_2D, (GLuint)previousTexture0);
            glActiveTexture((GLenum)previousActiveTexture);
            gFrameSharpenFailedWidth = width;
            gFrameSharpenFailedHeight = height;
            return;
        }
        gFrameSharpenWidth = width;
        gFrameSharpenHeight = height;
        gFrameSharpenFailedWidth = 0;
        gFrameSharpenFailedHeight = 0;
    }

    const GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    const GLboolean blendEnabled = glIsEnabled(GL_BLEND);
    const GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
    const GLboolean scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    const GLboolean stencilEnabled = glIsEnabled(GL_STENCIL_TEST);

    glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, width, height);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);

    glUseProgram(gCRTProgram);
    glUniform1i(gCRTSceneLocation, 0);
    if (gCRTTexelLocation >= 0)
        glUniform2f(gCRTTexelLocation, 1.0f / (float)width, 1.0f / (float)height);
    if (gCRTScanlineLocation >= 0)
        glUniform1f(gCRTScanlineLocation, scanlineStrength);
    if (gCRTCurvatureLocation >= 0)
        glUniform1f(gCRTCurvatureLocation, curvature);
    if (gCRTGlowLocation >= 0)
        glUniform1f(gCRTGlowLocation, glowStrength);
    if (gCRTCornersLocation >= 0)
        glUniform1f(gCRTCornersLocation, cornerDarkening);
    if (gCRTOverscanLocation >= 0)
        glUniform1f(gCRTOverscanLocation, overscan);
    if (gCRTBrightnessLocation >= 0)
        glUniform1f(gCRTBrightnessLocation, brightness);
    if (gCRTContrastLocation >= 0)
        glUniform1f(gCRTContrastLocation, contrast);
    if (gCRTCrtEnabledLocation >= 0)
        glUniform1f(gCRTCrtEnabledLocation, crtEnabled ? 1.0f : 0.0f);
    if (gCRTSaturationLocation >= 0)
        glUniform1f(gCRTSaturationLocation, saturation);
    if (gCRTSharpenLocation >= 0)
        glUniform1f(gCRTSharpenLocation, sharpen);

    SHARBindVertexArray(gCRTVao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    SHARBindVertexArray((GLuint)previousVao);

    glUseProgram((GLuint)previousProgram);
    glBindTexture(GL_TEXTURE_2D, (GLuint)previousTexture0);
    glActiveTexture((GLenum)previousActiveTexture);
    if (depthEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (blendEnabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (cullEnabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (scissorEnabled) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    if (stencilEnabled) glEnable(GL_STENCIL_TEST); else glDisable(GL_STENCIL_TEST);
}

#ifdef RAD_ANDROID
// Run full-frame filters after the game/UI scene is composed but before the
// touch HUD is drawn. This keeps CRT curvature and sharpening from moving or
// blurring the on-screen controls. SwapBuffers calls this as a fallback for
// frames that do not render either touch-HUD path.
void ApplySHARPostProcessingBeforeTouchHud()
{
    if (gSHARPostProcessAppliedBeforeTouchHud)
        return;

    if (!gSHARVertexArrayObjectsSupported)
    {
        gSHARPostProcessAppliedBeforeTouchHud = true;
        return;
    }

    int outputWidth = gSHARFrameWidth > 0 ? gSHARFrameWidth : 0;
    int outputHeight = gSHARFrameHeight > 0 ? gSHARFrameHeight : 0;
    QueryDefaultFramebufferSize(&outputWidth, &outputHeight);
    if (outputWidth <= 0 || outputHeight <= 0)
        return;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, outputWidth, outputHeight);

    ApplyCombinedFrameFilter(outputWidth, outputHeight);
    InvalidateSHARRenderStateCache();
    gSHARPostProcessAppliedBeforeTouchHud = true;
}
#endif

extern "C" JNIEXPORT void JNICALL
Java_org_libsdl_app_SDLSurface_nativeSetSHARRenderResolution(
    JNIEnv* env,
    jclass clazz,
    jint width,
    jint height
)
{
    if (width <= 0 || height <= 0)
        return;

    // Java surface callbacks can run on the UI thread while SDL reads this
    // pair on the render/event thread. Keep the dimensions as one consistent
    // snapshot and never call back into Java while holding this short lock.
    SDL_AtomicLock(&gSHARAndroidRenderResolutionLock);
    const int previousWidth = gSHARAndroidRenderWidth;
    const int previousHeight = gSHARAndroidRenderHeight;
    const bool changed =
        previousWidth != (int)width || previousHeight != (int)height;

    gSHARAndroidRenderWidth = (int)width;
    gSHARAndroidRenderHeight = (int)height;
    if (gSHARDynamicResolutionMaxHeight <= 0)
    {
        gSHARDynamicResolutionMaxHeight = (int)height;
        gSHARDynamicResolutionMinHeight =
            SHARGetDynamicResolutionMinimumHeight((int)height);
    }
    gSHARDynamicResolutionCurrentHeight = (int)height;
    if (changed)
        ++gSHARAndroidRenderResizeSerial;
    SDL_AtomicUnlock(&gSHARAndroidRenderResolutionLock);

}

// Request a preferred Android output refresh rate on API 30+ devices.
// The selected FPS cap still functions if Android cannot or chooses not to change modes.
bool RequestSHARAndroidDisplayFrameRate(int targetFPS)
{
#if defined(RAD_ANDROID)
    if( targetFPS != 30 && targetFPS != 60 && targetFPS != 90 &&
        targetFPS != 120 && targetFPS != 144 )
    {
        return false;
    }

    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    if( env == NULL )
    {
        return false;
    }

    jobject activity = (jobject)SDL_AndroidGetActivity();
    if( activity == NULL )
    {
        return false;
    }

    jclass activityClass = env->GetObjectClass(activity);
    jclass sdlActivityClass = activityClass != NULL
        ? env->GetSuperclass(activityClass) : NULL;
    if( env->ExceptionCheck() )
    {
        env->ExceptionClear();
        sdlActivityClass = NULL;
    }

    jmethodID requestMethod = sdlActivityClass != NULL
        ? env->GetStaticMethodID(
            sdlActivityClass, "requestSHARDisplayFrameRate", "(I)Z" )
        : NULL;
    if( env->ExceptionCheck() )
    {
        env->ExceptionClear();
        requestMethod = NULL;
    }

    bool queued = false;
    if( requestMethod != NULL )
    {
        const jboolean result = env->CallStaticBooleanMethod(
            sdlActivityClass, requestMethod, (jint)targetFPS );
        if( env->ExceptionCheck() )
        {
            env->ExceptionClear();
        }
        else
        {
            queued = (result == JNI_TRUE);
        }
    }

    if( sdlActivityClass != NULL ) env->DeleteLocalRef(sdlActivityClass);
    if( activityClass != NULL ) env->DeleteLocalRef(activityClass);
    env->DeleteLocalRef(activity);
    return queued;
#else
    (void)targetFPS;
    return false;
#endif
}

// Called by the in-game Graphics menu. This only requests a Java/UI-thread
// Surface resize; rendering and GL resource changes continue through SDL's
// regular surface/window resize handling.
bool RequestSHARAndroidRenderResolutionHeight(int targetHeight)
{
#if defined(RAD_ANDROID)
    if (targetHeight <= 0)
        return false;

    JNIEnv* env = (JNIEnv*)SDL_AndroidGetJNIEnv();
    if (env == NULL)
    {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "SHAR live resolution request failed: no JNI environment");
        return false;
    }

    jobject activity = (jobject)SDL_AndroidGetActivity();
    if (activity == NULL)
    {
        SDL_LogError(
            SDL_LOG_CATEGORY_APPLICATION,
            "SHAR live resolution request failed: Android activity unavailable");
        return false;
    }

    jclass activityClass = env->GetObjectClass(activity);
    jclass sdlActivityClass = activityClass != NULL
        ? env->GetSuperclass(activityClass) : NULL;

    if (env->ExceptionCheck())
    {
        env->ExceptionClear();
        sdlActivityClass = NULL;
    }

    jmethodID requestMethod = sdlActivityClass != NULL
        ? env->GetStaticMethodID(
            sdlActivityClass,
            "requestSHARRenderResolutionHeight",
            "(I)Z")
        : NULL;

    if (env->ExceptionCheck())
    {
        env->ExceptionClear();
        requestMethod = NULL;
    }

    bool queued = false;
    if (requestMethod != NULL)
    {
        const jboolean result = env->CallStaticBooleanMethod(
            sdlActivityClass,
            requestMethod,
            (jint)targetHeight);

        if (env->ExceptionCheck())
        {
            env->ExceptionClear();
        }
        else
        {
            queued = (result == JNI_TRUE);
        }
    }

    if (!queued)
    {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
            "Unable to queue live render resolution request for targetHeight=%d; saved setting will apply at next launch",
            targetHeight);
    }

    if (sdlActivityClass != NULL)
        env->DeleteLocalRef(sdlActivityClass);
    if (activityClass != NULL)
        env->DeleteLocalRef(activityClass);
    env->DeleteLocalRef(activity);
    return queued;
#else
    (void)targetHeight;
    return false;
#endif
}

static bool
GetSHARAndroidRenderResolution(int* width, int* height)
{
    if (width == NULL || height == NULL)
        return false;

    SDL_AtomicLock(&gSHARAndroidRenderResolutionLock);
    const int renderWidth = gSHARAndroidRenderWidth;
    const int renderHeight = gSHARAndroidRenderHeight;
    SDL_AtomicUnlock(&gSHARAndroidRenderResolutionLock);

    if (renderWidth <= 0 || renderHeight <= 0)
        return false;

    *width = renderWidth;
    *height = renderHeight;

    return true;
}

#endif

#ifdef RAD_ANDROID
void pglDisplay::CommitPendingResize()
{
    static int committedSerial = 0;
    InvalidateSHARDefaultFramebufferSizeCache();
    int requestedWidth = 0;
    int requestedHeight = 0;
    int serial = 0;

    SDL_AtomicLock(&gSHARAndroidRenderResolutionLock);
    requestedWidth = gSHARAndroidRenderWidth;
    requestedHeight = gSHARAndroidRenderHeight;
    serial = gSHARAndroidRenderResizeSerial;
    SDL_AtomicUnlock(&gSHARAndroidRenderResolutionLock);

    if (requestedWidth <= 0 || requestedHeight <= 0)
        return;

    if (serial == committedSerial)
    {
        // The previous frame may have temporarily exposed output dimensions to
        // the GUI. Restore the internal world target before the next frame.
        winWidth = requestedWidth;
        winHeight = requestedHeight;
        return;
    }


    winWidth = requestedWidth;
    winHeight = requestedHeight;
    committedSerial = serial;

    // Only the offscreen world target changes here. The Android output Surface
    // remains platform-sized, so there is no EGL size match/wait to perform.
    ResetAndroidPostProcess();

    int outputWidth = winWidth;
    int outputHeight = winHeight;
    QueryDefaultFramebufferSize(&outputWidth, &outputHeight);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, outputWidth, outputHeight);
}

#endif

bool pglDisplay::CheckExtension( const char *extName )
{
    return SDL_GL_ExtensionSupported(extName);
}

pglDisplay ::pglDisplay(pddiDisplayInfo* info)
{
    displayInfo = info;
    mode = PDDI_DISPLAY_WINDOW;
    winWidth = 640;
    winHeight = 480;
    winBitDepth = 32;

    context = NULL;

    win = NULL;
    hRC = NULL;
    prevRC = NULL;

    extBGRA = false;

    gammaR = gammaG = gammaB = 1.0f;

    reset = true;
	m_ForceVSync = false;
}

pglDisplay ::~pglDisplay()
{
#ifdef RAD_ANDROID
    SHARResetDynamicResolutionController();
    InvalidateSHARTrackedColorWriteMaskCache();
    InvalidateCelRenderTargetValidationCache();
    if (gCelRenderDepth)
    {
        glDeleteRenderbuffers(1, &gCelRenderDepth);
        gCelRenderDepth = 0;
    }
    if (gCelRenderDepthTexture)
    {
        glDeleteTextures(1, &gCelRenderDepthTexture);
        gCelRenderDepthTexture = 0;
    }
    if (gCelRenderFbo)
    {
        glDeleteFramebuffers(1, &gCelRenderFbo);
        gCelRenderFbo = 0;
    }
    if (gCelBloomFbo)
    {
        glDeleteFramebuffers(1, &gCelBloomFbo);
        gCelBloomFbo = 0;
    }
    if (gCelCloudShadowFbo)
    {
        glDeleteFramebuffers(1, &gCelCloudShadowFbo);
        gCelCloudShadowFbo = 0;
    }
    if (gCelCloudShadowTexture)
    {
        glDeleteTextures(1, &gCelCloudShadowTexture);
        gCelCloudShadowTexture = 0;
    }
    if (gCelCloudShadowProgram)
    {
        glDeleteProgram(gCelCloudShadowProgram);
        gCelCloudShadowProgram = 0;
    }
    if (gCelBloomOnlyPostProgram)
    {
        glDeleteProgram(gCelBloomOnlyPostProgram);
        gCelBloomOnlyPostProgram = 0;
    }
    gCelBloomOnlyPostProgramAttempted = false;
    gCelCloudShadowWidth = gCelCloudShadowHeight = 0;
    gCelCloudShadowFailedWidth = gCelCloudShadowFailedHeight = 0;
    gCelCloudShadowReady = false;
    if (gCelBloomTexture)
    {
        glDeleteTextures(1, &gCelBloomTexture);
        gCelBloomTexture = 0;
    }
    if (gCelBloomScratchTexture)
    {
        glDeleteTextures(1, &gCelBloomScratchTexture);
        gCelBloomScratchTexture = 0;
    }
    if (gCelBloomExtractProgram)
    {
        glDeleteProgram(gCelBloomExtractProgram);
        gCelBloomExtractProgram = 0;
    }
    if (gCelBloomBlurProgram)
    {
        glDeleteProgram(gCelBloomBlurProgram);
        gCelBloomBlurProgram = 0;
    }
    gCelBloomReady = false;
    if (gFrameSharpenTexture)
    {
        glDeleteTextures(1, &gFrameSharpenTexture);
        gFrameSharpenTexture = 0;
    }
    if (gCRTProgram)
    {
        glDeleteProgram(gCRTProgram);
        gCRTProgram = 0;
    }
    if (gSHARSaturationFetchProgram)
    {
        glDeleteProgram(gSHARSaturationFetchProgram);
        gSHARSaturationFetchProgram = 0;
    }
    gSHARSaturationFetchLocation = -1;
    gSHARSaturationFetchMode = 0;
    gSHARSaturationFetchChecked = false;
    if (gCRTVbo)
    {
        glDeleteBuffers(1, &gCRTVbo);
        gCRTVbo = 0;
    }
    if (gCRTVao != 0 && gSHARDeleteVertexArraysProc != NULL)
    {
        gSHARDeleteVertexArraysProc(1, &gCRTVao);
        gCRTVao = 0;
    }
    gCRTVaoReady = false;
#endif

    /* release and free the device context and rendering context */
#if SDL_MAJOR_VERSION < 3
    SDL_GL_DeleteContext(hRC);
    SDL_SetWindowGammaRamp(win, initialGammaRamp[0], initialGammaRamp[1], initialGammaRamp[2]);
#else
    SDL_GL_DestroyContext((SDL_GLContext)hRC);
#endif
}

#define KEYPRESSED(x) (GetKeyState((x)) & (1<<(sizeof(int)*8)-1))

long pglDisplay ::ProcessWindowMessage(SDL_Window* win, const SDL_WindowEvent* event)
{
#if SDL_MAJOR_VERSION < 3
    switch(event->event)
    {
        case SDL_WINDOWEVENT_SIZE_CHANGED:
        {
#if defined(RAD_ANDROID)
            // Android Surface callbacks run on the UI thread. Do not mutate
            // active PDDI dimensions or call OpenGL here; the GL thread commits
            // the latest confirmed target from BeginFrame().
#else
            SDL_GL_GetDrawableSize(win, &winWidth, &winHeight);
#endif
            break;
        }

        case SDL_WINDOWEVENT_CLOSE:
            /* release and free the device context and rendering context */
            SDL_GL_DeleteContext(hRC);
            break;

        default:
            return 0;
    }
#else
    switch(event->type)
    {
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        {
#if defined(RAD_ANDROID)
            // Defer active-size mutation and GL calls to CommitPendingResize()
            // on the render thread at the start of a frame.
#else
            SDL_GetWindowSizeInPixels(win, &winWidth, &winHeight);
#endif
            break;
        }

        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            /* release and free the device context and rendering context */
            SDL_GL_DestroyContext((SDL_GLContext)hRC);
            break;

        default:
            return 0;
    }
#endif

    /* return 1 if handled message, 0 if not */
    return 1;
}

void pglDisplay ::SetWindow(SDL_Window* wnd)
{
#if SDL_MAJOR_VERSION < 3
    SDL_GetWindowGammaRamp(wnd, initialGammaRamp[0], initialGammaRamp[1], initialGammaRamp[2]);
#endif
    win = wnd;
}

bool pglDisplay ::InitDisplay(int x, int y, int bpp)
{
    // check we are not trying to init to the same resolution
    if((x == winWidth) &&  (y == winHeight) && (bpp == winBitDepth))
    {
        return true;
    }

    // fill in the relevent portions of the casced display init structure
    displayInit.xsize = x;
    displayInit.ysize = y;
    displayInit.bpp = bpp;

    // do the full init
    return InitDisplay(&displayInit);
}

#ifdef RAD_DEBUG
void GLAPIENTRY
MessageCallback(GLenum source,
    GLenum type,
    GLuint id,
    GLenum severity,
    GLsizei length,
    const GLchar* message,
    const void* userParam)
{
    switch(severity)
    {
        case GL_DEBUG_SEVERITY_HIGH_KHR:
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", message);
            break;
        case GL_DEBUG_SEVERITY_MEDIUM_KHR:
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", message);
            break;
        case GL_DEBUG_SEVERITY_LOW_KHR:
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s", message);
            break;
        case GL_DEBUG_SEVERITY_NOTIFICATION_KHR:
            SDL_LogVerbose(SDL_LOG_CATEGORY_APPLICATION, "%s", message);
            break;
    }
}
#endif

bool pglDisplay ::InitDisplay(const pddiDisplayInit* init)
{
#ifdef RAD_ANDROID
    gSHARActiveDisplayWidthForGui = &winWidth;
    gSHARActiveDisplayHeightForGui = &winHeight;
#endif
    displayInit = *init;

    int x = init->xsize;
    int y = init->ysize;
    int bpp = init->bpp;
    pddiDisplayMode m = init->displayMode;
    int colourBufferCount = 1;
    unsigned bufMask = init->bufferMask;
    unsigned nSamples = 0;

    reset = true;

    mode = m;
    SDL_DisplayMode displayMode = {}, closestMode = {};
    displayMode.w = x;
    displayMode.h = y;
#if SDL_MAJOR_VERSION < 3
    SDL_DisplayMode* pDisplayMode = SDL_GetClosestDisplayMode(displayInfo->id, &displayMode, &closestMode);
    if (pDisplayMode)
        SDL_SetWindowDisplayMode(win, pDisplayMode);
#else
    if(SDL_GetClosestFullscreenDisplayMode((SDL_DisplayID)displayInfo->id, x, y, 0.0f, false, &closestMode))
        SDL_SetWindowFullscreenMode(win, &closestMode);
#endif

#ifndef __SWITCH__
    SDL_SetWindowFullscreen(win, mode == PDDI_DISPLAY_FULLSCREEN ? SDL_WINDOW_FULLSCREEN : 0);
#endif

#if defined(RAD_ANDROID) || defined(__ANDROID__)
    /*
     * Android adaptive render resolution:
     *
     * SDLSurface.java sends the selected internal world-render size through
     * nativeSetSHARRenderResolution(). The Android output Surface remains at its
     * platform size; this target is used for world projection, viewport and scissor.
     * After scene composition, ApplyCelPostProcessBeforeGui temporarily changes the
     * PDDI dimensions to output size for GUI drawing. CommitPendingResize restores
     * the internal render dimensions at the next frame start.
     */
    {
        int renderW = 0;
        int renderH = 0;

        if (GetSHARAndroidRenderResolution(&renderW, &renderH)) {
            winWidth = renderW;
            winHeight = renderH;

        } else {
#if SDL_MAJOR_VERSION < 3
            SDL_GL_GetDrawableSize(win, &winWidth, &winHeight);
#else
            SDL_GetWindowSizeInPixels(win, &winWidth, &winHeight);
#endif
        }
    }
#else
#if SDL_MAJOR_VERSION < 3
    SDL_GL_GetDrawableSize( win, &winWidth, &winHeight );
#else
    SDL_GetWindowSizeInPixels( win, &winWidth, &winHeight );
#endif
#endif

    winBitDepth = bpp;

    if (hRC)
    {
        return true;
    }

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, bpp == 16 ? 5 : 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, bpp == 16 ? 6 : 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, bpp == 16 ? 5 : 8);
    if (init->bufferMask & PDDI_BUFFER_DEPTH)
#ifdef __SWITCH__
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, init->bufferMask & PDDI_BUFFER_STENCIL ? 24 : 32);
#else
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
#endif
    else
        SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
    if (init->bufferMask & PDDI_BUFFER_STENCIL)
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    else
        SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
#ifndef RAD_VITA
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, true);
#ifdef RAD_DEBUG
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
#endif
#endif

    prevRC = SDL_GL_GetCurrentContext();
    hRC = SDL_GL_CreateContext(win);
#ifdef RAD_ANDROID
    if (hRC == NULL && gSHARActiveOpenGLESVersionMajor >= 3)
    {
        const char* es3Error = SDL_GetError();
        SDL_LogError(SDL_LOG_CATEGORY_RENDER,
            "[GRAPHICS] OpenGL ES 3.0 context creation failed: %s; retrying with ES 2.0",
            es3Error != NULL ? es3Error : "unknown SDL/EGL error");

        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
        gSHARActiveOpenGLESVersionMajor = 2;
        hRC = SDL_GL_CreateContext(win);
        if (hRC != NULL)
            SDL_Log("[GRAPHICS] OpenGL ES 2.0 fallback context created successfully");
    }

    if (hRC == NULL)
        SDL_LogError(SDL_LOG_CATEGORY_RENDER,
            "[GRAPHICS] OpenGL ES context creation failed: %s", SDL_GetError());
#else
    if (!hRC)
        SDL_Log("SDL_GL_CreateContext() error: %s", SDL_GetError());
#endif
    PDDIASSERT(hRC);

#ifdef RAD_CG
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
        return false;
#else
    if (!gladLoadGLES2Loader((GLADloadproc)SDL_GL_GetProcAddress))
        return false;
#endif

    char* glExtensions = (char*)glGetString(GL_EXTENSIONS);

    static bool doExtensions = false;

    if(doExtensions)
    {
        doExtensions = false;
        char* buffer = new char[strlen(glExtensions) + 2];
        strcpy(buffer, glExtensions);

        char* walk = buffer;
        char* last = buffer;
        while(*walk)
        {
            if(*walk == ' ')
            {
                *walk = 0;
                SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "%s", last);
                last = walk+1;
            }
            walk++;
        }
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "%s", last);
    }

    extBGRA = CheckExtension("GL_EXT_bgra") || CheckExtension("GL_EXT_texture_format_BGRA8888");

#ifdef RAD_ANDROID
    SHARInitializeGraphicsCapabilities();
#endif

#if defined RAD_DEBUG && !defined RAD_VITA
    glEnable(GL_DEBUG_OUTPUT_KHR);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_KHR);
    glDebugMessageCallback(MessageCallback, NULL);
#endif

    return true;
}

pddiDisplayInfo* pglDisplay ::GetDisplayInfo(void)
{
    return displayInfo;
}

unsigned pglDisplay ::GetFreeTextureMem()
{
    return unsigned(-1);
}

unsigned pglDisplay ::GetBufferMask()
{
    return unsigned(-1);
}

int pglDisplay ::GetHeight()
{
    return winHeight;
}

int pglDisplay ::GetWidth()
{
    return winWidth;
}

int pglDisplay::GetDepth()
{
    return winBitDepth;
}

pddiDisplayMode pglDisplay::GetDisplayMode(void)
{
    return mode;
}

int pglDisplay::GetNumColourBuffer(void)
{
    return 2;
}

void pglDisplay::GetGamma(float* r, float* g, float* b)
{
    *r = gammaR;
    *g = gammaG;
    *b = gammaB;
}

void pglDisplay::SetGamma(float r, float g, float b)
{
    gammaR = r;
    gammaG = g;
    gammaB = b;

    Uint16 gamma[3][256];

    double igr = 1.0 / (double)r;
    double igg = 1.0 / (double)g;
    double igb = 1.0 / (double)b;

    const double n = 1.0 / 65535.0;

    for(int i=0; i < 256; i++)
    {
        double gcr = pow((double)initialGammaRamp[0][i]   * n, igr);
        double gcg = pow((double)initialGammaRamp[1][i] * n, igg);
        double gcb = pow((double)initialGammaRamp[2][i]  * n, igb);

        gamma[0][i] =   (Uint16)(65535.0 * ((1.0 < gcr) ? 1.0 : gcr));
        gamma[1][i] = (Uint16)(65535.0 * ((1.0 < gcg) ? 1.0 : gcg));
        gamma[2][i] =  (Uint16)(65535.0 * ((1.0 < gcb) ? 1.0 : gcb));
    }

#if SDL_MAJOR_VERSION < 3
    SDL_SetWindowGammaRamp(win, gamma[0], gamma[1], gamma[2]);
#endif
}

#ifdef RAD_ANDROID
void SHARMarkFrameRenderedForPresentation(void)
{
    gSHARFrameReadyForPresentation = true;
}
#endif

void pglDisplay::SwapBuffers(void)
{
#ifdef RAD_ANDROID

    // RenderManager finishes a frame before marking it ready. SwapBuffers runs
    // at the start of the next render cycle, so don't filter or present stale
    // buffers when the renderer has not produced a new frame.
    const bool hasFreshFrame = gSHARFrameReadyForPresentation;
    gSHARFrameReadyForPresentation = false;
    if (hasFreshFrame)
    {
        // Apply optional filters before touch controls if they have not already
        // run in a touch-HUD rendering path.
        ApplySHARPostProcessingBeforeTouchHud();
        // Global saturation is folded into the pre-HUD sharpening pass, so there
        // is no separate full-screen saturation pass during presentation.
        SDL_GL_SwapWindow(win);
        SHAREndDynamicResolutionCpuFrameSample(true);
    }
    else
    {
        SHAREndDynamicResolutionCpuFrameSample(false);
    }
#else
    SDL_GL_SwapWindow(win);
#endif
    reset = false;
#ifdef RAD_ANDROID
    /*
     * Selectable Android gameplay FPS cap.
     *
     * m_only60 is retained as the legacy switch:
     * - true  = apply the selected gameplay FPS cap
     * - false = FMV/cinematics or special states, do not force this cap here
     */
    static Uint64 sCapFreq = 0;
    static Uint64 sLastFrameTime = 0;

    if (sCapFreq == 0)
    {
        sCapFreq = SDL_GetPerformanceFrequency();
    }

    if (m_only60)
    {
        const int configuredFPS = gSHARAndroidTargetFPS;
        const int targetFPS =
            (configuredFPS >= 30 && configuredFPS <= 144) ? configuredFPS : 60;
        const Uint64 targetTicks =
            (Uint64)((double)sCapFreq / (double)targetFPS + 0.5);

        Uint64 now = SDL_GetPerformanceCounter();

        if (sLastFrameTime == 0)
        {
            sLastFrameTime = now;
        }
        else
        {
            const Uint64 targetTime = sLastFrameTime + targetTicks;

            if (now < targetTime)
            {
                Uint64 remainingTicks = targetTime - now;

                double remainingMs =
                    (double)remainingTicks * 1000.0 / (double)sCapFreq;

                /*
                 * Coarse wait.
                 * Sleep most of the remaining time, leaving a small margin
                 * because SDL_Delay is not perfectly precise on Android.
                 */
                if (remainingMs > 2.0)
                {
                    SDL_Delay((Uint32)(remainingMs - 1.0));
                }

                /*
                 * Fine wait.
                 * Not a pure busy-wait: SDL_Delay(0) yields to the scheduler.
                 * This usually only runs for a tiny fraction of a millisecond.
                 */
                while (SDL_GetPerformanceCounter() < targetTime)
                {
                    SDL_Delay(0);
                }

                now = SDL_GetPerformanceCounter();
            }

            /*
             * Important:
             * Use the real current time, not targetTime.
             *
             * This avoids "catch-up" frames after a small hitch.
             * For this old game, we prefer never producing a too-fast frame.
             */
            sLastFrameTime = now;
        }
    }
    else
    {
        /*
         * If 60 FPS cap is disabled, reset the cap timer.
         *
         * This is important for FMV/cinematics:
         * when the game returns from a 30 FPS movie to gameplay,
         * we do not want to carry an old timestamp.
         */
        sLastFrameTime = 0;
    }

#endif

}

    
unsigned pglDisplay::Screenshot(pddiColour* buffer, int nBytes)
{
    if (buffer == NULL || nBytes <= 0 || winWidth <= 0 || winHeight <= 0)
        return 0;

    // Calculate sizes without signed-int overflow. The previous fixed
    // unsigned tmp[2048] row overflowed at 2048-pixel widths (including 1440p
    // and 2160p landscape render targets).
    const size_t rowBytes = (size_t)winWidth * 4;
    if (rowBytes / 4 != (size_t)winWidth ||
        (size_t)winHeight > ((size_t)-1) / rowBytes)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_RENDER,
            "SHAR screenshot refused: image dimensions overflow size_t (%dx%d)",
            winWidth, winHeight);
        return 0;
    }

    const size_t requiredBytes = rowBytes * (size_t)winHeight;
    if (requiredBytes > (size_t)nBytes || requiredBytes > (size_t)~0U)
        return 0;

    unsigned char* temporaryRow =
        (unsigned char*)malloc(rowBytes);
    if (temporaryRow == NULL)
    {
        SDL_LogWarn(SDL_LOG_CATEGORY_RENDER,
            "SHAR screenshot refused: could not allocate %u-byte row buffer",
            (unsigned)rowBytes);
        return 0;
    }

    glReadPixels(0, 0, winWidth, winHeight,
                 GL_BGRA_EXT, GL_UNSIGNED_BYTE, buffer);
#if defined(RAD_ANDROID)
    if (!CheckSHARAllocationError("screenshot readback", winWidth, winHeight))
    {
        free(temporaryRow);
        return 0;
    }
#endif

    for (int row = 0; row < winHeight / 2; ++row)
    {
        unsigned char* top = (unsigned char*)(buffer + ((size_t)row * (size_t)winWidth));
        unsigned char* bottom = (unsigned char*)(buffer +
            ((size_t)(winHeight - 1 - row) * (size_t)winWidth));

        memcpy(temporaryRow, top, rowBytes);
        memcpy(top, bottom, rowBytes);
        memcpy(bottom, temporaryRow, rowBytes);
    }

    free(temporaryRow);
    return (unsigned)requiredBytes;
}

void pglDisplay::BeginTiming()
{
    beginTime = (float)SDL_GetTicks();
}

float pglDisplay::EndTiming()
{
    return (float)SDL_GetTicks() - beginTime;
}

void pglDisplay::BeginContext(void)
{
    prevRC = SDL_GL_GetCurrentContext();
    PDDIASSERT(prevRC != hRC);
    int error = SDL_GL_MakeCurrent(win, (SDL_GLContext)hRC);
    PDDIASSERT(!error);
}

void pglDisplay::EndContext(void)
{
    int error = SDL_GL_MakeCurrent(win, (SDL_GLContext)prevRC);
    PDDIASSERT(!error);
}

