//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================

#include <pddi/gles/gl.hpp>
#include <pddi/gles/glprog.hpp>
#include <pddi/gles/glmat.hpp>

#include <string>
#include <vector>
#include <SDL.h>

#ifdef RAD_CG
#define CGGL_NO_OPENGL
#include <Cg/cg.h>    /* Can't include this?  Is Cg Toolkit installed! */
#include <Cg/cgGL.h>

void pglProgram::checkForCgError()
{
    CGerror error;
    const char* string = cgGetLastErrorString(&error);

    if (error != CG_NO_ERROR) {
        SDL_Log("%s\n", string);
        if (error == CG_COMPILER_ERROR) {
            printf("%s\n", cgGetLastListing(pglProgram::context));
        }
        __debugbreak();
    }
}

CGcontext pglProgram::context = nullptr;
CGprofile pglProgram::vertexProfile, pglProgram::fragmentProfile;
#endif


#if !defined(RAD_CG)
#ifdef RAD_ANDROID
#include <unordered_map>
#include <cstring>
#include <cstdint>
#include <functional>
namespace
{
struct SHARUniformCacheKey
{
    const pglProgram* owner; GLint location;
    bool operator==(const SHARUniformCacheKey& other) const { return owner == other.owner && location == other.location; }
};
struct SHARUniformCacheKeyHash
{
    size_t operator()(const SHARUniformCacheKey& key) const
    {
        const size_t a=std::hash<const void*>()(key.owner), b=std::hash<int>()((int)key.location);
        return a ^ (b+(a<<6)+(a>>2));
    }
};
struct SHARUniformCacheValue { GLenum type; size_t size; unsigned char bytes[64]; bool valid; };
static std::unordered_map<SHARUniformCacheKey,SHARUniformCacheValue,SHARUniformCacheKeyHash> gSHARUniformCache;
static bool SHARShouldUploadUniform(const pglProgram* owner,GLint location,GLenum type,const void* data,size_t size)
{
    if(location<0) return false;
    if(size>64) return true;
    SHARUniformCacheKey key={owner,location};
    SHARUniformCacheValue& cached=gSHARUniformCache[key];
    if(cached.valid&&cached.type==type&&cached.size==size&&memcmp(cached.bytes,data,size)==0) return false;
    cached.type=type; cached.size=size; memcpy(cached.bytes,data,size); cached.valid=true;
    return true;
}
static void SHARClearUniformCacheForProgram(const pglProgram* owner)
{
    for(auto it=gSHARUniformCache.begin();it!=gSHARUniformCache.end();)
        if(it->first.owner==owner) it=gSHARUniformCache.erase(it); else ++it;
}
}
void InvalidateSHARUniformCaches() { gSHARUniformCache.clear(); }
static inline void SHARUniform1i(const pglProgram* o,GLint l,GLint v)
{ if(!SHARShouldUploadUniform(o,l,GL_INT,&v,sizeof(v)))return; glUniform1i(l,v); }
static inline void SHARUniform1f(const pglProgram* o,GLint l,GLfloat v)
{ if(!SHARShouldUploadUniform(o,l,GL_FLOAT,&v,sizeof(v)))return; glUniform1f(l,v); }
static inline void SHARUniform3f(const pglProgram* o,GLint l,GLfloat x,GLfloat y,GLfloat z)
{ const GLfloat v[3]={x,y,z}; if(!SHARShouldUploadUniform(o,l,GL_FLOAT_VEC3,v,sizeof(v)))return; glUniform3f(l,x,y,z); }
static inline void SHARUniform4f(const pglProgram* o,GLint l,GLfloat x,GLfloat y,GLfloat z,GLfloat w)
{ const GLfloat v[4]={x,y,z,w}; if(!SHARShouldUploadUniform(o,l,GL_FLOAT_VEC4,v,sizeof(v)))return; glUniform4f(l,x,y,z,w); }
static inline void SHARUniform4fv(const pglProgram* o,GLint l,GLsizei n,const GLfloat* v)
{ const size_t sz=sizeof(GLfloat)*4*(size_t)n; if(!SHARShouldUploadUniform(o,l,GL_FLOAT_VEC4,v,sz))return; glUniform4fv(l,n,v); }
static inline void SHARUniformMatrix4fv(const pglProgram* o,GLint l,GLsizei n,GLboolean t,const GLfloat* v)
{ const size_t sz=sizeof(GLfloat)*16*(size_t)n; if(!SHARShouldUploadUniform(o,l,GL_FLOAT_MAT4,v,sz))return; glUniformMatrix4fv(l,n,t,v); }
#else
static inline void SHARUniform1i(const pglProgram*,GLint l,GLint v){glUniform1i(l,v);}
static inline void SHARUniform1f(const pglProgram*,GLint l,GLfloat v){glUniform1f(l,v);}
static inline void SHARUniform3f(const pglProgram*,GLint l,GLfloat x,GLfloat y,GLfloat z){glUniform3f(l,x,y,z);}
static inline void SHARUniform4f(const pglProgram*,GLint l,GLfloat x,GLfloat y,GLfloat z,GLfloat w){glUniform4f(l,x,y,z,w);}
static inline void SHARUniform4fv(const pglProgram*,GLint l,GLsizei n,const GLfloat* v){glUniform4fv(l,n,v);}
static inline void SHARUniformMatrix4fv(const pglProgram*,GLint l,GLsizei n,GLboolean t,const GLfloat* v){glUniformMatrix4fv(l,n,t,v);}
#endif
#endif

