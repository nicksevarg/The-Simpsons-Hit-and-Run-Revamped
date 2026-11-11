//=============================================================================
// Copyright (C) 2002 Radical Entertainment Ltd.  All rights reserved.
//
// File:        nisplayer.cpp
//
// Description: Implement NISPlayer
//
// History:     16/04/2002 + Created -- NAME
//
//=============================================================================

//========================================
// System Includes
//========================================
// Foundation Tech
#include <raddebug.hpp>
#include <p3d/anim/compositedrawable.hpp>
#include <p3d/utility.hpp>

//========================================
// Project Includes
//========================================
#include <presentation/nisplayer.h>
#ifdef RAD_ANDROID
void SetSHARGagCharacterCelPassActive(bool enabled);
void SetSHARGagCatapultCelPassActive(bool enabled);
void SetSHARObjectDimming(float brightness, float saturation);

#include <contexts/contextenum.h>
#include <gameflow/gameflow.h>
#include <interiors/interiormanager.h>
#include <mission/gameplaymanager.h>
#include <render/Enums/RenderEnums.h>

// Match the character and vehicle profiles for gag scene graphs in normal gameplay.
// The InteriorManager selects the indoor profile; outdoor gags use the open-world values.
static bool ShouldApplySHARGagDimming()
{
    GameFlow* gameFlow = GetGameFlow();
    GameplayManager* gameplay = GetGameplayManager();
    InteriorManager* interior = GetInteriorManager();
    if (gameFlow == NULL ||
        (gameFlow->GetCurrentContext() != CONTEXT_GAMEPLAY &&
         gameFlow->GetCurrentContext() != CONTEXT_PAUSE) ||
        gameplay == NULL || interior == NULL ||
        gameplay->GetGameType() != GameplayManager::GT_NORMAL)
    {
        return false;
    }

    const RenderEnums::LevelEnum level = gameplay->GetCurrentRawLevelIndex();
    return level >= RenderEnums::L3 && level <= RenderEnums::L7;
}

static void SetSHARGagDimmingForCurrentLevel()
{
    float brightness = 1.0f;
    float saturation = 1.0f;
    GameplayManager* gameplay = GetGameplayManager();
    InteriorManager* interior = GetInteriorManager();
    const bool inside = interior != NULL && interior->IsInside();

    if (gameplay != NULL)
    {
        switch (gameplay->GetCurrentRawLevelIndex())
        {
            case RenderEnums::L3:
                brightness = 0.80f;
                saturation = 0.85f;
                break;
            case RenderEnums::L4:
            case RenderEnums::L5:
            case RenderEnums::L6:
                brightness = inside ? 0.70f : 0.60f;
                saturation = 0.75f;
                break;
            case RenderEnums::L7:
                brightness = inside ? 0.70f : 0.50f;
                saturation = inside ? 0.75f : 0.65f;
                break;
            default:
                break;
        }
    }
    SetSHARObjectDimming(brightness, saturation);
}
#endif
#include <loading/filehandlerenum.h>
#include <loading/loadingmanager.h>

#include <render/RenderManager/RenderManager.h>


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

//==============================================================================
// NISPlayer::NISPlayer
//==============================================================================
// Description: Constructor.
//
// Parameters: None.
//
// Return:      N/A.
//
//==============================================================================
NISPlayer::NISPlayer() :
    mpSceneGraph( NULL ),
    mSHARCatapultOutlineGag(false)
{
    SetExclusive( false );    
}

//==============================================================================
// NISPlayer::~NISPlayer
//==============================================================================
// Description: Destructor.
//
// Parameters: None.
//
// Return:      N/A.
//
//==============================================================================
NISPlayer::~NISPlayer()
{
    
}


//=============================================================================
// NISPlayer::ClearData
//=============================================================================
// Description: Comment
//
// Parameters:  ()
//
// Return:      void 
//
//=============================================================================
void NISPlayer::ClearData()
{
    SimpleAnimationPlayer::ClearData();

    if( mpSceneGraph != NULL )
    {
        GetRenderManager()->mEntityDeletionList.Add((tRefCounted*&)mpSceneGraph);
        mpSceneGraph = NULL;
    }
}


//******************************************************************************
//
// Private Member Functions
//
//******************************************************************************

//=============================================================================
// NISPlayer::DoLoaded
//=============================================================================
// Description: Comment
//
// Parameters:  ()
//
// Return:      void 
//
//=============================================================================
void NISPlayer::DoLoaded()
{
    SimpleAnimationPlayer::DoLoaded();

    tRefCounted::Release(mpSceneGraph);
    tRefCounted::Assign(mpSceneGraph, (tDrawable*)p3d::find<Scenegraph::Scenegraph>( GetAnimationName() ));
    if(!mpSceneGraph)
    {
        tRefCounted::Assign(mpSceneGraph, (tDrawable*)p3d::find<tCompositeDrawable>( GetAnimationName() ));
    }
}

//=============================================================================
// NISPlayer::DoRender
//=============================================================================
// Description: Comment
//
// Parameters:  ()
//
// Return:      void 
//
//=============================================================================
void NISPlayer::DoRender()
{
    SimpleAnimationPlayer::DoRender();
    
    if(mpSceneGraph)
    {
#ifdef RAD_ANDROID
        // Interior gag scenes should use the same level-specific night tone as
        // regular characters and vehicles. The shared shader dimming also
        // covers static props/assets in this scene graph.
        const bool applyInteriorGagDimming = ShouldApplySHARGagDimming();
        if (applyInteriorGagDimming)
        {
            SetSHARGagDimmingForCurrentLevel();
        }

        // Skinned gag characters use their existing pass; only named static
        // catapult props in a catapult gag receive the additional prop pass.
        SetSHARGagCharacterCelPassActive(true);
        SetSHARGagCatapultCelPassActive(mSHARCatapultOutlineGag);
        mpSceneGraph->Display();
        SetSHARGagCatapultCelPassActive(false);
        SetSHARGagCharacterCelPassActive(false);

        // Avoid leaking this scene's lighting adjustment into later rendering.
        if (applyInteriorGagDimming)
        {
            SetSHARObjectDimming(1.0f, 1.0f);
        }
#else
        mpSceneGraph->Display();
#endif
    }
}

//=============================================================================
// NISPlayer::GetBoundingBox
//=============================================================================
// Description: Comment
//
// Parameters:  ( rmt::Box3D* box )
//
// Return:      true if box filled out, false otherwise 
//
//=============================================================================
bool NISPlayer::GetBoundingBox( rmt::Box3D* box )
{
    bool retval = false;

    if( mpSceneGraph )
    {
        mpSceneGraph->GetBoundingBox( box );
        retval = true;
    }

    return( retval );
}
