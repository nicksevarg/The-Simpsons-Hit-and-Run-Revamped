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

static bool SHARContainsNameToken(const char* name, const char* token)
{
    if (name == NULL || token == NULL || token[0] == '\0')
        return false;
    for (const char* start = name; *start != '\0'; ++start)
    {
        const char* a = start;
        const char* b = token;
        while (*a != '\0' && *b != '\0' &&
               tolower(static_cast<unsigned char>(*a)) ==
               tolower(static_cast<unsigned char>(*b)))
        {
            ++a;
            ++b;
        }
        if (*b == '\0')
            return true;
    }
    return false;
}

static bool SHARIsFoliageBillboardName(const char* name)
{
    return SHARContainsNameToken(name, "tree") ||
           SHARContainsNameToken(name, "foliage") ||
           SHARContainsNameToken(name, "leaf") ||
           SHARContainsNameToken(name, "leaves") ||
           SHARContainsNameToken(name, "bush") ||
           SHARContainsNameToken(name, "shrub") ||
           SHARContainsNameToken(name, "canopy") ||
           SHARContainsNameToken(name, "branch") ||
           SHARContainsNameToken(name, "plant") ||
           SHARContainsNameToken(name, "hedge");
}

static bool SHARIsRoadGeometryName(const char* name)
{
    return SHARContainsNameToken(name, "road") ||
           SHARContainsNameToken(name, "street") ||
           SHARContainsNameToken(name, "asphalt") ||
           SHARContainsNameToken(name, "pavement") ||
           SHARContainsNameToken(name, "sidewalk") ||
           SHARContainsNameToken(name, "curb") ||
           SHARContainsNameToken(name, "kerb") ||
           SHARContainsNameToken(name, "concrete") ||
           SHARContainsNameToken(name, "walkway") ||
           SHARContainsNameToken(name, "driveway") ||
           SHARContainsNameToken(name, "path");
}

static void LogSHAREnvironmentMode10EntityOnce(const char* ownerName,
                                                const char* drawableName,
                                                bool isGeometry,
                                                bool translucent,
                                                bool foliageCandidate,
                                                bool roadCandidate)
{
    static std::unordered_set<std::string> reportedEntities;
    if (reportedEntities.size() >= 700)
        return;
    std::string key = ownerName ? ownerName : "<unnamed>";
    key += '|';
    key += drawableName ? drawableName : "<unnamed-drawable>";
    if (!reportedEntities.insert(key).second)
        return;

    SDL_Log("[SHAR Outline Mode 10] entity owner=%s drawable=%s type=%s translucent=%d foliageCandidate=%d roadCandidate=%d",
            ownerName ? ownerName : "<unnamed>",
            drawableName ? drawableName : "<unnamed-drawable>",
            isGeometry ? "geometry" : "billboard",
            translucent ? 1 : 0,
            foliageCandidate ? 1 : 0,
            roadCandidate ? 1 : 0);
}

static void DisplaySHAREnvironmentCelDrawable(tDrawable* drawable, bool foliageBillboard = false)
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
    SetCelShadingOutlinePass(true);
    drawable->Display();
    SetCelShadingOutlinePass(false);
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
            const bool foliageName =
                SHARIsFoliageBillboardName(ownerName) ||
                SHARIsFoliageBillboardName(drawableName);
            const bool foliageBillboard =
                IsSHAREnvironmentOutlineDiagnosticMode10() &&
                !isGeometry && !mTranslucent && foliageName;
            const bool roadCandidate =
                isGeometry &&
                (SHARIsRoadGeometryName(ownerName) ||
                 SHARIsRoadGeometryName(drawableName));

            if (IsSHAREnvironmentOutlineDiagnosticMode10())
                LogSHAREnvironmentMode10EntityOnce(ownerName, drawableName,
                    isGeometry, mTranslucent, foliageName, roadCandidate);

            if (isGeometry)
                DisplaySHAREnvironmentCelDrawable(mpDrawstuff);
            else if (foliageBillboard)
                DisplaySHAREnvironmentCelDrawable(mpDrawstuff, true);
            else
                mpDrawstuff->Display();
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