#ifdef RAD_CG
static inline void UniformColour(const pglProgram*, CGparameter param, pddiColour c)
{
    cgSetParameter4f(param, float(c.Red()) / 255, float(c.Green()) / 255, float(c.Blue()) / 255, float(c.Alpha()) / 255);
}
#else
static inline void UniformColour(const pglProgram* owner, GLint loc, pddiColour c)
{
    SHARUniform4f(owner, loc, float(c.Red()) / 255, float(c.Green()) / 255,
                  float(c.Blue()) / 255, float(c.Alpha()) / 255);
}
#endif

pglProgram::pglProgram()
{
#ifdef RAD_ANDROID
    SHARClearUniformCacheForProgram(this);
#endif
#ifdef RAD_CG
    program = nullptr;
    projection = modelview = normalmatrix = alpharef = sampler = acs = nullptr;
#else
    program = 0;
    projection = modelview = normalmatrix = alpharef = sampler = acs = -1;

    #ifdef RAD_ANDROID
    lit = -1;
    toonEnabled = -1;
    toonObjectEnabled = -1;
    toonRimEnabled = -1;
    toonOutlinePass = -1;
    toonEnvironmentPass = -1;
    toonOutlineWidth = -1;
    nightObjectBrightness = -1;
    nightObjectSaturation = -1;
    #endif

#endif
}

pglProgram::~pglProgram()
{
#ifdef RAD_ANDROID
    SHARClearUniformCacheForProgram(this);
#endif
#ifdef RAD_CG
    if (program)
        cgDestroyProgram(program);
#else
    if (program)
        glDeleteProgram(program);
#endif
}

void pglProgram::SetProjectionMatrix(const pddiMatrix* matrix)
{
#ifdef RAD_CG
    if (projection)
        cgSetMatrixParameterfc(projection, matrix->m[0]);
    checkForCgError();
#else
    if (projection >= 0)
        SHARUniformMatrix4fv(this, projection, 1, GL_FALSE, matrix->m[0]);
#endif
}

void pglProgram::SetModelViewMatrix(const pddiMatrix* matrix)
{
#ifdef RAD_CG
    if (modelview)
        cgSetMatrixParameterfc(modelview, matrix->m[0]);
    checkForCgError();

    if (normalmatrix)
    {
        pddiMatrix inverse;
        inverse.Invert(*matrix);
        inverse.Transpose();
        cgSetMatrixParameterfc(normalmatrix, inverse.m[0]);
    }
    checkForCgError();
#else
    if (modelview >= 0)
        SHARUniformMatrix4fv(this, modelview, 1, GL_FALSE, matrix->m[0]);
    if (normalmatrix >= 0)
    {
        pddiMatrix inverse;
        inverse.Invert(*matrix);
        inverse.Transpose();
        SHARUniformMatrix4fv(this, normalmatrix, 1, GL_FALSE, inverse.m[0]);
    }
#endif
}

