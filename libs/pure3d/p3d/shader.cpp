//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================


#include "p3d/shader.hpp"
#include "constants/chunks.h"
#include "constants/chunkids.hpp"
#include "p3d/utility.hpp"
#include "p3d/chunkfile.hpp"
#include "p3d/texture.hpp"
#include <string.h>

tShader::tShader()
    : mTranslucent(false),
      mEnvironmentCelEligible(true),
      mEnvironmentCelAlphaTestEligible(false),
      mEnvironmentCelFoliageCutoutEligible(false),
      mEnvironmentCelHasAlphaTest(false),
      mEnvironmentCelHasAlphaBlend(false),
      mEnvironmentCelBlendMode(PDDI_BLEND_NONE),
      mEnvironmentCelHasTranslucency(false),
      mEnvironmentCelAlphaRef(0.5f),
      mEnvironmentCelAlphaCompare(PDDI_COMPARE_GREATEREQUAL),
      mEnvironmentCelTwoSided(false),
      mEnvironmentCelBaseTextureUID(0)
{
    shader = p3d::device->NewShader("simple");
    shader->AddRef();
}

tShader::tShader(const char* shaderName, char* definition)
    : mTranslucent(false),
      mEnvironmentCelEligible(true),
      mEnvironmentCelAlphaTestEligible(false),
      mEnvironmentCelFoliageCutoutEligible(false),
      mEnvironmentCelHasAlphaTest(false),
      mEnvironmentCelHasAlphaBlend(false),
      mEnvironmentCelBlendMode(PDDI_BLEND_NONE),
      mEnvironmentCelHasTranslucency(false),
      mEnvironmentCelAlphaRef(0.5f),
      mEnvironmentCelAlphaCompare(PDDI_COMPARE_GREATEREQUAL),
      mEnvironmentCelTwoSided(false),
      mEnvironmentCelBaseTextureUID(0)
{
    shader = p3d::device->NewShader(shaderName, definition);
    shader->AddRef();
}

tShader::~tShader()
{
    if(shader)
        shader->Release();

    shader = NULL;
}

tShaderLoader::tShaderLoader() : tSimpleChunkHandler(Pure3D::Shader::SHADER)
{
}

static void DrawInventory(int index = -1)
{
    tInventory::Iterator<tEntity> it;
    tEntity* obj = it.First();
    p3d::print("inventory dump:\n");
    while(obj)
    {
        char cls[128];
        p3d::ClassName(obj, cls, 128);
#ifdef P3D_ALLOW_ENTITY_GETNAME
        p3d::printf("\t'%s' (%s)\n", obj->GetName(), cls);
#endif
        obj = it.Next();            
    }
}

