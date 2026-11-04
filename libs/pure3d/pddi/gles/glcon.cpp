//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================

#include <pddi/gles/gl.hpp>
#include <pddi/gles/glcon.hpp>
#include <pddi/gles/gldev.hpp>
#include <pddi/gles/gldisplay.hpp>
#include <pddi/gles/gltex.hpp>
#include <pddi/gles/glmat.hpp>
#include <pddi/gles/glprog.hpp>
#ifdef RAD_ANDROID
bool GetSHARMotionBlurCameraBasis(float* output);
#endif

#ifdef RAD_ANDROID
#include <SDL_system.h>
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <unordered_map>
#include <unordered_set>
#include <vector>


static bool gCelShadingEnabled = true;
static bool gCelShadingObjectEnabled = false;
static bool gCelShadingOutlinePass = false;
static bool gSHAREnvironmentCelPassActive = false;
static bool gSHAREnvironmentCelMaterialEligible = true;
static bool gSHAREnvironmentBillboardFoliagePassActive = false;
static bool gSHAREnvironmentFoliageMeshPassActive = false;
// The viewport is supplied to the environment silhouette shader so its extrusion
// can be specified in pixels instead of world/view-space units.
static float gSHARCelOutlineViewportWidth = 1.0f;
static float gSHARCelOutlineViewportHeight = 1.0f;
// Temporary LITE outline diagnostics: 0=normal, 1=no depth test,
// 2=no face culling, 3=no depth test and no face culling, 8=environment
// outline depth writes; Mode 9 adds clip-space bias; Mode 10 investigates roads and foliage.
static int gSHAREnvironmentOutlineDiagnosticMode = 10;
static bool gSHAREnvironmentOutlineDiagnosticsLoaded = false;
// Monotonic per-process identity for retained primitive buffers in diagnostic logs.
static unsigned gSHAREnvironmentDiagnosticMeshId = 0;
static const char* gSHAREnvironmentDiagnosticOwnerName = NULL;
static const char* gSHAREnvironmentDiagnosticDrawableName = NULL;
static unsigned long long gSHAREnvironmentDiagnosticOwnerUID = 0;
static unsigned long long gSHAREnvironmentDiagnosticSourceAssetUID = 0;
static bool gSHAREnvironmentDiagnosticFoliageCandidate = false;
static bool gSHAREnvironmentDiagnosticRoadCandidate = false;
// Enabled by gameplay only for exterior Levels 1-6; Level 7 retains the prior behavior.
static bool gSHARExteriorTransparencyDepthWriteFixActive = false;
static pglContext* gCelShadingContext = nullptr;

// The scene texture alpha channel is a per-frame character-coverage mask during
// Android world rendering. Keep the game's requested alpha-write state and gate
// only effective GL alpha writes while the world target is active.
static bool gSHARCharacterMaskWorldEnabled = false;
static bool gSHARCharacterMaskModelPass = false;
static bool gSHARCharacterMaskAlphaWriteRequested = true;

// Enabled only while an NIS/presentation scene graph is being displayed.
// Actual skinned meshes use this flag to receive the same two-pass toon
// silhouette treatment as CharacterRenderable; static gag props are untouched.
static bool gSHARGagCharacterCelPassActive = false;
// Enabled while Scrooby displays the main-menu camera set. This is separate
// from the NIS gag flag because the front-end scene is not rendered by NISPlayer.
static bool gSHARFrontendMenuCelPassActive = false;

// Mirror the colour-write mask set through the Android GLES backend. Character
// mask policy changes usually only need to toggle alpha; querying GL state for
// both transitions on every character was unnecessary. All Android-side
// glColorMask calls in this backend go through SetSHARTrackedColorWriteMask().
static GLboolean gSHARTrackedColorWriteMask[4] = {
    GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE
};
static bool gSHARTrackedColorWriteMaskKnown = false;

#ifdef RAD_ANDROID
static float gSHARObjectBrightness = 1.0f;
static float gSHARObjectSaturation = 1.0f;
#endif

void SetSHARTrackedColorWriteMask(
    GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha)
{
    if (!gSHARTrackedColorWriteMaskKnown ||
        gSHARTrackedColorWriteMask[0] != red ||
        gSHARTrackedColorWriteMask[1] != green ||
        gSHARTrackedColorWriteMask[2] != blue ||
        gSHARTrackedColorWriteMask[3] != alpha)
    {
        glColorMask(red, green, blue, alpha);
    }

    gSHARTrackedColorWriteMask[0] = red;
    gSHARTrackedColorWriteMask[1] = green;
    gSHARTrackedColorWriteMask[2] = blue;
    gSHARTrackedColorWriteMask[3] = alpha;
    gSHARTrackedColorWriteMaskKnown = true;
}

void InvalidateSHARTrackedColorWriteMaskCache()
{
    gSHARTrackedColorWriteMaskKnown = false;
}

static void ApplySHARCharacterMaskAlphaPolicy()
{
    // Query only when the tracked state is unknown (startup/context reset), not
    // on every character's begin/end model-pass transition.
    if (!gSHARTrackedColorWriteMaskKnown)
    {
        glGetBooleanv(GL_COLOR_WRITEMASK, gSHARTrackedColorWriteMask);
        gSHARTrackedColorWriteMaskKnown = true;
    }

    // The repurposed alpha channel is a mask, so character geometry must
    // mark it even if a material disables ordinary framebuffer alpha writes.
    const GLboolean alphaAllowed = gSHARCharacterMaskWorldEnabled
        ? (gSHARCharacterMaskModelPass ? GL_TRUE : GL_FALSE)
        : (gSHARCharacterMaskAlphaWriteRequested ? GL_TRUE : GL_FALSE);
    SetSHARTrackedColorWriteMask(
        gSHARTrackedColorWriteMask[0],
        gSHARTrackedColorWriteMask[1],
        gSHARTrackedColorWriteMask[2],
        alphaAllowed);
}

void SetSHARCharacterMaskWorldEnabled(bool enabled)
{
    gSHARCharacterMaskWorldEnabled = enabled;
    ApplySHARCharacterMaskAlphaPolicy();
}

void SetSHARCharacterMaskModelPass(bool enabled)
{
    gSHARCharacterMaskModelPass = enabled;
    ApplySHARCharacterMaskAlphaPolicy();
}

void SetSHARGagCharacterCelPassActive(bool enabled)
{
    gSHARGagCharacterCelPassActive = enabled;
}

bool IsSHARGagCharacterCelPassActive()
{
    return gSHARGagCharacterCelPassActive;
}

void SetSHARFrontendMenuCelPassActive(bool enabled)
{
    gSHARFrontendMenuCelPassActive = enabled;
}

bool IsSHARFrontendMenuCelPassActive()
{
    return gSHARFrontendMenuCelPassActive;
}

#ifdef RAD_ANDROID
void SetSHARObjectDimming(float brightness, float saturation)
{
    if (brightness < 0.0f) brightness = 0.0f;
    if (brightness > 1.0f) brightness = 1.0f;
    if (saturation < 0.0f) saturation = 0.0f;
    if (saturation > 1.0f) saturation = 1.0f;

    gSHARObjectBrightness = brightness;
    gSHARObjectSaturation = saturation;
    if (gCelShadingContext)
        gCelShadingContext->ApplyCelShadingState();
}
#endif
static bool gCelShadingConfigurationLoaded = false;
// FULL preserves the existing screen-space contours; LITE keeps object toon
// lighting and expanded-mesh silhouettes but skips the additional contour passes.
static bool gSHARCelShadingFull = false;
static bool gSHARCelShadingQualityConfigurationLoaded = false;
// Fresh installs default to OFF. Existing explicit sharpening preferences are preserved.
// Legacy files containing only a non-zero sharpen= value are treated as enabled.
static float gFrameSharpenStrength = 0.0f;
static bool gFrameSharpeningEnabled = false;
static bool gSharpeningConfigurationLoaded = false;

static bool gCRTConfigurationLoaded = false;
static bool gMotionBlurConfigurationLoaded = false;
static bool gMotionBlurEnabled = false;
static bool gCRTEnabled = false;
static bool gSHARRimLightingEnabled = true;
static bool gSHARRimLightingConfigurationLoaded = false;
static float gCRTScanlineStrength = 0.5f;
static float gCRTCurvature = 0.06f;
static float gCRTGlowStrength = 0.25f;
static float gCRTCornerDarkening = 0.33f;
static float gCRTOverscan = 0.0f;
static float gCRTBrightness = 1.0f;
static float gCRTContrast = 1.0f;

static const char* CRT_CONFIGURATION_FILE_NAME = "Simpsons_crt_configuration.txt";
static const char* MOTION_BLUR_CONFIGURATION_FILE_NAME = "Simpsons_motion_blur.txt";
static const char* CEL_SHADING_CONFIGURATION_FILE_NAME = "Simpsons_cel_shading.txt";
static const char* CEL_SHADING_QUALITY_CONFIGURATION_FILE_NAME = "Simpsons_cel_shading_quality.txt";
static const char* SHARPENING_CONFIGURATION_FILE_NAME = "Simpsons_sharpening.txt";
static const char* RIM_LIGHTING_CONFIGURATION_FILE_NAME = "Simpsons_rim_lighting.txt";

static bool BuildGraphicsConfigurationPath(char* filePath, size_t filePathSize, const char* fileName)
{
    const char* storagePath = SDL_AndroidGetApplicationExternalFilesPath();
    if (storagePath == nullptr || storagePath[0] == '\0')
        return false;

    char graphicsPath[512];
    snprintf(graphicsPath, sizeof(graphicsPath), "%s/Graphics", storagePath);
    if (mkdir(graphicsPath, 0777) != 0 && errno != EEXIST)
        return false;

    struct stat directoryInfo;
    if (stat(graphicsPath, &directoryInfo) != 0 || !S_ISDIR(directoryInfo.st_mode))
        return false;

    snprintf(filePath, filePathSize, "%s/%s", graphicsPath, fileName);
    return true;
}

static void LoadSHAREnvironmentOutlineDiagnostics()
{
    if (gSHAREnvironmentOutlineDiagnosticsLoaded)
        return;
    gSHAREnvironmentOutlineDiagnosticsLoaded = true;
    gSHAREnvironmentOutlineDiagnosticMode = 10;
    SDL_Log("[SHAR Outline Diagnostics] mode 10 (compiled default; no external diagnostics file)");
}

static bool BuildLegacyRootConfigurationPath(char* filePath, size_t filePathSize, const char* fileName)
{
    const char* storagePath = SDL_AndroidGetApplicationExternalFilesPath();
    if (storagePath == nullptr || storagePath[0] == '\0')
        return false;

    snprintf(filePath, filePathSize, "%s/%s", storagePath, fileName);
    return true;
}

static bool GraphicsConfigurationFileExists(const char* path)
{
    FILE* file = fopen(path, "r");
    if (file == nullptr)
        return false;
    fclose(file);
    return true;
}

static void RemoveLegacyRootConfigurationIfMigrated(const char* oldName, const char* newPath)
{
    char oldPath[512];
    if (!BuildLegacyRootConfigurationPath(oldPath, sizeof(oldPath), oldName))
        return;

    if (GraphicsConfigurationFileExists(newPath))
        remove(oldPath);
}

static void LoadSharpeningConfiguration();


static bool ParseCelShadingBool(const char* value, bool defaultValue)
{
    if (value == nullptr)
        return defaultValue;

    while (*value == ' ' || *value == '\t')
        ++value;

    if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0)
        return true;

    if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0)
        return false;

    return defaultValue;
}

static void WriteCelShadingConfiguration()
{
    char filePath[512];
    if (!BuildGraphicsConfigurationPath(filePath, sizeof(filePath), CEL_SHADING_CONFIGURATION_FILE_NAME))
        return;

    FILE* file = fopen(filePath, "w");
    if (file == nullptr)
        return;

    fprintf(file, "# Simpsons Hit & Run - Revamped! [Cel Shading Setting]\n");
    fprintf(file, "# Changes the graphics to more resemble the original Simpsons cartoon style.\n");
    fprintf(file, "# Change with true/false or 1/0 values.\n");
    fprintf(file, "# You can also use the in-game settings toggle.\n");
    fprintf(file, "# Changes to this file will be reflected upon restart.\n");
    fprintf(file, "\n");
    fprintf(file, "cel_shading=%s\n", gCelShadingEnabled ? "true" : "false");
    fclose(file);
}

static void LoadCelShadingConfiguration()
{
    if (gCelShadingConfigurationLoaded)
        return;

    gCelShadingConfigurationLoaded = true;

    char filePath[512];
    if (!BuildGraphicsConfigurationPath(filePath, sizeof(filePath), CEL_SHADING_CONFIGURATION_FILE_NAME))
        return;

    FILE* file = fopen(filePath, "r");
    bool fromLegacyRoot = false;
    char legacyPath[512];
    if (file == nullptr &&
        BuildLegacyRootConfigurationPath(legacyPath, sizeof(legacyPath), CEL_SHADING_CONFIGURATION_FILE_NAME))
    {
        file = fopen(legacyPath, "r");
        fromLegacyRoot = (file != nullptr);
    }

    if (file == nullptr)
    {
        // New installs keep cel shading enabled unless explicitly disabled in the config.
        WriteCelShadingConfiguration();
        return;
    }

    // Older versions stored sharpen= in this same file. Read that value before
    // migrating/removing the old cel-shading file.
    if (fromLegacyRoot)
        LoadSharpeningConfiguration();

    char line[256];
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        char* key = strstr(line, "cel_shading=");
        if (key == nullptr)
            continue;

        key += strlen("cel_shading=");
        char* end = key + strlen(key);
        while (end > key &&
               (end[-1] == '\n' || end[-1] == '\r' ||
                end[-1] == ' ' || end[-1] == '\t'))
            --end;
        *end = '\0';

        gCelShadingEnabled = ParseCelShadingBool(key, gCelShadingEnabled);
    }
    fclose(file);

    if (fromLegacyRoot)
    {
        WriteCelShadingConfiguration();
        RemoveLegacyRootConfigurationIfMigrated(CEL_SHADING_CONFIGURATION_FILE_NAME, filePath);
    }
}

static void WriteSHARCelShadingQualityConfiguration()
{
    char filePath[512];
    if (!BuildGraphicsConfigurationPath(
            filePath, sizeof(filePath), CEL_SHADING_QUALITY_CONFIGURATION_FILE_NAME))
        return;

    FILE* file = fopen(filePath, "w");
    if (file == nullptr)
        return;

    fprintf(file, "# Simpsons Hit & Run - Revamped! [Cel Shading Quality]\n");
    fprintf(file, "# LITE keeps object toon shading and silhouette outlines.\n");
    fprintf(file, "# FULL enables the additional screen-space and living-room detail passes.\n");
    fprintf(file, "# Change with lite/full values.\n");
    fprintf(file, "# You can also use the in-game Graphics menu.\n");
    fprintf(file, "\n");
    fprintf(file, "cel_shading_quality=%s\n", gSHARCelShadingFull ? "full" : "lite");
    fclose(file);
}

static void LoadSHARCelShadingQualityConfiguration()
{
    if (gSHARCelShadingQualityConfigurationLoaded)
        return;

    gSHARCelShadingQualityConfigurationLoaded = true;

    char filePath[512];
    if (!BuildGraphicsConfigurationPath(
            filePath, sizeof(filePath), CEL_SHADING_QUALITY_CONFIGURATION_FILE_NAME))
        return;

    FILE* file = fopen(filePath, "r");
    if (file == nullptr)
    {
        WriteSHARCelShadingQualityConfiguration();
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        char* value = strstr(line, "cel_shading_quality=");
        if (value == nullptr)
            continue;

        value += strlen("cel_shading_quality=");
        while (*value == ' ' || *value == '\t')
            ++value;

        if (strncmp(value, "lite", 4) == 0 ||
            strncmp(value, "LITE", 4) == 0 || *value == '0')
        {
            gSHARCelShadingFull = false;
        }
        else if (strncmp(value, "full", 4) == 0 ||
                 strncmp(value, "FULL", 4) == 0 || *value == '1')
        {
            gSHARCelShadingFull = true;
        }
        break;
    }

    fclose(file);
}

static void WriteSharpeningConfiguration()
{
    char filePath[512];
    if (!BuildGraphicsConfigurationPath(filePath, sizeof(filePath), SHARPENING_CONFIGURATION_FILE_NAME))
        return;

    FILE* file = fopen(filePath, "w");
    if (file == nullptr)
        return;

    fprintf(file, "# Simpsons Hit & Run - Revamped! [Sharpening Setting]\n");
    fprintf(file, "# Enhances brightness contrast along edges to make graphics more defined.\n");
    fprintf(file, "# Change with 0.20/0.40/0.60/0.80/1.00 values and true/false or 1/0 values.\n");
    fprintf(file, "# You can also use the in-game settings toggle.\n");
    fprintf(file, "# Changes to this file will be reflected upon restart.\n");
    fprintf(file, "\n");
    fprintf(file, "sharpen=%.2f\n", gFrameSharpenStrength);
    fprintf(file, "sharpening_enabled=%s\n", gFrameSharpeningEnabled ? "true" : "false");

    fclose(file);
}

static bool ParseSharpeningStrengthLine(char* line)
{
    char* value = strstr(line, "sharpen=");
    if (value == nullptr)
        return false;

    value += strlen("sharpen=");
    char* end = value + strlen(value);
    while (end > value &&
           (end[-1] == '\n' || end[-1] == '\r' ||
            end[-1] == ' ' || end[-1] == '\t'))
    {
        --end;
    }
    *end = '\0';

    char* parseEnd = nullptr;
    const float parsed = strtof(value, &parseEnd);
    if (parseEnd != value && isfinite(parsed))
        gFrameSharpenStrength = fmaxf(0.0f, fminf(1.0f, parsed));
    return true;
}

static void ParseSharpeningEnabledLine(char* line)
{
    char* enabled = strstr(line, "sharpening_enabled=");
    if (enabled == nullptr)
        return;

    enabled += strlen("sharpening_enabled=");
    while (*enabled == ' ' || *enabled == '\t')
        ++enabled;

    gFrameSharpeningEnabled =
        (*enabled == '1' || strncmp(enabled, "true", 4) == 0 ||
         strncmp(enabled, "on", 2) == 0);
}

static void LoadSharpeningConfiguration()
{
    if (gSharpeningConfigurationLoaded)
        return;

    gSharpeningConfigurationLoaded = true;

    char filePath[512];
    if (!BuildGraphicsConfigurationPath(filePath, sizeof(filePath), SHARPENING_CONFIGURATION_FILE_NAME))
        return;

    FILE* file = fopen(filePath, "r");
    bool fromLegacyDedicatedFile = false;
    bool fromLegacyCombinedCelFile = false;
    char legacyPath[512];

    if (file == nullptr &&
        BuildLegacyRootConfigurationPath(legacyPath, sizeof(legacyPath), SHARPENING_CONFIGURATION_FILE_NAME))
    {
        file = fopen(legacyPath, "r");
        fromLegacyDedicatedFile = (file != nullptr);
    }

    if (file == nullptr &&
        BuildLegacyRootConfigurationPath(legacyPath, sizeof(legacyPath), CEL_SHADING_CONFIGURATION_FILE_NAME))
    {
        file = fopen(legacyPath, "r");
        fromLegacyCombinedCelFile = (file != nullptr);
    }

    if (file == nullptr)
    {
        WriteSharpeningConfiguration();
        return;
    }

    char line[256];
    bool hasExplicitEnabledSetting = false;
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        if (ParseSharpeningStrengthLine(line))
            continue;
        if (strstr(line, "sharpening_enabled=") != nullptr)
            hasExplicitEnabledSetting = true;
        ParseSharpeningEnabledLine(line);
    }

    fclose(file);

    // Older settings files stored only a non-zero strength. Preserve that
    // enabled preference while keeping the new-install fallback OFF.
    if (!hasExplicitEnabledSetting && gFrameSharpenStrength > 0.0f)
        gFrameSharpeningEnabled = true;

    if (fromLegacyDedicatedFile)
    {
        WriteSharpeningConfiguration();
        RemoveLegacyRootConfigurationIfMigrated(SHARPENING_CONFIGURATION_FILE_NAME, filePath);
    }
    else if (fromLegacyCombinedCelFile)
    {
        // Keep the combined file until its cel_shading value has been migrated.
        WriteSharpeningConfiguration();
    }
}

