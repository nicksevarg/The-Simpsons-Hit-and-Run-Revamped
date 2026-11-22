//========================================================================
// Copyright (C) 2002 Radical Entertainment Ltd.  All rights reserved.
//
// File:        StaticEntityDSG.cpp
//
// Description: Implementation for StaticEntityDSG class.
//
// History:     Implemented	                         --Devin [5/27/2002]
//========================================================================

//========================================
// System Includes
//========================================

//========================================
// Project Includes
//========================================
#include <render/DSG/StaticEntityDSG.h>
#include <memory/srrmemory.h>
#include <p3d/utility.hpp>
#include <p3d/texture.hpp>
#include <render/DSG/SHAREnvironmentCel.h>
#include <p3d/sharrendercontext.hpp>
#include <mission/gameplaymanager.h>
#include <interiors/interiormanager.h>
#ifdef RAD_ANDROID
#include <pddi/gles/shartransparencydepthwrite.hpp>
#endif

#ifdef RAD_ANDROID

bool IsCelShadingEnabled();
bool IsSHARCelShadingLite();
bool IsSHARGagCharacterCelPassActive();
void SetCelShadingObjectEnabled(bool enabled);
void SetCelShadingOutlinePass(bool enabled);
void SetSHAREnvironmentCelPassActive(bool enabled);
void SetSHARLevelOneOutsideVertexGrayscaleActive(bool enabled);

static bool IsSHARLevelOneOutside()
{
    GameplayManager* gameplayManager = GetGameplayManager();
    InteriorManager* interiorManager = GetInteriorManager();
    return gameplayManager != NULL &&
           gameplayManager->GetCurrentLevelIndex() == RenderEnums::L1 &&
           interiorManager != NULL &&
           !interiorManager->IsInside();
}
void SetSHAREnvironmentBillboardFoliagePassActive(bool enabled);
void SetSHAREnvironmentFoliageMeshPassActive(bool enabled);

static void DisplaySHARGagCelDrawable(tDrawable* drawable)
{
    if (drawable == NULL)
        return;

    if (!IsCelShadingEnabled())
    {
        drawable->Display();
        return;
    }

    SetCelShadingObjectEnabled(true);
    SetCelShadingOutlinePass(true);
    drawable->Display();
    SetCelShadingOutlinePass(false);
    drawable->Display();
    SetCelShadingObjectEnabled(false);
}

static void DisplaySHAREnvironmentCelDrawable(
    tDrawable* drawable,
    const rmt::Vector& worldCentre,
    float boundingRadius,
    bool foliageBillboard = false,
    bool foliageMesh = false)
{
    if (drawable == NULL)
        return;

    // Scope vertex-colour chroma removal to Level 1 exterior environment
    // draws. Keep it active for the base draw even when Lite mode skips outlines.
    const bool grayscaleLevelOneOutside =
        IsCelShadingEnabled() && IsSHARLevelOneOutside();
    SetSHARLevelOneOutsideVertexGrayscaleActive(grayscaleLevelOneOutside);

    float outlineWidth = 1.33f;
    if (!SHARShouldDrawEnvironmentCelOutline(
            foliageBillboard || foliageMesh, worldCentre, boundingRadius, &outlineWidth))
    {
        drawable->Display();
        SetSHARLevelOneOutsideVertexGrayscaleActive(false);
        return;
    }

    SetSHAREnvironmentCelPassActive(true);
    SetSHAREnvironmentOutlineWidth(outlineWidth);
    SetCelShadingObjectEnabled(true);
    if (foliageBillboard)
        SetSHAREnvironmentBillboardFoliagePassActive(true);
    if (foliageMesh)
        SetSHAREnvironmentFoliageMeshPassActive(true);
    SetCelShadingOutlinePass(true);
    drawable->Display();
    SetCelShadingOutlinePass(false);
    SetSHAREnvironmentOutlineWidth(1.33f);
    if (foliageMesh)
        SetSHAREnvironmentFoliageMeshPassActive(false);
    if (foliageBillboard)
        SetSHAREnvironmentBillboardFoliagePassActive(false);
    drawable->Display();
    SetCelShadingObjectEnabled(false);
    SetSHAREnvironmentCelPassActive(false);
    SetSHARLevelOneOutsideVertexGrayscaleActive(false);
}
#endif


//************************************************************************
//
// Global Data, Local Data, Local Classes
//
//************************************************************************