tEntity* tShaderLoader::LoadObject(tChunkFile* f, tEntityStore* store)
{
    char name[128];
    char shaderName[128];
    
    
    char pure [ ]= "pure3";
    char tempname[8];
    char lambert[ ] ="lambe";
    char buffer [255];

    
    f->GetString(name);

    strncpy(tempname,name,5);    
    tempname[5]='\0';

#ifndef RAD_RELEASE 
     //Chuck: adding this to SO no shaders called pure3dsimpleshader and lambert ever get loaded into our game
    if (strcmp(tempname,pure)==0)
    {
        sprintf(buffer,"Error: A Shader with %s in the name found in File %s \n",name,f->GetFilename()); 
        rReleasePrintf(buffer);

        //rTuneAssertMsg(0,buffer);
        rAssertMsg(0,buffer);
        return NULL;
    }

    if (strcmp(tempname,lambert) ==0)
    {
        sprintf(buffer,"Error: A Shader with %s in the name found in File %s \n",name,f->GetFilename());
        rReleasePrintf(buffer);

        //rTuneAssertMsg(0,buffer);
        rAssertMsg(0,buffer);
        return NULL;
    }
    
#endif

    int version = f->GetInt();
    P3DASSERT(version == 0);
    f->GetString(shaderName);

    // Gamecube soln to having chainlink and gate_chlink fence shaders that 
    // become solid at certain mipmap levels
#ifdef RAD_GAMECUBE
    const char* s_HighThresholdNames[] = 
    {
        "gate_chlink_m",
        "chainlink_m"
    };
    const int NUM_HIGH_THRESHOLD_NAMES = sizeof( s_HighThresholdNames ) / sizeof( s_HighThresholdNames[0] );
    const float HIGH_ALPHA_THRESHOLD = 0.8f;
    bool useHighAlphaThreshold = false;
    for ( int i = 0 ; i < NUM_HIGH_THRESHOLD_NAMES ; i++ )
    {
        if ( strcmp( name, s_HighThresholdNames[i] ) == 0 )
        {
            useHighAlphaThreshold = true;          
            break;
        }             
    }
#endif


    bool     HasTranslucency = (f->GetLong() != 0);
    unsigned VertexNeeds = f->GetLong();
    unsigned VertexMask = f->GetLong();

    // nv: hack to fix badly mipmapped chainlink fence
    bool foundEvilChainlink = false;

    int count = f->GetInt();


    tShader* shader = NULL;
    bool hasAlphaTest = false;
    bool hasAlphaBlend = false;
    int blendMode = PDDI_BLEND_NONE;
    float alphaRef = 0.5f;
    int alphaCompare = PDDI_COMPARE_GREATEREQUAL;
    bool twoSided = false;
    unsigned long long baseTextureUID = 0;
#ifdef RAD_ANDROID
    // The main Zombie Burger sign in l7r2 uses the same krust_sign_m
    // material as the working l7r7 sign, but its authored ATST flag is off.
    // Match only this file/material pair; do not change the smaller
    // L7krust_sign_m secondary component or any other Level 7 asset.
    const char* sourceFilename = f->GetFilename();
    const bool isL7r2ZombieBurgerSignMaterial =
        sourceFilename != NULL &&
        strcmp(sourceFilename, "l7r2.p3d") == 0 &&
        strcmp(name, "krust_sign_m") == 0;

    const bool isTomaccoPlantMaterial =
        sourceFilename != NULL &&
        (strcmp(sourceFilename, "l1r6.p3d") == 0 ||
         strcmp(sourceFilename, "l4r6.p3d") == 0) &&
        strcmp(name, "tomacco_plant_m") == 0;
#endif

    while(f->ChunksRemaining())
    {
        f->BeginChunk();

        switch(f->GetCurrentID())
        {
            case Pure3D::Shader::SHADER_DEFINITION:
                {
                    P3DASSERT(!shader);
                    char defName[256];
                    f->GetString(defName); // not used when embeded
                    int size = f->GetInt();
                    char* buffer = (char*)p3d::MallocTemp(size+1);
                    f->GetData(buffer, size, tFile::BYTE);
                    buffer[size] = 0;
                    shader = new tShader(shaderName, buffer);
                    p3d::FreeTemp(buffer);
                }
                break;

            case Pure3D::Shader::TEXTURE_PARAM:
                {
                    char texName[256];

                    if(!shader)
                        shader = new tShader(shaderName);

                    unsigned param = f->GetInt();
                    f->GetPString(texName);

                    // oct15/2002 amb - todo fix this
                    tTexture* tex = p3d::find<tTexture>(store, texName);
                    if(!tex)
                    {                        // try a global find.
                        tex = p3d::find<tTexture>(texName);
                    }
                    
                    // nv:  If this shader uses the chainlink texture, I'm going to turn off mipmapping
                    if( strstr(texName, "chainlink") )
                    {
                        foundEvilChainlink = true;
                    }

                    shader->SetTexture(param, tex);
                    if (param == PDDI_SP_BASETEX && tex != NULL && baseTextureUID == 0)
                    {
                        baseTextureUID = static_cast<unsigned long long>(tex->GetUID());
                    }
                }
                break;

            case Pure3D::Shader::INT_PARAM:
                {
                    if(!shader)
                        shader = new tShader(shaderName);

                    unsigned param = f->GetInt();
                    unsigned val = f->GetInt();
#ifdef RAD_ANDROID
                    // l7r7.p3d's matching krust_sign_m has alpha testing
                    // enabled (ATST=1). l7r2's main Zombie Burger sign is
                    // identical for ACMP/ACTH/BLMD; copy only ATST.
                    if (isL7r2ZombieBurgerSignMaterial &&
                        param == PDDI_SP_ALPHATEST && val == 0)
                    {
                        val = 1;
                    }

                    if (isTomaccoPlantMaterial &&
                        param == PDDI_SP_UVMODE && val == PDDI_UV_TILE)
                    {
                        val = PDDI_UV_CLAMP;
                    }

                    if (isTomaccoPlantMaterial &&
                        param == PDDI_SP_BLENDMODE && val != PDDI_BLEND_NONE)
                    {
                        val = PDDI_BLEND_NONE;
                    }
#endif
                    shader->SetInt(param, val);
                    if (param == PDDI_SP_BLENDMODE)
                    {
                        blendMode = static_cast<int>(val);
                        hasAlphaBlend = (val != PDDI_BLEND_NONE);
                    }
                    if (param == PDDI_SP_ALPHATEST)
                        hasAlphaTest = (val != 0);
                    if (param == PDDI_SP_ALPHACOMPARE)
                        alphaCompare = static_cast<int>(val);
                    if (param == PDDI_SP_TWOSIDED)
                        twoSided = (val != 0);
                    if ((param == PDDI_SP_BLENDMODE && val != PDDI_BLEND_NONE) ||
                        (param == PDDI_SP_ALPHATEST && val != 0))
                    {
                        shader->mEnvironmentCelEligible = false;
                    }
                }
                break;

            case Pure3D::Shader::FLOAT_PARAM:
                {
                    if(!shader)
                        shader = new tShader(shaderName);

                    unsigned param = f->GetInt();
                    float val = f->GetFloat();

#ifdef RAD_GAMECUBE
                    // MKR - we are forcing the alpha compare threshold to a higher level
                    // to fix the problem with chainlink fences
                    if ( useHighAlphaThreshold && param == PDDI_SP_ALPHACOMPARE_THRESHOLD )
                    {
                        val = HIGH_ALPHA_THRESHOLD;
                    }
#endif

                    shader->SetFloat(param, val);
                    if (param == PDDI_SP_ALPHACOMPARE_THRESHOLD)
                        alphaRef = val;
                }
                break;

            case Pure3D::Shader::COLOUR_PARAM:
                {
                    if(!shader)
                        shader = new tShader(shaderName);

                    unsigned param = f->GetInt();
                    unsigned val = f->GetInt();

                    if(param != PDDI_SP_AMBIENT)
                    {
                        shader->SetColour(param, pddiColour(val));
                    }

                    if(param == PDDI_SP_DIFFUSE)
                    {
                        shader->SetColour(PDDI_SP_AMBIENT, pddiColour(val));
                    }

                }
                break;

            case Pure3D::Shader::VECTOR_PARAM:
                {
                    if(!shader)
                        shader = new tShader(shaderName);

                    unsigned param = f->GetInt();

                    pddiVector v;
                    v.x = f->GetFloat();
                    v.y = f->GetFloat();
                    v.z = f->GetFloat();

                    shader->SetVector(param, v);
                }
                break;

            case Pure3D::Shader::MATRIX_PARAM:
                {
                    if(!shader)
                        shader = new tShader(shaderName);

                    unsigned param = f->GetInt();

                    pddiMatrix m;
                    f->GetData(&m, 16, tFile::DWORD);
                    shader->SetMatrix(param, m);
                }
                break;

        }

        f->EndChunk();
    }
    if(!shader)
        shader = new tShader(shaderName);

    // nv:  fix the solid chainlink fences
    if( foundEvilChainlink )
    {
        shader->SetFloat( PDDI_SP_MIPMAP_SHARPNESS, 0.0f );
    }
    shader->mTranslucent = HasTranslucency;
    shader->mEnvironmentCelAlphaTestEligible =
        hasAlphaTest && !hasAlphaBlend && !HasTranslucency;
    shader->mEnvironmentCelFoliageCutoutEligible =
        hasAlphaTest && !hasAlphaBlend;
    shader->mEnvironmentCelHasAlphaTest = hasAlphaTest;
    shader->mEnvironmentCelHasAlphaBlend = hasAlphaBlend;
    shader->mEnvironmentCelBlendMode = blendMode;
    shader->mEnvironmentCelHasTranslucency = HasTranslucency;
    shader->mEnvironmentCelAlphaRef = alphaRef;
    shader->mEnvironmentCelAlphaCompare = alphaCompare;
    shader->mEnvironmentCelTwoSided = twoSided;
    shader->mEnvironmentCelBaseTextureUID = baseTextureUID;
    if (HasTranslucency)
        shader->mEnvironmentCelEligible = false;
#ifdef RAD_ANDROID
    // int_kwik_atm_m is tagged alpha-blended in art data despite using a
    // verified fully-opaque 128x64 source texture and fully-opaque material
    // colours. glmat.cpp already bypasses GL blending only when that complete
    // runtime opacity check passes. Keep that exception aligned with the
    // load-time environment-outline eligibility gate, without admitting other
    // blended or alpha-tested materials.
    if (strcmp(name, "int_kwik_atm_m") == 0 &&
        blendMode == PDDI_BLEND_ALPHA &&
        hasAlphaBlend && !hasAlphaTest && !HasTranslucency)
    {
        shader->mEnvironmentCelEligible = true;
    }
#endif
    shader->SetName(name);
    return shader;
}


bool tShaderDefinitionLoader::CheckChunkID(unsigned i)
{
    return i == Pure3D::Shader::SHADER_DEFINITION;
}

tLoadStatus tShaderDefinitionLoader::Load(tChunkFile* f, tEntityStore* store)
{
    P3DASSERT(f->GetCurrentID() == Pure3D::Shader::SHADER_DEFINITION);

    char defName[256];
    f->GetString(defName);

    int size = f->GetInt();
    char* buffer = (char*)p3d::MallocTemp(size+1);
    f->GetData(buffer, size, tFile::BYTE);
    buffer[size] = 0;

    p3d::device->AddCustomShader(defName, buffer);

    p3d::FreeTemp(buffer);

    return LOAD_OK;
}