static bool ParseCRTFloatSetting(char* line, const char* key, float* target, float minimum, float maximum)
{
    char* value = strstr(line, key);
    if (value == nullptr)
        return false;
    value += strlen(key);
    char* end = value + strlen(value);
    while (end > value && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t'))
        --end;
    *end = '\0';
    char* parseEnd = nullptr;
    const float parsed = strtof(value, &parseEnd);
    if (parseEnd != value && isfinite(parsed))
        *target = fmaxf(minimum, fminf(maximum, parsed));
    return true;
}

static void WriteCRTConfiguration()
{
    char filePath[512];
    if (!BuildGraphicsConfigurationPath(filePath, sizeof(filePath), CRT_CONFIGURATION_FILE_NAME))
        return;

    FILE* file = fopen(filePath, "w");
    if (file == nullptr)
        return;

    fprintf(file, "# Simpsons Hit & Run - Revamped [CRT Filter Configuration]\n");
    fprintf(file, "# (GLES2 CRT-Geom-Style)\n");
    fprintf(file, "# Overlays a screen filter that imitates a 90s CRT TV.\n");
    fprintf(file, "# 0/off bypasses the CRT pass.\n");
    fprintf(file, "# You can also use the in-game settings toggle.\n");
    fprintf(file, "# Changes to this file will be reflected upon restart.\n");
    fprintf(file, "\n");
    fprintf(file, "crt_enabled=%d\n", gCRTEnabled ? 1 : 0);
    fprintf(file, "scanline_strength=%.2f\n", gCRTScanlineStrength);
    fprintf(file, "curvature=%.2f\n", gCRTCurvature);
    fprintf(file, "glow_strength=%.2f\n", gCRTGlowStrength);
    fprintf(file, "corner_darkening=%.2f\n", gCRTCornerDarkening);
    fprintf(file, "overscan=%.2f\n", gCRTOverscan);
    fprintf(file, "brightness=%.2f\n", gCRTBrightness);
    fprintf(file, "contrast=%.2f\n", gCRTContrast);
    fclose(file);
}

static void LoadCRTConfiguration()
{
    if (gCRTConfigurationLoaded)
        return;

    gCRTConfigurationLoaded = true;

    char filePath[512];
    if (!BuildGraphicsConfigurationPath(filePath, sizeof(filePath), CRT_CONFIGURATION_FILE_NAME))
        return;

    FILE* file = fopen(filePath, "r");
    bool fromLegacyRoot = false;
    char legacyPath[512];
    if (file == nullptr &&
        BuildLegacyRootConfigurationPath(legacyPath, sizeof(legacyPath), CRT_CONFIGURATION_FILE_NAME))
    {
        file = fopen(legacyPath, "r");
        fromLegacyRoot = (file != nullptr);
    }

    if (file == nullptr)
    {
        WriteCRTConfiguration();
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        char* enabled = strstr(line, "crt_enabled=");
        if (enabled != nullptr)
        {
            enabled += strlen("crt_enabled=");
            while (*enabled == ' ' || *enabled == '\t')
                ++enabled;
            gCRTEnabled = (*enabled == '1' || strncmp(enabled, "true", 4) == 0 || strncmp(enabled, "on", 2) == 0);
            continue;
        }
        ParseCRTFloatSetting(line, "scanline_strength=", &gCRTScanlineStrength, 0.0f, 1.0f);
        ParseCRTFloatSetting(line, "curvature=", &gCRTCurvature, 0.0f, 0.5f);
        ParseCRTFloatSetting(line, "glow_strength=", &gCRTGlowStrength, 0.0f, 1.0f);
        ParseCRTFloatSetting(line, "corner_darkening=", &gCRTCornerDarkening, 0.0f, 1.0f);
        ParseCRTFloatSetting(line, "overscan=", &gCRTOverscan, 0.0f, 0.05f);
        ParseCRTFloatSetting(line, "brightness=", &gCRTBrightness, 0.5f, 1.5f);
        ParseCRTFloatSetting(line, "contrast=", &gCRTContrast, 0.5f, 1.5f);
    }

    fclose(file);

    if (fromLegacyRoot)
    {
        WriteCRTConfiguration();
        RemoveLegacyRootConfigurationIfMigrated(CRT_CONFIGURATION_FILE_NAME, filePath);
    }
}

bool IsCRTFilterEnabled()
{
    LoadCRTConfiguration();
    return gCRTEnabled;
}

void SetCRTFilterEnabled(bool enabled)
{
    LoadCRTConfiguration();
    if (gCRTEnabled == enabled)
        return;
    gCRTEnabled = enabled;
    WriteCRTConfiguration();
}

void GetCRTFilterConfiguration(bool* enabled, float* scanlineStrength, float* curvature,
                               float* glowStrength, float* cornerDarkening, float* overscan,
                               float* brightness, float* contrast)
{
    LoadCRTConfiguration();
    if (enabled) *enabled = gCRTEnabled;
    if (scanlineStrength) *scanlineStrength = gCRTScanlineStrength;
    if (curvature) *curvature = gCRTCurvature;
    if (glowStrength) *glowStrength = gCRTGlowStrength;
    if (cornerDarkening) *cornerDarkening = gCRTCornerDarkening;
    if (overscan) *overscan = gCRTOverscan;
    if (brightness) *brightness = gCRTBrightness;
    if (contrast) *contrast = gCRTContrast;
}


static void WriteMotionBlurConfiguration()
{
    char filePath[512];
    if (!BuildGraphicsConfigurationPath(filePath, sizeof(filePath), MOTION_BLUR_CONFIGURATION_FILE_NAME))
        return;

    FILE* file = fopen(filePath, "w");
    if (file == nullptr)
        return;

    fprintf(file, "# Simpsons Hit & Run - Revamped! [Motion Blur Setting]\n");
    fprintf(file, "# Adds motion blur to the tail of vehicles and the bottom of the screen.\n");
    fprintf(file, "# Change with true/false or 1/0 values.\n");
    fprintf(file, "# You can also use the in-game settings toggle.\n");
    fprintf(file, "# Changes to this file will be reflected upon restart.\n");
    fprintf(file, "\n");
    fprintf(file, "motion_blur=%s\n", gMotionBlurEnabled ? "true" : "false");
    fclose(file);
}

static void LoadMotionBlurConfiguration()
{
    if (gMotionBlurConfigurationLoaded)
        return;

    gMotionBlurConfigurationLoaded = true;
    char filePath[512];
    if (!BuildGraphicsConfigurationPath(filePath, sizeof(filePath), MOTION_BLUR_CONFIGURATION_FILE_NAME))
        return;

    FILE* file = fopen(filePath, "r");
    bool fromLegacyRoot = false;
    char legacyPath[512];
    if (file == nullptr &&
        BuildLegacyRootConfigurationPath(legacyPath, sizeof(legacyPath), MOTION_BLUR_CONFIGURATION_FILE_NAME))
    {
        file = fopen(legacyPath, "r");
        fromLegacyRoot = (file != nullptr);
    }

    if (file == nullptr)
    {
        // Fresh installs default to motion blur disabled; an existing saved preference is preserved.
        WriteMotionBlurConfiguration();
        return;
    }

    char line[256];
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        char* value = strstr(line, "motion_blur=");
        if (value == nullptr)
            continue;
        value += strlen("motion_blur=");
        while (*value == ' ' || *value == '\t')
            ++value;
        gMotionBlurEnabled =
            (*value == '1' || strncmp(value, "true", 4) == 0 ||
             strncmp(value, "on", 2) == 0);
    }
    fclose(file);

    if (fromLegacyRoot)
    {
        WriteMotionBlurConfiguration();
        RemoveLegacyRootConfigurationIfMigrated(MOTION_BLUR_CONFIGURATION_FILE_NAME, filePath);
    }
}

bool IsMotionBlurEnabled()
{
    LoadMotionBlurConfiguration();
    return gMotionBlurEnabled;
}

void SetMotionBlurEnabled(bool enabled)
{
    LoadMotionBlurConfiguration();
    if (gMotionBlurEnabled == enabled)
        return;
    gMotionBlurEnabled = enabled;
    WriteMotionBlurConfiguration();
}


static void WriteSHARRimLightingConfiguration()
{
    char filePath[512];
    if( !BuildGraphicsConfigurationPath( filePath, sizeof(filePath),
                                         RIM_LIGHTING_CONFIGURATION_FILE_NAME ) )
        return;
    FILE* file = fopen( filePath, "w" );
    if( file == nullptr )
        return;
    fprintf( file, "# Simpsons Hit & Run - Revamped! [Rim Lighting Setting]\n" );
    fprintf( file, "# Illuminates the outer edges of characters and vehicles.\n" );
    fprintf( file, "# Change with true/false or 1/0 values.\n" );
    fprintf( file, "# You can also use the in-game settings toggle.\n" );
    fprintf( file, "# Changes to this file will be reflected upon restart.\n" );
    fprintf( file, "\n" );
    fprintf( file, "rim_lighting=%s\n", gSHARRimLightingEnabled ? "true" : "false" );
    fclose( file );
}

static void LoadSHARRimLightingConfiguration()
{
    if( gSHARRimLightingConfigurationLoaded )
        return;
    gSHARRimLightingConfigurationLoaded = true;

    char filePath[512];
    if( !BuildGraphicsConfigurationPath( filePath, sizeof(filePath),
                                         RIM_LIGHTING_CONFIGURATION_FILE_NAME ) )
        return;
    FILE* file = fopen( filePath, "r" );
    if( file == nullptr )
    {
        WriteSHARRimLightingConfiguration();
        return;
    }

    char line[256];
    while( fgets( line, sizeof(line), file ) != nullptr )
    {
        char* value = strstr( line, "rim_lighting=" );
        if( value == nullptr )
            continue;
        value += strlen( "rim_lighting=" );
        while( *value == ' ' || *value == '\t' )
            ++value;
        gSHARRimLightingEnabled =
            ( *value == '1' || strncmp(value, "true", 4) == 0 ||
              strncmp(value, "on", 2) == 0 || strncmp(value, "ON", 2) == 0 );
        break;
    }
    fclose( file );
}


bool GetSHARMotionBlurCameraData(float* output)
{
    if (output == NULL || gCelShadingContext == NULL)
        return false;

    float nearPlane = 0.0f;
    float farPlane = 0.0f;
    float fov = 0.0f;
    float aspect = 0.0f;
    gCelShadingContext->GetCamera(&nearPlane, &farPlane, &fov, &aspect);
    if (nearPlane <= 0.0f || farPlane <= nearPlane ||
        fov <= 0.01f || aspect <= 0.01f)
        return false;

    // Use CVMI from tContext via the small helper above, avoiding a full P3D
    // context-header dependency in the GLES backend.
    if (!GetSHARMotionBlurCameraBasis(output))
        return false;
    output[12] = tanf(fov * 0.5f);
    output[13] = output[12] / aspect;
    output[14] = nearPlane;
    output[15] = farPlane;
    return true;
}

bool IsCelShadingEnabled()
{
    LoadCelShadingConfiguration();
    return gCelShadingEnabled;
}

bool IsFrameSharpeningEnabled()
{
    LoadSharpeningConfiguration();
    return gFrameSharpeningEnabled;
}

void SetFrameSharpeningEnabled(bool enabled)
{
    LoadSharpeningConfiguration();
    gFrameSharpeningEnabled = enabled;
    WriteSharpeningConfiguration();
}

int GetFrameSharpeningLevel()
{
    LoadSharpeningConfiguration();

    if( !gFrameSharpeningEnabled )
    {
        return 0;
    }

    int level = static_cast<int>( gFrameSharpenStrength * 5.0f + 0.5f );
    if( level < 0 ) level = 0;
    if( level > 5 ) level = 5;
    return level;
}

void SetFrameSharpeningLevel( int level )
{
    LoadSharpeningConfiguration();

    if( level < 0 ) level = 0;
    if( level > 5 ) level = 5;

    gFrameSharpenStrength = static_cast<float>( level ) * 0.2f;
    gFrameSharpeningEnabled = ( level > 0 );
    WriteSharpeningConfiguration();
}

float GetFrameSharpenStrength()
{
    LoadSharpeningConfiguration();
    return gFrameSharpenStrength;
}
bool IsCelShadingObjectEnabled() { return gCelShadingObjectEnabled; }
bool IsCelShadingOutlinePass() { return gCelShadingOutlinePass; }

void SetSHARExteriorTransparencyDepthWriteFixActive(bool enabled)
{
    gSHARExteriorTransparencyDepthWriteFixActive = enabled;
}

bool IsSHARExteriorTransparencyDepthWriteFixActive()
{
    return gSHARExteriorTransparencyDepthWriteFixActive;
}

static void ApplyCelShadingState()
{
    if (gCelShadingContext)
        gCelShadingContext->ApplyCelShadingState();
}

bool IsSHARCelShadingFull()
{
    LoadSHARCelShadingQualityConfiguration();
    return gSHARCelShadingFull;
}

void SetSHARCelShadingFull(bool full)
{
    LoadSHARCelShadingQualityConfiguration();
    LoadCelShadingConfiguration();

    if (gSHARCelShadingFull != full)
    {
        gSHARCelShadingFull = full;
        WriteSHARCelShadingQualityConfiguration();
    }

    // Choosing LITE or FULL enables the base object-level toon lighting and
    // silhouette-outline rendering. OFF is handled by SetCelShadingEnabled().
    if (!gCelShadingEnabled)
    {
        gCelShadingEnabled = true;
        WriteCelShadingConfiguration();
        ApplyCelShadingState();
    }
}

bool IsSHARRimLightingEnabled()
{
    LoadSHARRimLightingConfiguration();
    return gSHARRimLightingEnabled;
}

void SetSHARRimLightingEnabled( bool enabled )
{
    LoadSHARRimLightingConfiguration();
    if( gSHARRimLightingEnabled == enabled )
        return;
    gSHARRimLightingEnabled = enabled;
    WriteSHARRimLightingConfiguration();
    ApplyCelShadingState();
}

void SetCelShadingEnabled(bool enabled)
{
    LoadCelShadingConfiguration();
    // Preserve a legacy sharpening value before rewriting the old combined file.
    LoadSharpeningConfiguration();
    gCelShadingEnabled = enabled;
    WriteCelShadingConfiguration();
    ApplyCelShadingState();
}

void SetCelShadingObjectEnabled(bool enabled)
{
    gCelShadingObjectEnabled = enabled;
    ApplyCelShadingState();
}

void SetCelShadingOutlinePass(bool enabled)
{
    gCelShadingOutlinePass = enabled;

    if (enabled && gCelShadingEnabled && gCelShadingObjectEnabled)
    {
        // Ordinary inverted-hull meshes draw only their backfaces. Flat
        // foliage cards have no rear shell surface, so render those outlines
        // two-sided instead of culling the visible card away.
        if (gCelShadingContext)
        {
            if (gSHAREnvironmentBillboardFoliagePassActive)
            {
                gCelShadingContext->SetGLCullEnabled(false);
            }
            else
            {
                gCelShadingContext->SetGLCullEnabled(true);
                gCelShadingContext->SetGLCullFace(GL_BACK);
            }
            // Mode 8 tests whether later opaque objects erase outlines because
            // outline fragments do not populate the depth buffer. Keep this
            // override limited to environment shells; character/vehicle outlines
            // retain their original no-depth-write behaviour.
            const bool writeEnvironmentOutlineDepth =
                gSHAREnvironmentCelPassActive &&
                gSHAREnvironmentCelMaterialEligible &&
                (gSHAREnvironmentOutlineDiagnosticMode == 8 ||
                 gSHAREnvironmentOutlineDiagnosticMode == 9 ||
                 gSHAREnvironmentOutlineDiagnosticMode == 10);
            gCelShadingContext->SetGLDepthMask(writeEnvironmentOutlineDepth ? GL_TRUE : GL_FALSE);
        }
        else
        {
            if (gSHAREnvironmentBillboardFoliagePassActive)
            {
                glDisable(GL_CULL_FACE);
            }
            else
            {
                glEnable(GL_CULL_FACE);
                glCullFace(GL_BACK);
            }
            const bool writeEnvironmentOutlineDepth =
                gSHAREnvironmentCelPassActive &&
                gSHAREnvironmentCelMaterialEligible &&
                (gSHAREnvironmentOutlineDiagnosticMode == 8 ||
                 gSHAREnvironmentOutlineDiagnosticMode == 9 ||
                 gSHAREnvironmentOutlineDiagnosticMode == 10);
            glDepthMask(writeEnvironmentOutlineDepth ? GL_TRUE : GL_FALSE);
        }
    }
    else
    {
        // Restore the renderer's normal culling and depth-write state after
        // the silhouette pass, even when the same material remains current.
        if (gCelShadingContext)
        {
            const pddiCullMode mode = gCelShadingContext->GetCullMode();
            if (mode == PDDI_CULL_NONE)
            {
                gCelShadingContext->SetGLCullEnabled(false);
            }
            else
            {
                gCelShadingContext->SetGLCullEnabled(true);
                gCelShadingContext->SetGLCullFace(mode == PDDI_CULL_INVERTED ? GL_BACK : GL_FRONT);
            }
            gCelShadingContext->SetGLDepthMask(GL_TRUE);
        }
        else
        {
            glDepthMask(GL_TRUE);
        }
    }

    ApplyCelShadingState();
}

void SetSHAREnvironmentCelPassActive(bool enabled)
{
    gSHAREnvironmentCelPassActive = enabled;
    gSHAREnvironmentCelMaterialEligible = true;
    if (!enabled)
    {
        gSHAREnvironmentBillboardFoliagePassActive = false;
        gSHAREnvironmentFoliageMeshPassActive = false;
    }
    ApplyCelShadingState();
}

bool IsSHAREnvironmentOutlineDiagnosticMode10()
{
    LoadSHAREnvironmentOutlineDiagnostics();
    return gSHAREnvironmentOutlineDiagnosticMode == 10;
}

void SetSHAREnvironmentCelDiagnosticEntityNames(const char* ownerName, const char* drawableName)
{
    gSHAREnvironmentDiagnosticOwnerName = ownerName;
    gSHAREnvironmentDiagnosticDrawableName = drawableName;
}

