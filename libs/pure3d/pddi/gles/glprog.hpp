//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================


#ifndef _GLPROG_HPP_
#define _GLPROG_HPP_

#include <pddi/pddi.hpp>
#include <pddi/base/basecontext.hpp>
#include <pddi/gles/gl.hpp>
struct pglTextureEnv;

#ifdef RAD_CG
#include <Cg/cg.h>    /* Can't include this?  Is Cg Toolkit installed! */
#endif

class pglProgram : public pddiObject
{
public:
    pglProgram();
    ~pglProgram();

#ifdef RAD_CG
    void UseProgram();
    static CGprogram CompileShader(GLenum type, const char* source);
    static pglProgram* CreateProgram(CGprogram vertexShader, CGprogram fragmentShader);
#else
    void UseProgram() { glUseProgram(program); }
    static GLuint CompileShader(GLenum type, const char* source);
    static pglProgram* CreateProgram(GLuint vertexShader, GLuint fragmentShader);
#endif

    void SetProjectionMatrix(const pddiMatrix* matrix);
    void SetModelViewMatrix(const pddiMatrix* matrix);
    void SetTextureEnvironment(const pglTextureEnv* texEnv);
    void SetLightState(int handle, const pddiLight* lightState);
    void SetAmbientLight(pddiColour ambient);
    void SetCelShadingEnabled(bool enabled);
#ifdef RAD_ANDROID
    struct SHARCelShadingStateCache
    {
        bool valid;
        unsigned int generation;
        bool enabled;
        bool objectEnabled;
        bool outlinePass;
        bool rimLightingEnabled;
        bool environmentPass;
        bool frontendMenuLighting;
        float objectBrightness;
        float objectSaturation;
        float viewportWidth;
        float viewportHeight;
        float moodTintRed;
        float moodTintGreen;
        float moodTintBlue;
    } celShadingStateCache;

    void SetCelShadingState(bool enabled, bool objectEnabled, bool outlinePass, float objectBrightness, float objectSaturation, bool rimLightingEnabled, bool environmentPass, float viewportWidth, float viewportHeight, bool frontendMenuLighting, float moodTintRed, float moodTintGreen, float moodTintBlue);
    void SetOpaqueOutlineTexture(bool opaque);
#endif

    inline bool SupportsLighting() { return acs >= 0; }
    inline bool SupportsTextures() { return sampler >= 0; }

protected:
#ifdef RAD_CG
    bool LinkProgram(CGprogram vertexShader, CGprogram fragmentShader);

    static void checkForCgError();
    static CGcontext context;
    static CGprofile vertexProfile, fragmentProfile;

    CGprogram program;

    // Uniform locations
    CGparameter projection, modelview, normalmatrix, alpharef, sampler;
    struct {
        CGparameter enabled, position, colour, attenuation;
    } lights[PDDI_MAX_LIGHTS];
    CGparameter acs, acm, dcm, scm, ecm, srm;
#else
    bool LinkProgram(GLuint vertexShader, GLuint fragmentShader);

    GLuint program;

    // Uniform locations
    GLint projection, modelview, normalmatrix, alpharef, sampler;
    struct {
        GLint enabled, position, colour, attenuation;
    } lights[PDDI_MAX_LIGHTS];
    GLint acs, acm, dcm, scm, ecm, srm;
    #ifdef RAD_ANDROID
    // Ubicación del uniform "lit" del vertex shader.
    GLint lit;
    GLint toonEnabled;
    GLint toonObjectEnabled;
    GLint toonRimEnabled;
    GLint toonOutlinePass;
    GLint toonOutlineOpaque;
    GLint toonEnvironmentPass;
    GLint toonOutlineWidth;
    GLint toonViewportSize;
    GLint nightObjectBrightness;
    GLint nightObjectSaturation;
    GLint toonFrontendMenuLighting;
    GLint toonMoodTint;
    #endif
#endif

    // CPU-side cache for the inverse-transpose normal matrix. It is independent
    // of GL context state and valid for this program object's lifetime.
    pddiMatrix normalMatrixModelViewCache;
    pddiMatrix normalMatrixValueCache;
    bool normalMatrixCacheValid;
};

#ifdef RAD_ANDROID
void InvalidateSHARUniformCaches();
#endif

#endif
