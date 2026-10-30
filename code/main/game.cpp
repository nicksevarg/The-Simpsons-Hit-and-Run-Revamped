//=============================================================================
// Copyright (C) 2002 Radical Entertainment Ltd.  All rights reserved.
//
// File:        game.cpp
//
// Description: The game loop
//
// History:     + Stolen and cleaned up from Penthouse -- Darwin Chau
//
//=============================================================================

//========================================
// System Includes
//========================================
// Standard Library
#include <stdlib.h>
#include <string.h>
// Foundation Tech
#include <raddebug.hpp>
#include <raddebugcommunication.hpp>
#include <raddebugconsole.hpp>
#include <raddebugwatch.hpp>
#include <radfile.hpp>
#include <radmemorymonitor.hpp>
#include <radtime.hpp>
// Pure3D
#include <p3d/loadmanager.hpp>
#include <p3d/utility.hpp>
#ifdef RAD_WIN32
#include <SDL.h>  // for SDL_PollEvent...
#endif

//========================================
// Project Includes
//========================================
#include <contexts/contextenum.h>
#include <debug/profiler.h>
#include <gameflow/gameflow.h>
#include <presentation/gui/ingame/guiscreenmissionload.h>
#include <main/commandlineoptions.h>
#include <main/game.h>
#include <main/platform.h>

#ifdef RAD_GAMECUBE
#include <main/gamecube_extras/gcmanager.h>
#endif

#include <memory/srrmemory.h>
#include <memory/memoryutilities.h>

#include <render/RenderFlow/renderflow.h>
#include <sound/soundmanager.h>
#include <input/inputmanager.h>
#ifdef RAD_WIN32
#include <input/touch/touchinputmodemanager.h>
#include <input/touch/touchinputadapter.h>
#endif
#ifdef RAD_ANDROID
#include <input/touch/touchcontextresolver.h>
#include <input/touch/touchhudsystem.h>
#include <input/touch/touchinputmodemanager.h>
#include <input/touch/touchinputadapter.h>
#include <worldsim/avatarmanager.h>
#include <input/touch/touchassetextractor.h>
#include <input/touch/touchassetmanager.h>
#include <input/touch/touchhudrenderer.h>
#include <data/config/androidconfigurationmanager.h>
#include <input/touch/touchcontrolsconfigurationmanager.h>
#endif

#if defined(RAD_ANDROID)
  #include <android/log.h>
  #define LOG_TAG "SimpsonsHitAndRun"
  #define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  LOG_TAG, __VA_ARGS__)
  #define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
  void InitializeSHARAndroidFrameRateSelection();
#endif
//******************************************************************************
//
// Global Data, Local Data, Local Classes
//
//******************************************************************************

//
// Static pointer to instance of this singleton.
//
Game* Game::spInstance = NULL;

bool g_inDemoMode = false;

#ifdef RAD_ANDROID

static bool GetTouchAvatarInVehicleForPlayer0()
{
    /*
     * Keep this helper conservative.
     *
     * It is only queried when the current context is gameplay.
     * AvatarManager is the correct long-term source for detecting whether
     * player 0 is currently inside a vehicle.
     */
    Avatar* avatar = GetAvatarManager()->GetAvatarForPlayer( 0 );

    if ( avatar == NULL )
    {
        return false;
    }

    return avatar->IsInCar();
}

static void UpdateTouchContextResolverFromGame()
{
    if ( GetGameFlow() == NULL || InputManager::GetInstance() == NULL )
    {
        return;
    }

    const int currentContext = GetGameFlow()->GetCurrentContext();
    const unsigned inputGameState = InputManager::GetInstance()->GetGameState();

    bool avatarInVehicle = false;

    /*
     * Only query AvatarManager while actually in gameplay.
     * This avoids touching avatar systems during boot, frontend, loading or exit.
     */
    if ( currentContext == CONTEXT_GAMEPLAY )
    {
        avatarInVehicle = GetTouchAvatarInVehicleForPlayer0();
    }

    TouchContextResolver::GetInstance().UpdateFromGameState(
        currentContext,
        inputGameState,
        avatarInVehicle
    );
}