void pglProgram::SetTextureEnvironment(const pglTextureEnv* texEnv)
{
#ifdef RAD_CG
    if (texEnv->lit)
    {
        UniformColour(this, acm, texEnv->ambient);
        UniformColour(this, ecm, texEnv->emissive);
        UniformColour(this, dcm, texEnv->diffuse);
        UniformColour(this, scm, texEnv->specular);
        cgSetParameter1f(srm, texEnv->shininess);
    }
    else
    {
        UniformColour(this, acm, pddiColour(-1));
        UniformColour(this, ecm, pddiColour(-1));
        UniformColour(this, dcm, pddiColour(-1));
        UniformColour(this, scm, pddiColour(-1));
        cgSetParameter1f(srm, 0.0f);
    }
    checkForCgError();

    if (texEnv->alphaTest && alpharef >= 0)
    {
        PDDIASSERT(texEnv->alphaCompareMode == PDDI_COMPARE_GREATER ||
            texEnv->alphaCompareMode == PDDI_COMPARE_GREATEREQUAL);
        cgSetParameter1f(alpharef, texEnv->alphaTest ? texEnv->alphaRef : 0.0f);
    }
    checkForCgError();
#else
    if (sampler >= 0)
        SHARUniform1i(this, sampler, 0);


    #ifdef RAD_ANDROID
    if (lit >= 0)
        SHARUniform1i(this, lit, texEnv->lit ? 1 : 0);
    #endif

    if (texEnv->lit)
    {
        UniformColour(this, acm, texEnv->ambient);
        UniformColour(this, ecm, texEnv->emissive);
        UniformColour(this, dcm, texEnv->diffuse);
        UniformColour(this, scm, texEnv->specular);
        SHARUniform1f(this, srm, texEnv->shininess);
    }
    else
    {
        UniformColour(this, acm, pddiColour(-1));
        UniformColour(this, ecm, pddiColour(-1));
        UniformColour(this, dcm, pddiColour(-1));
        UniformColour(this, scm, pddiColour(-1));
        SHARUniform1f(this, srm, 0.0f);
    }

    if (texEnv->alphaTest && alpharef >= 0)
    {
        PDDIASSERT(texEnv->alphaCompareMode == PDDI_COMPARE_GREATER ||
            texEnv->alphaCompareMode == PDDI_COMPARE_GREATEREQUAL);
        SHARUniform1f(this, alpharef, texEnv->alphaTest ? texEnv->alphaRef : 0.0f);
    }
#endif
}

void pglProgram::SetLightState(int handle, const pddiLight* lightState)
{
    if( handle >= PDDI_MAX_LIGHTS )
        return;

    float dir[4];
    switch(lightState->type)
    {
        case PDDI_LIGHT_DIRECTIONAL :
            dir[0] = -lightState->worldDirection.x;
            dir[1] = -lightState->worldDirection.y;
            dir[2] = -lightState->worldDirection.z;
            dir[3] = 0.0f;
            break;

        case PDDI_LIGHT_POINT :
            dir[0] = lightState->worldPosition.x;
            dir[1] = lightState->worldPosition.y;
            dir[2] = lightState->worldPosition.z;
            dir[3] = 1.0f;
            break;

        case PDDI_LIGHT_SPOT :
            PDDIASSERT(0);
            break;
    }

#ifdef RAD_CG
    if (lights[handle].enabled)
    {
        cgSetParameter1i(lights[handle].enabled, lightState->enabled ? 1 : 0);
        cgSetParameter4fv(lights[handle].position, dir);
        UniformColour(this, lights[handle].colour, lightState->colour);
        cgSetParameter3f(lights[handle].attenuation, lightState->attenA, lightState->attenB, lightState->attenC);
    }
    checkForCgError();
#else
    SHARUniform1i(this, lights[handle].enabled, lightState->enabled ? 1 : 0);
    SHARUniform4fv(this, lights[handle].position, 1, dir);
    UniformColour(this, lights[handle].colour, lightState->colour);
    SHARUniform3f(this, lights[handle].attenuation, lightState->attenA, lightState->attenB, lightState->attenC);
#endif
}

