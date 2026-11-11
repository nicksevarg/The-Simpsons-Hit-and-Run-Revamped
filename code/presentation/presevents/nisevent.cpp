//=============================================================================
// Copyright (C) 2002 Radical Entertainment Ltd.  All rights reserved.
//
// File:        NISevent.cpp
//
// Description: Implement NISEvent
//
// History:     23/05/2002 + Created -- NAME
//
//=============================================================================

//========================================
// System Includes
//========================================
// Foundation Tech
#include <raddebug.hpp>

//========================================
// Project Includes
//========================================

#include <presentation/animplayer.h>
#include <presentation/cameraplayer.h>
#include <presentation/nisplayer.h>
#include <presentation/presentation.h>
#include <presentation/presevents/nisevent.h>

#include <string.h>

#ifdef RAD_ANDROID
static bool IsSHARCatapultGagFile(const char* path)
{
    if (path == NULL || path[0] == 0)
        return false;

    const char* baseName = path;
    for (const char* cursor = path; *cursor != 0; ++cursor)
    {
        if (*cursor == '/' || *cursor == 92)
            baseName = cursor + 1;
    }

    return strcmp(baseName, "gag_cata.p3d") == 0;
}
#endif

//******************************************************************************
//
// Global Data, Local Data, Local Classes
//
//******************************************************************************

//******************************************************************************
//
// Public Member Functions
//
//******************************************************************************

//=============================================================================
// NISEvent::NISEvent
//=============================================================================
// Description: Comment
//
// Parameters:  ()
//
// Return:      NISEvent
//
//=============================================================================
NISEvent::NISEvent()
{
    type = NIS_SCENEGRAPH;
}

//=============================================================================
// NISEvent::~NISEvent
//=============================================================================
// Description: Comment
//
// Parameters:  ()
//
// Return:      NISEvent
//
//=============================================================================
NISEvent::~NISEvent()
{
}

//=============================================================================
// NISEvent::LoadNow
//=============================================================================
// Description: Comment
//
// Parameters:  ()
//
// Return:      void 
//
//=============================================================================
void NISEvent::LoadNow()
{
    AnimationPlayer* player = GetPlayer();

    SetNames();
    
    player->LoadData( fileName, false, GetUserData() );

    player->SetPlayAfterLoad( false );

    SetLoaded( true );
}

//=============================================================================
// NISEvent::LoadFromInventory
//=============================================================================
// Description: Comment
//
// Parameters:  ()
//
// Return:      void 
//
//=============================================================================
void NISEvent::LoadFromInventory()
{
    AnimationPlayer* player = GetPlayer();

    SetNames();
    
    player->LoadData( fileName, true, GetUserData() );

    SetClearWhenDone( false );
    SetLoaded( true );
}

//=============================================================================
// NISEvent::Init
//=============================================================================
// Description: Comment
//
// Parameters:  ()
//
// Return:      void 
//
//=============================================================================
void NISEvent::Init()
{
    PresentationEvent::Init();

    mbHasSetNames = false;
}

//=============================================================================
// NISEvent::Start
//=============================================================================
// Description: Comment
//
// Parameters:  ()
//
// Return:      void 
//
//=============================================================================
void NISEvent::Start()
{
    SetNames();
    
    PresentationEvent::Start();
}

//******************************************************************************
//
// Private Member Functions
//
//******************************************************************************

//=============================================================================
// NISEvent::Update
//=============================================================================
// Description: Comment
//
// Parameters:  ( unsigned int elapsedTime )
//
// Return:      void 
//
//=============================================================================
AnimationPlayer* NISEvent::GetPlayer()
{
    AnimationPlayer* player = NULL;

    switch( type )
    {
    case NIS_CAMERA:
        {
            player = GetPresentationManager()->GetCameraPlayer();
            break;
        }
    case NIS_SCENEGRAPH:
        {
            player = GetPresentationManager()->GetNISPlayer();
            break;
        }
    default:
        {
            // trouble!
            rAssert( false );
            break;
        }
    }

    return( player );
}

//=============================================================================
// NISEvent::SetNames
//=============================================================================
// Description: Comment
//
// Parameters:  ()
//
// Return:      void 
//
//=============================================================================
void NISEvent::SetNames()
{
    if( !mbHasSetNames )
    {
        mbHasSetNames = true;

        switch( type )
        {
        case NIS_CAMERA:
            {
                CameraPlayer* player = GetPresentationManager()->GetCameraPlayer();
                player->SetNameData( controller, camera, animation );
                break;
            }
        case NIS_SCENEGRAPH:
            {
                NISPlayer* player = GetPresentationManager()->GetNISPlayer();
#ifdef RAD_ANDROID
                // Set this for every scene-graph NIS so the flag cannot leak
                // from the catapult into unrelated gag scenes.
                player->SetSHARCatapultOutlineGag(IsSHARCatapultGagFile(fileName));
#endif
                player->SetNameData( controller, camera, animation );
                break;
            }
        default:
            {
                // trouble!
                rAssert( false );
                break;
            }
        }
    }
}
