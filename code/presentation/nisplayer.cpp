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
void SetSHARObjectDimming(float brightness, float saturation);

#include <contexts/contextenum.h>
#include <gameflow/gameflow.h>
#include <interiors/interiormanager.h>
#include <mission/gameplaymanager.h>
#include <render/Enums/RenderEnums.h>

// Match CharacterRenderable's established Level 3-7 brightness/saturation,
// but only apply it to interior gag scene graphs in normal gameplay.
static bool ShouldApplySHARInteriorGagDimming()
{
    GameFlow* gameFlow = GetGameFlow();
    GameplayManager* gameplay = GetGameplayManager();
    InteriorManager* interior = GetInteriorManager();
    if (gameFlow == NULL ||
        (gameFlow->GetCurrentContext() != CONTEXT_GAMEPLAY &&
         gameFlow->GetCurrentContext() != CONTEXT_PAUSE) ||
        gameplay == NULL || interior == NULL || !interior->IsInside() ||
        gameplay->GetGameType() != GameplayManager::GT_NORMAL)
    {
        return false;
    }

    const RenderEnums::LevelEnum level = gameplay->GetCurrentRawLevelIndex();
    return level >= RenderEnums::L3 && level <= RenderEnums::L7;
}

static void SetSHARInteriorGagDimmingForCurrentLevel()
{
    float brightness = 1.0f;
    float saturation = 1.0f;
    GameplayManager* gameplay = GetGameplayManager();
    if (gameplay != NULL)
    {
        switch (gameplay->GetCurrentLevelIndex())
        {
            case RenderEnums::L3:
                brightness = 0.90f;
                saturation = 0.85f;
                break;
            case RenderEnums::L4:
            case RenderEnums::L5:
            case RenderEnums::L6:
                brightness = 0.82f;
                saturation = 0.75f;
                break;
            case RenderEnums::L7:
                brightness = 0.78f;
                saturation = 0.58f;
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
    mpSceneGraph( NULL )
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
        const bool applyInteriorGagDimming = ShouldApplySHARInteriorGagDimming();
        if (applyInteriorGagDimming)
        {
            SetSHARInteriorGagDimmingForCurrentLevel();
        }

        // Enable character-only cel-pass handling for this presentation scene.
        // The skinned-mesh layer filters the actual tPolySkin draws, so static
        // props and effects are not needlessly rendered twice.
        SetSHARGagCharacterCelPassActive(true);
        mpSceneGraph->Display();
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