static void UpdateTouchHudSystemFromSDLEvent( const SDL_Event& msg )
{
#if SDL_MAJOR_VERSION < 3

    switch ( msg.type )
    {
        case SDL_FINGERDOWN:
        {
            // A screen touch explicitly reselects the touchscreen mode and
            // restores the in-game HUD before the touch is routed.
            TouchInputModeManager::GetInstance().NotifyTouchInput();
            TouchInputModeManager::GetInstance().SetTouchHudEnabled( true );
            TouchHudSystem::GetInstance().SetEnabled( true );
            UpdateTouchContextResolverFromGame();

            TouchInputAdapter::GetInstance().SetEnabled( true );
            TouchInputAdapter::GetInstance().SetTargetControllerIndex( 0 );

            TouchHudSystem::GetInstance().HandleFingerDown(
                static_cast<TouchHudFingerId>( msg.tfinger.fingerId ),
                msg.tfinger.x,
                msg.tfinger.y,
                msg.tfinger.pressure
            );

            break;
        }

        case SDL_FINGERMOTION:
        {
            TouchHudSystem::GetInstance().HandleFingerMove(
                static_cast<TouchHudFingerId>( msg.tfinger.fingerId ),
                msg.tfinger.x,
                msg.tfinger.y,
                msg.tfinger.pressure
            );

            break;
        }

        case SDL_FINGERUP:
        {
            TouchHudSystem::GetInstance().HandleFingerUp(
                static_cast<TouchHudFingerId>( msg.tfinger.fingerId ),
                msg.tfinger.x,
                msg.tfinger.y,
                msg.tfinger.pressure
            );

            break;
        }

        default:
        {
            break;
        }
    }

#else

    switch ( msg.type )
    {
        case SDL_EVENT_FINGER_DOWN:
        {
            // A screen touch explicitly reselects the touchscreen mode and
            // restores the in-game HUD before the touch is routed.
            TouchInputModeManager::GetInstance().NotifyTouchInput();
            TouchInputModeManager::GetInstance().SetTouchHudEnabled( true );
            TouchHudSystem::GetInstance().SetEnabled( true );
            UpdateTouchContextResolverFromGame();

            TouchInputAdapter::GetInstance().SetEnabled( true );
            TouchInputAdapter::GetInstance().SetTargetControllerIndex( 0 );

            TouchHudSystem::GetInstance().HandleFingerDown(
                static_cast<TouchHudFingerId>( msg.tfinger.fingerID ),
                msg.tfinger.x,
                msg.tfinger.y,
                msg.tfinger.pressure
            );

            break;
        }

        case SDL_EVENT_FINGER_MOTION:
        {
            TouchHudSystem::GetInstance().HandleFingerMove(
                static_cast<TouchHudFingerId>( msg.tfinger.fingerID ),
                msg.tfinger.x,
                msg.tfinger.y,
                msg.tfinger.pressure
            );

            break;
        }

        case SDL_EVENT_FINGER_UP:
        {
            TouchHudSystem::GetInstance().HandleFingerUp(
                static_cast<TouchHudFingerId>( msg.tfinger.fingerID ),
                msg.tfinger.x,
                msg.tfinger.y,
                msg.tfinger.pressure
            );

            break;
        }

        default:
        {
            break;
        }
    }

#endif
}

#endif // RAD_ANDROID

#ifdef RAD_WIN32

static void UpdateTouchInputModeFromSDLEvent( const SDL_Event& msg )
{
#if SDL_MAJOR_VERSION < 3

    switch ( msg.type )
    {
        case SDL_FINGERDOWN:
        case SDL_FINGERMOTION:
        case SDL_FINGERUP:
        {
            TouchInputModeManager::GetInstance().NotifyTouchInput();
            break;
        }

        case SDL_CONTROLLERDEVICEADDED:
        {
            // Device enumeration alone must not hide touchscreen controls.
            TouchInputModeManager::GetInstance().NotifyGamepadCandidateConnected();
            break;
        }

        case SDL_CONTROLLERDEVICEREMOVED:
        {
            TouchInputModeManager::GetInstance().NotifyGamepadDisconnected();
            break;
        }

        case SDL_CONTROLLERBUTTONDOWN:
        {
            TouchInputModeManager::GetInstance().NotifyGamepadInput();
            break;
        }

        case SDL_CONTROLLERAXISMOTION:
        {
            if ( msg.caxis.value > 8000 || msg.caxis.value < -8000 )
            {
                TouchInputModeManager::GetInstance().NotifyGamepadInput();
            }
            break;
        }

        case SDL_JOYDEVICEADDED:
        {
            // Device enumeration alone must not hide touchscreen controls.
            TouchInputModeManager::GetInstance().NotifyGamepadCandidateConnected();
            break;
        }

        case SDL_JOYDEVICEREMOVED:
        {
            TouchInputModeManager::GetInstance().NotifyGamepadDisconnected();
            break;
        }

        case SDL_JOYBUTTONDOWN:
        {
            TouchInputModeManager::GetInstance().NotifyGamepadInput();
            break;
        }

        case SDL_JOYAXISMOTION:
        {
            if ( msg.jaxis.value > 8000 || msg.jaxis.value < -8000 )
            {
                TouchInputModeManager::GetInstance().NotifyGamepadInput();
            }
            break;
        }

        default:
        {
            break;
        }
    }

#else

    switch ( msg.type )
    {
        case SDL_EVENT_FINGER_DOWN:
        case SDL_EVENT_FINGER_MOTION:
        case SDL_EVENT_FINGER_UP:
        {
            TouchInputModeManager::GetInstance().NotifyTouchInput();
            break;
        }

        case SDL_EVENT_GAMEPAD_ADDED:
        {
            // Device enumeration alone must not hide touchscreen controls.
            TouchInputModeManager::GetInstance().NotifyGamepadCandidateConnected();
            break;
        }

        case SDL_EVENT_GAMEPAD_REMOVED:
        {
            TouchInputModeManager::GetInstance().NotifyGamepadDisconnected();
            break;
        }

        case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        {
            TouchInputModeManager::GetInstance().NotifyGamepadInput();
            break;
        }

        case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        {
            if ( msg.gaxis.value > 8000 || msg.gaxis.value < -8000 )
            {
                TouchInputModeManager::GetInstance().NotifyGamepadInput();
            }
            break;
        }

        case SDL_EVENT_JOYSTICK_ADDED:
        {
            // Device enumeration alone must not hide touchscreen controls.
            TouchInputModeManager::GetInstance().NotifyGamepadCandidateConnected();
            break;
        }

        case SDL_EVENT_JOYSTICK_REMOVED:
        {
            TouchInputModeManager::GetInstance().NotifyGamepadDisconnected();
            break;
        }

        case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
        {
            TouchInputModeManager::GetInstance().NotifyGamepadInput();
            break;
        }

        case SDL_EVENT_JOYSTICK_AXIS_MOTION:
        {
            if ( msg.jaxis.value > 8000 || msg.jaxis.value < -8000 )
            {
                TouchInputModeManager::GetInstance().NotifyGamepadInput();
            }
            break;
        }

        default:
        {
            break;
        }
    }

#endif
}