void SetSHAREnvironmentCelDiagnosticEntityUIDs(unsigned long long ownerUID,
                                                unsigned long long sourceAssetUID,
                                                bool foliageCandidate,
                                                bool roadCandidate)
{
    gSHAREnvironmentDiagnosticOwnerUID = ownerUID;
    gSHAREnvironmentDiagnosticSourceAssetUID = sourceAssetUID;
    gSHAREnvironmentDiagnosticFoliageCandidate = foliageCandidate;
    gSHAREnvironmentDiagnosticRoadCandidate = roadCandidate;
}

struct SHARMode10FoliagePassEventKey
{
    unsigned long long assetUID;
    unsigned char kind;
    bool enabled;

    bool operator==(const SHARMode10FoliagePassEventKey& other) const
    {
        return assetUID == other.assetUID && kind == other.kind &&
               enabled == other.enabled;
    }
};

struct SHARMode10FoliagePassEventKeyHash
{
    size_t operator()(const SHARMode10FoliagePassEventKey& key) const
    {
        size_t hash = static_cast<size_t>(key.assetUID ^ (key.assetUID >> 32));
        hash ^= static_cast<size_t>(key.kind) + 0x9e3779b9U + (hash << 6) + (hash >> 2);
        hash ^= static_cast<size_t>(key.enabled ? 1U : 0U) + 0x9e3779b9U + (hash << 6) + (hash >> 2);
        return hash;
    }
};

static std::unordered_set<unsigned long long>& SHARMode10FoliageUIDs()
{
    static std::unordered_set<unsigned long long> registeredUIDs;
    return registeredUIDs;
}

void RegisterSHARMode10FoliageUID(unsigned long long uid)
{
    if (uid != 0)
        SHARMode10FoliageUIDs().insert(uid);
}

bool IsSHARMode10FoliageUID(unsigned long long uid)
{
    return uid != 0 && SHARMode10FoliageUIDs().find(uid) !=
        SHARMode10FoliageUIDs().end();
}

static bool SHARMode10ShouldLogFoliagePassEvent(unsigned char kind, bool enabled)
{
    static std::unordered_set<SHARMode10FoliagePassEventKey,
                              SHARMode10FoliagePassEventKeyHash> reportedEvents;
    SHARMode10FoliagePassEventKey key = {
        gSHAREnvironmentDiagnosticSourceAssetUID ?
            gSHAREnvironmentDiagnosticSourceAssetUID :
            gSHAREnvironmentDiagnosticOwnerUID,
        kind,
        enabled
    };
    return reportedEvents.insert(key).second;
}

void SetSHAREnvironmentBillboardFoliagePassActive(bool enabled)
{
    if (gSHAREnvironmentBillboardFoliagePassActive == enabled)
        return;
    if (IsSHAREnvironmentOutlineDiagnosticMode10() &&
        SHARMode10ShouldLogFoliagePassEvent(1, enabled))
    {
        SDL_Log("[SHAR Outline Mode 10] foliage-pass event=%s kind=billboard owner=%s drawable=%s ownerUID=%016llx sourceAssetUID=%016llx foliageCandidate=%d celPassActive=%d outlinePass=%d",
            enabled ? "begin" : "end",
            gSHAREnvironmentDiagnosticOwnerName ? gSHAREnvironmentDiagnosticOwnerName : "<no-owner-context>",
            gSHAREnvironmentDiagnosticDrawableName ? gSHAREnvironmentDiagnosticDrawableName : "<no-drawable-context>",
            gSHAREnvironmentDiagnosticOwnerUID, gSHAREnvironmentDiagnosticSourceAssetUID,
            gSHAREnvironmentDiagnosticFoliageCandidate ? 1 : 0,
            gSHAREnvironmentCelPassActive ? 1 : 0, gCelShadingOutlinePass ? 1 : 0);
    }
    gSHAREnvironmentBillboardFoliagePassActive = enabled;
}

bool IsSHAREnvironmentBillboardFoliagePassActive()
{
    return gSHAREnvironmentBillboardFoliagePassActive;
}

bool IsSHAREnvironmentCelPassActive()
{
    return gSHAREnvironmentCelPassActive;
}

void SetSHAREnvironmentCelMaterialEligible(bool eligible)
{
    if (gSHAREnvironmentCelMaterialEligible == eligible)
        return;
    gSHAREnvironmentCelMaterialEligible = eligible;
    ApplyCelShadingState();
}

void SetSHAREnvironmentFoliageMeshPassActive(bool enabled)
{
    if (gSHAREnvironmentFoliageMeshPassActive == enabled)
        return;
    if (IsSHAREnvironmentOutlineDiagnosticMode10() &&
        SHARMode10ShouldLogFoliagePassEvent(2, enabled))
    {
        SDL_Log("[SHAR Outline Mode 10] foliage-pass event=%s kind=mesh owner=%s drawable=%s ownerUID=%016llx sourceAssetUID=%016llx foliageCandidate=%d celPassActive=%d outlinePass=%d",
            enabled ? "begin" : "end",
            gSHAREnvironmentDiagnosticOwnerName ? gSHAREnvironmentDiagnosticOwnerName : "<no-owner-context>",
            gSHAREnvironmentDiagnosticDrawableName ? gSHAREnvironmentDiagnosticDrawableName : "<no-drawable-context>",
            gSHAREnvironmentDiagnosticOwnerUID, gSHAREnvironmentDiagnosticSourceAssetUID,
            gSHAREnvironmentDiagnosticFoliageCandidate ? 1 : 0,
            gSHAREnvironmentCelPassActive ? 1 : 0, gCelShadingOutlinePass ? 1 : 0);
    }
    gSHAREnvironmentFoliageMeshPassActive = enabled;
}

bool IsSHAREnvironmentFoliageMeshPassActive()
{
    return gSHAREnvironmentFoliageMeshPassActive;
}

static bool SHARNameHasFoliagePrefix(const char* name, const char* token)
{
    if (name == NULL || token == NULL || token[0] == '\0')
        return false;

    const size_t tokenLength = strlen(token);
    for (const char* start = name; *start != '\0'; ++start)
    {
        // Require a name boundary so "street" is not mistaken for "tree".
        if (start != name && start[-1] != '_' && start[-1] != '-' && start[-1] != '/')
            continue;

        size_t i = 0;
        while (i < tokenLength && start[i] != '\0' &&
               tolower(static_cast<unsigned char>(start[i])) ==
               tolower(static_cast<unsigned char>(token[i])))
        {
            ++i;
        }
        if (i == tokenLength)
            return true;
    }
    return false;
}

bool IsSHAREnvironmentFoliageEntityName(const char* ownerName, const char* drawableName)
{
    static const char* foliagePrefixes[] =
    {
        "tree", "bush", "shrub", "forest", "pumpkin", "cemtree", "gens153",
        "fern", "palm"
    };
    for (size_t i = 0; i < sizeof(foliagePrefixes) / sizeof(foliagePrefixes[0]); ++i)
    {
        if (SHARNameHasFoliagePrefix(ownerName, foliagePrefixes[i]) ||
            SHARNameHasFoliagePrefix(drawableName, foliagePrefixes[i]))
            return true;
    }
    return false;
}

bool IsSHAREnvironmentRoadEntityName(const char* name)
{
    static const char* roadTokens[] =
    {
        "road", "street", "track", "asphalt", "pavement", "sidewalk",
        "curb", "kerb", "concrete", "walkway", "driveway", "path"
    };
    for (size_t i = 0; i < sizeof(roadTokens) / sizeof(roadTokens[0]); ++i)
    {
        if (SHARNameHasFoliagePrefix(name, roadTokens[i]))
            return true;
    }
    return false;
}

// Deduplicate by source asset + shader + eligibility state rather than by the
// transient material pointer. That retains unique material behaviour throughout
// the level sequence without exhausting a global pointer-count budget.
struct SHARMode10MaterialSummaryKey
{
    unsigned long long assetUID;
    unsigned int shaderHash;
    unsigned int detailHash;
    unsigned char eligibilityFlags;
    bool foliagePass;
    bool operator==(const SHARMode10MaterialSummaryKey& other) const
    {
        return assetUID == other.assetUID && shaderHash == other.shaderHash &&
               detailHash == other.detailHash && eligibilityFlags == other.eligibilityFlags &&
               foliagePass == other.foliagePass;
    }
};
struct SHARMode10MaterialSummaryKeyHash
{
    size_t operator()(const SHARMode10MaterialSummaryKey& key) const
    {
        size_t hash = static_cast<size_t>(key.assetUID);
        hash ^= static_cast<size_t>(key.shaderHash) + 0x9e3779b9U + (hash << 6) + (hash >> 2);
        hash ^= static_cast<size_t>(key.detailHash) + 0x9e3779b9U + (hash << 6) + (hash >> 2);
        hash ^= static_cast<size_t>(key.eligibilityFlags) + (hash << 6) + (hash >> 2);
        hash ^= static_cast<size_t>(key.foliagePass ? 1U : 0U) + (hash << 6) + (hash >> 2);
        return hash;
    }
};
static unsigned int SHARMode10HashText(const char* value)
{
    unsigned int hash = 2166136261U;
    if (value != NULL)
    {
        for (const unsigned char* p = reinterpret_cast<const unsigned char*>(value); *p; ++p)
        {
            hash ^= *p;
            hash *= 16777619U;
        }
    }
    return hash;
}
static bool SHARMode10RecordMaterialSummary(const char* shaderName,
                                             bool environmentEligible,
                                             bool alphaTestOnlyEligible,
                                             bool translucent,
                                             bool outlineWillDraw,
                                             bool foliagePass,
                                             unsigned int detailHash)
{
    static std::unordered_set<SHARMode10MaterialSummaryKey, SHARMode10MaterialSummaryKeyHash> reported;
    const SHARMode10MaterialSummaryKey key = {
        gSHAREnvironmentDiagnosticSourceAssetUID ? gSHAREnvironmentDiagnosticSourceAssetUID : gSHAREnvironmentDiagnosticOwnerUID,
        SHARMode10HashText(shaderName), detailHash,
        static_cast<unsigned char>((environmentEligible ? 1 : 0) |
            (alphaTestOnlyEligible ? 2 : 0) | (translucent ? 4 : 0) |
            (outlineWillDraw ? 8 : 0)),
        foliagePass
    };
    return reported.insert(key).second;
}

void LogSHAREnvironmentMode10MaterialState(const void* materialIdentity,
                                           const char* shaderName,
                                           bool environmentEligible,
                                           bool alphaTestOnlyEligible,
                                           bool translucent,
                                           bool outlineWillDraw)
{
    if (materialIdentity == NULL || !IsSHAREnvironmentOutlineDiagnosticMode10() ||
        !gSHAREnvironmentCelPassActive || !gCelShadingOutlinePass)
        return;
    if (!SHARMode10RecordMaterialSummary(shaderName, environmentEligible,
        alphaTestOnlyEligible, translucent, outlineWillDraw, false, 0))
        return;

    SDL_Log("[SHAR Outline Mode 10] material-state owner=%s drawable=%s ownerUID=%016llx sourceAssetUID=%016llx foliageCandidate=%d roadCandidate=%d shader=%s environmentEligible=%d alphaTestOnlyEligible=%d translucent=%d outlineWillDraw=%d",
            gSHAREnvironmentDiagnosticOwnerName ? gSHAREnvironmentDiagnosticOwnerName : "<no-owner-context>",
            gSHAREnvironmentDiagnosticDrawableName ? gSHAREnvironmentDiagnosticDrawableName : "<no-drawable-context>",
            gSHAREnvironmentDiagnosticOwnerUID, gSHAREnvironmentDiagnosticSourceAssetUID,
            gSHAREnvironmentDiagnosticFoliageCandidate ? 1 : 0,
            gSHAREnvironmentDiagnosticRoadCandidate ? 1 : 0,
            shaderName ? shaderName : "<unnamed-shader>",
            environmentEligible ? 1 : 0, alphaTestOnlyEligible ? 1 : 0,
            translucent ? 1 : 0, outlineWillDraw ? 1 : 0);
}

void LogSHAREnvironmentFoliageMaterialDiagnostic(const void* materialIdentity,
                                                 const char* shaderName,
                                                 bool environmentEligible,
                                                 bool alphaTestOnlyEligible,
                                                 bool translucent,
                                                 bool outlineWillDraw)
{
    if (materialIdentity == NULL || !IsSHAREnvironmentOutlineDiagnosticMode10() ||
        !gSHAREnvironmentFoliageMeshPassActive || !gCelShadingOutlinePass)
        return;
    if (!SHARMode10RecordMaterialSummary(shaderName, environmentEligible,
        alphaTestOnlyEligible, translucent, outlineWillDraw, true, 0))
        return;

    SDL_Log("[SHAR Outline Mode 10] foliage-material callbackActive=1 passActive=1 owner=%s drawable=%s ownerUID=%016llx sourceAssetUID=%016llx foliageCandidate=%d shader=%s environmentEligible=%d alphaTestOnlyEligible=%d translucent=%d outlineWillDraw=%d",
            gSHAREnvironmentDiagnosticOwnerName ? gSHAREnvironmentDiagnosticOwnerName : "<no-owner-context>",
            gSHAREnvironmentDiagnosticDrawableName ? gSHAREnvironmentDiagnosticDrawableName : "<no-drawable-context>",
            gSHAREnvironmentDiagnosticOwnerUID, gSHAREnvironmentDiagnosticSourceAssetUID,
            gSHAREnvironmentDiagnosticFoliageCandidate ? 1 : 0,
            shaderName ? shaderName : "<unnamed-shader>",
            environmentEligible ? 1 : 0, alphaTestOnlyEligible ? 1 : 0,
            translucent ? 1 : 0, outlineWillDraw ? 1 : 0);
}

void LogSHAREnvironmentFoliageRejectedMaterial(const void* materialIdentity,
                                                const char* shaderName,
                                                unsigned long long materialUID,
                                                bool hasAlphaTest,
                                                bool hasAlphaBlend,
                                                bool hasTranslucency,
                                                float alphaRef,
                                                int alphaCompare,
                                                bool twoSided,
                                                unsigned long long textureUID,
                                                bool foliageCutoutEligible,
                                                bool outlineWillDraw)
{
    if (materialIdentity == NULL || !IsSHAREnvironmentOutlineDiagnosticMode10() ||
        !gSHAREnvironmentFoliageMeshPassActive || !gCelShadingOutlinePass)
        return;

    const int alphaRefMilli = static_cast<int>(
        alphaRef * 1000.0f + (alphaRef >= 0.0f ? 0.5f : -0.5f));
    unsigned int detailHash = static_cast<unsigned int>(alphaRefMilli);
    detailHash ^= static_cast<unsigned int>(materialUID ^ (materialUID >> 32)) +
        0x9e3779b9U + (detailHash << 6) + (detailHash >> 2);
    detailHash ^= static_cast<unsigned int>(textureUID ^ (textureUID >> 32)) +
        0x9e3779b9U + (detailHash << 6) + (detailHash >> 2);
    detailHash ^= static_cast<unsigned int>(alphaCompare) +
        0x9e3779b9U + (detailHash << 6) + (detailHash >> 2);
    detailHash ^= static_cast<unsigned int>((hasAlphaTest ? 1U : 0U) |
        (hasAlphaBlend ? 2U : 0U) | (hasTranslucency ? 4U : 0U) |
        (twoSided ? 8U : 0U) | (foliageCutoutEligible ? 16U : 0U));

    // The caller invokes this only for materials rejected by the legacy gate.
    // Keying through SHARMode10RecordMaterialSummary deduplicates by source
    // asset UID, shader identity/name and the captured material state.
    if (!SHARMode10RecordMaterialSummary(shaderName, false, false,
        hasTranslucency, outlineWillDraw, true, detailHash))
        return;

    SDL_Log("[SHAR Outline Mode 10] foliage-cutout-gate owner=%s drawable=%s ownerUID=%016llx sourceAssetUID=%016llx materialUID=%016llx shader=%s previousGateRejected=1 hasAlphaTest=%d hasAlphaBlend=%d HasTranslucency=%d alphaRef=%.3f alphaCompare=%d twoSided=%d textureUID=%016llx foliageCutoutEligible=%d outlineWillDraw=%d",
        gSHAREnvironmentDiagnosticOwnerName ? gSHAREnvironmentDiagnosticOwnerName : "<no-owner-context>",
        gSHAREnvironmentDiagnosticDrawableName ? gSHAREnvironmentDiagnosticDrawableName : "<no-drawable-context>",
        gSHAREnvironmentDiagnosticOwnerUID, gSHAREnvironmentDiagnosticSourceAssetUID,
        materialUID, shaderName ? shaderName : "<unnamed-shader>",
        hasAlphaTest ? 1 : 0, hasAlphaBlend ? 1 : 0,
        hasTranslucency ? 1 : 0, alphaRef, alphaCompare,
        twoSided ? 1 : 0, textureUID,
        foliageCutoutEligible ? 1 : 0, outlineWillDraw ? 1 : 0);
}

void LogSHAREnvironmentMode10RuntimeMaterial(bool alphaTest, float alphaRef,
                                              int alphaCompare, int blendMode,
                                              bool twoSided, bool hasTexture)
{
    if (!IsSHAREnvironmentOutlineDiagnosticMode10() || !gSHAREnvironmentCelPassActive ||
        !gCelShadingOutlinePass ||
        (!gSHAREnvironmentFoliageMeshPassActive && !gSHAREnvironmentBillboardFoliagePassActive))
        return;
    const int alphaRefMilli = static_cast<int>(alphaRef * 1000.0f + (alphaRef >= 0.0f ? 0.5f : -0.5f));
    unsigned int detailHash = static_cast<unsigned int>(alphaRefMilli);
    detailHash ^= static_cast<unsigned int>(alphaCompare) + 0x9e3779b9U + (detailHash << 6) + (detailHash >> 2);
    detailHash ^= static_cast<unsigned int>(blendMode) + 0x9e3779b9U + (detailHash << 6) + (detailHash >> 2);
    detailHash ^= static_cast<unsigned int>((twoSided ? 1 : 0) | (hasTexture ? 2 : 0) | (alphaTest ? 4 : 0));
    detailHash ^= gSHAREnvironmentBillboardFoliagePassActive ? 0xB111B04DU : 0xA11CE001U;
    if (!SHARMode10RecordMaterialSummary("<runtime-pddi-state>", alphaTest, false,
        blendMode != 0, true, true, detailHash))
        return;

    SDL_Log("[SHAR Outline Mode 10] foliage-runtime-material owner=%s drawable=%s ownerUID=%016llx sourceAssetUID=%016llx pass=%s alphaTest=%d alphaRef=%.3f alphaCompare=%d blendMode=%d twoSided=%d hasTexture=%d",
        gSHAREnvironmentDiagnosticOwnerName ? gSHAREnvironmentDiagnosticOwnerName : "<no-owner-context>",
        gSHAREnvironmentDiagnosticDrawableName ? gSHAREnvironmentDiagnosticDrawableName : "<no-drawable-context>",
        gSHAREnvironmentDiagnosticOwnerUID, gSHAREnvironmentDiagnosticSourceAssetUID,
        gSHAREnvironmentFoliageMeshPassActive ? "mesh" : "billboard",
        alphaTest ? 1 : 0, alphaRef, alphaCompare, blendMode,
        twoSided ? 1 : 0, hasTexture ? 1 : 0);
}

#endif

#include <pddi/base/debug.hpp>
#include <math.h>
#include <string.h>
#include <SDL.h>
#include <vector>

#include <microprofile.h>

