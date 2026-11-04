//===========================================================================
// Copyright (C) 2002 Radical Entertainment Ltd.  All rights reserved.
//
// Component:   BreakableObjectDSG
//
// Description: Breakleobjects
//
// Authors:     Michael Riegger
//
//===========================================================================

//---------------------------------------------------------------------------
// Includes
//===========================================================================

#include <render/DSG/breakableobjectdsg.h>
#include <p3d/anim/animatedobject.hpp>
#include <p3d/matrixstack.hpp>
#include <p3d/utility.hpp>
#include <p3d/anim/compositedrawable.hpp>

#ifdef RAD_ANDROID
#include <SDL.h>
void LogSHAREnvironmentMode10DSGClassOnce(const void* entityIdentity,
                                          const char* className,
                                          const char* ownerName,
                                          unsigned long long ownerUID,
                                          unsigned long long sourceAssetUID);
bool IsCelShadingEnabled();
bool IsSHAREnvironmentOutlineDiagnosticMode10();
void SetCelShadingObjectEnabled(bool enabled);
void SetCelShadingOutlinePass(bool enabled);
void SetSHAREnvironmentCelPassActive(bool enabled);
void SetSHAREnvironmentFoliageMeshPassActive(bool enabled);
bool IsSHAREnvironmentFoliageEntityName(const char* ownerName, const char* drawableName);
bool IsSHAREnvironmentRoadEntityName(const char* name);
bool IsSHARMode10FoliageUID(unsigned long long uid);
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
                                        unsigned long long sourceAssetUID);
#endif

//===========================================================================
// Local Constants, Typedefs, and Macros
//===========================================================================




//===========================================================================
// Global Data, Local Data, Local Classes
//===========================================================================


//===========================================================================
// Member Functions
//===========================================================================

//===========================================================================
// BreakableObjectDSG::BreakableObjectDSG
//===========================================================================
// Description:
//		A breakable DSG object's ctor
//
// Constraints:
//		Most other DSG objects expect a preallocated matrix to be placed handed 
//		to the object on loadsetup. This class allocates its own matrix here
//		mTranslucent flag set to true. Probably a pretty safe assumption, since 
//		breakables would have to fade out to look decent
//
// Parameters:
//
//
// Return:
//
//
//===========================================================================

BreakableObjectDSG::BreakableObjectDSG()
: mpObject( NULL ),
mpController( NULL )
{
	IEntityDSG::mTranslucent = true;
    mpMatrix = new rmt::Matrix;   
    mpMatrix->Identity();
}

//===========================================================================
// BreakableObjectDSG::~BreakableObjectDSG
//===========================================================================
// Description:
//		A breakable DSG object's dtor
//
// Constraints:
//		
//	
// Parameters:
//
//
// Return:
//
//
//===========================================================================


BreakableObjectDSG::~BreakableObjectDSG()
{
BEGIN_PROFILE( "BreakableObjectDSG Destroy" );
    // The controller should also release the attached effect, use the debugger to 
    // make sure this happens!
    if (mpController != NULL)
    {
        mpController->Release();
        mpController = NULL;
    }

    delete mpMatrix;
    mpMatrix = NULL;
END_PROFILE( "BreakableObjectDSG Destroy" );
}
//===========================================================================
// BreakableObjectDSG::Init
//===========================================================================
// Description:
//		Create a breakable from a tAnimatedObject via cloning
//
// Constraints:
//		Only call it once
//	
// Parameters:
//
//
// Return:
//
//
//===========================================================================