#endif // RAD_WIN32

//#define DEMO_MODE_PROFILER

#ifdef DEMO_MODE_PROFILER
    #define DEMOPROFILE(X) X
#else
    #define DEMOPROFILE(X)
#endif

#ifdef DEMO_MODE_PROFILER

#ifdef RAD_PS2
#include <libgraph.h>
#endif

DemoProfiler::DemoProfiler(unsigned mf) :
    recording(false), maxFrames(mf), nChannel(0), currentFrame(0),
    alertStatus(PROFILER_ALERT_GREEN),
    numFramesBelow_20(0), numFramesBetween_20_30(0),numFramesBetween_30_40(0), numFramesAbove_40(0)
{
    for( int i=0; i < MAX_CHANNEL; i++)
    {
        channel[i] = NULL;
    }
}
    
void DemoProfiler::AddChannel(unsigned c, const char* name)
{
    rReleaseAssert(c < MAX_CHANNEL);
    channel[c] = new Channel;
    channel[c]->samples = new unsigned[maxFrames];
    memset(channel[c]->samples, 0, maxFrames*sizeof(unsigned));
    channel[c]->t0 = 0;
    strncpy(channel[c]->name, name, 254);
    nChannel++;
}

void DemoProfiler::Start(unsigned c)
{
    if(!channel[c]) return;
    if( recording )
    {
        channel[c]->t0 = radTimeGetMicroseconds64();
    }
}


void DemoProfiler::Stop(unsigned c)
{
    if(!channel[c]) return;
    if( recording )
    {
        radTime64 elapsed = radTimeGetMicroseconds64() - channel[c]->t0;
        channel[c]->samples[currentFrame] += elapsed;

        if(c == 0)
        {
            alertStatus = PROFILER_ALERT_GREEN;
            if( elapsed >= 50000 )
            {
                numFramesBelow_20++;
                alertStatus = PROFILER_ALERT_RED;
            }
            else if( (elapsed < 50000) && (elapsed >= 33333) )
            {
                numFramesBetween_20_30++;
                alertStatus = PROFILER_ALERT_YELLOW;
            }
            else if( (elapsed < 33333) && (elapsed >= 25000) )
            {
                numFramesBetween_30_40++;
            }
            else
            {
                numFramesAbove_40++;
            }
        }
    }
}

void DemoProfiler::Set(unsigned c, unsigned val)
{
    if(!channel[c]) return;
    if( recording )
    {
        channel[c]->samples[currentFrame] = val;
    }
}

void DemoProfiler::StartRecording()
{
    recording = true;
}

bool DemoProfiler::IsRecording()
{
    return recording;
}

unsigned DemoProfiler::GetSample(unsigned c)
{
    return (recording && channel[c]) ? channel[c]->samples[currentFrame] : 0;
}

unsigned DemoProfiler::GetCurrentFrame()
{
    return currentFrame;
}

void DemoProfiler::Accumulate(unsigned c, unsigned val)
{
    if(!channel[c]) return;
    if( recording )
    {
        channel[c]->samples[currentFrame] += val;
    }
}

void DemoProfiler::NextFrame()
{
    if( recording )
    {
        currentFrame++;
        if(currentFrame >= maxFrames)
        {
            Dump();
            recording = false;
        }
    }
}

DemoProfiler::AlertStatus DemoProfiler::GetAlertStatus()
{
    return alertStatus;
}