// vertex arrays rendering
GLenum primTypeTable[5] =
{
    GL_TRIANGLES, //PDDI_PRIM_TRIANGLES
    GL_TRIANGLE_STRIP, //PDDI_PRIM_TRISTRIP
    GL_LINES, //PDDI_PRIM_LINES
    GL_LINE_STRIP, // PDDI_PRIM_LINESTRIP
    GL_POINTS, //PDDI_PRIM_POINTS
};

static inline void FillGLColour(pddiColour c, float* f)
{
    f[0] = float(c.Red()) / 255;
    f[1] = float(c.Green()) / 255;
    f[2] = float(c.Blue()) / 255;
    f[3] = float(c.Alpha()) / 255;
}

// extensions
class pglExtContext : public pddiExtGLContext 
{
public:
    pglExtContext(pglDisplay* d) : display(d) {}

    void BeginContext()
    {
        display->BeginContext();
    }

    void EndContext()
    {
        display->EndContext();
    }

private:
    pglDisplay* display;
};

class pglExtGamma : public pddiExtGammaControl
{
public:
    pglExtGamma(pglDisplay* d) { display = d;}

    void SetGamma(float r, float g, float b)     {display->SetGamma(r,g,b);}
    void GetGamma(float *r, float *g, float *b)  {display->GetGamma(r,g,b);}

protected:
    pglDisplay* display;
};

pglContext::pglContext(pglDevice* dev, pglDisplay* disp) : pddiBaseContext((pddiDisplay*)disp,(pddiDevice*)dev)
{
    device = dev;
    display = disp;
    currentProgram = nullptr;
    InvalidateGLStateCache();

#ifdef RAD_ANDROID
    gCelShadingContext = this;
#endif

    device->AddRef();
    display->AddRef();
    disp->SetContext(this);

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTexSize);
    DefaultState();
    contextID = 0;

    extContext = new pglExtContext(display);
    extGamma = new pglExtGamma(display);

#ifdef RAD_CG
    CGprogram vertexShader = pglProgram::CompileShader(GL_VERTEX_SHADER,
        "typedef struct {\n"
        "    int enabled;\n"
        "    float4 position;\n"
        "    float4 colour;\n"
        "    float3 attenuation;\n"
        "} LightParams;\n"

        "float3 direction(float4 p1, float4 p2) { return normalize(p2.xyz * sign(p1.w) - p1.xyz * sign(p2.w)); }\n"
        "float power(float x, float y) { return y != 0.0 ? pow(x,y) : 1.0; }\n"
        "float product(float3 x, float3 y) { return max(dot(x,y), 0.0); }\n"

        "void main(float3 position      : ATTR0,\n"
        "          float3 normal        : ATTR1,\n"
        "          float2 texcoord      : ATTR2,\n"
        "          float4 color         : ATTR3,\n"
        "          out float4 oPosition : POSITION,\n"
        "          out float2 tc        : TEXCOORD0,\n"
        "          out float4 cpri      : COLOR0,\n"
        "          out float4 csec      : COLOR1,\n"
        "          uniform float4x4 projection,\n"
        "          uniform float4x4 modelview,\n"
        "          uniform float4x4 normalmatrix,\n"
        "          uniform float4 acs,\n"
        "          uniform float4 acm,\n"
        "          uniform float4 dcm,\n"
        "          uniform float4 scm,\n"
        "          uniform float4 ecm,\n"
        "          uniform float srm,\n"
        "          uniform LightParams lights[" PDDI_STRINGIZE( PDDI_MAX_LIGHTS ) "]) {\n"
        "    float4 V = mul(modelview, float4(position, 1.0));\n"
        "    float3 n = normalize(mul(float3x3(normalmatrix), normal));\n"

        "    float3 diff = ecm.rgb + acm.rgb * acs.rgb;\n"
        "    float3 spec = float3(0.0);\n"
        "    for (int i = 0; i < " PDDI_STRINGIZE(PDDI_MAX_LIGHTS) "; i++) {\n"
        "        if (lights[i].enabled == 0) continue;\n"

        "        float3 VP = direction(V, lights[i].position);\n"
        "        float f = product(n,VP) != 0.0 ? 1.0 : 0.0;\n"
        "        float3 h = normalize(VP + float3(0.0, 0.0, 1.0));\n"

        "        float3 k = lights[i].attenuation;\n"
        "        float d = distance(V.xyz, lights[i].position.xyz);\n"
        "        float att = lights[i].position.w != 0.0 ? 1.0 / (k[0] + k[1] * d + k[2] * d * d) : 1.0;\n"

        "        diff += att * product(n,VP) * dcm.rgb * lights[i].colour.rgb;\n"
        "        spec += att * f * power(product(n,h),srm) * scm.rgb * lights[i].colour.rgb;\n"
        "    }\n"

        "    tc = texcoord;\n"
        "    cpri = color * float4(diff, dcm.a);\n"
        "    csec = float4(spec, 0.0);\n"
        "    oPosition = mul(projection, V);\n"
        "}\n"
    );

    CGprogram fragmentShader = pglProgram::CompileShader( GL_FRAGMENT_SHADER,
        "void main(float2 tc        : TEXCOORD0,\n"
        "          float4 cpri      : COLOR0,\n"
        "          float4 csec      : COLOR1,\n"
        "          out float4 color : COLOR) {\n"
        "    color = cpri + csec;\n"
        "}\n"
    );

    CGprogram textureShader = pglProgram::CompileShader(GL_FRAGMENT_SHADER,
        "void main(float2 tc : TEXCOORD0,\n"
        "          float4 cpri : COLOR0,\n"
        "          float4 csec : COLOR1,\n"
        "          out float4 color : COLOR,\n"
        "          uniform sampler2D tex) {\n"
        "    color = tex2D(tex, tc) * cpri + csec;\n"
        "}\n"
    );

    CGprogram alphaTestShader = pglProgram::CompileShader(GL_FRAGMENT_SHADER,
        "void main(float2 tc : TEXCOORD0,\n"
        "          float4 cpri : COLOR0,\n"
        "          float4 csec : COLOR1,\n"
        "          out float4 color : COLOR,\n"
        "          uniform float alpharef,\n"
        "          uniform sampler2D tex) {\n"
        "    float4 c = tex2D(tex, tc) * cpri + csec;\n"
        "    if (c.a < alpharef) discard;\n"
        "    color = c;\n"
        "}\n"
    );
#else
    GLuint vertexShader = pglProgram::CompileShader(GL_VERTEX_SHADER,
        "precision highp float;\n"
        "precision mediump int;\n"

        "attribute vec3 position;\n"
        "attribute vec3 normal;\n"
        "attribute vec2 texcoord;\n"
        "attribute vec4 color;\n"
#ifdef RAD_ANDROID
        "attribute vec3 outlineNormal;\n"
#endif

        "uniform mat4 projection;\n"
        "uniform mat4 modelview;\n"
        "uniform mat4 normalmatrix;\n"

        "varying vec2 tc;\n"
        "varying vec4 cpri;\n"
        "varying vec3 toonNormal;\n"
        "varying vec3 toonViewPos;\n"
#ifdef RAD_ANDROID
        "varying float toonOutlineProjectionState;\n"
        "uniform int toonEnabled;\n"
        "uniform int toonObjectEnabled;\n"
        "uniform int toonOutlinePass;\n"
        "uniform int toonEnvironmentPass;\n"
        "uniform int toonOutlineDiagnosticMode;\n"
        "uniform float toonOutlineWidth;\n"
        "uniform vec2 toonViewportSize;\n"
#endif
        "void main() {\n"
        "    vec4 V = modelview * vec4(position, 1.0);\n"
#ifdef RAD_ANDROID
        "    vec3 vertexNormal = (toonOutlinePass != 0 && toonEnvironmentPass != 0) ? outlineNormal : normal;\n"
        "    toonNormal = normalize(mat3(normalmatrix) * vertexNormal);\n"
#else
        "    toonNormal = normalize(mat3(normalmatrix) * normal);\n"
#endif
        "    vec4 clipPosition = projection * V;\n"
#ifdef RAD_ANDROID
        "    toonOutlineProjectionState = 1.0;\n"
        "    if (toonEnabled != 0 && toonObjectEnabled != 0 && toonOutlinePass != 0) {\n"
        "        if (toonEnvironmentPass != 0) {\n"
        "            toonOutlineProjectionState = 0.0;\n"
        "            if (clipPosition.w > 0.00001) {\n"
        "                vec4 clipNormal = projection * vec4(toonNormal, 0.0);\n"
        "                vec2 projectedNormalPixels = (clipNormal.xy * clipPosition.w - clipPosition.xy * clipNormal.w) * toonViewportSize;\n"
        "                float projectedNormalLength = length(projectedNormalPixels);\n"
        "                if (projectedNormalLength > 0.00001) {\n"
        "                    toonOutlineProjectionState = 1.0;\n"
        "                    vec2 pixelDirection = projectedNormalPixels / projectedNormalLength;\n"
        "                    vec2 ndcOffset = pixelDirection * (2.0 * toonOutlineWidth / toonViewportSize);\n"
        "                    clipPosition.xy += ndcOffset * clipPosition.w;\n"
        "                }\n"
        "            }\n"
        "            if ((toonOutlineDiagnosticMode == 9 || toonOutlineDiagnosticMode == 10) && clipPosition.w > 0.00001) {\n"
        "                // Move the expanded shell slightly farther away so the original mesh wins.\n"
        "                clipPosition.z += 0.00025 * clipPosition.w;\n"
        "            }\n"
        "        } else {\n"
        "            V.xyz += toonNormal * 0.009;\n"
        "            clipPosition = projection * V;\n"
        "        }\n"
        "    }\n"
#endif
        "    toonViewPos = V.xyz;\n"
        "    tc = texcoord;\n"
        "    cpri = color;\n"
        "    gl_Position = clipPosition;\n"
        "}\n"
    );

    // Evaluate lighting per-pixel so the toon thresholds remain crisp across
    // each polygon instead of being interpolated from the triangle vertices.
    const std::string toonLighting =
        "precision mediump float;\n"
        "precision mediump int;\n"
        "varying vec2 tc;\n"
        "varying vec4 cpri;\n"
        "varying vec3 toonNormal;\n"
        "varying vec3 toonViewPos;\n"

        "uniform struct LightParams {\n"
        "    int enabled;\n"
        "    vec4 position;\n"
        "    vec4 colour;\n"
        "    vec3 attenuation;\n"
        "} lights[" PDDI_STRINGIZE(PDDI_MAX_LIGHTS) "];\n"

        "uniform vec4 acs;\n"
        "uniform vec4 acm;\n"
        "uniform vec4 dcm;\n"
        "uniform vec4 scm;\n"
        "uniform vec4 ecm;\n"
        "uniform float srm;\n"
#ifdef RAD_ANDROID
        "uniform int lit;\n"
        "uniform int toonEnabled;\n"
        "uniform int toonObjectEnabled;\n"
        "uniform int toonRimEnabled;\n"
        "uniform int toonOutlinePass;\n"
        "uniform int toonEnvironmentPass;\n"
        "uniform int toonOutlineDiagnosticMode;\n"
        "uniform float toonOutlineWidth;\n"
        "uniform float nightObjectBrightness;\n"
        "uniform float nightObjectSaturation;\n"
        "varying float toonOutlineProjectionState;\n"
#endif

        "vec3 direction(vec4 p1, vec4 p2) { return normalize(p2.xyz * sign(p1.w) - p1.xyz * sign(p2.w)); }\n"
        "float product(vec3 x, vec3 y) { return max(dot(x,y), 0.0); }\n"
        "float power(float x, float y) { return y != 0.0 ? pow(x,y) : 1.0; }\n"

        // One flat toon tone for enabled characters and vehicles. This removes
        // the two-band light/shadow jump and keeps the source texture colours intact.
        "const float toonFlatLight = 0.78;\n"
#ifdef RAD_ANDROID
        "const float toonRimStrength = 0.20;\n"
#endif

        "vec3 toonLightingColor() {\n"
        "    if (toonEnabled == 0 || toonObjectEnabled == 0) {\n"
        "        vec3 n = normalize(toonNormal);\n"
        "        vec3 diff = ecm.rgb + acm.rgb * acs.rgb;\n"
        "        vec3 spec = vec3(0.0);\n"
#ifdef RAD_ANDROID
        "        if (lit == 0) return vec3(1.0);\n"
#endif
        "        for (int i = 0; i < " PDDI_STRINGIZE(PDDI_MAX_LIGHTS) "; i++) {\n"
        "            if (lights[i].enabled == 0) continue;\n"
        "            vec3 VP = direction(vec4(toonViewPos, 1.0), lights[i].position);\n"
        "            float ndotl = product(n, VP);\n"
        "            vec3 h = normalize(VP + vec3(0.0, 0.0, 1.0));\n"
        "            float specular = power(product(n, h), srm);\n"
        "            vec3 k = lights[i].attenuation;\n"
        "            float d = distance(toonViewPos, lights[i].position.xyz);\n"
        "            float att = lights[i].position.w != 0.0 ? 1.0 / (k[0] + k[1] * d + k[2] * d * d) : 1.0;\n"
        "            diff += att * ndotl * dcm.rgb * lights[i].colour.rgb;\n"
        "            spec += att * specular * scm.rgb * lights[i].colour.rgb;\n"
        "        }\n"
        "        return diff + spec;\n"
        "    }\n"
#ifdef RAD_ANDROID
        "    if (lit == 0) return toonEnvironmentPass != 0 ? vec3(toonFlatLight) : vec3(1.0);\n"
        "    if (toonRimEnabled == 0) return dcm.rgb * toonFlatLight;\n"
        // Guard the rim calculation against zero-length vectors and precision
        // overshoot. Some mobile GPUs can round a normalized dot product above
        // 1.0; pow() with a negative base and fractional exponent is undefined.
        "    vec3 nRaw = toonNormal;\n"
        "    vec3 vRaw = -toonViewPos;\n"
        "    float nLen2 = dot(nRaw, nRaw);\n"
        "    float vLen2 = dot(vRaw, vRaw);\n"
        "    float rim = 0.0;\n"
        "    if (nLen2 > 0.000001 && vLen2 > 0.000001) {\n"
        "        vec3 n = nRaw * inversesqrt(nLen2);\n"
        "        vec3 v = vRaw * inversesqrt(vLen2);\n"
        "        float facing = clamp(dot(n, v), 0.0, 1.0);\n"
        "        rim = pow(max(1.0 - facing, 0.0), 2.5);\n"
        "    }\n"
        "    return dcm.rgb * (toonFlatLight + toonRimStrength * rim);\n"
#else
        "    return dcm.rgb * toonFlatLight;\n"
#endif
        "}\n"
#ifdef RAD_ANDROID
        "vec3 applySHARObjectDimming(vec3 colour) {\n"
        "    float luma = dot(colour, vec3(0.299, 0.587, 0.114));\n"
        "    return mix(vec3(luma), colour, clamp(nightObjectSaturation, 0.0, 1.0)) * clamp(nightObjectBrightness, 0.0, 1.0);\n"
        "}\n"
#endif
        ;

    GLuint fragmentShader = pglProgram::CompileShader(GL_FRAGMENT_SHADER,
        (toonLighting +
        "void main() {\n"
        "    if (toonOutlinePass != 0 && toonEnabled != 0 && toonObjectEnabled != 0) {\n"
#ifdef RAD_ANDROID
        "        if (toonEnvironmentPass != 0 && (toonOutlineDiagnosticMode == 7 || toonOutlineDiagnosticMode == 8 || toonOutlineDiagnosticMode == 9 || toonOutlineDiagnosticMode == 10)) {\n"
        "            vec3 diagnosticColour = mix(vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), clamp(toonOutlineProjectionState, 0.0, 1.0));\n"
        "            gl_FragColor = vec4(diagnosticColour, cpri.a * dcm.a);\n"
        "            return;\n"
        "        }\n"
#endif
        "        gl_FragColor = vec4(0.0, 0.0, 0.0, cpri.a * dcm.a);\n"
        "        return;\n"
        "    }\n"
#ifdef RAD_ANDROID
        "    vec4 finalColour = cpri * vec4(toonLightingColor(), dcm.a);\n"
        "    finalColour.rgb = applySHARObjectDimming(finalColour.rgb);\n"
        "    gl_FragColor = finalColour;\n"
#else
        "    gl_FragColor = cpri * vec4(toonLightingColor(), dcm.a);\n"
#endif
        "}\n"
    ).c_str());

    GLuint textureShader = pglProgram::CompileShader(GL_FRAGMENT_SHADER,
        (toonLighting +
        "uniform sampler2D tex;\n"
        "void main() {\n"
        "    vec4 base = texture2D(tex, tc) * cpri;\n"
        "    if (toonOutlinePass != 0 && toonEnabled != 0 && toonObjectEnabled != 0) {\n"
#ifdef RAD_ANDROID
        "        if (toonEnvironmentPass != 0 && (toonOutlineDiagnosticMode == 7 || toonOutlineDiagnosticMode == 8 || toonOutlineDiagnosticMode == 9 || toonOutlineDiagnosticMode == 10)) {\n"
        "            vec3 diagnosticColour = mix(vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), clamp(toonOutlineProjectionState, 0.0, 1.0));\n"
        "            gl_FragColor = vec4(diagnosticColour, base.a);\n"
        "            return;\n"
        "        }\n"
#endif
        "        gl_FragColor = vec4(0.0, 0.0, 0.0, base.a);\n"
        "        return;\n"
        "    }\n"
#ifdef RAD_ANDROID
        "    vec4 finalColour = base * vec4(toonLightingColor(), dcm.a);\n"
        "    finalColour.rgb = applySHARObjectDimming(finalColour.rgb);\n"
        "    gl_FragColor = finalColour;\n"
#else
        "    gl_FragColor = base * vec4(toonLightingColor(), dcm.a);\n"
#endif
        "}\n"
    ).c_str());

    GLuint alphaTestShader = pglProgram::CompileShader(GL_FRAGMENT_SHADER,
        (toonLighting +
        "uniform float alpharef;\n"
        "uniform sampler2D tex;\n"
        "void main() {\n"
        "    vec4 base = texture2D(tex, tc) * cpri;\n"
        "    if (toonOutlinePass != 0 && toonEnabled != 0 && toonObjectEnabled != 0) {\n"
        "        if (base.a < alpharef) discard;\n"
#ifdef RAD_ANDROID
        "        if (toonEnvironmentPass != 0 && (toonOutlineDiagnosticMode == 7 || toonOutlineDiagnosticMode == 8 || toonOutlineDiagnosticMode == 9 || toonOutlineDiagnosticMode == 10)) {\n"
        "            vec3 diagnosticColour = mix(vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0), clamp(toonOutlineProjectionState, 0.0, 1.0));\n"
        "            gl_FragColor = vec4(diagnosticColour, base.a);\n"
        "            return;\n"
        "        }\n"
#endif
        "        gl_FragColor = vec4(0.0, 0.0, 0.0, base.a);\n"
        "        return;\n"
        "    }\n"
        "    vec4 c = base * vec4(toonLightingColor(), dcm.a);\n"
        "    if (c.a < alpharef) discard;\n"
#ifdef RAD_ANDROID
        "    c.rgb = applySHARObjectDimming(c.rgb);\n"
#endif
        "    gl_FragColor = c;\n"
        "}\n"
    ).c_str());