//************************************************************************
//
// Public Member Functions : StaticEntityDSG Interface
//
//************************************************************************
//========================================================================
// StaticEntityDSG::
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
StaticEntityDSG::StaticEntityDSG()
{
   mpDrawstuff = NULL;
#ifdef RAD_ANDROID
   mSHAREnvironmentFoliageBillboard = false;
#endif
}
//========================================================================
// StaticEntityDSG::
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
StaticEntityDSG::~StaticEntityDSG()
{
BEGIN_PROFILE( "StaticEntityDSG Destroy" );
   if(mpDrawstuff != NULL)
   {
      mpDrawstuff->Release();
   }
END_PROFILE( "StaticEntityDSG Destroy" );
}
//========================================================================
// StaticEntityDSG::SetRank
//========================================================================
//
// Description: Sets rank, defaults to default SetRank for normal geo
//              however, shadows always get drawn first in the translucent pass
//
// Parameters:  rmt::Vector& irRefPosn, rmt::Vector& mViewVector.
//
// Return:      None.
//
// Constraints: None.
//
//========================================================================
void StaticEntityDSG::SetRank(rmt::Vector& irRefPosn, rmt::Vector& mViewVector)
{
    if ( ( mIsGeo & IS_SHADOW ) == false )
    {
        IEntityDSG::SetRank( irRefPosn, mViewVector );
    }
    else
    {
        mRank = FLT_MAX;
    }
}

//========================================================================
// StaticEntityDSG::
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
void StaticEntityDSG::SetGeometry(tGeometry* ipGeo)
{
   if(mpDrawstuff != NULL)
   {
      mpDrawstuff->Release();
   }
   
   mpDrawstuff = ipGeo;

   if(mpDrawstuff != NULL)
   {
      mpDrawstuff->AddRef();
   }
   mIsGeo = GEO;

   if(ipGeo->CastsShadow())
   {
       mIsGeo = mIsGeo | IS_SHADOW;
   }
   
//   mShaderUID = ipGeo->GetShader(0)->GetUID();
   ipGeo->ProcessShaders(*this);

   SetInternalState();
}
//========================================================================
// StaticEntityDSG::
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
tGeometry* StaticEntityDSG::mpGeo()
{
   return (tGeometry*)mpDrawstuff;
}
//========================================================================
// StaticEntityDSG::
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
void StaticEntityDSG::SetDrawable(tDrawable* ipDraw)
{
   if(mpDrawstuff != NULL)
   {
      mpDrawstuff->Release();
   }
   
   mpDrawstuff = ipDraw;

   if(mpDrawstuff != NULL)
   {
      mpDrawstuff->AddRef();
   }

   mIsGeo = NOT_GEO;

   SetInternalState();
}
//========================================================================
// StaticEntityDSG::
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
tDrawable* StaticEntityDSG::mpDraw()
{
    return mpDrawstuff;

}