void DemoProfiler::Dump()
{
    rReleasePrintf("\n\n~~~~~~~~~~~~~~~~~~~~~ PROFILER STATS ~~~~~~~~~~~~~~~~~~~~~\n");
    rReleasePrintf(    "Total Frames: %d\n"
                       "< 20 fps: %d (%.2f%%)\n"
                       "20-30 fps: %d (%.2f%%)\n"
                       "30-40 fps: %d (%.2f%%)\n"
                       ">40 fps: %d (%.2f%%)\n",
                       maxFrames,
                       numFramesBelow_20, 100.0f * (float)numFramesBelow_20 / (float)maxFrames,
                       numFramesBetween_20_30, 100.0f * (float)numFramesBetween_20_30 / (float)maxFrames,
                       numFramesBetween_30_40, 100.0f * (float)numFramesBetween_30_40 / (float)maxFrames,
                       numFramesAbove_40, 100.0f * (float)numFramesAbove_40 / (float)maxFrames );

    for( unsigned i=0; i < MAX_CHANNEL; i++)
    {
        if(channel[i])
        {
            rReleasePrintf("%s\t", channel[i]->name);
        }
    }
    rReleasePrintf("\n");

    for( unsigned i=0; i < maxFrames; i++ )
    {
        for( unsigned j=0; j < MAX_CHANNEL; j++ )
        {
            if(channel[j])
            {
                rReleasePrintf("%.1f\t", (float)channel[j]->samples[i] * 0.001f);
            }
        }
        rReleasePrintf("\n");

        if( !(i % 10) )
        {
            rmt::Sin(0.0f);
#ifdef RAD_PS2
            sceGsSyncV(0);
#endif
        }
    }

    while(1)
    {
        rmt::Sin(0.0f);
    }
}


DemoProfiler g_DemoProfiler( 1850 );  // about a minute at 30 fps

#include <pddi/pddi.hpp>

static bool g_DemoProfiler_Started = false;
static const int g_DemoProfiler_StartFrame = 150;
static int g_DemoProfiler_CurrentFrame = 0;

#endif


//******************************************************************************
//
// Public Member Functions
//
//******************************************************************************

//==============================================================================
// Game::CreateInstance
//==============================================================================
// Description: Create the game
//
// Parameters:	platform - the platform that the game is to be created on
//
// Return:      pointer to the created game
//
// Constraints: This is a singleton so only one instance is allowed.
//
//==============================================================================
Game* Game::CreateInstance( Platform* platform )
{
    rAssert( platform != NULL );

    rAssertMsg( (spInstance == NULL), "Trying to create more than one instance of the game!" );

MEMTRACK_PUSH_GROUP( "Game" );
    if( spInstance == NULL )
    {
        spInstance = new(GMA_PERSISTENT) Game( platform );
        rAssert( spInstance != NULL );
    }
MEMTRACK_POP_GROUP( "Game" );
    
    return spInstance;
}


//==============================================================================
// Game::DestroyInstance
//==============================================================================
// Description: Destroy the game
//
// Parameters:	None.
//
// Return:      None.
//
//==============================================================================
void Game::DestroyInstance()
{
    delete( GMA_PERSISTENT, spInstance );
    spInstance = NULL;
}


//==============================================================================
// Game::GetInstance
//==============================================================================
// Synopsis:    Get an instance of the game
//
// Parameters:  None.
//
// Returns:     a poitner to the game
//
// Constraints: Game must be created before this is called
//
//==============================================================================
Game* Game::GetInstance()
{
    rAssertMsg((spInstance != NULL), "Trying to get an instance of the game before it is created!");
    
    return spInstance;
}

//=============================================================================
// Game::GetPlatform
//=============================================================================
// Description: Comment
//
// Parameters:  ()
//
// Return:      Platform
//
//=============================================================================
Platform* Game::GetPlatform()
{
    return mpPlatform;
}