#endif

    colorProgram = pglProgram::CreateProgram(vertexShader, fragmentShader);

    textureProgram = pglProgram::CreateProgram(vertexShader, textureShader);

    alphaTestProgram = pglProgram::CreateProgram(vertexShader, alphaTestShader);

    // Don't leak shaders
#ifdef RAD_CG
    cgDestroyProgram(vertexShader);
    cgDestroyProgram(fragmentShader);
    cgDestroyProgram(textureShader);
    cgDestroyProgram(alphaTestShader);
#else
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    glDeleteShader(textureShader);
    glDeleteShader(alphaTestShader);
#endif

    defaultShader = new pglMat(this);
    defaultShader->AddRef();
    SetShaderProgram(colorProgram);
}

pglContext::~pglContext()
{
#ifdef RAD_ANDROID
    if (gCelShadingContext == this)
        gCelShadingContext = nullptr;
#endif

    if (defaultShader) defaultShader->Release();
    if (currentProgram) currentProgram->Release();
    if (colorProgram) colorProgram->Release();
    if (textureProgram) textureProgram->Release();
    if (alphaTestProgram) alphaTestProgram->Release();

    delete extContext;
    delete extGamma;

    display->SetContext(NULL);
    display->Release();
    device->Release();
}


void pglContext::InvalidateGLStateCache()
{
    mGLBlendEnabledKnown = false;
    mGLBlendEquationKnown = false;
    mGLBlendFuncKnown = false;
    mGLDepthMaskKnown = false;
    mGLCullEnabledKnown = false;
    mGLCullFaceKnown = false;
}
#ifdef RAD_ANDROID
void InvalidateSHARRenderStateCache()
{
    if (gCelShadingContext)
        gCelShadingContext->InvalidateGLStateCache();
}
#endif

void pglContext::SetGLBlendEnabled(bool enabled)
{
    if (!mGLBlendEnabledKnown || mGLBlendEnabled != enabled)
    {
        if (enabled) glEnable(GL_BLEND); else glDisable(GL_BLEND);
        mGLBlendEnabled = enabled; mGLBlendEnabledKnown = true;
    }
}
void pglContext::SetGLBlendEquation(GLenum equation)
{
    if (!mGLBlendEquationKnown || mGLBlendEquation != equation)
    {
        glBlendEquation(equation);
        mGLBlendEquation = equation; mGLBlendEquationKnown = true;
    }
}
void pglContext::SetGLBlendFunc(GLenum source, GLenum destination)
{
    if (!mGLBlendFuncKnown || mGLBlendSource != source || mGLBlendDestination != destination)
    {
        glBlendFunc(source, destination);
        mGLBlendSource = source; mGLBlendDestination = destination; mGLBlendFuncKnown = true;
    }
}
void pglContext::SetGLDepthMask(GLboolean enabled)
{
    if (!mGLDepthMaskKnown || mGLDepthMask != enabled)
    {
        glDepthMask(enabled);
        mGLDepthMask = enabled; mGLDepthMaskKnown = true;
    }
}
void pglContext::SetGLCullEnabled(bool enabled)
{
    if (!mGLCullEnabledKnown || mGLCullEnabled != enabled)
    {
        if (enabled) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
        mGLCullEnabled = enabled; mGLCullEnabledKnown = true;
    }
}
void pglContext::SetGLCullFace(GLenum mode)
{
    if (!mGLCullFaceKnown || mGLCullFaceMode != mode)
    {
        glCullFace(mode);
        mGLCullFaceMode = mode; mGLCullFaceKnown = true;
    }
}

// frame synchronisation
void pglContext::BeginFrame()
{
    InvalidateGLStateCache();
#ifdef RAD_ANDROID
    // Commit the selected internal render target and reset GL resources on this
    // render thread. The platform-sized Android output Surface is not resized.
    display->CommitPendingResize();
#endif
    pddiBaseContext::BeginFrame();

    static int cachedSwapInterval = -2;
    if (display->HasReset())
        cachedSwapInterval = -2;
    const int requestedSwapInterval = display->GetForceVSync() ? 1 : 0;
    if (requestedSwapInterval != cachedSwapInterval)
    {
        SDL_GL_SetSwapInterval(requestedSwapInterval);
        cachedSwapInterval = requestedSwapInterval;
    }

    if(display->HasReset())
    {
        InvalidateGLStateCache();
#ifdef RAD_ANDROID
        InvalidateSHARUniformCaches();
#endif
        contextID++;

        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
#ifndef RAD_VITAGL
        glEnable(GL_DITHER);
#endif

        SyncState(0xffffffff);
    }

    projection.Identity();

#ifdef RAD_ANDROID
    /*
     * Always route the world into the offscreen render target before draw calls.
     * Cel shading selects either the toon compose or a plain resolve; GUI rendering
     * then uses the actual output Surface dimensions.
     */
    BeginCelPostProcessFrame(display->GetWidth(), display->GetHeight());
    InvalidateGLStateCache();
#endif
}

void pglContext::EndFrame()
{
    pddiBaseContext::EndFrame();
}

// buffer clearing
void pglContext::Clear(unsigned bufferMask)
{
    pddiBaseContext::Clear(bufferMask);

    int myClearMask = 0;
    myClearMask |= (bufferMask & PDDI_BUFFER_COLOUR) ? GL_COLOR_BUFFER_BIT : 0;
    myClearMask |= (bufferMask & PDDI_BUFFER_DEPTH) ? GL_DEPTH_BUFFER_BIT : 0;
    //myClearMask |= (bufferMask & PDDI_BUFFER_STENCIL) ? GL_STENCIL_BUFFER_BIT : 0;

    glClearDepthf(state.viewState->clearDepth);
    glClearColor(float(state.viewState->clearColour.Red())/255.0f, 
                     float(state.viewState->clearColour.Green())/255.0f, 
                     float(state.viewState->clearColour.Blue())/255.0f,
                     float(state.viewState->clearColour.Alpha())/255.0f);
    glClearStencil(state.viewState->clearStencil);
    glClear(myClearMask);
}

void pglContext::SetupHardwareProjection(void)
{
    switch(state.viewState->projectionMode)
    {
        case PDDI_PROJECTION_DEVICE :
            projection.Identity();
            projection.SetOrthographic(0, display->GetWidth(),
                      display->GetHeight(), 0,
                      (state.viewState->camera.nearPlane),(state.viewState->camera.farPlane));
            glViewport(0, 0, display->GetWidth(), display->GetHeight());
            break;

        case PDDI_PROJECTION_ORTHOGRAPHIC :
            projection.Identity();
            projection.SetOrthographic(-0.5,  0.5,
                      -((1/state.viewState->camera.aspect)/2),  ((1/state.viewState->camera.aspect)/2),
                      (state.viewState->camera.nearPlane),(state.viewState->camera.farPlane));
            glViewport(int(state.viewState->viewWindow.left * display->GetWidth()), 
                              int((1.0f - state.viewState->viewWindow.bottom) * display->GetHeight() ),
                              int((state.viewState->viewWindow.right - state.viewState->viewWindow.left) * display->GetWidth()), 
                              int((state.viewState->viewWindow.bottom - state.viewState->viewWindow.top) * display->GetHeight()));
            break;

        case PDDI_PROJECTION_PERSPECTIVE :
            projection.Identity();
            projection.SetPerspective(state.viewState->camera.fov,state.viewState->camera.aspect,state.viewState->camera.nearPlane,state.viewState->camera.farPlane);
            glViewport(int(state.viewState->viewWindow.left * display->GetWidth()), 
                            int((1.0f - state.viewState->viewWindow.bottom) * display->GetHeight() ),
                            int((state.viewState->viewWindow.right - state.viewState->viewWindow.left) * display->GetWidth()), 
                            int((state.viewState->viewWindow.bottom - state.viewState->viewWindow.top) * display->GetHeight()));
            break;
        default:
            PDDIASSERTMSG(0, "Bad projection mode","");
            break;
    }

#ifdef RAD_ANDROID
    // Match the shader's pixel-width extrusion to the active 3D viewport.
    if (state.viewState->projectionMode == PDDI_PROJECTION_DEVICE)
    {
        gSHARCelOutlineViewportWidth = (float)display->GetWidth();
        gSHARCelOutlineViewportHeight = (float)display->GetHeight();
    }
    else
    {
        gSHARCelOutlineViewportWidth =
            (float)display->GetWidth() *
            (state.viewState->viewWindow.right - state.viewState->viewWindow.left);
        gSHARCelOutlineViewportHeight =
            (float)display->GetHeight() *
            (state.viewState->viewWindow.bottom - state.viewState->viewWindow.top);
    }
    if (gSHARCelOutlineViewportWidth < 1.0f) gSHARCelOutlineViewportWidth = 1.0f;
    if (gSHARCelOutlineViewportHeight < 1.0f) gSHARCelOutlineViewportHeight = 1.0f;
#endif
    if(currentProgram)
    {
        currentProgram->SetProjectionMatrix(&projection);
#ifdef RAD_ANDROID
        ApplyCelShadingState();
#endif
    }
}

void pglContext::LoadHardwareMatrix(pddiMatrixType id)
{
    switch(id)
    {
        case PDDI_MATRIX_MODELVIEW :
        {
            if(currentProgram)
                currentProgram->SetModelViewMatrix(state.matrixStack[id]->Top());
        }
        break;
        default :
            PDDIASSERTMSG(0, "Invalid matrix load","");
            break;
    }
}

// viewport clipping
void pglContext::SetScissor(pddiRect* rect)
{
    pddiBaseContext::SetScissor(rect);
    if(!rect)
    {
        glDisable(GL_SCISSOR_TEST);
    }
    else
    {
        glScissor(rect->left, display->GetHeight() - rect->bottom, rect->right - rect->left, rect->bottom - rect->top);
        glEnable(GL_SCISSOR_TEST);
    }
}

#include <vector>
class pglPrimStream : public pddiPrimStream
{
public:
    std::vector<pddiVector> coords;
    std::vector<pddiVector> normals;
    std::vector<GLubyte> colours;
    std::vector<pddiVector2> uvs;

    GLenum primitive;
    unsigned vertexType;

    void Coord(float x, float y, float z)  
    {
        coords.push_back( pddiVector{ x, y, z } );
    }

    void Normal(float x, float y, float z) 
    {
        normals.push_back( pddiVector{ x, y, z } );
    }

    void Colour(pddiColour colour, int channel = 0)
    {
        colours.push_back( colour.Red() );
        colours.push_back( colour.Green() );
        colours.push_back( colour.Blue() );
        colours.push_back( colour.Alpha() );
    }

    void UV(float u, float v, int channel = 0) 
    { 
        if(channel == 0)
        {
            uvs.push_back( pddiVector2{ u, v } );
        }
    }

    void Specular(pddiColour colour) 
    {
        //
    }

    void Vertex(pddiVector* v, pddiColour c) 
    {
        colours.push_back( c.Red() );
        colours.push_back( c.Green() );
        colours.push_back( c.Blue() );
        colours.push_back( c.Alpha() );
        coords.push_back( *v );
    }

    void Vertex(pddiVector* v, pddiVector* n)
    {
        normals.push_back( *n );
        coords.push_back( *v );
    }

    void Vertex(pddiVector* v, pddiVector2* uv)
    {
        uvs.push_back( *uv );
        coords.push_back( *v );
    }

    void Vertex(pddiVector* v, pddiColour c, pddiVector2* uv)
    {
        colours.push_back( c.Red() );
        colours.push_back( c.Green() );
        colours.push_back( c.Blue() );
        colours.push_back( c.Alpha() );
        uvs.push_back( *uv );
        coords.push_back( *v );
    }

    void Vertex(pddiVector* v, pddiVector* n, pddiVector2* uv)
    {
        normals.push_back( *n );
        uvs.push_back( *uv );
        coords.push_back( *v );
    }

} thePrimStream;

pddiPrimStream* pglContext::BeginPrims(pddiShader* mat, pddiPrimType primType, unsigned vertexType, int vertexCount, unsigned pass)
{
    if(!mat)
        mat = defaultShader;

    pddiBaseContext::BeginPrims(mat, primType, vertexType, vertexCount);
    pddiBaseShader* material = (pddiBaseShader*)mat;
    ADD_STAT( PDDI_STAT_MATERIAL_OPS, !material->IsCurrent() );
    material->SetMaterial();
    thePrimStream.primitive = primTypeTable[primType];
    thePrimStream.vertexType = vertexType;
    return &thePrimStream;
}

void pglContext::EndPrims(pddiPrimStream* stream)
{
    MICROPROFILE_SCOPEI("SRR2", "pglContext::EndPrims", MP_RED);

    pddiBaseContext::EndPrims(stream);
    pglPrimStream* glstream = (pglPrimStream*)stream;

    glBindVertexArrayOES( 0 );
    glBindBuffer( GL_ARRAY_BUFFER, 0 );
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, 0 );
    glEnableVertexAttribArray( 0 );
    glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, 0, glstream->coords.data() );

    if( !glstream->normals.empty() )
    {
        glEnableVertexAttribArray( 1 );
        glVertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, 0, glstream->normals.data() );
    }
    else
    {
        glDisableVertexAttribArray( 1 );
        glVertexAttrib3f( 1, 0.0f, 0.0f, 0.0f );
    }

    if( !glstream->uvs.empty() )
    {
        glEnableVertexAttribArray( 2 );
        glVertexAttribPointer( 2, 2, GL_FLOAT, GL_FALSE, 0, glstream->uvs.data() );
    }
    else
    {
        glDisableVertexAttribArray( 2 );
        glVertexAttrib2f( 2, 0.0f, 0.0f );
    }

    if( !glstream->colours.empty() )
    {
        glEnableVertexAttribArray( 3 );
        glVertexAttribPointer( 3, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, glstream->colours.data() );
    }
    else
    {
        glDisableVertexAttribArray( 3 );
        glVertexAttrib4f( 3, 1.0f, 1.0f, 1.0f, 1.0f );
    }

#ifdef RAD_ANDROID
    // Environment foliage shaders read outlineNormal at attribute 4, not the
    // ordinary normal at attribute 1. Immediate-mode billboard streams are
    // used by both direct and baked billboard draws, so bind the streamed
    // corner normals to both shader inputs for this pass.
    const bool useFoliageOutlineNormals =
        IsSHAREnvironmentBillboardFoliagePassActive() &&
        !glstream->normals.empty();
    if (useFoliageOutlineNormals)
    {
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, 0, glstream->normals.data());
    }
    else
    {
        glDisableVertexAttribArray(4);
        glVertexAttrib3f(4, 0.0f, 0.0f, 1.0f);
    }
#endif

    glDrawArrays( glstream->primitive, 0, glstream->coords.size() );

#ifdef RAD_ANDROID
    // Do not leave attribute 4 pointing into the temporary stream vector.
    glDisableVertexAttribArray(4);
    glVertexAttrib3f(4, 0.0f, 0.0f, 1.0f);
#endif

    glstream->coords.clear();
    glstream->normals.clear();
    glstream->colours.clear();
    glstream->uvs.clear();
}

class pglPrimBufferStream : public pddiPrimBufferStream
{
public:
    pglPrimBuffer* buffer;

    pglPrimBufferStream(pglPrimBuffer* b)
    {
        buffer = b;
    }

    void Next(void)  
    {
        if(buffer->coord)
            buffer->coord = (float*)((char*)buffer->coord + buffer->stride);

        if(buffer->normal)
            buffer->normal = (float*)((char*)buffer->normal + buffer->stride);

        if(buffer->uv)
            buffer->uv = (float*)((char*)buffer->uv + buffer->stride);

        if(buffer->colour)
            buffer->colour += buffer->stride;

        buffer->total++;
        PDDIASSERT(buffer->total <= buffer->allocated);
    }

    void Position(float x, float y, float z)  
    { 
        buffer->coord[0] = x;
        buffer->coord[1] = y;
        buffer->coord[2] = z;
        Next();
    }

    void Normal(float x, float y, float z) 
    { 
        buffer->normal[0] = x;
        buffer->normal[1] = y;
        buffer->normal[2] = z;
    }

    void Colour(pddiColour colour, int channel = 0)         
    {
        // HBW: Multiple CBVs not yet implemented.  For now just ignore channel.
        buffer->colour[0] = colour.Red();
        buffer->colour[1] = colour.Green();
        buffer->colour[2] = colour.Blue();
        buffer->colour[3] = colour.Alpha();
    }

    void TexCoord1(float u, int channel = 0) {}

    void TexCoord2(float u, float v, int channel = 0) 
    { 
        if(channel == 0)
        {
            buffer->uv[0] = u;
            buffer->uv[1] = v;
        }
    }

    void TexCoord3(float u, float v, float s, int channel = 0) {}
    void TexCoord4(float u, float v, float s, float t, int channel = 0) {}

    void Specular(pddiColour colour) 
    {
        //
    }

    void SkinIndices(unsigned, unsigned, unsigned, unsigned)
    {
    }

    void SkinWeights(float, float, float)
    {
    }

    void Vertex(pddiVector* v, pddiColour c) 
    {
        buffer->colour[0] = c.Red();
        buffer->colour[1] = c.Green();
        buffer->colour[2] = c.Blue();
        buffer->colour[3] = c.Alpha();
        buffer->coord[0] = v->x;
        buffer->coord[1] = v->y;
        buffer->coord[2] = v->z;
        Next();
    }

    void Vertex(pddiVector* v, pddiVector* n)
    {
        buffer->normal[0] = n->x;
        buffer->normal[1] = n->y;
        buffer->normal[2] = n->z;
        buffer->coord[0] = v->x;
        buffer->coord[1] = v->y;
        buffer->coord[2] = v->z;
        Next();
    }

    void Vertex(pddiVector* v, pddiVector2* uv)
    {
        buffer->uv[0] = uv->u;
        buffer->uv[1] = uv->v;
        buffer->coord[0] = v->x;
        buffer->coord[1] = v->y;
        buffer->coord[2] = v->z;
        Next();
    }

    void Vertex(pddiVector* v, pddiColour c, pddiVector2* uv)
    {
        buffer->colour[0] = c.Red();
        buffer->colour[1] = c.Green();
        buffer->colour[2] = c.Blue();
        buffer->colour[3] = c.Alpha();
        buffer->uv[0] = uv->u;
        buffer->uv[1] = uv->v;
        buffer->coord[0] = v->x;
        buffer->coord[1] = v->y;
        buffer->coord[2] = v->z;
        Next();
    }

    void Vertex(pddiVector* v, pddiVector* n, pddiVector2* uv)
    {
        buffer->normal[0] = n->x;
        buffer->normal[1] = n->y;
        buffer->normal[2] = n->z;
        buffer->uv[0] = uv->u;
        buffer->uv[1] = uv->v;
        buffer->coord[0] = v->x;
        buffer->coord[1] = v->y;
        buffer->coord[2] = v->z;
        Next();
    }

    bool CheckMemImageVersion(int version) { return false; }
    void* GetMemImagePtr()                 { return NULL; }
    unsigned GetMemImageLength()           { return 0; }

};

