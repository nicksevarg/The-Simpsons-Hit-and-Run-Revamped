//========================================================================
// Copyright (C) 2002 Radical Entertainment Ltd.  All rights reserved.
//
// File:        FrontEndRenderLayer.cpp
//
// Description: Implementation for FrontEndRenderLayer class.
//
// History:     + Initial Implementation -- Tony [6/05/2002]
//
//========================================================================

//========================================
// System Includes
//========================================
#include <raddebug.hpp>
#include <radtime.hpp>
#include <p3d/view.hpp>
#include <p3d/billboardobject.hpp>

//========================================
// Project Includes
//========================================
#include <render/RenderManager/FrontEndRenderLayer.h>
#include <debug/profiler.h>
#include <presentation/gui/guisystem.h>
#include <presentation/presentation.h>
#include <gameflow/gameflow.h>
#include <contexts/contextenum.h>
#include <contexts/gameplay/gameplaycontext.h>
#include <App.h>
#include <Project.h>
#include <Screen.h>
#include <Page.h>
#include <worldsim/coins/coinmanager.h>
#include <worldsim/coins/sparkle.h>
#ifdef RAD_WIN32
#include <input/inputmanager.h>
#endif
#ifdef RAD_ANDROID
#include <SDL.h>
#include <SDL_system.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstring>
#include <presentation/fmvplayer/fmvplayer.h>
#include <input/touch/touchhudrenderer.h>
void ApplySHARPostProcessingBeforeTouchHud();
void ApplySHARFrontendMenuFineDetail();
bool IsSHARFrontendMenuCelPassActive();

enum SHARFrontendPerfStage
{
    SHAR_FE_BACKGROUND = 0,
    SHAR_FE_FOREGROUND,
    SHAR_FE_DRAW_FRAME,
    SHAR_FE_FINE_DETAIL,
    SHAR_FE_POST_PROCESS,
    SHAR_FE_TOUCH_HUD,
    SHAR_FE_COIN_EFFECTS,
    SHAR_FE_STAGE_COUNT
};

static bool gSHARFrontendDiagnosticConfigLoaded = false;
static bool gSHARFrontendSkipBackground = false;
static unsigned long long gSHARFrontendPerfWindowStartUs = 0;
static unsigned long long gSHARFrontendPerfTotals[SHAR_FE_STAGE_COUNT] = {};
static unsigned long long gSHARFrontendPerfMaxima[SHAR_FE_STAGE_COUNT] = {};
static unsigned int gSHARFrontendPerfCalls[SHAR_FE_STAGE_COUNT] = {};
static unsigned int gSHARFrontendPerfFrames = 0;
static unsigned int gSHARFrontendPerfSplitFrames = 0;
static unsigned int gSHARFrontendPerfFullFrames = 0;
static unsigned int gSHARFrontendPerfBackgroundSkippedFrames = 0;

static void SHARLoadFrontendDiagnosticConfig()
{
    if (gSHARFrontendDiagnosticConfigLoaded)
        return;

    gSHARFrontendDiagnosticConfigLoaded = true;
    const char* basePath = SDL_AndroidGetApplicationExternalFilesPath();
    if (basePath == NULL || basePath[0] == '\0')
    {
        SDL_Log("[PERF][FRONTEND] diagnostic config unavailable; using normal background rendering");
        return;
    }

    char directory[512];
    int length = snprintf(directory, sizeof(directory), "%s/Graphics", basePath);
    if (length < 0 || length >= static_cast<int>(sizeof(directory)))
        return;

    struct stat directoryInfo;
    if (stat(directory, &directoryInfo) != 0)
        mkdir(directory, 0777);

    char path[512];
    length = snprintf(path, sizeof(path), "%s/Simpsons_frontend_diagnostics.txt", directory);
    if (length < 0 || length >= static_cast<int>(sizeof(path)))
        return;

    FILE* file = fopen(path, "r");
    if (file == NULL)
    {
        file = fopen(path, "w");
        if (file != NULL)
        {
            fprintf(file, "# Simpsons Hit & Run - Revamped! [Front-end diagnostics]\n");
            fprintf(file, "# Set skip_frontend_background=true to temporarily bypass Scrooby background drawing.\n");
            fprintf(file, "# Leave false for the normal rendering path. Restart the game after editing.\n");
            fprintf(file, "skip_frontend_background=false\n");
            fclose(file);
        }
        SDL_Log("[PERF][FRONTEND] created diagnostic config; skip_frontend_background=false path=%s", path);
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file) != NULL)
    {
        char* value = strchr(line, '=');
        if (value == NULL)
            continue;

        *value++ = '\0';
        while (*value == ' ' || *value == '\t')
            ++value;

        if (strcmp(line, "skip_frontend_background") != 0)
            continue;

        gSHARFrontendSkipBackground =
            strncmp(value, "true", 4) == 0 ||
            strncmp(value, "TRUE", 4) == 0 ||
            strncmp(value, "yes", 3) == 0 ||
            strncmp(value, "YES", 3) == 0 ||
            *value == '1' ||
            strncmp(value, "on", 2) == 0 ||
            strncmp(value, "ON", 2) == 0;
        break;
    }

    fclose(file);
    SDL_Log("[PERF][FRONTEND] diagnostic config skip_frontend_background=%s path=%s",
         gSHARFrontendSkipBackground ? "true" : "false", path);
}