//==============================================================================
// Game::Initialize
//==============================================================================
// Synopsis:    Initialize the game
//
// Parameters:  None.
//
// Returns:     None.
//
//==============================================================================
void Game::Initialize()
{
    rAssert( mpPlatform != NULL );

    //
    // Initialize the platform and core systems.
    //
    mpPlatform->InitializePlatform();

    // Extraemos el layout tactil contenido en el apk en archivos leibles para que el pure3d mas adelante lo pueda renderizar en el juego 
    //APK->com.c4rlox.simpsons/files/touch_controls/icono.png
    #ifdef RAD_ANDROID
    TouchAssetExtractor::GetInstance().EnsureAssetsExtracted();
    // Realizamos la carga con pure3d de los iconos extraidos anteriormente 
    TouchAssetManager::GetInstance().Initialize();
    // Cargamos la configuración personalizada de controles táctiles del jugador (en caso de que exista) antes de inicializar el sistema de renderizado de iconos 
    TouchControlsConfigurationManager::GetInstance().Initialize();

    // Realizamos inicializacion del render, pero aqui no es donde solicitamos el renderizado,solo iniciamos el sistema de render
    TouchHudRenderer::GetInstance().Initialize();

    // hacemos que se utilice en el ciclo del juego al arrancar la configuración del archivo Simpsons_configuration.txt
    GetAndroidConfigurationManager()->Initialize();

    // Load the persisted FPS cap and request a matching Android display mode where supported.
    InitializeSHARAndroidFrameRateSelection();

    #endif

    //
    // Initialize the sound manager.
    //
    SoundManager::GetInstance()->Initialize();
    
    //
    // Initialize the timer system
    //
    ::radTimeCreateList( &mpTimerList,
                         16, // Default
                         GMA_PERSISTENT );

    rAssert( mpTimerList != NULL );

    //
    // Create the GameFlow & Couple the RenderFlow
    //
    mpGameFlow = GameFlow::CreateInstance();
    mpRenderFlow = RenderFlow::GetInstance();
    mpRenderFlow->DoAllRegistration();

    CGuiScreenMissionLoad::InitializePermanentVariables();

#ifdef RAD_E3
    rReleasePrintf( "\n----------=[  SIMPSONS HIT & RUN - E3 BUILD  ]=----------\n\n" );
#endif

    //
    // Set the starting context
    //
    mpGameFlow->SetContext( CONTEXT_BOOTUP );
}


//==============================================================================
// Game::Terminate
//==============================================================================
// Synopsis:    Clean up and shut down.
//
// Parameters:  None.
//
// Returns:     None.
//
//==============================================================================
void Game::Terminate() 
{
    
    rAssert( mpGameFlow != NULL );
    rAssert( mpRenderFlow != NULL );
    rAssert( mpTimerList != NULL );
    rAssert( mpPlatform != NULL );

    #ifdef RAD_ANDROID
    // Primero apagamos el render y despues liberamos los assets(iconos tactiles)
    TouchHudRenderer::GetInstance().Shutdown();
    TouchAssetManager::GetInstance().Shutdown();
    #endif

    //
    // Kill the flow servers.
    //
    mpGameFlow->DestroyInstance();
    mpGameFlow = NULL;
    
    // Render flow destroyed by singletons.cpp
    //mpRenderFlow->DestroyInstance();
    mpRenderFlow = NULL;
    
    //
    // Release the game's references to the timer list.
    //
    mpTimerList->Release();
    mpTimerList = NULL;
}


//==============================================================================
// Game::Run
//==============================================================================
// Synopsis:    This is where game loop spins.  It exits after Stop() is called.
//
// Parameters:  None.
//
// Returns:     None.
//
//==============================================================================
const unsigned PROFILE_CHANNEL_ALL = 0;
const unsigned PROFILE_CHANNEL_AI = 1;
const unsigned PROFILE_CHANNEL_RENDER = 2;
const unsigned PROFILE_CHANNEL_LOAD = 3;