pglPrimBuffer::pglPrimBuffer(pglContext* c, pddiPrimType type, unsigned vertexFormat, int nVertex, int nIndex) : context(c)
{
    stream = new pglPrimBufferStream(this);

    total = allocated = stride = nStrips = 0;
    coord = normal = uv = NULL;
    colour = NULL;
    strips = NULL;
    indices = NULL;

    valid = false;
#ifdef RAD_ANDROID
    environmentNormalsAttempted = false;
    environmentNormalsAvailable = false;
    environmentOutlineNormalsAttempted = false;
    environmentOutlineNormalsAvailable = false;
    environmentOutlineNormalsUploaded = false;
    environmentOutlineNormals = NULL;
    environmentOutlineNormalBuffer = 0;
    environmentOutlineSmoothedGroups = 0;
    environmentOutlineSmoothedVertices = 0;
    environmentOutlineCreaseSeparatedPairs = 0;
    environmentWindingDiagnosticsReported = false;
    environmentMeshQualityDiagnosticsReported = false;
    environmentOutlineDiagnosticsReported = false;
    environmentDiagnosticMeshId = ++gSHAREnvironmentDiagnosticMeshId;
#endif
    vertexBuffer = indexBuffer = vertexArray = 0;

    primType = type;
    vertexType = vertexFormat;

    allocated = nVertex;
    
    stride = 36;

    mem = stride * nVertex;
    buffer = new unsigned char[mem];

    unsigned char* ptr = buffer;
    coord = (float*)ptr;
    ptr += 12;
    
    #ifdef RAD_ANDROID
    // Reserve the existing unused 12-byte vertex slot for generated normals.
    normal = (float*)ptr;
    ptr += 12;
    #else
    if(vertexFormat & PDDI_V_NORMAL)
    {
        normal = (float*)ptr;
        ptr += 12;
    }
    #endif
    
    if(vertexFormat & 0xf)
    {
        uv = (float*)ptr;
        ptr += 8;
    }
    
    if(vertexFormat & PDDI_V_COLOUR)
    {
        colour = ptr;
        ptr += 4;
    }

    indexCount = nIndex;
    if(indexCount) 
        indices = new unsigned short[indexCount];

    nStrips = 0;
    strips = NULL;

    context->ADD_STAT(PDDI_STAT_BUFFERED_COUNT, 1);
    context->ADD_STAT(PDDI_STAT_BUFFERED_ALLOC, mem / 1024.0f);
}

pglPrimBuffer::~pglPrimBuffer()
{
    delete stream;

    delete [] strips;
    delete [] indices;
    delete [] buffer;

    context->ADD_STAT(PDDI_STAT_BUFFERED_COUNT, -1);
    context->ADD_STAT(PDDI_STAT_BUFFERED_ALLOC, -mem / 1024.0f);

    GLuint buffers[] = { vertexBuffer, indexBuffer };
    glDeleteBuffers(2, buffers);
#ifdef RAD_ANDROID
    if (environmentOutlineNormalBuffer)
        glDeleteBuffers(1, &environmentOutlineNormalBuffer);
    delete [] environmentOutlineNormals;
#endif
    glDeleteVertexArraysOES(1, &vertexArray);
}

pddiPrimBufferStream* pglPrimBuffer::Lock()
{
    total = 0;
    return stream;
}

void pglPrimBuffer::Unlock(pddiPrimBufferStream* stream)
{
    if(coord)
        coord = (float*)((char*)coord - total * stride);

    if(normal)
        normal = (float*)((char*)normal - total * stride);

    if(uv)
        uv = (float*)((char*)uv - total * stride);

    if(colour)
        colour -= total * stride;

#ifdef RAD_ANDROID
    environmentNormalsAttempted = false;
    environmentNormalsAvailable = false;
    environmentOutlineNormalsAttempted = false;
    environmentOutlineNormalsAvailable = false;
    environmentOutlineNormalsUploaded = false;
    environmentOutlineSmoothedGroups = 0;
    environmentOutlineSmoothedVertices = 0;
    environmentOutlineCreaseSeparatedPairs = 0;
    delete [] environmentOutlineNormals;
    environmentOutlineNormals = NULL;
#endif
    valid = false;
}

unsigned char* pglPrimBuffer::LockIndexBuffer()
{
    PDDIASSERT(0);
    return NULL;
}

void pglPrimBuffer::UnlockIndexBuffer(int count)
{
    PDDIASSERT(0);
}

void pglPrimBuffer::SetIndices(unsigned short* i, int count)
{
    PDDIASSERT(count <= (int)indexCount);
    memcpy(indices, i, count * sizeof(unsigned short));
#ifdef RAD_ANDROID
    environmentNormalsAttempted = false;
    environmentNormalsAvailable = false;
    environmentOutlineNormalsAttempted = false;
    environmentOutlineNormalsAvailable = false;
    environmentOutlineNormalsUploaded = false;
    environmentOutlineSmoothedGroups = 0;
    environmentOutlineSmoothedVertices = 0;
    environmentOutlineCreaseSeparatedPairs = 0;
    delete [] environmentOutlineNormals;
    environmentOutlineNormals = NULL;
#endif
    valid = false;
}

#ifdef RAD_ANDROID
bool pglPrimBuffer::DiagnoseEnvironmentWinding()
{
    if (environmentWindingDiagnosticsReported)
        return true;

    environmentWindingDiagnosticsReported = true;
    if (coord == NULL || allocated < 3)
        return false;

    const int elementCount = (indexCount > 0 && indices != NULL)
        ? (int)indexCount : (int)total;
    if (elementCount < 3)
        return false;

    struct EdgeUseCounts
    {
        unsigned forward;
        unsigned reverse;
        EdgeUseCounts() : forward(0), reverse(0) {}
    };

    std::unordered_map<uint64_t, EdgeUseCounts> edgeUses;
    edgeUses.reserve((size_t)elementCount);

    unsigned validTriangles = 0;
    unsigned degenerateTriangles = 0;
    unsigned invalidIndexTriangles = 0;

    const auto addEdge = [&edgeUses](unsigned a, unsigned b)
    {
        const unsigned low = a < b ? a : b;
        const unsigned high = a < b ? b : a;
        const uint64_t key = (static_cast<uint64_t>(low) << 32) |
                             static_cast<uint64_t>(high);
        EdgeUseCounts& use = edgeUses[key];
        if (a < b)
            ++use.forward;
        else
            ++use.reverse;
    };

    const auto addTriangle = [this, &addEdge, &validTriangles,
                              &degenerateTriangles, &invalidIndexTriangles]
                             (unsigned a, unsigned b, unsigned c)
    {
        if (a >= allocated || b >= allocated || c >= allocated)
        {
            ++invalidIndexTriangles;
            return;
        }
        if (a == b || b == c || a == c)
        {
            ++degenerateTriangles;
            return;
        }

        const float* pa = (const float*)((const unsigned char*)coord + a * stride);
        const float* pb = (const float*)((const unsigned char*)coord + b * stride);
        const float* pc = (const float*)((const unsigned char*)coord + c * stride);
        const float abx = pb[0] - pa[0], aby = pb[1] - pa[1], abz = pb[2] - pa[2];
        const float acx = pc[0] - pa[0], acy = pc[1] - pa[1], acz = pc[2] - pa[2];
        const float nx = aby * acz - abz * acy;
        const float ny = abz * acx - abx * acz;
        const float nz = abx * acy - aby * acx;
        if (nx * nx + ny * ny + nz * nz <= 1.0e-12f)
        {
            ++degenerateTriangles;
            return;
        }

        ++validTriangles;
        addEdge(a, b);
        addEdge(b, c);
        addEdge(c, a);
    };

    if (primType == PDDI_PRIM_TRIANGLES)
    {
        for (int i = 0; i + 2 < elementCount; i += 3)
            addTriangle(indices ? indices[i] : (unsigned)i,
                        indices ? indices[i + 1] : (unsigned)(i + 1),
                        indices ? indices[i + 2] : (unsigned)(i + 2));
    }
    else if (primType == PDDI_PRIM_TRISTRIP)
    {
        for (int i = 0; i + 2 < elementCount; ++i)
        {
            unsigned a = indices ? indices[i] : (unsigned)i;
            unsigned b = indices ? indices[i + 1] : (unsigned)(i + 1);
            const unsigned c = indices ? indices[i + 2] : (unsigned)(i + 2);
            if (i & 1) { const unsigned swap = a; a = b; b = swap; }
            addTriangle(a, b, c);
        }
    }
    else
    {
        return false;
    }

    unsigned boundaryEdges = 0;
    unsigned sharedEdges = 0;
    unsigned windingConflictEdges = 0;
    unsigned nonManifoldEdges = 0;
    for (std::unordered_map<uint64_t, EdgeUseCounts>::const_iterator it = edgeUses.begin();
         it != edgeUses.end(); ++it)
    {
        const unsigned totalUses = it->second.forward + it->second.reverse;
        if (totalUses == 1)
            ++boundaryEdges;
        else if (totalUses >= 2)
        {
            ++sharedEdges;
            if (it->second.forward == 0 || it->second.reverse == 0)
                ++windingConflictEdges;
            if (totalUses > 2)
                ++nonManifoldEdges;
        }
    }

    static unsigned reportedEnvironmentMeshes = 0;
    if (reportedEnvironmentMeshes < 96)
    {
        SDL_Log("[SHAR Outline Winding] mesh=%p prim=%d vertexType=0x%X "
                "vertices=%u elements=%d triangles=%u boundaryEdges=%u "
                "sharedEdges=%u sameDirectionSharedEdges=%u nonManifoldEdges=%u "
                "degenerateTriangles=%u invalidIndexTriangles=%u",
                static_cast<void*>(this), (int)primType, vertexType,
                allocated, elementCount, validTriangles, boundaryEdges,
                sharedEdges, windingConflictEdges, nonManifoldEdges,
                degenerateTriangles, invalidIndexTriangles);
        ++reportedEnvironmentMeshes;
    }

    return true;
}

bool pglPrimBuffer::DiagnoseEnvironmentMeshQuality()
{
    if (environmentMeshQualityDiagnosticsReported)
        return true;
    environmentMeshQualityDiagnosticsReported = true;

    if (coord == NULL || normal == NULL || allocated < 3)
        return false;

    const int elementCount = (indexCount > 0 && indices != NULL)
        ? (int)indexCount : (int)total;
    if (elementCount < 3)
        return false;

    struct QuantizedPoint
    {
        int64_t x, y, z;
        bool operator==(const QuantizedPoint& other) const
        {
            return x == other.x && y == other.y && z == other.z;
        }
    };
    struct QuantizedPointHash
    {
        size_t operator()(const QuantizedPoint& p) const
        {
            uint64_t hash = 1469598103934665603ULL;
            const int64_t values[3] = { p.x, p.y, p.z };
            for (int i = 0; i < 3; ++i)
            {
                const uint64_t value = static_cast<uint64_t>(values[i]);
                for (unsigned byte = 0; byte < 8; ++byte)
                {
                    hash ^= (value >> (byte * 8)) & 0xffULL;
                    hash *= 1099511628211ULL;
                }
            }
            return static_cast<size_t>(hash ^ (hash >> 32));
        }
    };
    struct GeometricEdge
    {
        QuantizedPoint low;
        QuantizedPoint high;
        bool operator==(const GeometricEdge& other) const
        {
            return low == other.low && high == other.high;
        }
    };
    struct GeometricEdgeHash
    {
        size_t operator()(const GeometricEdge& edge) const
        {
            QuantizedPointHash pointHash;
            const uint64_t a = static_cast<uint64_t>(pointHash(edge.low));
            const uint64_t b = static_cast<uint64_t>(pointHash(edge.high));
            const uint64_t combined = (a * 0x9e3779b185ebca87ULL) ^
                                      (b + 0x9e3779b97f4a7c15ULL);
            return static_cast<size_t>(combined ^ (combined >> 32));
        }
    };
    struct GeometricEdgeUse
    {
        unsigned forward;
        unsigned reverse;
        std::unordered_set<uint64_t> indexedVariants;
        GeometricEdgeUse() : forward(0), reverse(0) {}
    };
    struct NormalAccumulation
    {
        double x, y, z;
        double totalMagnitude;
        unsigned contributions;
        NormalAccumulation() : x(0.0), y(0.0), z(0.0),
                               totalMagnitude(0.0), contributions(0) {}
    };
    struct PositionNormalGroup
    {
        float referenceX, referenceY, referenceZ;
        float minDotToFirstCopy;
        unsigned count;
        PositionNormalGroup()
            : referenceX(0.0f), referenceY(0.0f), referenceZ(0.0f),
              minDotToFirstCopy(1.0f), count(0) {}
    };

    const auto quantizePoint = [](const float* position, QuantizedPoint* point) -> bool
    {
        if (!isfinite(position[0]) || !isfinite(position[1]) || !isfinite(position[2]))
            return false;
        // Quantize local positions to 0.0001 units to reconnect duplicated seam vertices.
        point->x = static_cast<int64_t>(llround(static_cast<double>(position[0]) * 10000.0));
        point->y = static_cast<int64_t>(llround(static_cast<double>(position[1]) * 10000.0));
        point->z = static_cast<int64_t>(llround(static_cast<double>(position[2]) * 10000.0));
        return true;
    };
    const auto pointLess = [](const QuantizedPoint& a, const QuantizedPoint& b) -> bool
    {
        if (a.x != b.x) return a.x < b.x;
        if (a.y != b.y) return a.y < b.y;
        return a.z < b.z;
    };

    std::unordered_map<GeometricEdge, GeometricEdgeUse, GeometricEdgeHash> geometricEdges;
    std::unordered_map<QuantizedPoint, unsigned, QuantizedPointHash> vertexPositionUses;
    std::unordered_map<QuantizedPoint, PositionNormalGroup, QuantizedPointHash> positionNormalGroups;
    geometricEdges.reserve(static_cast<size_t>(elementCount));
    vertexPositionUses.reserve(static_cast<size_t>(allocated));
    positionNormalGroups.reserve(static_cast<size_t>(allocated));
    std::vector<NormalAccumulation> normalAccumulations(allocated);
    unsigned zeroLengthActualNormals = 0;

    // Stable FNV-1a ID over quantized positions and the primitive index stream.
    // Unlike a pointer, this can identify equivalent mesh buffers in separate runs.
    uint64_t fingerprint = 1469598103934665603ULL;
    const auto hashValue = [&fingerprint](uint64_t value)
    {
        for (unsigned byte = 0; byte < 8; ++byte)
        {
            fingerprint ^= (value >> (byte * 8)) & 0xffULL;
            fingerprint *= 1099511628211ULL;
        }
    };
    hashValue(static_cast<uint64_t>(primType));
    hashValue(static_cast<uint64_t>(vertexType));
    hashValue(static_cast<uint64_t>(allocated));
    hashValue(static_cast<uint64_t>(elementCount));

    for (unsigned i = 0; i < allocated; ++i)
    {
        const float* position = (const float*)((const unsigned char*)coord + i * stride);
        QuantizedPoint point;
        if (quantizePoint(position, &point))
        {
            hashValue(static_cast<uint64_t>(point.x));
            hashValue(static_cast<uint64_t>(point.y));
            hashValue(static_cast<uint64_t>(point.z));
            ++vertexPositionUses[point];

            // Compare the actual normal attribute consumed by the outline shader
            // for vertex indices that occupy the same quantized local position.
            const float* sourceNormal =
                (const float*)((const unsigned char*)normal + i * stride);
            const float normalLengthSquared =
                sourceNormal[0] * sourceNormal[0] +
                sourceNormal[1] * sourceNormal[1] +
                sourceNormal[2] * sourceNormal[2];
            if (isfinite(sourceNormal[0]) && isfinite(sourceNormal[1]) &&
                isfinite(sourceNormal[2]) && normalLengthSquared > 1.0e-12f)
            {
                const float inverseLength = 1.0f / sqrtf(normalLengthSquared);
                const float nx = sourceNormal[0] * inverseLength;
                const float ny = sourceNormal[1] * inverseLength;
                const float nz = sourceNormal[2] * inverseLength;
                PositionNormalGroup& group = positionNormalGroups[point];
                if (group.count == 0)
                {
                    group.referenceX = nx;
                    group.referenceY = ny;
                    group.referenceZ = nz;
                }
                else
                {
                    const float dot = nx * group.referenceX +
                                      ny * group.referenceY +
                                      nz * group.referenceZ;
                    group.minDotToFirstCopy = fminf(group.minDotToFirstCopy, dot);
                }
                ++group.count;
            }
            else
            {
                ++zeroLengthActualNormals;
            }
        }
        else
        {
            hashValue(0xffffffffffffffffULL);
            hashValue(0xfffffffffffffffeULL);
            hashValue(0xfffffffffffffffdULL);
        }
    }
    for (int i = 0; i < elementCount; ++i)
    {
        const unsigned index = (indices != NULL) ? indices[i] : static_cast<unsigned>(i);
        hashValue(static_cast<uint64_t>(index));
    }

    unsigned validTriangles = 0;
    unsigned degenerateTriangles = 0;
    unsigned invalidIndexTriangles = 0;
    unsigned invalidPositionTriangles = 0;
    unsigned collapsedPositionEdges = 0;

    const auto addGeometricEdge = [&geometricEdges, &pointLess, &collapsedPositionEdges]
        (unsigned indexA, unsigned indexB,
         const QuantizedPoint& pointA, const QuantizedPoint& pointB)
    {
        if (pointA == pointB)
        {
            ++collapsedPositionEdges;
            return;
        }
        GeometricEdge edge;
        const bool forward = pointLess(pointA, pointB);
        if (forward)
        {
            edge.low = pointA;
            edge.high = pointB;
        }
        else
        {
            edge.low = pointB;
            edge.high = pointA;
        }
        GeometricEdgeUse& use = geometricEdges[edge];
        if (forward) ++use.forward;
        else ++use.reverse;

        const unsigned lowIndex = indexA < indexB ? indexA : indexB;
        const unsigned highIndex = indexA < indexB ? indexB : indexA;
        const uint64_t indexEdge = (static_cast<uint64_t>(lowIndex) << 32) |
                                   static_cast<uint64_t>(highIndex);
        use.indexedVariants.insert(indexEdge);
    };

    const auto addTriangle = [this, &quantizePoint, &normalAccumulations,
                              &addGeometricEdge, &validTriangles,
                              &degenerateTriangles, &invalidIndexTriangles,
                              &invalidPositionTriangles]
                             (unsigned a, unsigned b, unsigned c)
    {
        if (a >= allocated || b >= allocated || c >= allocated)
        {
            ++invalidIndexTriangles;
            return;
        }
        if (a == b || b == c || a == c)
        {
            ++degenerateTriangles;
            return;
        }

        const float* pa = (const float*)((const unsigned char*)coord + a * stride);
        const float* pb = (const float*)((const unsigned char*)coord + b * stride);
        const float* pc = (const float*)((const unsigned char*)coord + c * stride);
        QuantizedPoint qa, qb, qc;
        if (!quantizePoint(pa, &qa) || !quantizePoint(pb, &qb) ||
            !quantizePoint(pc, &qc))
        {
            ++invalidPositionTriangles;
            return;
        }

        const float abx = pb[0] - pa[0], aby = pb[1] - pa[1], abz = pb[2] - pa[2];
        const float acx = pc[0] - pa[0], acy = pc[1] - pa[1], acz = pc[2] - pa[2];
        const double nx = static_cast<double>(aby) * acz - static_cast<double>(abz) * acy;
        const double ny = static_cast<double>(abz) * acx - static_cast<double>(abx) * acz;
        const double nz = static_cast<double>(abx) * acy - static_cast<double>(aby) * acx;
        const double lengthSquared = nx * nx + ny * ny + nz * nz;
        if (lengthSquared <= 1.0e-12)
        {
            ++degenerateTriangles;
            return;
        }

        const double magnitude = sqrt(lengthSquared);
        const unsigned vertexIndices[3] = { a, b, c };
        for (int vertex = 0; vertex < 3; ++vertex)
        {
            NormalAccumulation& accumulation = normalAccumulations[vertexIndices[vertex]];
            accumulation.x += nx;
            accumulation.y += ny;
            accumulation.z += nz;
            accumulation.totalMagnitude += magnitude;
            ++accumulation.contributions;
        }
        addGeometricEdge(a, b, qa, qb);
        addGeometricEdge(b, c, qb, qc);
        addGeometricEdge(c, a, qc, qa);
        ++validTriangles;
    };

    if (primType == PDDI_PRIM_TRIANGLES)
    {
        for (int i = 0; i + 2 < elementCount; i += 3)
            addTriangle(indices ? indices[i] : static_cast<unsigned>(i),
                        indices ? indices[i + 1] : static_cast<unsigned>(i + 1),
                        indices ? indices[i + 2] : static_cast<unsigned>(i + 2));
    }
    else if (primType == PDDI_PRIM_TRISTRIP)
    {
        for (int i = 0; i + 2 < elementCount; ++i)
        {
            unsigned a = indices ? indices[i] : static_cast<unsigned>(i);
            unsigned b = indices ? indices[i + 1] : static_cast<unsigned>(i + 1);
            const unsigned c = indices ? indices[i + 2] : static_cast<unsigned>(i + 2);
            if (i & 1) { const unsigned swap = a; a = b; b = swap; }
            addTriangle(a, b, c);
        }
    }
    else
    {
        return false;
    }

    unsigned duplicatePositionVertices = 0;
    for (std::unordered_map<QuantizedPoint, unsigned, QuantizedPointHash>::const_iterator it =
             vertexPositionUses.begin(); it != vertexPositionUses.end(); ++it)
    {
        if (it->second > 1)
            duplicatePositionVertices += it->second - 1;
    }

    unsigned duplicateNormalGroups = 0;
    unsigned normalMismatchGroups5Deg = 0;
    unsigned normalMismatchGroups15Deg = 0;
    unsigned normalMismatchGroups30Deg = 0;
    unsigned normalMismatchGroups60Deg = 0;
    double maximumNormalAngleFromFirstCopy = 0.0;
    double sumMaximumNormalAngleFromFirstCopy = 0.0;
    for (std::unordered_map<QuantizedPoint, PositionNormalGroup, QuantizedPointHash>::const_iterator it =
             positionNormalGroups.begin(); it != positionNormalGroups.end(); ++it)
    {
        const PositionNormalGroup& group = it->second;
        if (group.count < 2)
            continue;

        const double dot = fmax(-1.0, fmin(1.0, (double)group.minDotToFirstCopy));
        const double angleDegrees = acos(dot) * 57.29577951308232;
        ++duplicateNormalGroups;
        if (angleDegrees > 5.0) ++normalMismatchGroups5Deg;
        if (angleDegrees > 15.0) ++normalMismatchGroups15Deg;
        if (angleDegrees > 30.0) ++normalMismatchGroups30Deg;
        if (angleDegrees > 60.0) ++normalMismatchGroups60Deg;
        if (angleDegrees > maximumNormalAngleFromFirstCopy)
            maximumNormalAngleFromFirstCopy = angleDegrees;
        sumMaximumNormalAngleFromFirstCopy += angleDegrees;
    }
    const double averageMaximumNormalAngleFromFirstCopy = duplicateNormalGroups > 0
        ? sumMaximumNormalAngleFromFirstCopy / duplicateNormalGroups : 0.0;

    unsigned geometricBoundaryEdges = 0;
    unsigned geometricSharedEdges = 0;
    unsigned geometricWindingConflicts = 0;
    unsigned geometricNonManifoldEdges = 0;
    unsigned splitIndexEdges = 0;
    for (std::unordered_map<GeometricEdge, GeometricEdgeUse, GeometricEdgeHash>::const_iterator it =
             geometricEdges.begin(); it != geometricEdges.end(); ++it)
    {
        const unsigned totalUses = it->second.forward + it->second.reverse;
        if (totalUses == 1) ++geometricBoundaryEdges;
        else if (totalUses == 2) ++geometricSharedEdges;
        else if (totalUses > 2) ++geometricNonManifoldEdges;
        if (totalUses >= 2 &&
            (it->second.forward == 0 || it->second.reverse == 0))
            ++geometricWindingConflicts;
        if (totalUses >= 2 && it->second.indexedVariants.size() > 1)
            ++splitIndexEdges;
    }

    // This describes triangle-derived face-normal coherence per vertex index;
    // it is separate from the actual attribute comparison across duplicate positions.
    unsigned normalVertices = 0;
    unsigned lowCoherenceVertices = 0;
    unsigned mediumCoherenceVertices = 0;
    unsigned zeroResultantVertices = 0;
    double coherenceSum = 0.0;
    double minimumCoherence = 1.0;
    for (unsigned i = 0; i < allocated; ++i)
    {
        const NormalAccumulation& accumulation = normalAccumulations[i];
        if (accumulation.contributions == 0 || accumulation.totalMagnitude <= 0.0)
            continue;
        const double resultant = sqrt(accumulation.x * accumulation.x +
                                       accumulation.y * accumulation.y +
                                       accumulation.z * accumulation.z);
        double coherence = resultant / accumulation.totalMagnitude;
        if (coherence > 1.0) coherence = 1.0;
        if (coherence < 0.0) coherence = 0.0;
        ++normalVertices;
        coherenceSum += coherence;
        if (coherence < minimumCoherence) minimumCoherence = coherence;
        if (coherence < 0.25) ++lowCoherenceVertices;
        else if (coherence < 0.50) ++mediumCoherenceVertices;
        if (resultant <= 1.0e-8) ++zeroResultantVertices;
    }

    const double averageCoherence = normalVertices > 0
        ? coherenceSum / normalVertices : 0.0;
    if (normalVertices == 0) minimumCoherence = 0.0;

    SDL_Log("[SHAR Outline MeshQuality] meshId=%u mesh=%p fingerprint=%016llX "
            "normalSource=%s prim=%d vertexType=0x%X vertices=%u elements=%d "
            "triangles=%u degenerateTriangles=%u invalidIndexTriangles=%u "
            "invalidPositionTriangles=%u duplicatePositionVertices=%u "
            "actualZeroLengthNormals=%u duplicateNormalGroups=%u "
            "normalMismatchGroups(>5deg)=%u (>15deg)=%u (>30deg)=%u (>60deg)=%u "
            "maxAngleFromFirstCopy=%.2f avgMaxAngleFromFirstCopy=%.2f "
            "faceAccumulationVertices=%u lowFaceCoherence(<0.25)=%u mediumFaceCoherence(<0.50)=%u "
            "zeroResultantFaceNormals=%u avgIndexFaceCoherence=%.4f minIndexFaceCoherence=%.4f "
            "geoBoundaryEdges=%u geoSharedEdges=%u geoWindingConflicts=%u "
            "geoNonManifoldEdges=%u splitIndexEdges=%u collapsedPositionEdges=%u "
            "outlineSmoothedPositionGroups=%u outlineSmoothedVertices=%u "
            "outlineCreaseSeparatedPairs=%u positionTolerance=0.0001",
            environmentDiagnosticMeshId, static_cast<void*>(this),
            static_cast<unsigned long long>(fingerprint),
            (vertexType & PDDI_V_NORMAL) ? "authored" : "generated",
            (int)primType, vertexType, allocated, elementCount, validTriangles,
            degenerateTriangles, invalidIndexTriangles, invalidPositionTriangles,
            duplicatePositionVertices, zeroLengthActualNormals, duplicateNormalGroups,
            normalMismatchGroups5Deg, normalMismatchGroups15Deg,
            normalMismatchGroups30Deg, normalMismatchGroups60Deg,
            maximumNormalAngleFromFirstCopy, averageMaximumNormalAngleFromFirstCopy,
            normalVertices, lowCoherenceVertices, mediumCoherenceVertices,
            zeroResultantVertices, averageCoherence, minimumCoherence,
            geometricBoundaryEdges, geometricSharedEdges, geometricWindingConflicts,
            geometricNonManifoldEdges, splitIndexEdges, collapsedPositionEdges,
            environmentOutlineSmoothedGroups, environmentOutlineSmoothedVertices,
            environmentOutlineCreaseSeparatedPairs);

    return true;
}

