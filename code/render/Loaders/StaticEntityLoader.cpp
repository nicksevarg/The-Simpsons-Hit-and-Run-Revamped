//========================================================================
// Copyright (C) 2002 Radical Entertainment Ltd.  All rights reserved.
//
// File:        StaticEntityLoader.cpp
//
// Description: Implementation for StaticEntityLoader class.
//
// History:     Implemented	                         --Devin [5/27/2002]
//========================================================================

//========================================
// System Includes
//========================================
#include <p3d/chunkfile.hpp>
#include <p3d/utility.hpp>
#include <p3d/geometry.hpp>
#include <p3d/shader.hpp>

//========================================
// Project Includes
//========================================
#include <render/Loaders/StaticEntityLoader.h>
#include <render/Loaders/GeometryWrappedLoader.h>
#include <render/DSG/StaticEntityDSG.h>

#include <constants/srrchunks.h>
#include <constants/chunkids.hpp>
#include <memory/srrmemory.h>

#include <render/Loaders/AllWrappers.h>
#include <string.h>
#ifdef RAD_ANDROID
bool IsSHAREnvironmentFoliageEntityName(const char* ownerName, const char* drawableName);
bool IsSHAREnvironmentRoadEntityName(const char* name);

static bool IsSHARHouseLeafMaterial(const tShader* shader)
{
    return shader != NULL &&
        static_cast<unsigned long long>(shader->GetUID()) == 0x396C6BB87163F912ULL;
}
#endif

//************************************************************************
//
// Global Data, Local Data, Local Classes
//
//************************************************************************

//************************************************************************
//
// Public Member Functions : StaticEntityLoader Interface
//
//************************************************************************
//========================================================================
// StaticEntityLoader::
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
StaticEntityLoader::StaticEntityLoader() :
tSimpleChunkHandler(SRR2::ChunkID::ENTITY_DSG)
{
   mpListenerCB  = NULL;
   mUserData     = -1;
}
///////////////////////////////////////////////////////////////////////
// tSimpleChunkHandler
///////////////////////////////////////////////////////////////////////
//========================================================================
// StaticEntityLoader::
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
tEntity* StaticEntityLoader::LoadObject(tChunkFile* f, tEntityStore* store)
{
    IEntityDSG::msDeletionsSafe=true;
    char name[255];
    f->GetPString(name);

    int version = f->GetLong();
    int HasAlpha = f->GetLong();

    StaticEntityDSG *pStaticEntityDSG = new StaticEntityDSG;
    pStaticEntityDSG->SetName(name);

    if(HasAlpha)
    {
        pStaticEntityDSG->mTranslucent = true;
    }
    
    while(f->ChunksRemaining())
    {      
        f->BeginChunk();
        switch(f->GetCurrentID())
        {
            case Pure3D::Mesh::MESH:
            {
                GeometryWrappedLoader* pGeoLoader = (GeometryWrappedLoader*)AllWrappers::GetInstance()->mpLoader(AllWrappers::msGeometry) ;
                rAssert(pGeoLoader!=NULL);
				tGeometry* pGeo = (tGeometry*)pGeoLoader->LoadObject(f, store);
                rAssert(pGeo!=NULL);
                pStaticEntityDSG->SetGeometry(pGeo);
#ifdef RAD_ANDROID
                const bool foliageNameTag = IsSHAREnvironmentFoliageEntityName(name, NULL);
                bool houseLeafMaterialTag = false;
                unsigned long long matchedHouseLeafShaderUID = 0;
                for (int shaderIndex = 0; shaderIndex < pGeo->GetNumShader(); ++shaderIndex)
                {
                    tShader* shader = pGeo->GetShader(shaderIndex);
                    if (IsSHARHouseLeafMaterial(shader))
                    {
                        houseLeafMaterialTag = true;
                        matchedHouseLeafShaderUID =
                            static_cast<unsigned long long>(shader->GetUID());
                        break;
                    }
                }
                const bool foliageTag = foliageNameTag || houseLeafMaterialTag;
                const bool roadTag = IsSHAREnvironmentRoadEntityName(name);
                tUID sourceAssetUID = tName::MakeUID(name);
                pStaticEntityDSG->SetSHAREnvironmentFoliageMesh(foliageTag);
                pStaticEntityDSG->SetSHAREnvironmentRoadCandidate(roadTag);
                pStaticEntityDSG->SetSHAREnvironmentSourceAssetUID(sourceAssetUID);

                
                #endif
            }

            default:
                break;
        } // switch
        f->EndChunk();
    } // while

    mpListenerCB->OnChunkLoaded( pStaticEntityDSG, mUserData, _id );
    IEntityDSG::msDeletionsSafe=false;
    return pStaticEntityDSG;
}
///////////////////////////////////////////////////////////////////////
// IWrappedLoader
///////////////////////////////////////////////////////////////////////
//========================================================================
// StaticEntityLoader::SetRegdListener
//========================================================================
//
// Description: Register a new listener/caretaker, notify old listener of 
//              severed connection.
//
// Parameters:  pListenerCB: Callback to call OnChunkLoaded
//              pUserData:   Data to pass along for filtering, etc
//
// Return:      None.
//
// Constraints: None.
//
//========================================================================
void StaticEntityLoader::SetRegdListener
(
   ChunkListenerCallback* pListenerCB,
   int iUserData 
)
{
   //
   // Follow protocol; notify old Listener, that it has been 
   // "disconnected".
   //
   if( mpListenerCB != NULL )
   {
      mpListenerCB->OnChunkLoaded( NULL, iUserData, 0 );
   }
   mpListenerCB  = pListenerCB;
   mUserData     = iUserData;
}

//========================================================================
// StaticEntityLoader::ModRegdListener
//========================================================================
//
// Description: Just fuck with the current pUserData
//
// Parameters:  None.
//
// Return:      None.
//
// Constraints: None.
//
//========================================================================
void StaticEntityLoader::ModRegdListener
( 
   ChunkListenerCallback* pListenerCB,
   int iUserData 
)
{
#if 0
   char DebugBuf[255];
   sprintf( DebugBuf, "GeometryWrappedLoader::ModRegdListener: pListenerCB %X vs mpListenerCB %X\n", pListenerCB, mpListenerCB );
   rDebugString( DebugBuf );
#endif
   rAssert( pListenerCB == mpListenerCB );

   mUserData = iUserData;
}
//************************************************************************
//
// Protected Member Functions : StaticEntityLoader 
//
//************************************************************************

//************************************************************************
//
// Private Member Functions : StaticEntityLoader 
//
//************************************************************************