static void SHARRecordFrontendStage(int stage, unsigned long long startUs)
{
    if (stage < 0 || stage >= SHAR_FE_STAGE_COUNT)
        return;

    const unsigned long long nowUs = (unsigned long long)radTimeGetMicroseconds64();
    const unsigned long long elapsedUs = nowUs >= startUs ? nowUs - startUs : 0;
    gSHARFrontendPerfTotals[stage] += elapsedUs;
    if (elapsedUs > gSHARFrontendPerfMaxima[stage])
        gSHARFrontendPerfMaxima[stage] = elapsedUs;
    ++gSHARFrontendPerfCalls[stage];
}

static unsigned long long SHARFrontendStageAverage(int stage)
{
    return gSHARFrontendPerfCalls[stage] > 0 ?
        gSHARFrontendPerfTotals[stage] / gSHARFrontendPerfCalls[stage] : 0;
}

static void SHARLogFrontendPerf()
{
    const unsigned long long nowUs = (unsigned long long)radTimeGetMicroseconds64();
    if (gSHARFrontendPerfWindowStartUs == 0)
        gSHARFrontendPerfWindowStartUs = nowUs;

    ++gSHARFrontendPerfFrames;
    if (nowUs - gSHARFrontendPerfWindowStartUs < 1000000ULL)
        return;

    SDL_Log("[PERF][FRONTEND] frames=%u split=%u full=%u bg_skipped=%u "
         "background_us=%llu/%llu foreground_us=%llu/%llu drawframe_us=%llu/%llu "
         "fine_detail_us=%llu/%llu post_us=%llu/%llu touch_us=%llu/%llu coin_us=%llu/%llu",
         gSHARFrontendPerfFrames,
         gSHARFrontendPerfSplitFrames,
         gSHARFrontendPerfFullFrames,
         gSHARFrontendPerfBackgroundSkippedFrames,
         SHARFrontendStageAverage(SHAR_FE_BACKGROUND), gSHARFrontendPerfMaxima[SHAR_FE_BACKGROUND],
         SHARFrontendStageAverage(SHAR_FE_FOREGROUND), gSHARFrontendPerfMaxima[SHAR_FE_FOREGROUND],
         SHARFrontendStageAverage(SHAR_FE_DRAW_FRAME), gSHARFrontendPerfMaxima[SHAR_FE_DRAW_FRAME],
         SHARFrontendStageAverage(SHAR_FE_FINE_DETAIL), gSHARFrontendPerfMaxima[SHAR_FE_FINE_DETAIL],
         SHARFrontendStageAverage(SHAR_FE_POST_PROCESS), gSHARFrontendPerfMaxima[SHAR_FE_POST_PROCESS],
         SHARFrontendStageAverage(SHAR_FE_TOUCH_HUD), gSHARFrontendPerfMaxima[SHAR_FE_TOUCH_HUD],
         SHARFrontendStageAverage(SHAR_FE_COIN_EFFECTS), gSHARFrontendPerfMaxima[SHAR_FE_COIN_EFFECTS]);

    gSHARFrontendPerfWindowStartUs = nowUs;
    for (int i = 0; i < SHAR_FE_STAGE_COUNT; ++i)
    {
        gSHARFrontendPerfTotals[i] = 0;
        gSHARFrontendPerfMaxima[i] = 0;
        gSHARFrontendPerfCalls[i] = 0;
    }
    gSHARFrontendPerfFrames = 0;
    gSHARFrontendPerfSplitFrames = 0;
    gSHARFrontendPerfFullFrames = 0;
    gSHARFrontendPerfBackgroundSkippedFrames = 0;
}
#endif