bool pglPrimBuffer::GenerateEnvironmentNormals()
{
    if (environmentNormalsAttempted)
        return environmentNormalsAvailable;
    environmentNormalsAttempted = true;
    if (normal == NULL || coord == NULL || allocated < 3 ||
        (primType != PDDI_PRIM_TRIANGLES && primType != PDDI_PRIM_TRISTRIP))
        return false;

    const int elementCount = (indexCount > 0 && indices != NULL)
        ? (int)indexCount : (int)total;
    if (elementCount < 3)
        return false;

    // Coordinates and normals are interleaved at the renderer's 36-byte stride.
    for (unsigned i = 0; i < allocated; ++i)
    {
        float* n = (float*)((unsigned char*)normal + i * stride);
        n[0] = n[1] = n[2] = 0.0f;
    }

    float fallbackX = 0.0f, fallbackY = 1.0f, fallbackZ = 0.0f;
    bool haveFallback = false;
    unsigned validTriangles = 0;

    const auto addTriangle = [this, &fallbackX, &fallbackY, &fallbackZ,
                              &haveFallback, &validTriangles](unsigned a, unsigned b, unsigned c)
    {
        if (a >= allocated || b >= allocated || c >= allocated ||
            a == b || b == c || a == c)
            return;

        const float* pa = (const float*)((const unsigned char*)coord + a * stride);
        const float* pb = (const float*)((const unsigned char*)coord + b * stride);
        const float* pc = (const float*)((const unsigned char*)coord + c * stride);
        const float abx = pb[0] - pa[0], aby = pb[1] - pa[1], abz = pb[2] - pa[2];
        const float acx = pc[0] - pa[0], acy = pc[1] - pa[1], acz = pc[2] - pa[2];
        const float nx = aby * acz - abz * acy;
        const float ny = abz * acx - abx * acz;
        const float nz = abx * acy - aby * acx;
        const float lengthSquared = nx * nx + ny * ny + nz * nz;
        if (lengthSquared <= 1.0e-12f)
            return;

        float* na = (float*)((unsigned char*)normal + a * stride);
        float* nb = (float*)((unsigned char*)normal + b * stride);
        float* nc = (float*)((unsigned char*)normal + c * stride);
        na[0] += nx; na[1] += ny; na[2] += nz;
        nb[0] += nx; nb[1] += ny; nb[2] += nz;
        nc[0] += nx; nc[1] += ny; nc[2] += nz;

        if (!haveFallback)
        {
            const float inverseLength = 1.0f / sqrtf(lengthSquared);
            fallbackX = nx * inverseLength;
            fallbackY = ny * inverseLength;
            fallbackZ = nz * inverseLength;
            haveFallback = true;
        }
        ++validTriangles;
    };

    if (primType == PDDI_PRIM_TRIANGLES)
    {
        for (int i = 0; i + 2 < elementCount; i += 3)
            addTriangle(indices ? indices[i] : (unsigned)i,
                        indices ? indices[i + 1] : (unsigned)(i + 1),
                        indices ? indices[i + 2] : (unsigned)(i + 2));
    }
    else
    {
        // Alternation is based on index position, even across degenerate joins.
        for (int i = 0; i + 2 < elementCount; ++i)
        {
            unsigned a = indices ? indices[i] : (unsigned)i;
            unsigned b = indices ? indices[i + 1] : (unsigned)(i + 1);
            const unsigned c = indices ? indices[i + 2] : (unsigned)(i + 2);
            if (i & 1) { const unsigned swap = a; a = b; b = swap; }
            addTriangle(a, b, c);
        }
    }

    if (validTriangles == 0 || !haveFallback)
        return false;

    for (unsigned i = 0; i < allocated; ++i)
    {
        float* n = (float*)((unsigned char*)normal + i * stride);
        const float lengthSquared = n[0] * n[0] + n[1] * n[1] + n[2] * n[2];
        if (lengthSquared > 1.0e-12f)
        {
            const float inverseLength = 1.0f / sqrtf(lengthSquared);
            n[0] *= inverseLength; n[1] *= inverseLength; n[2] *= inverseLength;
        }
        else
        {
            n[0] = fallbackX; n[1] = fallbackY; n[2] = fallbackZ;
        }
    }

    environmentNormalsAvailable = true;
    valid = false;
    return true;
}

bool pglPrimBuffer::GenerateEnvironmentOutlineNormals()
{
    if (environmentOutlineNormalsAttempted)
        return environmentOutlineNormalsAvailable;
    environmentOutlineNormalsAttempted = true;
    environmentOutlineNormalsAvailable = false;
    environmentOutlineSmoothedGroups = 0;
    environmentOutlineSmoothedVertices = 0;
    environmentOutlineCreaseSeparatedPairs = 0;

    if (normal == NULL || coord == NULL || allocated < 3 ||
        (vertexType & PDDI_V_NORMAL) ||
        !environmentNormalsAvailable)
        return false;

    float* outlineData = new float[static_cast<size_t>(allocated) * 3];
    if (outlineData == NULL)
        return false;

    // Start with the exact generated normals used by the existing environment
    // outline. Only the separate outline copy is modified below; the original
    // interleaved vertex normal data remains untouched for ordinary rendering.
    for (unsigned i = 0; i < allocated; ++i)
    {
        const float* source = (const float*)((const unsigned char*)normal + i * stride);
        outlineData[i * 3] = source[0];
        outlineData[i * 3 + 1] = source[1];
        outlineData[i * 3 + 2] = source[2];
    }

    struct OutlinePosition
    {
        int64_t x, y, z;
        bool operator==(const OutlinePosition& other) const
        {
            return x == other.x && y == other.y && z == other.z;
        }
    };
    struct OutlinePositionHash
    {
        size_t operator()(const OutlinePosition& p) const
        {
            uint64_t hash = 1469598103934665603ULL;
            const int64_t values[3] = { p.x, p.y, p.z };
            for (int i = 0; i < 3; ++i)
            {
                const uint64_t value = static_cast<uint64_t>(values[i]);
                for (unsigned byte = 0; byte < 8; ++byte)
                {
                    hash ^= (value >> (byte * 8)) & 0xffULL;
                    hash *= 1099511628211ULL;
                }
            }
            return static_cast<size_t>(hash ^ (hash >> 32));
        }
    };

    std::unordered_map<OutlinePosition, std::vector<unsigned>, OutlinePositionHash> positionVertices;
    positionVertices.reserve(allocated);
    for (unsigned i = 0; i < allocated; ++i)
    {
        const float* p = (const float*)((const unsigned char*)coord + i * stride);
        if (!isfinite(p[0]) || !isfinite(p[1]) || !isfinite(p[2]))
            continue;

        OutlinePosition key;
        key.x = static_cast<int64_t>(llround(static_cast<double>(p[0]) * 10000.0));
        key.y = static_cast<int64_t>(llround(static_cast<double>(p[1]) * 10000.0));
        key.z = static_cast<int64_t>(llround(static_cast<double>(p[2]) * 10000.0));
        positionVertices[key].push_back(i);
    }

    // A 60-degree crease threshold is deliberately conservative: copies that
    // belong to a smooth seam can share an outline direction, while sharper
    // geometric edges keep their independent directions.
    const float creaseCosine = 0.5f;
    for (std::unordered_map<OutlinePosition, std::vector<unsigned>, OutlinePositionHash>::const_iterator group =
             positionVertices.begin(); group != positionVertices.end(); ++group)
    {
        const std::vector<unsigned>& vertices = group->second;
        if (vertices.size() < 2)
            continue;

        bool groupChanged = false;
        for (size_t i = 0; i < vertices.size(); ++i)
        {
            const unsigned vertex = vertices[i];
            const float* base = (const float*)((const unsigned char*)normal + vertex * stride);
            if (!isfinite(base[0]) || !isfinite(base[1]) || !isfinite(base[2]))
                continue;

            float sumX = base[0];
            float sumY = base[1];
            float sumZ = base[2];
            unsigned compatibleCopies = 1;

            for (size_t j = 0; j < vertices.size(); ++j)
            {
                if (i == j)
                    continue;

                const unsigned otherVertex = vertices[j];
                const float* other = (const float*)((const unsigned char*)normal + otherVertex * stride);
                if (!isfinite(other[0]) || !isfinite(other[1]) || !isfinite(other[2]))
                    continue;

                const float dot = base[0] * other[0] +
                                  base[1] * other[1] +
                                  base[2] * other[2];
                if (dot >= creaseCosine)
                {
                    sumX += other[0];
                    sumY += other[1];
                    sumZ += other[2];
                    ++compatibleCopies;
                }
                else if (vertex < otherVertex)
                {
                    ++environmentOutlineCreaseSeparatedPairs;
                }
            }

            if (compatibleCopies < 2)
                continue;

            const float lengthSquared = sumX * sumX + sumY * sumY + sumZ * sumZ;
            if (lengthSquared <= 1.0e-12f)
                continue;

            const float inverseLength = 1.0f / sqrtf(lengthSquared);
            const float smoothX = sumX * inverseLength;
            const float smoothY = sumY * inverseLength;
            const float smoothZ = sumZ * inverseLength;
            const float alignment = base[0] * smoothX +
                                    base[1] * smoothY +
                                    base[2] * smoothZ;

            if (alignment < 0.99999f)
            {
                outlineData[vertex * 3] = smoothX;
                outlineData[vertex * 3 + 1] = smoothY;
                outlineData[vertex * 3 + 2] = smoothZ;
                ++environmentOutlineSmoothedVertices;
                groupChanged = true;
            }
        }

        if (groupChanged)
            ++environmentOutlineSmoothedGroups;
    }

    if (environmentOutlineSmoothedVertices == 0)
    {
        delete [] outlineData;
        return false;
    }

    environmentOutlineNormals = outlineData;
    environmentOutlineNormalsAvailable = true;
    environmentOutlineNormalsUploaded = false;
    return true;
}

#endif