///////////////////////////////////////////////////////////////////////
// Drawable
///////////////////////////////////////////////////////////////////////
//========================================================================
// StaticEntityDSG::
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
void StaticEntityDSG::Display()
{
#ifdef PROFILER_ENABLED
    char profileName[] = "  StaticEntityDSG Display";
#endif
    if(IS_DRAW_LONG) return;
#ifdef RAD_ANDROID
    UpdateSelectedShadowAlphaForCelState();
    unsigned long long drawableUID = 0;
    if (mpDrawstuff != NULL)
    {
        drawableUID = static_cast<unsigned long long>(mpDrawstuff->GetUID());
        SetSHARCurrentDrawableUID(drawableUID);
    }

    // Moe's Tavern mirror is a blended surface that must not write depth
    // while cel shading is OFF. Apply this around the complete drawable draw,
    // rather than in pglMat::SetDevPass(), which may be skipped by the
    // single-pass material cache.
    const bool disableMoeMirrorDepthWrites =
        mpDrawstuff != NULL &&
        !IsCelShadingEnabled() &&
        drawableUID == 0x7CE29D71CD240C0AULL;
    const bool previousZWrite =
        disableMoeMirrorDepthWrites ? p3d::pddi->GetZWrite() : false;
    if (disableMoeMirrorDepthWrites)
    {
        p3d::pddi->SetZWrite(false);
    }

    // Moe's Tavern cash register keeps its genuine alpha blending, but its
    // translucent material must still write depth while cel shading is ON.
    // Scope the existing suppression bypass and the actual PDDI depth mask
    // around this drawable because SetDevPass() may be skipped by the
    // single-pass material cache. The same asset UID is used in L2 and L5.
    const bool enableMoeRegisterDepthWrites =
        mpDrawstuff != NULL &&
        IsCelShadingEnabled() &&
        drawableUID == 0x7CE29D71CD240C09ULL;
    const bool previousMoeRegisterDepthWritePolicy =
        enableMoeRegisterDepthWrites
            ? IsSHARExteriorTransparencyDepthWriteFixActive()
            : false;
    const bool previousMoeRegisterZWrite =
        enableMoeRegisterDepthWrites ? p3d::pddi->GetZWrite() : false;
    if (enableMoeRegisterDepthWrites)
    {
        SetSHARExteriorTransparencyDepthWriteFixActive(true);
        p3d::pddi->SetZWrite(true);
    }
#endif
    DSG_BEGIN_PROFILE(profileName)

    if(mIsGeo & IS_SHADOW)
    {
        p3d::pddi->SetZWrite(false);
        mpDrawstuff->Display();
        p3d::pddi->SetZWrite(true);
    }
    else
    {
#ifdef RAD_ANDROID
        if (mpDrawstuff != NULL)
        {
            const bool isGeometry = (mIsGeo & GEO) != 0;
            const bool foliageName = mSHAREnvironmentFoliageBillboard;
            const bool foliageBillboard =
                !isGeometry && !mTranslucent && foliageName;
            const bool foliageMesh =
                isGeometry && IsSHAREnvironmentFoliageMesh();
            rmt::Sphere outlineBounds;
            mpDrawstuff->GetBoundingSphere(&outlineBounds);
            if (IsSHARGagCharacterCelPassActive() && !mTranslucent)
                DisplaySHARGagCelDrawable(mpDrawstuff);
            else if (isGeometry)
                DisplaySHAREnvironmentCelDrawable(mpDrawstuff, mPosn, outlineBounds.radius, false, foliageMesh);
            else if (foliageBillboard)
                DisplaySHAREnvironmentCelDrawable(mpDrawstuff, mPosn, outlineBounds.radius, true, false);
            else
                mpDrawstuff->Display();
        }
#else
        mpDrawstuff->Display();
#endif
    }
#ifdef RAD_ANDROID
    if (enableMoeRegisterDepthWrites)
    {
        SetSHARExteriorTransparencyDepthWriteFixActive(
            previousMoeRegisterDepthWritePolicy);
        p3d::pddi->SetZWrite(previousMoeRegisterZWrite);
    }
    if (disableMoeMirrorDepthWrites)
    {
        p3d::pddi->SetZWrite(previousZWrite);
    }
    ClearSHARCurrentDrawableUID();
#endif
    DSG_END_PROFILE(profileName)
}

#ifndef RAD_RELEASE
//========================================================================
// StaticEntityDSG::
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
void StaticEntityDSG::DisplayBoundingBox(tColour colour)
{
#ifndef RAD_RELEASE
   mpDrawstuff->DisplayBoundingBox(colour);
#endif
}
//========================================================================
// StaticEntityDSG::
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
void StaticEntityDSG::DisplayBoundingSphere(tColour colour)
{
   mpDrawstuff->DisplayBoundingSphere(colour);
}
#endif

//========================================================================
// StaticEntityDSG::
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
void StaticEntityDSG::GetBoundingBox(rmt::Box3D* box)
{
   mpDrawstuff->GetBoundingBox(box);
}
//========================================================================
// StaticEntityDSG::
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
void StaticEntityDSG::GetBoundingSphere(rmt::Sphere* sphere)
{
   mpDrawstuff->GetBoundingSphere(sphere);
}
///////////////////////////////////////////////////////////////////////
// IEntityDSG
///////////////////////////////////////////////////////////////////////
//========================================================================
// StaticEntityDSG::
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
rmt::Vector* StaticEntityDSG::pPosition()
{
   return &mPosn;
}
//========================================================================
// StaticEntityDSG::
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
const rmt::Vector& StaticEntityDSG::rPosition()
{
   return mPosn;
}
//========================================================================
// StaticEntityDSG::
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
void StaticEntityDSG::GetPosition( rmt::Vector* ipPosn )
{
   *ipPosn = mPosn;
}
//========================================================================
// StaticEntityDSG::
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
void StaticEntityDSG::RenderUpdate()
{
   //Do Nothing
}
//************************************************************************
//
// Protected Member Functions : StaticEntityDSG 
//
//************************************************************************
//========================================================================
// StaticEntityDSG::
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
void StaticEntityDSG::SetInternalState()
{
   rmt::Sphere sphere;

   mpDrawstuff->GetBoundingSphere(&sphere);
   mPosn = sphere.centre;
}
//************************************************************************
//
// Private Member Functions : StaticEntityDSG 
//
//************************************************************************