#if defined(RAD_ANDROID)
  #include <android/log.h>
#include "RenderManager.h"

#define LOG_TAG "SimpsonsHitAndRun"
  #define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
  #define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#elif defined(RAD_VITA)
  #include <psp2/kernel/clib.h>
  #define LOGI(...) do { sceClibPrintf(__VA_ARGS__); sceClibPrintf("\n"); } while(0)
  #define LOGE(...) do { sceClibPrintf(__VA_ARGS__); sceClibPrintf("\n"); } while(0)

#else
  #include <cstdio>
  #define LOGI(...) do { std::printf(__VA_ARGS__); std::printf("\n"); std::fflush(stdout); } while(0)
  #define LOGE(...) do { std::printf(__VA_ARGS__); std::printf("\n"); std::fflush(stdout); } while(0)
#endif
//************************************************************************
//
// Global Data, Local Data, Local Classes
//
//************************************************************************

//************************************************************************
//
// Public Member Functions : Context Interface
//
//************************************************************************

//========================================================================
// FrontEndFrontEndRenderLayer::FrontEndRenderLayer
//========================================================================
//
// Description: Inits state and variables to represent Dead State
//
// Parameters:	 None
//
// Return:      None.
//
// Constraints: 
//
//========================================================================
FrontEndRenderLayer::FrontEndRenderLayer()
:   RenderLayer(),
    mpScroobyApp( NULL )
{
#ifdef DEBUGWATCH
    radDbgWatchAddUnsignedInt(&mDebugRenderTime, "Render Time", "Front End Render Layer" );
#endif
}

//========================================================================
// FrontEndFrontEndRenderLayer::~FrontEndRenderLayer
//========================================================================
//
// Description: Cleans state and variables to represent Dead State
//
// Parameters:	 None
//
// Return:      None.
//
// Constraints: 
//
//========================================================================
FrontEndRenderLayer::~FrontEndRenderLayer()
{
#ifdef DEBUGWATCH
    radDbgWatchDelete(&mDebugRenderTime);
#endif
}

void FrontEndRenderLayer::DrawCoinObject()
{
    // Render HUD coin effects.
    if((GetGameFlow()->GetCurrentContext() == CONTEXT_GAMEPLAY ||
        GetGameFlow()->GetCurrentContext() == CONTEXT_PAUSE) &&
	   !GetPresentationManager()->IsBusy())
	{
        GetCoinManager()->HUDRender();
        GetSparkleManager()->HUDRender();
        //??? GetHitnRunManager()->HUDRender();
    }
	else
	{
		GetCoinManager()->ClearHUDCoins();
	}

}
//************************************************************************
// Render Interface
//************************************************************************
//========================================================================
// FrontEndFrontEndRenderLayer::Render
//========================================================================
//
// Description: Renders all (TODO:visible/DSG) drawables
//
// Parameters:	 None
//
// Return:      None.
//
// Constraints: 
//
//========================================================================

