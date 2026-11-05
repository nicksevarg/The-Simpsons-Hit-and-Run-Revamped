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

#ifdef RAD_ANDROID
#include <SDL.h>
#include <ctype.h>
#include <string>
#include <unordered_set>

bool IsCelShadingEnabled();
bool IsSHAREnvironmentOutlineDiagnosticMode10();
void SetCelShadingObjectEnabled(bool enabled);
void SetCelShadingOutlinePass(bool enabled);
void SetSHAREnvironmentCelPassActive(bool enabled);
void SetSHAREnvironmentBillboardFoliagePassActive(bool enabled);
void SetSHAREnvironmentFoliageMeshPassActive(bool enabled);
void SetSHAREnvironmentCelDiagnosticEntityNames(const char* ownerName, const char* drawableName);
void SetSHAREnvironmentCelDiagnosticEntityUIDs(unsigned long long ownerUID,
                                                unsigned long long sourceAssetUID,
                                                bool foliageCandidate,
                                                bool roadCandidate);

void LogSHAREnvironmentMode10EntityOnce(const void* entityIdentity,
                                        const char* ownerName,
                                        const char* drawableName,
                                        bool isGeometry,
                                        bool translucent,
                                        bool foliageCandidate,
                                        bool roadCandidate,
                                        unsigned long long ownerUID,
                                        unsigned long long sourceAssetUID)
{
    static std::unordered_set<const void*> reportedEntities;
    if (entityIdentity == NULL || reportedEntities.size() >= 20000)
        return;
    if (!reportedEntities.insert(entityIdentity).second)
        return;

    SDL_Log("[SHAR Outline Mode 10] entity=%p owner=%s drawable=%s ownerUID=%016llx sourceAssetUID=%016llx type=%s translucent=%d foliageCandidate=%d roadCandidate=%d",
            entityIdentity,
            ownerName ? ownerName : "<unnamed>",
            drawableName ? drawableName : "<unnamed-drawable>",
            ownerUID, sourceAssetUID,
            isGeometry ? "geometry" : "billboard",
            translucent ? 1 : 0,
            foliageCandidate ? 1 : 0,
            roadCandidate ? 1 : 0);
}

void LogSHAREnvironmentMode10DSGClassOnce(const void* entityIdentity,
                                          const char* className,
                                          const char* ownerName,
                                          unsigned long long ownerUID,
                                          unsigned long long sourceAssetUID)
{
    static std::unordered_set<const void*> reportedClasses;
    if (entityIdentity == NULL || reportedClasses.size() >= 20000 ||
        !reportedClasses.insert(entityIdentity).second)
        return;
    SDL_Log("[SHAR Outline Mode 10] dsg-display entity=%p class=%s owner=%s ownerUID=%016llx sourceAssetUID=%016llx",
            entityIdentity, className ? className : "<unknown>",
            ownerName ? ownerName : "<unnamed>", ownerUID, sourceAssetUID);
}

static void DisplaySHAREnvironmentCelDrawable(tDrawable* drawable,
                                                bool foliageBillboard = false,
                                                bool foliageMesh = false)
{
    if (drawable == NULL)
        return;
    if (!IsCelShadingEnabled())
    {
        drawable->Display();
        return;
    }
    SetSHAREnvironmentCelPassActive(true);
    SetCelShadingObjectEnabled(true);
    if (foliageBillboard)
        SetSHAREnvironmentBillboardFoliagePassActive(true);
    if (foliageMesh)
        SetSHAREnvironmentFoliageMeshPassActive(true);
    SetCelShadingOutlinePass(true);
    drawable->Display();
    SetCelShadingOutlinePass(false);
    if (foliageMesh)
        SetSHAREnvironmentFoliageMeshPassActive(false);
    if (foliageBillboard)
        SetSHAREnvironmentBillboardFoliagePassActive(false);
    drawable->Display();
    SetCelShadingObjectEnabled(false);
    SetSHAREnvironmentCelPassActive(false);
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
            const char* ownerName = GetName();
            const char* drawableName = mpDrawstuff->GetName();
            const bool foliageName = mSHAREnvironmentFoliageBillboard;
            const bool foliageBillboard =
                !isGeometry && !mTranslucent && foliageName;
            const bool foliageMesh =
                isGeometry && IsSHAREnvironmentFoliageMesh();
            const bool roadCandidate =
                isGeometry && IsSHAREnvironmentRoadCandidate();

            const bool mode10Diagnostics = IsSHAREnvironmentOutlineDiagnosticMode10();
            if (mode10Diagnostics)
            {
                const unsigned long long ownerUID = static_cast<unsigned long long>(GetUID());
                const unsigned long long sourceAssetUID =
                    static_cast<unsigned long long>(GetSHAREnvironmentSourceAssetUID());
                LogSHAREnvironmentMode10EntityOnce(this, ownerName, drawableName,
                    isGeometry, mTranslucent, foliageMesh || foliageName, roadCandidate,
                    ownerUID, sourceAssetUID);
                LogSHAREnvironmentMode10DSGClassOnce(this, "StaticEntityDSG",
                    ownerName, ownerUID, sourceAssetUID);
                SetSHAREnvironmentCelDiagnosticEntityNames(ownerName, drawableName);
                SetSHAREnvironmentCelDiagnosticEntityUIDs(
                    ownerUID, sourceAssetUID, foliageMesh || foliageName, roadCandidate);
            }

            if (isGeometry)
                DisplaySHAREnvironmentCelDrawable(mpDrawstuff, false, foliageMesh);
            else if (foliageBillboard)
                DisplaySHAREnvironmentCelDrawable(mpDrawstuff, true);
            else
                mpDrawstuff->Display();

            if (mode10Diagnostics)
            {
                SetSHAREnvironmentCelDiagnosticEntityNames(NULL, NULL);
                SetSHAREnvironmentCelDiagnosticEntityUIDs(0, 0, false, false);
            }
        }
#else
        mpDrawstuff->Display();
#endif
    }
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