void pglProgram::SetCelShadingEnabled(bool enabled)
{
#ifdef RAD_ANDROID
    if (toonEnabled >= 0)
        SHARUniform1i(this, toonEnabled, enabled ? 1 : 0);
#else
    (void)enabled;
#endif
}

#ifdef RAD_ANDROID
void pglProgram::SetCelShadingState(bool enabled, bool objectEnabled, bool outlinePass, float objectBrightness, float objectSaturation, bool rimLightingEnabled, bool environmentPass)
{
    if (toonEnabled >= 0)
        SHARUniform1i(this, toonEnabled, enabled ? 1 : 0);
    if (toonObjectEnabled >= 0)
        SHARUniform1i(this, toonObjectEnabled, objectEnabled ? 1 : 0);
    if (toonRimEnabled >= 0)
        SHARUniform1i(this, toonRimEnabled, rimLightingEnabled ? 1 : 0);
    if (toonOutlinePass >= 0)
        SHARUniform1i(this, toonOutlinePass, outlinePass ? 1 : 0);
    if (toonEnvironmentPass >= 0)
        SHARUniform1i(this, toonEnvironmentPass, environmentPass ? 1 : 0);
    if (toonOutlineWidth >= 0)
        SHARUniform1f(this, toonOutlineWidth, 0.009f);
    if (nightObjectBrightness >= 0)
        SHARUniform1f(this, nightObjectBrightness, objectBrightness);
    if (nightObjectSaturation >= 0)
        SHARUniform1f(this, nightObjectSaturation, objectSaturation);
}
#endif

void pglProgram::SetAmbientLight(pddiColour ambient)
{
    if (acs)
        UniformColour(this, acs, ambient);
}

#ifdef RAD_CG
bool pglProgram::LinkProgram(CGprogram vertexShader, CGprogram fragmentShader)
{
    program = cgCombinePrograms2(vertexShader, fragmentShader);
    checkForCgError();
    if (!program)
        return false;

    cgGLLoadProgram(program);
    checkForCgError();

    projection = cgGetNamedParameter(program, "projection");
    modelview = cgGetNamedParameter(program, "modelview");
    normalmatrix = cgGetNamedParameter(program, "normalmatrix");
    alpharef = cgGetNamedParameter(program, "alpharef");
    sampler = cgGetNamedParameter(program, "tex");
    checkForCgError();

    for (int i = 0; i < PDDI_MAX_LIGHTS; i++)
    {
        std::string prefix = std::string("lights[") + char('0' + i) + "].";
        lights[i].enabled = cgGetNamedParameter(program, (prefix + "enabled").c_str());
        lights[i].position = cgGetNamedParameter(program, (prefix + "position").c_str());
        lights[i].colour = cgGetNamedParameter(program, (prefix + "colour").c_str());
        lights[i].attenuation = cgGetNamedParameter(program, (prefix + "attenuation").c_str());
        checkForCgError();
    }

    acs = cgGetNamedParameter(program, "acs");
    acm = cgGetNamedParameter(program, "acm");
    dcm = cgGetNamedParameter(program, "dcm");
    scm = cgGetNamedParameter(program, "scm");
    ecm = cgGetNamedParameter(program, "ecm");
    srm = cgGetNamedParameter(program, "srm");
    checkForCgError();
    return true;
}
#else
bool pglProgram::LinkProgram(GLuint vertexShader, GLuint fragmentShader)
{
    program = glCreateProgram();

    if(vertexShader)
        glAttachShader(program, vertexShader);
    if(fragmentShader)
        glAttachShader(program, fragmentShader);

    glBindAttribLocation(program, 0, "position");
    glBindAttribLocation(program, 1, "normal");
    glBindAttribLocation(program, 2, "texcoord");
    glBindAttribLocation(program, 3, "color");

    glLinkProgram(program);

    GLint isLinked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, (int *)&isLinked);
    if (isLinked == GL_FALSE)
    {
        GLint maxLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

        // The maxLength includes the NULL character
        std::vector<GLchar> infoLog(maxLength);
        glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);

        SDL_Log("Program linking error: %s", infoLog.data());
        return false;
    }
    
    projection = glGetUniformLocation(program, "projection");
    modelview = glGetUniformLocation(program, "modelview");
    normalmatrix = glGetUniformLocation(program, "normalmatrix");
    alpharef = glGetUniformLocation(program, "alpharef");
    sampler = glGetUniformLocation(program, "tex");

    for (int i = 0; i < PDDI_MAX_LIGHTS; i++)
    {
        std::string prefix = std::string("lights[") + char('0' + i) + "].";
        lights[i].enabled = glGetUniformLocation(program, (prefix + "enabled").c_str());
        lights[i].position = glGetUniformLocation(program, (prefix + "position").c_str());
        lights[i].colour = glGetUniformLocation(program, (prefix + "colour").c_str());
        lights[i].attenuation = glGetUniformLocation( program, (prefix + "attenuation").c_str() );
    }

    acs = glGetUniformLocation(program, "acs");
    acm = glGetUniformLocation(program, "acm");
    dcm = glGetUniformLocation(program, "dcm");
    scm = glGetUniformLocation(program, "scm");
    ecm = glGetUniformLocation(program, "ecm");
    srm = glGetUniformLocation(program, "srm");

    #ifdef RAD_ANDROID
    lit = glGetUniformLocation(program, "lit");
    toonEnabled = glGetUniformLocation(program, "toonEnabled");
    toonObjectEnabled = glGetUniformLocation(program, "toonObjectEnabled");
    toonRimEnabled = glGetUniformLocation(program, "toonRimEnabled");
    toonOutlinePass = glGetUniformLocation(program, "toonOutlinePass");
    toonEnvironmentPass = glGetUniformLocation(program, "toonEnvironmentPass");
    toonOutlineWidth = glGetUniformLocation(program, "toonOutlineWidth");
    nightObjectBrightness = glGetUniformLocation(program, "nightObjectBrightness");
    nightObjectSaturation = glGetUniformLocation(program, "nightObjectSaturation");
    #endif