void Game::Run() 
{
    extern bool g_AllowDebugOutput;
    
#ifdef DEMO_MODE_PROFILER
    g_AllowDebugOutput = false;
    g_DemoProfiler.AddChannel(0, "All");
    g_DemoProfiler.AddChannel(1, "Ai");
    g_DemoProfiler.AddChannel(2, "Render");
    g_DemoProfiler.AddChannel(3, "Load");
    g_DemoProfiler.AddChannel(4, "Opaque");
    g_DemoProfiler.AddChannel(5, "Translucent");
    g_DemoProfiler.AddChannel(6, "World Sphere");
    g_DemoProfiler.AddChannel(7, "Anim Entity");
    g_DemoProfiler.AddChannel(8, "Breakable");
    g_DemoProfiler.AddChannel(9, "InstAnimDynaPhys");
    g_DemoProfiler.AddChannel(10, "InstDynaPhys");
    g_DemoProfiler.AddChannel(11, "InstStat");
    g_DemoProfiler.AddChannel(12, "InstStatPhys");
    g_DemoProfiler.AddChannel(13,"Lens Flare");
    g_DemoProfiler.AddChannel(14, "State Prop");
    g_DemoProfiler.AddChannel(15, "Static");
    g_DemoProfiler.AddChannel(16, "Tristrip");
    g_DemoProfiler.AddChannel(17, "AnimCollision");
    g_DemoProfiler.AddChannel(18, "Cars");
    g_DemoProfiler.AddChannel(19, "Characters");
    g_DemoProfiler.AddChannel(20, "ABH");
    g_DemoProfiler.AddChannel(21, "Actor");
    g_DemoProfiler.AddChannel(22, "AnimatedIcon");
    g_DemoProfiler.AddChannel(23, "ParticleSystem");

#endif

    unsigned time = radTimeGetMilliseconds();
#ifdef RAD_ANDROID
    unsigned long long perfWindowStartUs = (unsigned long long)radTimeGetMicroseconds64();
    unsigned long long perfLoopTotalUs = 0, perfLoopMaxUs = 0;
    unsigned long long perfTouchTotalUs = 0, perfTouchMaxUs = 0;
    unsigned long long perfUpdateTotalUs = 0, perfUpdateMaxUs = 0;
    unsigned long long perfRenderTotalUs = 0, perfRenderMaxUs = 0;
    unsigned long long perfServiceTotalUs = 0, perfServiceMaxUs = 0;
    unsigned long long perfSoundTotalUs = 0, perfSoundMaxUs = 0;
    unsigned long long perfLoadTotalUs = 0, perfLoadMaxUs = 0;
    unsigned int perfWindowFrames = 0;
    unsigned int perfSlow16Frames = 0, perfSlow33Frames = 0, perfSlow50Frames = 0;
#endif
    while( !mExitNow )
    {
#ifdef RAD_ANDROID
        const unsigned long long perfLoopStartUs = (unsigned long long)radTimeGetMicroseconds64();
        unsigned long long perfPhaseStartUs = perfLoopStartUs;
        unsigned long long perfTouchUs = 0, perfUpdateUs = 0, perfRenderUs = 0;
        unsigned long long perfServiceUs = 0, perfSoundUs = 0, perfLoadUs = 0;
#endif
        DEMOPROFILE( g_DemoProfiler.Start(PROFILE_CHANNEL_ALL); )

        BEGIN_PROFILER_FRAME();

        BEGIN_PROFILE( "GameLoop" )

        unsigned newTime =  radTimeGetMilliseconds();
        unsigned elapsed = newTime - time;
        time = newTime;
        #ifdef RAD_ANDROID
        perfPhaseStartUs = (unsigned long long)radTimeGetMicroseconds64();
        UpdateTouchContextResolverFromGame();
        TouchHudSystem::GetInstance().Update( elapsed );
        perfTouchUs = (unsigned long long)radTimeGetMicroseconds64() - perfPhaseStartUs;
        #endif
       

        #ifdef RAD_WIN32
            TouchInputModeManager::GetInstance().Update( elapsed );
        #endif
        //
        // Service the windows message loop.
        //
#ifdef RAD_WIN32
        SDL_Event msg;
        while( SDL_PollEvent( &msg ) )
        {
            UpdateTouchInputModeFromSDLEvent( msg );
            #ifdef RAD_ANDROID
                UpdateTouchHudSystemFromSDLEvent( msg );
            #endif
#if SDL_MAJOR_VERSION < 3
            if( msg.type == SDL_QUIT )
#else
            if( msg.type == SDL_EVENT_QUIT )
#endif
            {
                //Chuck someone closed the Window we are going to try to exit the game 
                //if the game isnt in a context that can easily transition to the EXIT context we 
                // are going to call the Launch Dashboard and return to the main loop and shutdown
                if ( GetGameFlow()->GetCurrentContext() != CONTEXT_FRONTEND &&
                     GetGameFlow()->GetCurrentContext() != CONTEXT_GAMEPLAY && 
                     GetGameFlow()->GetCurrentContext() != CONTEXT_PAUSE )
                {
                    GetGame()->GetPlatform()->LaunchDashboard();
                    //return to the winmain and shutdown
                    return;
                }
                //we are in a context that will transition nicely to the Exit context.
                else
                {
                    mpGameFlow->SetContext( CONTEXT_EXIT );
                }               
            }
        }
#endif // RAD_WIN32

        //
        // Service the GameFlow and RenderFlow.
        //

#ifdef RAD_ANDROID
        perfPhaseStartUs = (unsigned long long)radTimeGetMicroseconds64();
#endif
        if ( !mpPlatform->PausedForErrors() )
        {
            DEMOPROFILE( g_DemoProfiler.Start(PROFILE_CHANNEL_AI); )
            mpTimerList->Service(); //nv
            mpGameFlow->OnTimerDone(elapsed, NULL);
            DEMOPROFILE( g_DemoProfiler.Stop(PROFILE_CHANNEL_AI); )

            if( !mExitNow )
            {
#ifdef RAD_ANDROID
                const unsigned long long perfRenderStartUs = (unsigned long long)radTimeGetMicroseconds64();
#endif
                DEMOPROFILE( g_DemoProfiler.Start(PROFILE_CHANNEL_RENDER); )
                mpRenderFlow->OnTimerDone(elapsed, NULL);
                DEMOPROFILE( g_DemoProfiler.Stop(PROFILE_CHANNEL_RENDER); )
#ifdef RAD_ANDROID
                perfRenderUs = (unsigned long long)radTimeGetMicroseconds64() - perfRenderStartUs;
#endif
            }
        }
        else if (mpPlatform->IsControllerError()) // if controller unplugged
        {                                         // need to update input manager
            if (InputManager::GetInstance())
            {
                InputManager::GetInstance()->Update( elapsed );
            }
        }
        else
        {
#ifdef RAD_GAMECUBE
            GCManager::GetInstance()->OnTimerDone( elapsed, NULL );
#endif
        }
#ifdef RAD_ANDROID
        {
            const unsigned long long perfUpdateEndUs = (unsigned long long)radTimeGetMicroseconds64();
            const unsigned long long perfUpdateAndRenderUs = perfUpdateEndUs - perfPhaseStartUs;
            perfUpdateUs = perfUpdateAndRenderUs > perfRenderUs
                ? perfUpdateAndRenderUs - perfRenderUs : 0;
            perfPhaseStartUs = perfUpdateEndUs;
        }
#endif

        //
        // Service FTech subsystems.
        //
#ifdef RAD_ANDROID
        perfPhaseStartUs = (unsigned long long)radTimeGetMicroseconds64();
#endif
        ::radFileService();
        ::radDbgComService();
        ::radDebugConsoleService();
        
        if( CommandLineOptions::Get( CLO_MEMORY_MONITOR) )
        {
            ::radMemoryMonitorService();
        }
#ifdef RAD_ANDROID
        perfServiceUs = (unsigned long long)radTimeGetMicroseconds64() - perfPhaseStartUs;
        perfPhaseStartUs = (unsigned long long)radTimeGetMicroseconds64();
#endif

        //
        // Service the sound renderer.
        //
        SoundManager::GetInstance()->Update();

        if ( mpPlatform->PausedForErrors() )
        {
            //
            // [ps] We update sound without a valid context or elapsed time. 
            // We use 0 and NUM_CONTEXTS, since these values are unlikely to
            // happen in the real game.
            //
            SoundManager::GetInstance()->UpdateOncePerFrame( 0, NUM_CONTEXTS, false, true );
        }
#ifdef RAD_ANDROID
        perfSoundUs = (unsigned long long)radTimeGetMicroseconds64() - perfPhaseStartUs;
        perfPhaseStartUs = (unsigned long long)radTimeGetMicroseconds64();
#endif

        //
        // Spin Pure3D async loading.
        //
        DEMOPROFILE( g_DemoProfiler.Start(PROFILE_CHANNEL_LOAD); )
        p3d::loadManager->SwitchTask();
#ifdef RAD_ANDROID
        perfLoadUs = (unsigned long long)radTimeGetMicroseconds64() - perfPhaseStartUs;
#endif
        DEMOPROFILE( g_DemoProfiler.Stop(PROFILE_CHANNEL_LOAD); )

        ++mFrameCount;

        DEMOPROFILE( g_DemoProfiler.Stop(PROFILE_CHANNEL_ALL); )

#ifdef DEMO_MODE_PROFILER
        g_AllowDebugOutput = true;
        if(g_inDemoMode)
        {
            if( !g_DemoProfiler_Started && (g_DemoProfiler_CurrentFrame == g_DemoProfiler_StartFrame) )
            {
                g_DemoProfiler_Started = true;
                g_DemoProfiler.StartRecording();
                rReleasePrintf("Beginning demo profile run\n");
            }

            if(g_DemoProfiler.IsRecording())
            {
                pddiColour colour(255,255,255);
                if( g_DemoProfiler.GetAlertStatus() == DemoProfiler::PROFILER_ALERT_YELLOW)
                    colour.Set(255,255,0);
                else
                if( g_DemoProfiler.GetAlertStatus() == DemoProfiler::PROFILER_ALERT_RED)
                    colour.Set(255,0,0);

                char duff[255];
                sprintf(duff, "%d %d", g_DemoProfiler.GetCurrentFrame(), (g_DemoProfiler.GetSample(0) + 500) / 1000);
                p3d::pddi->DrawString( duff, 10, 400, colour );
            }
            
            g_DemoProfiler.NextFrame();
            g_DemoProfiler_CurrentFrame++;
        }

        g_AllowDebugOutput = false;
#endif // DEMO_MODE_PROFILER

        END_PROFILE( "GameLoop" )

        END_PROFILER_FRAME();

#ifdef RAD_ANDROID
        {
            const unsigned long long perfLoopEndUs = (unsigned long long)radTimeGetMicroseconds64();
            const unsigned long long perfLoopUs = perfLoopEndUs - perfLoopStartUs;

            ++perfWindowFrames;
            perfLoopTotalUs += perfLoopUs;
            if ( perfLoopUs > perfLoopMaxUs ) perfLoopMaxUs = perfLoopUs;
            perfTouchTotalUs += perfTouchUs;
            if ( perfTouchUs > perfTouchMaxUs ) perfTouchMaxUs = perfTouchUs;
            perfUpdateTotalUs += perfUpdateUs;
            if ( perfUpdateUs > perfUpdateMaxUs ) perfUpdateMaxUs = perfUpdateUs;
            perfRenderTotalUs += perfRenderUs;
            if ( perfRenderUs > perfRenderMaxUs ) perfRenderMaxUs = perfRenderUs;
            perfServiceTotalUs += perfServiceUs;
            if ( perfServiceUs > perfServiceMaxUs ) perfServiceMaxUs = perfServiceUs;
            perfSoundTotalUs += perfSoundUs;
            if ( perfSoundUs > perfSoundMaxUs ) perfSoundMaxUs = perfSoundUs;
            perfLoadTotalUs += perfLoadUs;
            if ( perfLoadUs > perfLoadMaxUs ) perfLoadMaxUs = perfLoadUs;

            if ( perfLoopUs > 50000ULL ) ++perfSlow50Frames;
            else if ( perfLoopUs > 33333ULL ) ++perfSlow33Frames;
            else if ( perfLoopUs > 16667ULL ) ++perfSlow16Frames;

            const unsigned long long perfWindowElapsedUs = perfLoopEndUs - perfWindowStartUs;
            if ( perfWindowElapsedUs >= 1000000ULL && perfWindowFrames > 0 )
            {
                const double perfFps = (double)perfWindowFrames * 1000000.0 / (double)perfWindowElapsedUs;
                const unsigned long long perfMeasuredUs = perfTouchTotalUs + perfUpdateTotalUs +
                    perfRenderTotalUs + perfServiceTotalUs + perfSoundTotalUs + perfLoadTotalUs;
                const unsigned long long perfOtherUs = perfLoopTotalUs > perfMeasuredUs
                    ? perfLoopTotalUs - perfMeasuredUs : 0;

                LOGI("[PERF][GAME] fps=%.1f frames=%u loop_us=%llu/%llu slow16/33/50=%u/%u/%u touch_us=%llu/%llu update_us=%llu/%llu render_us=%llu/%llu service_us=%llu/%llu sound_us=%llu/%llu load_us=%llu/%llu other_us=%llu",
                     perfFps, perfWindowFrames,
                     perfLoopTotalUs / perfWindowFrames, perfLoopMaxUs,
                     perfSlow16Frames, perfSlow33Frames, perfSlow50Frames,
                     perfTouchTotalUs / perfWindowFrames, perfTouchMaxUs,
                     perfUpdateTotalUs / perfWindowFrames, perfUpdateMaxUs,
                     perfRenderTotalUs / perfWindowFrames, perfRenderMaxUs,
                     perfServiceTotalUs / perfWindowFrames, perfServiceMaxUs,
                     perfSoundTotalUs / perfWindowFrames, perfSoundMaxUs,
                     perfLoadTotalUs / perfWindowFrames, perfLoadMaxUs,
                     perfOtherUs / perfWindowFrames);

                perfWindowStartUs = perfLoopEndUs;
                perfLoopTotalUs = perfLoopMaxUs = 0;
                perfTouchTotalUs = perfTouchMaxUs = 0;
                perfUpdateTotalUs = perfUpdateMaxUs = 0;
                perfRenderTotalUs = perfRenderMaxUs = 0;
                perfServiceTotalUs = perfServiceMaxUs = 0;
                perfSoundTotalUs = perfSoundMaxUs = 0;
                perfLoadTotalUs = perfLoadMaxUs = 0;
                perfWindowFrames = 0;
                perfSlow16Frames = perfSlow33Frames = perfSlow50Frames = 0;
            }
        }
#endif
    }
}