// FUNCION ORIGINAL DA PROBLEMAS EN ANDROID 64 bits por un problema de orden de render de capas, la capa gui se pinta por encima tapando el video 
/*
void FrontEndRenderLayer::Render()
{
    BEGIN_PROFILE( "FE Render" );

#ifdef DEBUGWATCH
    mDebugRenderTime = radTimeGetMicroseconds();
#endif

    for( unsigned int view = 0; view < mNumViews; view++ )
    {
        mpView[ view ]->BeginRender();

        rAssert(!IsDead());

        HeapMgr()->PushHeap( GMA_TEMP );

        if (!GetCoinManager()->DrawAfterGui())
            DrawCoinObject();
        // display Scrooby screen (and updates all Pure3d objects)
        //
        mpScroobyApp->DrawFrame( static_cast<float>( g_scroobySimulationTime ) );
#ifdef RAD_PC
        // Update the frontend cursor.
        GetInputManager()->GetFEMouse()->Update();
#endif

        if (GetCoinManager()->DrawAfterGui())
            DrawCoinObject();

        HeapMgr()->PopHeap ( GMA_TEMP );

//        GetBillboardQuadManager()->DisplayAll();

        mpView[ view ]->EndRender();
    }

#ifdef DEBUGWATCH
    mDebugRenderTime = radTimeGetMicroseconds()-mDebugRenderTime;
#endif

    END_PROFILE( "FE Render" );
}

*/




// ESTA FUNCION ES LA DEFINITIVA(6/03/2026 19:00)  (Esta es la que actualmente funciona las cinematicas in game con parpadeo )

/*
void FrontEndRenderLayer::Render()
{
    BEGIN_PROFILE( "FE Render" );

#ifdef DEBUGWATCH
    mDebugRenderTime = radTimeGetMicroseconds();
#endif

    bool fmvPlaying = false;
    PresentationManager* pm = GetPresentationManager();
    if( pm && pm->GetFMVPlayer() )
    {
        fmvPlaying = pm->GetFMVPlayer()->IsPlaying();
    }

    for( unsigned int view = 0; view < mNumViews; view++ )
    {
        rAssert(!IsDead());

        // nuevos cambios

        if( mpView[ view ] == NULL )
        {
            continue;
        }

        mpView[ view ]->BeginRender();

        if( !fmvPlaying )
        {
            HeapMgr()->PushHeap( GMA_TEMP );

            if( !GetCoinManager()->DrawAfterGui() )
                DrawCoinObject();

            if( mpScroobyApp != NULL )
            {
                mpScroobyApp->DrawFrame( static_cast<float>( g_scroobySimulationTime ) );
            }

#ifdef RAD_PC
            GetInputManager()->GetFEMouse()->Update();
#endif

            if( GetCoinManager()->DrawAfterGui() )
                DrawCoinObject();

            HeapMgr()->PopHeap( GMA_TEMP );
        }

        mpView[ view ]->EndRender();

        //Fin nuevos cambios
    }

#ifdef DEBUGWATCH
    mDebugRenderTime = radTimeGetMicroseconds() - mDebugRenderTime;
#endif

    END_PROFILE( "FE Render" );
}
*/


// PRUEBO ESTA NUEVA VERSION 