void BreakableObjectDSG::Init( tAnimatedObjectFactory* pFactory, tAnimatedObjectFrameController* pController)
{
    rAssert( pFactory != NULL );
    rAssert( pController != NULL );

    rAssert( mpController == NULL );
    rAssert( mpObject == NULL );

    // Clone a new frame controller off the given controller
    mpController = static_cast<tAnimatedObjectFrameController*> (pController->Clone() );
    // Check that the clone and downcast both worked correctly
    rAssert( mpController != NULL );
          
    // Clone a new animated object from the factory
    mpObject = pFactory->CreateObject( mpController );

    mpObject->ProcessShaders(*this);
#ifdef RAD_ANDROID
    // Breakables are allocated from a shared animated-object factory. Tag the
    // DSG from both the factory and its source drawable so release builds do
    // not depend on instance names being retained.
    tDrawable* sourceDrawable = mpObject->GetBaseObject();
    const char* factoryName = pFactory->GetName();
    const char* drawableName = sourceDrawable ? sourceDrawable->GetName() : NULL;
    const bool foliageTag =
        IsSHARMode10FoliageUID(
            static_cast<unsigned long long>(pFactory->GetUID())) ||
        (sourceDrawable && IsSHARMode10FoliageUID(
            static_cast<unsigned long long>(sourceDrawable->GetUID()))) ||
        IsSHAREnvironmentFoliageEntityName(factoryName, drawableName);
    const bool roadTag =
        IsSHAREnvironmentRoadEntityName(factoryName) ||
        IsSHAREnvironmentRoadEntityName(drawableName);
    tUID sourceAssetUID = sourceDrawable ?
        sourceDrawable->GetUID() : pFactory->GetUID();
    if (IsSHAREnvironmentOutlineDiagnosticMode10())
    {
        SDL_Log("[SHAR Outline Mode 10] breakable-identity factoryUID=%016llx animatedObjectUID=%016llx baseDrawableUID=%016llx storedSourceAssetUID=%016llx",
            static_cast<unsigned long long>(pFactory->GetUID()),
            static_cast<unsigned long long>(mpObject->GetUID()),
            sourceDrawable ? static_cast<unsigned long long>(sourceDrawable->GetUID()) : 0ULL,
            static_cast<unsigned long long>(sourceAssetUID));
    }
    SetSHAREnvironmentFoliageMesh(foliageTag);
    SetSHAREnvironmentRoadCandidate(roadTag);
    SetSHAREnvironmentSourceAssetUID(sourceAssetUID);
    if (IsSHAREnvironmentOutlineDiagnosticMode10() && (foliageTag || roadTag))
    {
        SDL_Log("[SHAR Outline Mode 10] asset-tag route=BreakableObject factory=%s drawable=%s sourceAssetUID=%016llx foliage=%d road=%d",
            factoryName ? factoryName : "<unnamed-factory>",
            drawableName ? drawableName : "<unnamed-drawable>",
            static_cast<unsigned long long>(sourceAssetUID),
            foliageTag ? 1 : 0, roadTag ? 1 : 0);
    }
#endif
}
//===========================================================================
// BreakableObjectDSG::Display
//===========================================================================
// Description:
//		Draw the breakable
//
// Constraints:
//		
//	
// Parameters:
//
//
// Return:
//
//
//===========================================================================