//==============================================================================
// Game::Stop
//==============================================================================
// Synopsis:    Sets the flag to break us out of the game loop.
//
// Parameters:  None.
//
// Returns:     None.
//
//==============================================================================
void Game::Stop() 
{
    //
    // Stop any further rendering from happening.
    //
    mExitNow = true;
}


unsigned Game::GetRandomSeed ()
{
    radDate date;
    ::radTimeGetDate (&date);
    return ( ( date.m_Year << 16 ) | (  date.m_Month << 8 ) | ( date.m_Day ) ) ^ ( ( date.m_Second << 24 ) | ( date.m_Minute << 8 ) | ( date.m_Hour ) );
}



//******************************************************************************
//
// Private Member Functions
//
//******************************************************************************

//==============================================================================
// Game::Game
//==============================================================================
// Synopsis:    Constructor.
//
// Parameters:  platform - platform on which game should be created
//
// Returns:     N/A.
//
//==============================================================================
Game::Game( Platform* platform ) :
    mpPlatform( platform ),
    mpTimerList( NULL ),
    mpGameFlow( NULL ),
    mpRenderFlow( NULL ),
    mFrameCount( 0 ),
    mExitNow( false ),
    mDemoCount( 0 ),
    mTimeMS( 0 )
{
}


//==============================================================================
// Game::~Game
//==============================================================================
// Synopsis:    Destructor.
//
// Parameters:  None.
//
// Returns:     N/A.
//
//==============================================================================
Game::~Game()
{
}
