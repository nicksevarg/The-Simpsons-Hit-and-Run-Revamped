//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================


#ifndef _SHADER_HPP_
#define _SHADER_HPP_

#include <p3d/entity.hpp>
#include <p3d/loadmanager.hpp>
#include <pddi/pddi.hpp>
#include <p3d/texture.hpp>

class pddiShader;

class tShader : public tEntity
{
public:
    tShader();
    tShader(const char* shaderName, char* definition = NULL);

    pddiShader* GetShader(void) const { return shader; }

    const char* GetType(void) const {return shader->GetType();} 

    void  SetTexture(unsigned paramName, tTexture* v) {shader->SetTexture(paramName, v ? v->GetTexture() : NULL);}
    void  SetInt(unsigned paramName, int v) {shader->SetInt(paramName, v);}
    void  SetFloat(unsigned paramName, float v) {shader->SetFloat(paramName, v);}
    void  SetColour(unsigned paramName, pddiColour v) {shader->SetColour(paramName, v);}
    void  SetVector(unsigned paramName, const pddiVector& v) {shader->SetVector(paramName, v);}
    void  SetMatrix(unsigned paramName, const pddiMatrix& v) {shader->SetMatrix(paramName, v);}

    bool mTranslucent;
    bool mEnvironmentCelEligible;
    // Legacy alpha-test exception; keep its original translucency requirement.
    bool mEnvironmentCelAlphaTestEligible;
    // Foliage-only exception: alpha test enabled and no alpha blending.
    // This deliberately ignores the coarse HasTranslucency header flag.
    bool mEnvironmentCelFoliageCutoutEligible;

    // Loaded shader-definition values used by Mode 10 foliage diagnostics.
    bool mEnvironmentCelHasAlphaTest;
    bool mEnvironmentCelHasAlphaBlend;
    // Preserve the source blend mode for temporary outline-only overrides.
    int mEnvironmentCelBlendMode;
    bool mEnvironmentCelHasTranslucency;
    float mEnvironmentCelAlphaRef;
    int mEnvironmentCelAlphaCompare;
    bool mEnvironmentCelTwoSided;
    unsigned long long mEnvironmentCelBaseTextureUID;
protected:
    ~tShader();
    pddiShader* shader;

};

class tShaderLoader : public tSimpleChunkHandler
{
public:
    tShaderLoader();
    tEntity* LoadObject(tChunkFile* file, tEntityStore* store);

protected:
    ~tShaderLoader() {}

};

class tShaderDefinitionLoader : public tChunkHandler
{
public:
    bool CheckChunkID(unsigned);
    tLoadStatus Load(tChunkFile* file, tEntityStore* store);

protected:
    ~tShaderDefinitionLoader() {}

};

#endif