void BreakableObjectDSG::Display()
{
    if(IS_DRAW_LONG) return;
#ifdef PROFILER_ENABLED
    char profileName[] = "  BreakableObjectDSG Display";
#endif
    DSG_BEGIN_PROFILE(profileName)
    p3d::stack->PushMultiply( *mpMatrix );
#ifdef RAD_ANDROID
    const bool mode10Diagnostics = IsSHAREnvironmentOutlineDiagnosticMode10();
    const bool foliageMesh = IsSHAREnvironmentFoliageMesh();
    const bool roadCandidate = IsSHAREnvironmentRoadCandidate();
    tDrawable* sourceDrawable = mpObject ? mpObject->GetBaseObject() : NULL;
    if (mode10Diagnostics && sourceDrawable != NULL)
    {
        const unsigned long long ownerUID = static_cast<unsigned long long>(GetUID());
        const unsigned long long sourceAssetUID =
            static_cast<unsigned long long>(GetSHAREnvironmentSourceAssetUID());
        const char* ownerName = GetName();
        const char* drawableName = sourceDrawable->GetName();
        LogSHAREnvironmentMode10DSGClassOnce(this, "BreakableObjectDSG",
            ownerName, ownerUID, sourceAssetUID);
        LogSHAREnvironmentMode10EntityOnce(this, ownerName, drawableName, true,
            mTranslucent, foliageMesh, roadCandidate, ownerUID, sourceAssetUID);
        SetSHAREnvironmentCelDiagnosticEntityNames(ownerName, drawableName);
        SetSHAREnvironmentCelDiagnosticEntityUIDs(
            ownerUID, sourceAssetUID, foliageMesh, roadCandidate);
    }

    if (IsCelShadingEnabled())
    {
        SetSHAREnvironmentCelPassActive(true);
        SetCelShadingObjectEnabled(true);
        if (foliageMesh)
            SetSHAREnvironmentFoliageMeshPassActive(true);
        SetCelShadingOutlinePass(true);
        // The first animated-object display advances its accumulated time
        // and draws the updated base object as the outline shell. Draw the
        // already-updated base drawable directly for the normal surface so
        // the animation wrapper is not evaluated a second time.
        mpObject->Display();
        SetCelShadingOutlinePass(false);
        if (foliageMesh)
            SetSHAREnvironmentFoliageMeshPassActive(false);
        if (sourceDrawable != NULL)
            sourceDrawable->Display();
        SetCelShadingObjectEnabled(false);
        SetSHAREnvironmentCelPassActive(false);
    }
    else
    {
        mpObject->Display();
    }

    if (mode10Diagnostics)
    {
        SetSHAREnvironmentCelDiagnosticEntityNames(NULL, NULL);
        SetSHAREnvironmentCelDiagnosticEntityUIDs(0, 0, false, false);
        SetSHAREnvironmentFoliageMeshPassActive(false);
    }
#else
    mpObject->Display();
#endif
    p3d::stack->Pop();
    DSG_END_PROFILE(profileName)
}

//===========================================================================
// BreakableObjectDSG::GetBoundingBox
//===========================================================================
// Description:
//		Fetches the bounding box
//
// Constraints:
//		Its not accurate!
//	
// Parameters:
//
//
// Return:
//
//
//===========================================================================

void BreakableObjectDSG::GetBoundingBox( rmt::Box3D* box )
{

    mpObject->GetBaseObject()->GetBoundingBox( box );
    // Lets inflate the bounding box, as particle systems do not have accurate ones
    rmt::Vector inflate( 5.0f,5.0f,5.0f );
	box->low += ( rPosition() - inflate );
	box->high += ( rPosition() + inflate );

}
void BreakableObjectDSG::GetBoundingSphere( rmt::Sphere* pSphere )
{
    mpObject->GetBaseObject()->GetBoundingSphere( pSphere );
    pSphere->centre += rPosition();
    // Lets inflate the bounding sphere, as particle systems do not have accurate ones
    pSphere->radius += 5.0f;
}


//===========================================================================
// BreakableObjectDSG::Update
//===========================================================================
// Description:
//      Advances effect animation
//
// Constraints:
//      None.
//
// Parameters:
//      Time to advance effect in milliseconds
//
// Return:
//      None
//
//===========================================================================
void BreakableObjectDSG::Update( float deltaTime )
{
    mpController->Advance( deltaTime, true );
}
rmt::Vector* BreakableObjectDSG::pPosition()
{
    rAssert( 0 ); // no breaking encapsulation!
    return &mPosn;
}
const rmt::Vector& BreakableObjectDSG::rPosition()
{
    return mpMatrix->Row( 3 );
}
void BreakableObjectDSG::GetPosition( rmt::Vector* ipPosn )
{
    *ipPosn = mpMatrix->Row( 3 );
}
void BreakableObjectDSG::SetTransform( const rmt::Matrix& transform )
{
    *mpMatrix = transform;
}
void BreakableObjectDSG::Reset()
{
    mpController->Reset();
}
int BreakableObjectDSG::LastFrameReached()
{
    return mpController->LastFrameReached();
}