#ifndef RAD_VITAGL
    // Always detach shaders after a successful link
    if(vertexShader)
        glDetachShader(program, vertexShader);
    if(fragmentShader)
        glDetachShader(program, fragmentShader);
#endif
    return true;
}
#endif

#ifdef RAD_CG
CGprogram pglProgram::CompileShader(GLenum type, const char* source)
{
    if(!context)
    {
        context = cgCreateContext();
        cgGLSetDebugMode(CG_FALSE);
        cgSetParameterSettingMode(context, CG_IMMEDIATE_PARAMETER_SETTING);
        vertexProfile = cgGetProfile("glslv");
        assert(cgGLIsProfileSupported(vertexProfile));
        cgGLSetOptimalOptions(vertexProfile);
        fragmentProfile = cgGetProfile("glslf");
        assert(cgGLIsProfileSupported(fragmentProfile));
        cgGLSetOptimalOptions(fragmentProfile);
    }
    checkForCgError();

    const char* args[] = { "version=330", NULL };
    CGprofile profile = type == GL_VERTEX_SHADER ?
        vertexProfile : fragmentProfile;
    cgGLSetOptimalOptions(profile);
    CGprogram program = cgCreateProgram(
        context,
        CG_SOURCE,
        source,
        profile,
        "main",
        args);
    checkForCgError();
    return program;
}
#else
GLuint pglProgram::CompileShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, 0);
    glCompileShader(shader);

    GLint isCompiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
    if(isCompiled == GL_FALSE)
    {
        GLint maxLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

        // The maxLength includes the NULL character
        std::vector<GLchar> infoLog(maxLength);
        glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);

        // We don't need the shader anymore.
        glDeleteShader(shader);

        SDL_Log("Shader compilation error: %s", infoLog.data());
        return 0;
    }
    return shader;
}
#endif

#ifdef RAD_CG
void pglProgram::UseProgram()
{
    cgUpdateProgramParameters(program);
    cgGLBindProgram(program);
    checkForCgError();
}
#endif

#ifdef RAD_CG
pglProgram* pglProgram::CreateProgram(CGprogram vertexShader, CGprogram fragmentShader)
#else
pglProgram* pglProgram::CreateProgram(GLuint vertexShader, GLuint fragmentShader)
#endif
{
    pglProgram* program = new pglProgram();
    program->AddRef();
    if(!program->LinkProgram(vertexShader, fragmentShader))
    {
        program->Release();
        return nullptr;
    }
    return program;
}