void FrontEndRenderLayer::Render()
{
    BEGIN_PROFILE( "FE Render" );

#ifdef DEBUGWATCH
    mDebugRenderTime = radTimeGetMicroseconds();
#endif

    bool fmvPlaying = false;
    PresentationManager* pm = GetPresentationManager();
    if( pm && pm->GetFMVPlayer() )
    {
        fmvPlaying = pm->GetFMVPlayer()->IsPlaying();
    }
    if( fmvPlaying )
    {
#ifdef RAD_ANDROID
        // Do not call DrawFrame() during an FMV: that would render the full
        // movie screen UI and can cover the video. Render only the foreground
        // pages, which is where the standalone TVFrame page belongs.
        for( unsigned int view = 0; view < mNumViews; view++ )
        {
            if( mpView[ view ] == NULL )
            {
                continue;
            }

            mpView[ view ]->BeginRender();

            if( mpScroobyApp != NULL )
            {

                mpScroobyApp->DrawForeground( static_cast<float>( g_scroobySimulationTime ) );
            }

            mpView[ view ]->EndRender();
        }
#else
        // Preserve the original non-Android behavior during FMV playback.
#endif
#ifdef DEBUGWATCH
        mDebugRenderTime = radTimeGetMicroseconds() - mDebugRenderTime;
#endif
        END_PROFILE( "FE Render" );
        return;
    }

#ifdef RAD_ANDROID
    SHARLoadFrontendDiagnosticConfig();
#endif

    for( unsigned int view = 0; view < mNumViews; view++ )
    {
        rAssert( !IsDead() );

        if( mpView[ view ] == NULL )
        {
            continue;
        }

        mpView[ view ]->BeginRender();

        HeapMgr()->PushHeap( GMA_TEMP );

        if( !GetCoinManager()->DrawAfterGui() )
        {
#ifdef RAD_ANDROID
            const unsigned long long coinStartUs = (unsigned long long)radTimeGetMicroseconds64();
#endif
            DrawCoinObject();
#ifdef RAD_ANDROID
            SHARRecordFrontendStage(SHAR_FE_COIN_EFFECTS, coinStartUs);
#endif
        }

        if( mpScroobyApp != NULL )
        {
#ifdef RAD_ANDROID
            // The front-end fine-contour pass must only see the 3D living-room
            // scene. Scrooby's FeDivide split lets us process the background
            // before the 2D foreground menu, so text, sprites and loading UI
            // are never sampled by the contour shader.
            Scrooby::Project* project = mpScroobyApp->GetProject();
            Scrooby::Screen* screen = project != NULL ? project->GetCurrentScreen() : NULL;
            const bool celPassActive = IsSHARFrontendMenuCelPassActive();
            Scrooby::Page* menu3dPage = screen != NULL ? screen->GetPage( "3dFE" ) : NULL;

            // FeDivide is Scrooby's internal drawable divider, not a page.
            // DrawBackground/DrawForeground already split on that drawable, so
            // do not require a separate divider-page lookup here.
            const bool hasMenuCameraSet =
                celPassActive && screen != NULL && menu3dPage != NULL;

            if (gSHARFrontendSkipBackground)
            {
                // Diagnostic-only path: retain Scrooby's foreground (including
                // the language selector) while bypassing background drawables.
                const float frameDelta = static_cast<float>(g_scroobySimulationTime);
                ++gSHARFrontendPerfBackgroundSkippedFrames;
                const unsigned long long foregroundStartUs =
                    (unsigned long long)radTimeGetMicroseconds64();
                mpScroobyApp->DrawForeground(frameDelta);
                SHARRecordFrontendStage(SHAR_FE_FOREGROUND, foregroundStartUs);
                ++gSHARFrontendPerfSplitFrames;
            }
            else if( hasMenuCameraSet )
            {
                const float frameDelta = static_cast<float>( g_scroobySimulationTime );
                unsigned long long stageStartUs =
                    (unsigned long long)radTimeGetMicroseconds64();
                mpScroobyApp->DrawBackground( frameDelta );
                SHARRecordFrontendStage(SHAR_FE_BACKGROUND, stageStartUs);

                stageStartUs = (unsigned long long)radTimeGetMicroseconds64();
                ApplySHARFrontendMenuFineDetail();
                SHARRecordFrontendStage(SHAR_FE_FINE_DETAIL, stageStartUs);

                stageStartUs = (unsigned long long)radTimeGetMicroseconds64();
                mpScroobyApp->DrawForeground( frameDelta );
                SHARRecordFrontendStage(SHAR_FE_FOREGROUND, stageStartUs);
                ++gSHARFrontendPerfSplitFrames;
            }
            else
            {
                const unsigned long long drawFrameStartUs =
                    (unsigned long long)radTimeGetMicroseconds64();
                mpScroobyApp->DrawFrame( static_cast<float>( g_scroobySimulationTime ) );
                SHARRecordFrontendStage(SHAR_FE_DRAW_FRAME, drawFrameStartUs);
                ++gSHARFrontendPerfFullFrames;
            }
#else
            mpScroobyApp->DrawFrame( static_cast<float>( g_scroobySimulationTime ) );
#endif
        }

#ifdef RAD_PC
        GetInputManager()->GetFEMouse()->Update();
#endif

        if( GetCoinManager()->DrawAfterGui() )
        {
#ifdef RAD_ANDROID
            const unsigned long long coinStartUs = (unsigned long long)radTimeGetMicroseconds64();
#endif
            DrawCoinObject();
#ifdef RAD_ANDROID
            SHARRecordFrontendStage(SHAR_FE_COIN_EFFECTS, coinStartUs);
#endif
        }

#ifdef RAD_ANDROID
        {
            const unsigned long long postStartUs = (unsigned long long)radTimeGetMicroseconds64();
            ApplySHARPostProcessingBeforeTouchHud();
            SHARRecordFrontendStage(SHAR_FE_POST_PROCESS, postStartUs);
        }
        {
            const unsigned long long touchStartUs = (unsigned long long)radTimeGetMicroseconds64();
            TouchHudRenderer::GetInstance().Render(); // Render touch controls after full-frame filters.
            SHARRecordFrontendStage(SHAR_FE_TOUCH_HUD, touchStartUs);
        }
#endif
        HeapMgr()->PopHeap( GMA_TEMP );

        mpView[ view ]->EndRender();

        
    }

#ifdef DEBUGWATCH
    mDebugRenderTime = radTimeGetMicroseconds() - mDebugRenderTime;
#endif

#ifdef RAD_ANDROID
    SHARLogFrontendPerf();
#endif
    END_PROFILE( "FE Render" );
}