void pglPrimBuffer::Display(void)
{
    MICROPROFILE_SCOPEI("PDDI", "pglPrimBuffer::Display", MP_RED);

#ifdef RAD_ANDROID
    bool useCreaseAwareEnvironmentOutlineNormals = false;
    const bool environmentOutlineGeometryPass =
        gSHAREnvironmentCelPassActive && gSHAREnvironmentCelMaterialEligible &&
        gCelShadingEnabled && gCelShadingObjectEnabled && gCelShadingOutlinePass;
    if (environmentOutlineGeometryPass)
    {
        LoadSHAREnvironmentOutlineDiagnostics();
        if (gSHAREnvironmentOutlineDiagnosticMode == 4 ||
            gSHAREnvironmentOutlineDiagnosticMode == 5)
            DiagnoseEnvironmentWinding();
    }

    if (environmentOutlineGeometryPass &&
        !(vertexType & PDDI_V_NORMAL) && !GenerateEnvironmentNormals())
    {
        static unsigned skippedEnvironmentOutlineBuffers = 0;
        if (gSHAREnvironmentOutlineDiagnosticMode != 0 &&
            skippedEnvironmentOutlineBuffers < 32)
        {
            SDL_Log("[SHAR Outline Diagnostics] mesh skipped: generated normals unavailable; "
                    "primType=%d vertexType=0x%X allocated=%u total=%u indices=%u",
                    (int)primType, vertexType, allocated, total, indexCount);
            ++skippedEnvironmentOutlineBuffers;
        }
        return;
    }

    if (environmentOutlineGeometryPass && !(vertexType & PDDI_V_NORMAL))
        useCreaseAwareEnvironmentOutlineNormals = GenerateEnvironmentOutlineNormals();

    // Run after both normal-generation stages so Mode 6 can report the original
    // generated normals alongside the number of outline-only crease corrections.
    if (environmentOutlineGeometryPass &&
        gSHAREnvironmentOutlineDiagnosticMode == 6)
        DiagnoseEnvironmentMeshQuality();

    if (environmentOutlineGeometryPass &&
        gSHAREnvironmentOutlineDiagnosticMode == 10 &&
        !environmentOutlineDiagnosticsReported)
    {
        SDL_Log("[SHAR Outline Mode 10] outline-mesh id=%u buffer=%p owner=%s drawable=%s ownerUID=%016llx sourceAssetUID=%016llx foliageCandidate=%d roadCandidate=%d primType=%d vertexType=0x%X allocated=%u total=%u indices=%u sourceNormals=%d generatedNormals=%d outlineNormals=%d smoothedGroups=%u smoothedVertices=%u creasePairs=%u",
                environmentDiagnosticMeshId,
                static_cast<void*>(this),
                gSHAREnvironmentDiagnosticOwnerName ? gSHAREnvironmentDiagnosticOwnerName : "<no-owner-context>",
                gSHAREnvironmentDiagnosticDrawableName ? gSHAREnvironmentDiagnosticDrawableName : "<no-drawable-context>",
                gSHAREnvironmentDiagnosticOwnerUID,
                gSHAREnvironmentDiagnosticSourceAssetUID,
                gSHAREnvironmentDiagnosticFoliageCandidate ? 1 : 0,
                gSHAREnvironmentDiagnosticRoadCandidate ? 1 : 0,
                (int)primType,
                vertexType,
                allocated,
                total,
                indexCount,
                (vertexType & PDDI_V_NORMAL) ? 1 : 0,
                environmentNormalsAvailable ? 1 : 0,
                environmentOutlineNormalsAvailable ? 1 : 0,
                environmentOutlineSmoothedGroups,
                environmentOutlineSmoothedVertices,
                environmentOutlineCreaseSeparatedPairs);
        environmentOutlineDiagnosticsReported = true;
    }
#endif

    if(!valid)
    {
        if(!vertexArray)
            glGenVertexArraysOES(1, &vertexArray);
        glBindVertexArrayOES(vertexArray);

        if(!vertexBuffer)
            glGenBuffers(1, &vertexBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        glBufferData(GL_ARRAY_BUFFER, mem, buffer, GL_STATIC_DRAW);

        if(indexCount && indices)
        {
            if(!indexBuffer)
                glGenBuffers(1, &indexBuffer);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,indexBuffer);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,indexCount*sizeof(unsigned short),indices,GL_STATIC_DRAW);
        }
        else
        {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,0);
        }

        GLintptr offset = 0;
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,stride,(void*)offset);
        offset += 12;

#ifdef RAD_ANDROID
        if ((vertexType & PDDI_V_NORMAL) || environmentNormalsAvailable)
        {
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,stride,(void*)offset);
        }
        else
        {
            glDisableVertexAttribArray(1);
            glVertexAttrib3f(1, 0.0f, 0.0f, 0.0f);
        }
        offset += 12;
#else
        if(vertexType & PDDI_V_NORMAL)
        {
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,stride,(void*)offset);
            offset += 12;
        }
        else
        {
            glDisableVertexAttribArray(1);
            glVertexAttrib3f(1, 0.0f, 0.0f, 0.0f);
        }
#endif

        if(vertexType & 0xf)
        {
            glEnableVertexAttribArray(2);
            glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,stride,(void*)offset);
            offset += 8;
        }
        else
        {
            glDisableVertexAttribArray(2);
            glVertexAttrib2f(2, 0.0f, 0.0f);
        }

        if(vertexType & PDDI_V_COLOUR)
        {
            glEnableVertexAttribArray(3);
            glVertexAttribPointer(3,4,GL_UNSIGNED_BYTE,GL_TRUE,stride,(void*)offset);
            offset += 4;
        }
        else
        {
            glDisableVertexAttribArray(3);
            glVertexAttrib4f(3, 1.0f, 1.0f, 1.0f, 1.0f);
        }
        valid = true;
    }
    else
    {
        glBindVertexArrayOES(vertexArray);
    }

#ifdef RAD_ANDROID
    // The outline-only normal stream is a separate attribute/VBO. During all
    // ordinary draws attribute 4 aliases the original interleaved normal slot.
    // This prevents crease-aware smoothing from altering the source normals.
    if (useCreaseAwareEnvironmentOutlineNormals)
    {
        if (!environmentOutlineNormalBuffer)
            glGenBuffers(1, &environmentOutlineNormalBuffer);
        glBindBuffer(GL_ARRAY_BUFFER, environmentOutlineNormalBuffer);
        if (!environmentOutlineNormalsUploaded)
        {
            glBufferData(GL_ARRAY_BUFFER,
                         allocated * 3 * sizeof(float),
                         environmentOutlineNormals, GL_STATIC_DRAW);
            environmentOutlineNormalsUploaded = true;
        }
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE,
                              3 * sizeof(float), (void*)0);
    }
    else if ((vertexType & PDDI_V_NORMAL) || environmentNormalsAvailable)
    {
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, (void*)12);
    }
    else
    {
        glBindBuffer(GL_ARRAY_BUFFER, vertexBuffer);
        glDisableVertexAttribArray(4);
        glVertexAttrib3f(4, 0.0f, 0.0f, 1.0f);
    }
#endif

#ifdef RAD_ANDROID
    // Apply diagnostic overrides only to the environment inverted-hull pass.
    // Each override is restored immediately so normal scene rendering and the
    // renderer's cached cull state remain unchanged.
    int outlineDiagnosticMode = 0;
    GLboolean depthTestWasEnabled = GL_FALSE;
    GLboolean cullWasEnabled = GL_FALSE;
    GLint cullFaceModeWas = GL_BACK;
    const bool isEnvironmentOutlinePass =
        gSHAREnvironmentCelPassActive && gSHAREnvironmentCelMaterialEligible &&
        gCelShadingEnabled && gCelShadingObjectEnabled && gCelShadingOutlinePass;
    if (isEnvironmentOutlinePass)
    {
        LoadSHAREnvironmentOutlineDiagnostics();
        outlineDiagnosticMode = gSHAREnvironmentOutlineDiagnosticMode;

        if (outlineDiagnosticMode == 1 || outlineDiagnosticMode == 3)
        {
            depthTestWasEnabled = glIsEnabled(GL_DEPTH_TEST);
            if (depthTestWasEnabled)
                glDisable(GL_DEPTH_TEST);
        }

        if (outlineDiagnosticMode == 2 || outlineDiagnosticMode == 3)
        {
            cullWasEnabled = glIsEnabled(GL_CULL_FACE);
            if (cullWasEnabled)
                glDisable(GL_CULL_FACE);
        }
        else if (outlineDiagnosticMode == 4)
        {
            cullWasEnabled = glIsEnabled(GL_CULL_FACE);
            glGetIntegerv(GL_CULL_FACE_MODE, &cullFaceModeWas);
            // Inverted-hull outlines should expose the reverse-facing shell.
            // This diagnostic culls front faces instead of the normal back faces.
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
        }
    }
#endif

    if (environmentOutlineGeometryPass &&
        gSHAREnvironmentOutlineDiagnosticMode == 10)
    {
        static std::unordered_set<const pglPrimBuffer*> reportedOutlineSubmissions;
        if (reportedOutlineSubmissions.insert(this).second)
        {
            SDL_Log("[SHAR Outline Mode 10] outline-submit buffer=%p owner=%s drawable=%s ownerUID=%016llx sourceAssetUID=%016llx foliageCandidate=%d roadCandidate=%d call=%s primType=%d count=%u",
                    static_cast<const void*>(this),
                    gSHAREnvironmentDiagnosticOwnerName ? gSHAREnvironmentDiagnosticOwnerName : "<no-owner-context>",
                    gSHAREnvironmentDiagnosticDrawableName ? gSHAREnvironmentDiagnosticDrawableName : "<no-drawable-context>",
                    gSHAREnvironmentDiagnosticOwnerUID,
                    gSHAREnvironmentDiagnosticSourceAssetUID,
                    gSHAREnvironmentDiagnosticFoliageCandidate ? 1 : 0,
                    gSHAREnvironmentDiagnosticRoadCandidate ? 1 : 0,
                    (indexCount && indices) ? "glDrawElements" : "glDrawArrays",
                    static_cast<int>(primType),
                    (indexCount && indices) ? indexCount : total);
        }
    }

    if(indexCount && indices)
    {
        glDrawElements(primTypeTable[primType],indexCount,GL_UNSIGNED_SHORT,0);
    }
    else
    {
        glDrawArrays(primTypeTable[primType], 0, total);
    }

#ifdef RAD_ANDROID
    if ((outlineDiagnosticMode == 1 || outlineDiagnosticMode == 3) &&
        depthTestWasEnabled)
        glEnable(GL_DEPTH_TEST);

    if (outlineDiagnosticMode == 2 || outlineDiagnosticMode == 3)
    {
        if (cullWasEnabled)
            glEnable(GL_CULL_FACE);
    }
    else if (outlineDiagnosticMode == 4)
    {
        glCullFace((GLenum)cullFaceModeWas);
        if (cullWasEnabled)
            glEnable(GL_CULL_FACE);
        else
            glDisable(GL_CULL_FACE);
    }
#endif
}

/*
protected:
    float* coord;
    float* normal;
    float* uv;
    unsigned char* colour;

    unsigned allocated;
    unsigned total;

};
*/

void pglContext::DrawPrimBuffer(pddiShader* mat, pddiPrimBuffer* buffer)
{
    if(!mat)
        mat = defaultShader;

    pddiBaseShader* material = (pddiBaseShader*)mat;
    ADD_STAT(PDDI_STAT_MATERIAL_OPS, !material->IsCurrent());
    material->SetMaterial();
    if (!currentProgram)
        return;
    ((pglPrimBuffer*)buffer)->Display();
}

// lighting

int pglContext::GetMaxLights(void)
{
    return PDDI_MAX_LIGHTS;
}

void pglContext::SetupHardwareLight(int handle)
{
    if(currentProgram)
        currentProgram->SetLightState(handle, &state.lightingState->light[handle]);
}

void pglContext::SetAmbientLight(pddiColour col)
{
    pddiBaseContext::SetAmbientLight(col);
    if(currentProgram)
        currentProgram->SetAmbientLight(col);
}


// backface culling
GLenum cullModeTable[3] =
{
    GL_FRONT, // PDDI_CULL_NONE (disabled using glDisable())
    GL_FRONT, // PDDI_CULL_NORMAL
    GL_BACK   // PDDI_CULL_INVERTED
};
    
void pglContext::SetCullMode(pddiCullMode mode)
{
    pddiBaseContext::SetCullMode(mode);

    if(mode == PDDI_CULL_NONE)
    {
        SetGLCullEnabled(false);
    }
    else
    {
        SetGLCullEnabled(true);
        SetGLCullFace(cullModeTable[mode]);
    }
}

// z-buffer control
GLenum compTable[8] = {
    GL_NEVER,
    GL_ALWAYS,  
    GL_LESS,
    GL_LEQUAL,
    GL_GREATER,    
    GL_GEQUAL,  
    GL_EQUAL,
    GL_NOTEQUAL,
};

void pglContext::SetColourWrite( bool red, bool green, bool blue, bool alpha )
{
    pddiBaseContext::SetColourWrite(red, green, blue, alpha);
#ifdef RAD_ANDROID
    gSHARCharacterMaskAlphaWriteRequested = alpha;
    const bool effectiveAlpha = gSHARCharacterMaskWorldEnabled
        ? gSHARCharacterMaskModelPass
        : alpha;
    SetSHARTrackedColorWriteMask(
        red ? GL_TRUE : GL_FALSE,
        green ? GL_TRUE : GL_FALSE,
        blue ? GL_TRUE : GL_FALSE,
        effectiveAlpha ? GL_TRUE : GL_FALSE);
#else
    glColorMask(red, green, blue, alpha);
#endif
}

void pglContext::EnableZBuffer(bool enable)
{
    pddiBaseContext::EnableZBuffer(enable);
    if(enable)
    {
        glEnable(GL_DEPTH_TEST);
    }
    else
    {
        glDisable(GL_DEPTH_TEST);
    }
}


void pglContext::SetZCompare(pddiCompareMode compareMode)
{
    pddiBaseContext::SetZCompare(compareMode);
    glDepthFunc(compTable[compareMode]);
}

void pglContext::SetZWrite(bool b)
{
    pddiBaseContext::SetZWrite(b);
    SetGLDepthMask(b ? GL_TRUE : GL_FALSE);
}

void pglContext::SetZBias(float bias)
{
    pddiBaseContext::SetZBias(bias);
//TODO : Figure out how ro do this
}

void pglContext::SetZRange(float n, float f)
{
    pddiBaseContext::SetZRange(n,f);
    glDepthRangef(n,f);
}

// stencil buffer control
GLenum stencilTable[6] = {
    GL_KEEP,
    GL_ZERO,
    GL_REPLACE,
    GL_INCR,
    GL_DECR,
    GL_INVERT
};

void pglContext::EnableStencilBuffer(bool enable)
{
    pddiBaseContext::EnableStencilBuffer(enable);
    if(enable)
        glEnable(GL_STENCIL_TEST);
    else
        glDisable(GL_STENCIL_TEST);
}
        
void pglContext::SetStencilCompare(pddiCompareMode compare)
{
    pddiBaseContext::SetStencilCompare(compare);
    glStencilFunc(compTable[compare], state.stencilState->ref, state.stencilState->mask);
}

void pglContext::SetStencilRef(int ref)
{
    pddiBaseContext::SetStencilRef(ref);
    glStencilFunc(compTable[state.stencilState->compare], ref, state.stencilState->mask);
}

void pglContext::SetStencilMask(unsigned mask)
{
    pddiBaseContext::SetStencilMask(mask);
    glStencilFunc(compTable[state.stencilState->compare], state.stencilState->ref, mask);
}

void pglContext::SetStencilWriteMask(unsigned mask)
{
    pddiBaseContext::SetStencilWriteMask(mask);
    glStencilMask(mask);
}

void pglContext::SetStencilOp(pddiStencilOp failOp, pddiStencilOp zFailOp, pddiStencilOp zPassOp)
{
    pddiBaseContext::SetStencilOp(failOp, zFailOp, zPassOp);
    glStencilOp(stencilTable[failOp],stencilTable[zFailOp],stencilTable[zPassOp]);
}

void pglContext::SetFillMode(pddiFillMode mode)
{
    pddiBaseContext::SetFillMode(mode);
}

// fog
void pglContext::EnableFog(bool enable)
{
    pddiBaseContext::EnableFog(enable);
}

void pglContext::SetFog(pddiColour colour, float start, float end)
{
    pddiBaseContext::SetFog(colour,start,end);

    float fog[4];
    fog[0] = float(colour.Red()) / 255;
    fog[1] = float(colour.Green()) / 255;
    fog[2] = float(colour.Blue()) / 255;
    fog[3] = float(colour.Alpha()) / 255;
}

int pglContext::GetMaxTextureDimension(void)
{
    return maxTexSize;
}

pddiExtension* pglContext::GetExtension(unsigned extID)
{ 
    switch(extID)
    {
        case PDDI_EXT_GL_CONTEXT :
            return extContext;
        case PDDI_EXT_GAMMACONTROL :
            return extGamma;
    }

    return pddiBaseContext::GetExtension(extID);
}

bool pglContext::VerifyExtension(unsigned extID)
{ 
    switch(extID)
    {
        case PDDI_EXT_GL_CONTEXT :
        case PDDI_EXT_GAMMACONTROL :
            return true;
    }

    return pddiBaseContext::VerifyExtension(extID);
}

void  pglContext::BeginTiming(void)
{
    display->BeginTiming();
}

float pglContext::EndTiming(void)
{
    return display->EndTiming();
}

void pglContext::SetShaderProgram(pglProgram* program)
{
#ifdef RAD_ANDROID
    LoadSHAREnvironmentOutlineDiagnostics();
#endif
    if(program == currentProgram)
    {
#ifdef RAD_ANDROID
        if (currentProgram)
            currentProgram->SetCelShadingState(gCelShadingEnabled, gCelShadingObjectEnabled, gCelShadingOutlinePass, gSHARObjectBrightness, gSHARObjectSaturation, IsSHARRimLightingEnabled(),
            gSHAREnvironmentCelPassActive && gSHAREnvironmentCelMaterialEligible,
            gSHARCelOutlineViewportWidth, gSHARCelOutlineViewportHeight,
            gSHAREnvironmentOutlineDiagnosticMode);
#endif
        return;
    }

    if(currentProgram)
        currentProgram->Release();
    currentProgram = program;
    if(!currentProgram)
        return;

    currentProgram->AddRef();
    currentProgram->UseProgram();
#ifdef RAD_ANDROID
    currentProgram->SetCelShadingState(gCelShadingEnabled, gCelShadingObjectEnabled, gCelShadingOutlinePass, gSHARObjectBrightness, gSHARObjectSaturation, IsSHARRimLightingEnabled(),
            gSHAREnvironmentCelPassActive && gSHAREnvironmentCelMaterialEligible,
            gSHARCelOutlineViewportWidth, gSHARCelOutlineViewportHeight,
            gSHAREnvironmentOutlineDiagnosticMode);
#endif
    currentProgram->SetProjectionMatrix(&projection);

    LoadHardwareMatrix(PDDI_MATRIX_MODELVIEW);
    if(currentProgram->SupportsLighting())
    {
        for (int i = 0; i < PDDI_MAX_LIGHTS; i++)
            SetupHardwareLight(i);
        SetAmbientLight(state.lightingState->ambient);
    }
}

#ifdef RAD_ANDROID
void pglContext::ApplyCelShadingState()
{
    LoadSHAREnvironmentOutlineDiagnostics();
    if (currentProgram)
        currentProgram->SetCelShadingState(gCelShadingEnabled, gCelShadingObjectEnabled, gCelShadingOutlinePass, gSHARObjectBrightness, gSHARObjectSaturation, IsSHARRimLightingEnabled(),
            gSHAREnvironmentCelPassActive && gSHAREnvironmentCelMaterialEligible,
            gSHARCelOutlineViewportWidth, gSHARCelOutlineViewportHeight,
            gSHAREnvironmentOutlineDiagnosticMode);
}
#endif

void pglContext::SetTextureEnvironment(const pglTextureEnv* texEnv)
{
    if(texEnv->texture)
        SetShaderProgram(texEnv->alphaTest ? alphaTestProgram : textureProgram);
    else
        SetShaderProgram(colorProgram);
    // A failed shader link leaves no active program. Skip this material rather
    // than dereferencing a null program and crashing the SDL render thread.
    if (!currentProgram)
        return;
    currentProgram->SetTextureEnvironment(texEnv);
}