//************************************************************************
// Resource Interface
//************************************************************************
//////////////////////////////////////////////////////////////////////////
// Guts; Renderable Type Things
//////////////////////////////////////////////////////////////////////////
//========================================================================
// FrontEndRenderLayer::AddGuts
//========================================================================
//
// Description: Add a tDrawable
//
// Parameters:	 tDrawable to add
//
// Return:      None.
//
// Constraints: 
//
//========================================================================
void FrontEndRenderLayer::AddGuts( tDrawable* ipDrawable )
{
   //The Basic FrontEndRenderLayer does not support this type
   rAssert(false);
}

//========================================================================
// FrontEndRenderLayer::AddGuts
//========================================================================
//
// Description: Add a tGeometry
//
// Parameters:	 tGeometry to add
//
// Return:      None.
//
// Constraints: 
//
//========================================================================
void FrontEndRenderLayer::AddGuts( tGeometry* ipGeometry )
{
   //The Basic FrontEndRenderLayer does not support this type
   rAssert(false);
}

//========================================================================
// FrontEndRenderLayer::AddGuts
//========================================================================
//
// Description: Add an IntersectDSG
//
// Parameters:  IntersectDSG to add
//
// Return:      None.
//
// Constraints: None.
//
//========================================================================
void FrontEndRenderLayer::AddGuts( IntersectDSG* ipIntersectDSG )
{
   //The Basic FrontEndRenderLayer does not support this type
   rAssert(false);
}

//========================================================================
// FrontEndRenderLayer::
//========================================================================
//
// Description: 
//
// Parameters:  None.
//
// Return:      None.
//
// Constraints: None.
//
//========================================================================
void FrontEndRenderLayer::AddGuts( StaticEntityDSG* ipStaticEntityDSG )
{
   //The Basic FrontEndRenderLayer does not support this type
   rAssert(false);
}

//========================================================================
// FrontEndRenderLayer::
//========================================================================
//
// Description: 
//
// Parameters:  None.
//
// Return:      None.
//
// Constraints: None.
//
//========================================================================
void FrontEndRenderLayer::AddGuts( StaticPhysDSG* ipStaticPhysDSG )
{
   //The Basic FrontEndRenderLayer does not support this type
   rAssert(false);
}

//========================================================================
// FrontEndRenderLayer::AddGuts
//========================================================================
//
// Description: Add the Scrooby App reference
//
// Parameters:  Scrooby App to add
//
// Return:      None.
//
// Constraints: None.
//
//========================================================================
void FrontEndRenderLayer::AddGuts( Scrooby::App* ipScroobyApp )
{
    rAssert( mpScroobyApp == NULL );

    mpScroobyApp = ipScroobyApp;
}

//========================================================================
// FrontEndRenderLayer::SetUpGuts
//========================================================================
//
// Description: 
//
// Parameters:  None.
//
// Return:      None.
//
// Constraints: None.
//
//========================================================================
void FrontEndRenderLayer::SetUpGuts()
{
    // do nothing
}

//========================================================================
// FrontEndRenderLayer::NullifyGuts
//========================================================================
//
// Description: 
//
// Parameters:  None.
//
// Return:      None.
//
// Constraints: None.
//
//========================================================================
void FrontEndRenderLayer::NullifyGuts()
{
    mpScroobyApp = NULL;
}
