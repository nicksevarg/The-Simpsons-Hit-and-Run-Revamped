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
// Enabled only around draws using one of the verified flat leaf-card materials.
static bool gSHAREnvironmentFoliageCardOutlineNormalModeActive = false;
// The viewport is supplied to the environment silhouette shader so its extrusion
// can be specified in pixels instead of world/view-space units.
static float gSHARCelOutlineViewportWidth = 1.0f;
static float gSHARCelOutlineViewportHeight = 1.0f;
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
static bool gSHARGagCatapultCelPassActive = false;
// Enabled while Scrooby displays the main-menu camera set. This is separate
// from the NIS gag flag because the front-end scene is not rendered by NISPlayer.
static bool gSHARFrontendMenuCelPassActive = false;
static bool gSHARFrontendMenuLightingActive = false;
#ifdef RAD_ANDROID
// Scoped by FePure3dObject to cel-shade only the live clothing preview model.
static bool gSHARPreviewCharacterCelPassActive = false;
#endif

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
static float gSHARMoodTintRed = 1.0f;
static float gSHARMoodTintGreen = 1.0f;
static float gSHARMoodTintBlue = 1.0f;
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

void SetSHARGagCatapultCelPassActive(bool enabled)
{
    gSHARGagCatapultCelPassActive = enabled;
}

bool IsSHARGagCatapultCelPassActive()
{
    return gSHARGagCatapultCelPassActive;
}

void SetSHARFrontendMenuCelPassActive(bool enabled)
{
    gSHARFrontendMenuCelPassActive = enabled;
}

void SetSHARFrontendMenuLightingActive(bool enabled)
{
    gSHARFrontendMenuLightingActive = enabled;
    if (gCelShadingContext)
        gCelShadingContext->ApplyCelShadingState();
}

bool IsSHARFrontendMenuCelPassActive()
{
    return gSHARFrontendMenuCelPassActive;
}

#ifdef RAD_ANDROID
void SetSHARPreviewCharacterCelPassActive( bool enabled )
{
    gSHARPreviewCharacterCelPassActive = enabled;
}

bool IsSHARPreviewCharacterCelPassActive()
{
    return gSHARPreviewCharacterCelPassActive;
}
#endif

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

void SetSHARMoodTint(float red, float green, float blue)
{
    if (red < 0.0f) red = 0.0f;
    if (red > 1.0f) red = 1.0f;
    if (green < 0.0f) green = 0.0f;
    if (green > 1.0f) green = 1.0f;
    if (blue < 0.0f) blue = 0.0f;
    if (blue > 1.0f) blue = 1.0f;

    gSHARMoodTintRed = red;
    gSHARMoodTintGreen = green;
    gSHARMoodTintBlue = blue;
    if (gCelShadingContext)
        gCelShadingContext->ApplyCelShadingState();
}
#endif
static bool gCelShadingConfigurationLoaded = false;
// LITE limits expanded-mesh silhouettes to characters, vehicles and gag assets.
// FULL retains broad object-toon and environment-outline coverage.
static bool gSHARCelShadingLite = false;
// Fresh installs default to OFF. Existing explicit sharpening preferences are preserved.
// Legacy files containing only a non-zero sharpen= value are treated as enabled.
static float gFrameSharpenStrength = 0.0f;
static bool gFrameSharpeningEnabled = false;
static bool gSharpeningConfigurationLoaded = false;

static bool gCRTConfigurationLoaded = false;
static bool gMotionBlurConfigurationLoaded = false;
static bool gMotionBlurEnabled = false;
static bool gCRTEnabled = false;
static bool gSHARRimLightingEnabled = false;
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
static const char* CEL_SHADING_LEGACY_QUALITY_CONFIGURATION_FILE_NAME = "Simpsons_cel_shading_quality.txt";
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


static bool ParseCelShadingModeValue(const char* value, int* mode, bool* legacyToggle)
{
    if (value == nullptr || mode == nullptr)
        return false;

    while (*value == ' ' || *value == '\t')
        ++value;

    char normalized[32];
    size_t length = 0;
    while (*value != '\0' && *value != '\n' && *value != '\r')
    {
        if (length >= sizeof(normalized) - 1)
            return false;
        normalized[length++] = static_cast<char>(
            tolower(static_cast<unsigned char>(*value++)));
    }
    while (length > 0 && (normalized[length - 1] == ' ' || normalized[length - 1] == '\t'))
        --length;
    normalized[length] = '\0';

    if (legacyToggle != nullptr)
        *legacyToggle = false;

    if (strcmp(normalized, "off") == 0)
    {
        *mode = 0;
        return true;
    }
    if (strcmp(normalized, "lite") == 0)
    {
        *mode = 1;
        return true;
    }
    if (strcmp(normalized, "full") == 0)
    {
        *mode = 2;
        return true;
    }

    // Accept the previous boolean format during migration. A legacy TRUE value
    // inherits the old quality file, while FALSE always takes precedence as OFF.
    if (strcmp(normalized, "false") == 0 || strcmp(normalized, "0") == 0 ||
        strcmp(normalized, "no") == 0)
    {
        *mode = 0;
        if (legacyToggle != nullptr)
            *legacyToggle = true;
        return true;
    }
    if (strcmp(normalized, "true") == 0 || strcmp(normalized, "1") == 0 ||
        strcmp(normalized, "on") == 0 || strcmp(normalized, "yes") == 0)
    {
        *mode = 2;
        if (legacyToggle != nullptr)
            *legacyToggle = true;
        return true;
    }

    return false;
}

static bool ReadLegacySHARCelShadingQualityConfiguration(int* mode)
{
    if (mode == nullptr)
        return false;

    char filePath[512];
    if (!BuildGraphicsConfigurationPath(
            filePath, sizeof(filePath),
            CEL_SHADING_LEGACY_QUALITY_CONFIGURATION_FILE_NAME))
    {
        return false;
    }

    FILE* file = fopen(filePath, "r");
    if (file == nullptr)
        return false;

    int configurationVersion = 0;
    char qualityValue[32] = { 0 };
    char line[256];
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        char* version = strstr(line, "cel_shading_quality_version=");
        if (version != nullptr)
        {
            version += strlen("cel_shading_quality_version=");
            configurationVersion = atoi(version);
            continue;
        }

        char* value = strstr(line, "cel_shading_quality=");
        if (value == nullptr)
            continue;

        value += strlen("cel_shading_quality=");
        while (*value == ' ' || *value == '\t')
            ++value;

        char* lineEnd = value + strlen(value);
        while (lineEnd > value &&
               (lineEnd[-1] == '\n' || lineEnd[-1] == '\r' ||
                lineEnd[-1] == ' ' || lineEnd[-1] == '\t'))
        {
            --lineEnd;
        }

        size_t length = static_cast<size_t>(lineEnd - value);
        if (length >= sizeof(qualityValue))
            length = sizeof(qualityValue) - 1;
        for (size_t i = 0; i < length; ++i)
        {
            qualityValue[i] = static_cast<char>(
                tolower(static_cast<unsigned char>(value[i])));
        }
        qualityValue[length] = '\0';
    }

    fclose(file);

    // Version 2 stores lite/full. Older MIN maps to LITE; the old MID/MAX
    // and two-choice LITE values map to FULL, matching the previous migration.
    if (configurationVersion >= 2)
        *mode = strcmp(qualityValue, "lite") == 0 ? 1 : 2;
    else
        *mode = strcmp(qualityValue, "min") == 0 ? 1 : 2;

    return true;
}

static bool WriteCelShadingConfiguration()
{
    char filePath[512];
    if (!BuildGraphicsConfigurationPath(
            filePath, sizeof(filePath), CEL_SHADING_CONFIGURATION_FILE_NAME))
    {
        return false;
    }

    FILE* file = fopen(filePath, "w");
    if (file == nullptr)
        return false;

    const char* mode = !gCelShadingEnabled ? "OFF" :
        (gSHARCelShadingLite ? "LITE" : "FULL");

    fprintf(file, "# Simpsons Hit & Run - Revamped! [Cel Shading Setting]\n");
    fprintf(file, "# Changes the graphics to more resemble the original Simpsons cartoon style.\n");
    fprintf(file, "# Change with OFF/LITE/FULL values.\n");
    fprintf(file, "# You can also use the in-game settings toggle.\n");
    fprintf(file, "# Changes to this file will be reflected upon restart.\n");
    fprintf(file, "\n");
    fprintf(file, "cel_shading=%s\n", mode);

    return fclose(file) == 0;
}

static void RemoveLegacyCelShadingQualityConfiguration()
{
    char filePath[512];
    if (BuildGraphicsConfigurationPath(
            filePath, sizeof(filePath),
            CEL_SHADING_LEGACY_QUALITY_CONFIGURATION_FILE_NAME))
    {
        remove(filePath);
    }
}

static void LoadCelShadingConfiguration()
{
    if (gCelShadingConfigurationLoaded)
        return;

    gCelShadingConfigurationLoaded = true;

    // Read the old quality preference before parsing the old boolean toggle.
    // It is used only when migrating a legacy cel_shading=true setting.
    int mode = 2;
    const bool hasLegacyQuality =
        ReadLegacySHARCelShadingQualityConfiguration(&mode);

    char filePath[512];
    if (!BuildGraphicsConfigurationPath(
            filePath, sizeof(filePath), CEL_SHADING_CONFIGURATION_FILE_NAME))
    {
        return;
    }

    FILE* file = fopen(filePath, "r");
    bool fromLegacyRoot = false;
    char legacyPath[512];
    if (file == nullptr &&
        BuildLegacyRootConfigurationPath(
            legacyPath, sizeof(legacyPath), CEL_SHADING_CONFIGURATION_FILE_NAME))
    {
        file = fopen(legacyPath, "r");
        fromLegacyRoot = (file != nullptr);
    }

    if (file == nullptr)
    {
        // New installations default to FULL. If only the old quality file is
        // present, preserve its supported LITE/FULL preference.
        gCelShadingEnabled = mode != 0;
        gSHARCelShadingLite = mode == 1;
        if (WriteCelShadingConfiguration() && hasLegacyQuality)
            RemoveLegacyCelShadingQualityConfiguration();
        return;
    }

    // Older root-level combined files could also contain sharpen=. Load that
    // preference before the old cel file is replaced by the new single setting.
    if (fromLegacyRoot)
        LoadSharpeningConfiguration();

    bool hasCelShadingSetting = false;
    bool rewriteConfiguration = fromLegacyRoot || hasLegacyQuality;
    char line[256];
    while (fgets(line, sizeof(line), file) != nullptr)
    {
        char* value = strstr(line, "cel_shading=");
        if (value == nullptr)
            continue;

        value += strlen("cel_shading=");
        int parsedMode = mode;
        bool legacyToggle = false;
        if (ParseCelShadingModeValue(value, &parsedMode, &legacyToggle))
        {
            if (legacyToggle && parsedMode != 0 && hasLegacyQuality)
                parsedMode = mode;

            mode = parsedMode;
            hasCelShadingSetting = true;
            if (legacyToggle)
                rewriteConfiguration = true;
        }
    }
    fclose(file);

    if (!hasCelShadingSetting)
    {
        mode = hasLegacyQuality ? mode : 2;
        rewriteConfiguration = true;
    }

    gCelShadingEnabled = mode != 0;
    gSHARCelShadingLite = mode == 1;

    if (rewriteConfiguration)
    {
        // Do not discard the old quality setting until the consolidated file
        // has been written successfully.
        if (WriteCelShadingConfiguration())
        {
            RemoveLegacyCelShadingQualityConfiguration();
            if (fromLegacyRoot)
            {
                RemoveLegacyRootConfigurationIfMigrated(
                    CEL_SHADING_CONFIGURATION_FILE_NAME, filePath);
            }
        }
    }
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
    LoadCelShadingConfiguration();
    return !gSHARCelShadingLite;
}

bool IsSHARCelShadingLite()
{
    LoadCelShadingConfiguration();
    return gSHARCelShadingLite;
}

int GetSHARCelShadingMode()
{
    LoadCelShadingConfiguration();
    if (!gCelShadingEnabled)
        return 0;
    return gSHARCelShadingLite ? 1 : 2;
}

void SetSHARCelShadingMode(int mode)
{
    LoadCelShadingConfiguration();
    if (mode < 0 || mode > 2)
        mode = 2;

    const bool enabled = mode != 0;
    const bool lite = mode == 1;
    const bool changed =
        gCelShadingEnabled != enabled || gSHARCelShadingLite != lite;

    gCelShadingEnabled = enabled;
    gSHARCelShadingLite = lite;

    if (changed)
        WriteCelShadingConfiguration();

    ApplyCelShadingState();
}

// Compatibility entry point: false selects LITE, true selects FULL.
void SetSHARCelShadingFull(bool full)
{
    SetSHARCelShadingMode(full ? 2 : 1);
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

    const int mode = !enabled ? 0 :
        (gCelShadingEnabled && gSHARCelShadingLite ? 1 : 2);
    SetSHARCelShadingMode(mode);
}

void SetCelShadingObjectEnabled(bool enabled)
{
    gCelShadingObjectEnabled = enabled;
    ApplyCelShadingState();
}

void SetCelShadingOutlinePass(bool enabled)
{
    gCelShadingOutlinePass = enabled;

    // Screen-space extrusion moves environment shell vertices sideways while
    // retaining their original depth. Use slope-scaled polygon offset so the
    // shell stays behind the source surface on steep depth gradients.
    // Keep this scoped to the environment outline draw, not its base-material
    // redraw or character/vehicle silhouette passes.
    const bool useEnvironmentPolygonOffset =
        enabled &&
        gSHAREnvironmentCelPassActive &&
        gSHAREnvironmentCelMaterialEligible;
    if (gCelShadingContext)
        gCelShadingContext->SetGLPolygonOffset(
            useEnvironmentPolygonOffset, 3.0f, 1.0f);

    if (enabled && gCelShadingEnabled && gCelShadingObjectEnabled)
    {
        // Ordinary inverted-hull meshes draw only their backfaces. Flat
        // foliage cards have no rear shell surface, so render those outlines
        // two-sided instead of culling the visible card away.
        if (gCelShadingContext)
        {
            if (gSHAREnvironmentBillboardFoliagePassActive ||
                gSHAREnvironmentFoliageMeshPassActive)
            {
                // pglMat::SetDevPass selects the actual per-material cull state.
                gCelShadingContext->SetGLCullEnabled(false);
            }
            else
            {
                gCelShadingContext->SetGLCullEnabled(true);
                gCelShadingContext->SetGLCullFace(GL_BACK);
            }
            // Eligible environment shells and CharacterRenderable silhouette
            // shells write depth so later-rendered objects behind them cannot
            // overwrite their outlines. The character flag is active only around
            // CharacterRenderable's model draw; vehicle outlines remain unchanged.
            const bool writeCelOutlineDepth =
                (gSHAREnvironmentCelPassActive &&
                 gSHAREnvironmentCelMaterialEligible) ||
                gSHARCharacterMaskModelPass;
            gCelShadingContext->SetGLDepthMask(writeCelOutlineDepth ? GL_TRUE : GL_FALSE);
        }
        else
        {
            if (gSHAREnvironmentBillboardFoliagePassActive ||
                gSHAREnvironmentFoliageMeshPassActive)
            {
                // pglMat::SetDevPass selects the actual per-material cull state.
                glDisable(GL_CULL_FACE);
            }
            else
            {
                glEnable(GL_CULL_FACE);
                glCullFace(GL_BACK);
            }
            // Match the context-backed path: environment and character
            // silhouette shells write depth, but vehicle shells do not.
            const bool writeCelOutlineDepth =
                (gSHAREnvironmentCelPassActive &&
                 gSHAREnvironmentCelMaterialEligible) ||
                gSHARCharacterMaskModelPass;
            glDepthMask(writeCelOutlineDepth ? GL_TRUE : GL_FALSE);
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

bool IsSHAREnvironmentCelPassActive()
{
    return gSHAREnvironmentCelPassActive;
}

bool IsSHAREnvironmentCelMaterialEligible()
{
    return gSHAREnvironmentCelMaterialEligible;
}

void SetSHAREnvironmentCelMaterialEligible(bool eligible)
{
    if (gSHAREnvironmentCelMaterialEligible == eligible)
        return;
    gSHAREnvironmentCelMaterialEligible = eligible;
    ApplyCelShadingState();
}

void SetSHAREnvironmentBillboardFoliagePassActive(bool enabled)
{
    gSHAREnvironmentBillboardFoliagePassActive = enabled;
}

bool IsSHAREnvironmentBillboardFoliagePassActive()
{
    return gSHAREnvironmentBillboardFoliagePassActive;
}

void SetSHAREnvironmentFoliageMeshPassActive(bool enabled)
{
    gSHAREnvironmentFoliageMeshPassActive = enabled;
}

bool IsSHAREnvironmentFoliageMeshPassActive()
{
    return gSHAREnvironmentFoliageMeshPassActive;
}

void SetSHAREnvironmentFoliageCardOutlineNormalMode(bool enabled)
{
    gSHAREnvironmentFoliageCardOutlineNormalModeActive = enabled;
}

bool IsSHAREnvironmentFoliageCardOutlineNormalModeActive()
{
    return gSHAREnvironmentFoliageCardOutlineNormalModeActive;
}

static std::unordered_set<unsigned long long>& SHAREnvironmentFoliageUIDs()
{
    static std::unordered_set<unsigned long long> registeredUIDs;
    return registeredUIDs;
}

void RegisterSHAREnvironmentFoliageUID(unsigned long long uid)
{
    if (uid != 0)
        SHAREnvironmentFoliageUIDs().insert(uid);
}

bool IsSHAREnvironmentFoliageUID(unsigned long long uid)
{
    return uid != 0 && SHAREnvironmentFoliageUIDs().find(uid) !=
        SHAREnvironmentFoliageUIDs().end();
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
        "fern", "palm", "plant", "pottedplant"
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
        "            if (clipPosition.w > 0.00001) {\n"
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
        "uniform int toonFrontendMenuLighting;\n"

        "uniform float toonOutlineWidth;\n"
        "uniform float nightObjectBrightness;\n"
        "uniform float nightObjectSaturation;\n"
        "uniform vec3 toonMoodTint;\n"
        "varying float toonOutlineProjectionState;\n"
#endif

        "vec3 direction(vec4 p1, vec4 p2) { return normalize(p2.xyz * sign(p1.w) - p1.xyz * sign(p2.w)); }\n"
        "float product(vec3 x, vec3 y) { return max(dot(x,y), 0.0); }\n"
        "float power(float x, float y) { return y != 0.0 ? pow(x,y) : 1.0; }\n"

        // Keep cel shading's base intensity neutral so enabling it does not
        // dim the world. Intentional per-level character/gag night dimming remains
        // separate in nightObjectBrightness and nightObjectSaturation.
        "const float toonFlatLight = 1.0;\n"
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
        "    if (lit == 0) return vec3(1.0);\n"
        "    if (toonFrontendMenuLighting != 0) {\n"
        "        // Keep the camset lighting level while averaging directional light over the surface.\n"
        "        // A fixed hemispherical contribution prevents normals from restoring 3D face shading.\n"
        "        vec3 ambient = ecm.rgb + acm.rgb * acs.rgb;\n"
        "        float illumination = dot(max(ambient, vec3(0.0)), vec3(0.299, 0.587, 0.114));\n"
        "        for (int i = 0; i < " PDDI_STRINGIZE(PDDI_MAX_LIGHTS) "; i++) {\n"
        "            if (lights[i].enabled == 0) continue;\n"
        "            vec3 k = lights[i].attenuation;\n"
        "            float d = distance(toonViewPos, lights[i].position.xyz);\n"
        "            float att = lights[i].position.w != 0.0 ? 1.0 / (k[0] + k[1] * d + k[2] * d * d) : 1.0;\n"
        "            illumination += att * 0.5 * dot(max(lights[i].colour.rgb, vec3(0.0)), vec3(0.299, 0.587, 0.114));\n"
        "        }\n"
        "        return dcm.rgb * clamp(illumination, 0.0, 1.0);\n"
        "    }\n"
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
        "vec3 applySHARMoodTint(vec3 colour) {\n"
        "    if (toonEnabled != 0 && toonObjectEnabled != 0 && lit != 0 && toonOutlinePass == 0 && toonEnvironmentPass == 0) {\n"
        "        return colour * clamp(toonMoodTint, vec3(0.0), vec3(1.0));\n"
        "    }\n"
        "    return colour;\n"
        "}\n"
#endif
        ;

    GLuint fragmentShader = pglProgram::CompileShader(GL_FRAGMENT_SHADER,
        (toonLighting +
        "void main() {\n"
        "    if (toonOutlinePass != 0 && toonEnabled != 0 && toonObjectEnabled != 0) {\n"
        "        gl_FragColor = vec4(0.0, 0.0, 0.0, cpri.a * dcm.a);\n"
        "        return;\n"
        "    }\n"
#ifdef RAD_ANDROID
        "    vec4 finalColour = cpri * vec4(toonLightingColor(), dcm.a);\n"
        "    finalColour.rgb = applySHARMoodTint(finalColour.rgb);\n"
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
#ifdef RAD_ANDROID
        "uniform int toonOutlineOpaque;\n"
#endif
        "void main() {\n"
#ifdef RAD_ANDROID
        "    if (toonOutlinePass != 0 && toonEnabled != 0 && toonObjectEnabled != 0 && toonOutlineOpaque != 0) {\n"
        "        gl_FragColor = vec4(0.0, 0.0, 0.0, cpri.a);\n"
        "        return;\n"
        "    }\n"
#endif
        "    vec4 base = texture2D(tex, tc) * cpri;\n"
        "    if (toonOutlinePass != 0 && toonEnabled != 0 && toonObjectEnabled != 0) {\n"
        "        gl_FragColor = vec4(0.0, 0.0, 0.0, base.a);\n"
        "        return;\n"
        "    }\n"
#ifdef RAD_ANDROID
        "    vec4 finalColour = base * vec4(toonLightingColor(), dcm.a);\n"
        "    finalColour.rgb = applySHARMoodTint(finalColour.rgb);\n"
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
        "        gl_FragColor = vec4(0.0, 0.0, 0.0, base.a);\n"
        "        return;\n"
        "    }\n"
        "    vec4 c = base * vec4(toonLightingColor(), dcm.a);\n"
        "    if (c.a < alpharef) discard;\n"
#ifdef RAD_ANDROID
        "    c.rgb = applySHARMoodTint(c.rgb);\n"
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
    mGLPolygonOffsetKnown = false;
    mGLPolygonOffsetFactorsKnown = false;
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
void pglContext::SetGLPolygonOffset(bool enabled, float factor, float units)
{
    if (!mGLPolygonOffsetKnown || mGLPolygonOffsetEnabled != enabled)
    {
        if (enabled)
            glEnable(GL_POLYGON_OFFSET_FILL);
        else
            glDisable(GL_POLYGON_OFFSET_FILL);
        mGLPolygonOffsetEnabled = enabled;
    }

    if (enabled &&
        (!mGLPolygonOffsetFactorsKnown ||
         mGLPolygonOffsetFactor != factor ||
         mGLPolygonOffsetUnits != units))
    {
        glPolygonOffset(factor, units);
        mGLPolygonOffsetFactor = factor;
        mGLPolygonOffsetUnits = units;
        mGLPolygonOffsetFactorsKnown = true;
    }

    mGLPolygonOffsetKnown = true;
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
    indexDataValid = false;
#ifdef RAD_ANDROID
    environmentNormalsAttempted = false;
    environmentNormalsAvailable = false;
    environmentOutlineNormalsAttempted = false;
    environmentOutlineNormalsAvailable = false;
    environmentOutlineNormalsUploaded = false;
    environmentOutlineNormalsRadialMode = false;
    environmentOutlineNormals = NULL;
    environmentOutlineNormalBuffer = 0;
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

    indexCapacity = nIndex;
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
    environmentOutlineNormalsRadialMode = false;
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
    PDDIASSERT(count >= 0 && (unsigned)count <= indexCapacity);
    if (count > 0)
        memcpy(indices, i, count * sizeof(unsigned short));
    indexCount = (unsigned)count;
#ifdef RAD_ANDROID
    environmentNormalsAttempted = false;
    environmentNormalsAvailable = false;
    environmentOutlineNormalsAttempted = false;
    environmentOutlineNormalsAvailable = false;
    environmentOutlineNormalsUploaded = false;
    delete [] environmentOutlineNormals;
    environmentOutlineNormals = NULL;
#endif
    // Only the element data changed; keep the vertex array and vertex data valid.
    indexDataValid = false;
}

#ifdef RAD_ANDROID
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
    const bool radialCardMode =
        IsSHAREnvironmentFoliageCardOutlineNormalModeActive();
    if (environmentOutlineNormalsAttempted &&
        environmentOutlineNormalsRadialMode == radialCardMode)
        return environmentOutlineNormalsAvailable;

    // A retained buffer can be drawn by different materials. If its outline
    // mode changes, rebuild the separate outline-only stream for that mode.
    if (environmentOutlineNormalsAttempted)
    {
        delete [] environmentOutlineNormals;
        environmentOutlineNormals = NULL;
        environmentOutlineNormalsAvailable = false;
        environmentOutlineNormalsUploaded = false;
    }

    environmentOutlineNormalsAttempted = true;
    environmentOutlineNormalsRadialMode = radialCardMode;
    environmentOutlineNormalsAvailable = false;

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

    // Alpha-cutout leaf cards need an in-plane expansion direction. Face
    // normals move the whole card sideways, leaving a one-sided outline fringe.
    // Group connected, nearly-coplanar triangles and expand each patch around
    // its own centroid. Only the separate outline normal stream is modified.
    if (radialCardMode &&
        (primType == PDDI_PRIM_TRIANGLES || primType == PDDI_PRIM_TRISTRIP))
    {
        struct OutlineTriangle
        {
            unsigned a, b, c;
            float nx, ny, nz;
            float weight;
        };

        const int radialElementCount = (indexCount > 0 && indices != NULL)
            ? static_cast<int>(indexCount) : static_cast<int>(total);
        std::vector<OutlinePosition> vertexPositions(allocated);
        std::vector<unsigned char> validVertexPositions(allocated, 0);
        for (unsigned i = 0; i < allocated; ++i)
        {
            const float* p = (const float*)((const unsigned char*)coord + i * stride);
            if (!isfinite(p[0]) || !isfinite(p[1]) || !isfinite(p[2]))
                continue;

            OutlinePosition key;
            key.x = static_cast<int64_t>(llround(static_cast<double>(p[0]) * 10000.0));
            key.y = static_cast<int64_t>(llround(static_cast<double>(p[1]) * 10000.0));
            key.z = static_cast<int64_t>(llround(static_cast<double>(p[2]) * 10000.0));
            vertexPositions[i] = key;
            validVertexPositions[i] = 1;
        }

        std::vector<OutlineTriangle> triangles;
        triangles.reserve(radialElementCount > 0
            ? static_cast<size_t>(radialElementCount / 3) : 0);
        const auto appendTriangle = [this, &triangles](unsigned a, unsigned b, unsigned c)
        {
            if (a >= allocated || b >= allocated || c >= allocated ||
                a == b || b == c || a == c)
                return;

            const float* pa = (const float*)((const unsigned char*)coord + a * stride);
            const float* pb = (const float*)((const unsigned char*)coord + b * stride);
            const float* pc = (const float*)((const unsigned char*)coord + c * stride);
            if (!isfinite(pa[0]) || !isfinite(pa[1]) || !isfinite(pa[2]) ||
                !isfinite(pb[0]) || !isfinite(pb[1]) || !isfinite(pb[2]) ||
                !isfinite(pc[0]) || !isfinite(pc[1]) || !isfinite(pc[2]))
                return;

            const float abx = pb[0] - pa[0], aby = pb[1] - pa[1], abz = pb[2] - pa[2];
            const float acx = pc[0] - pa[0], acy = pc[1] - pa[1], acz = pc[2] - pa[2];
            const float nx = aby * acz - abz * acy;
            const float ny = abz * acx - abx * acz;
            const float nz = abx * acy - aby * acx;
            const float lengthSquared = nx * nx + ny * ny + nz * nz;
            if (lengthSquared <= 1.0e-12f)
                return;

            const float length = sqrtf(lengthSquared);
            OutlineTriangle triangle;
            triangle.a = a;
            triangle.b = b;
            triangle.c = c;
            triangle.nx = nx / length;
            triangle.ny = ny / length;
            triangle.nz = nz / length;
            triangle.weight = length;
            triangles.push_back(triangle);
        };

        if (primType == PDDI_PRIM_TRIANGLES)
        {
            for (int i = 0; i + 2 < radialElementCount; i += 3)
                appendTriangle(indices ? indices[i] : static_cast<unsigned>(i),
                               indices ? indices[i + 1] : static_cast<unsigned>(i + 1),
                               indices ? indices[i + 2] : static_cast<unsigned>(i + 2));
        }
        else
        {
            // Match generated-normal strip parity across degenerate joins.
            for (int i = 0; i + 2 < radialElementCount; ++i)
            {
                unsigned a = indices ? indices[i] : static_cast<unsigned>(i);
                unsigned b = indices ? indices[i + 1] : static_cast<unsigned>(i + 1);
                const unsigned c = indices ? indices[i + 2] : static_cast<unsigned>(i + 2);
                if (i & 1) { const unsigned swap = a; a = b; b = swap; }
                appendTriangle(a, b, c);
            }
        }

        struct OutlineEdge
        {
            OutlinePosition first;
            OutlinePosition second;
            bool operator==(const OutlineEdge& other) const
            {
                return first == other.first && second == other.second;
            }
        };
        struct OutlineEdgeHash
        {
            size_t operator()(const OutlineEdge& edge) const
            {
                OutlinePositionHash hash;
                const size_t firstHash = hash(edge.first);
                const size_t secondHash = hash(edge.second);
                return firstHash ^ (secondHash + 0x9e3779b9U +
                                    (firstHash << 6) + (firstHash >> 2));
            }
        };

        const auto lessPosition = [](const OutlinePosition& a, const OutlinePosition& b)
        {
            if (a.x != b.x) return a.x < b.x;
            if (a.y != b.y) return a.y < b.y;
            return a.z < b.z;
        };
        std::unordered_map<OutlineEdge, std::vector<unsigned>, OutlineEdgeHash> edgeTriangles;
        edgeTriangles.reserve(triangles.size() * 3);
        const auto addEdge = [&edgeTriangles, &vertexPositions, &validVertexPositions,
                              &lessPosition](unsigned a, unsigned b, unsigned triangleIndex)
        {
            if (a >= validVertexPositions.size() || b >= validVertexPositions.size() ||
                !validVertexPositions[a] || !validVertexPositions[b])
                return;

            OutlineEdge edge;
            edge.first = vertexPositions[a];
            edge.second = vertexPositions[b];
            if (lessPosition(edge.second, edge.first))
            {
                const OutlinePosition swap = edge.first;
                edge.first = edge.second;
                edge.second = swap;
            }
            edgeTriangles[edge].push_back(triangleIndex);
        };

        for (unsigned i = 0; i < triangles.size(); ++i)
        {
            addEdge(triangles[i].a, triangles[i].b, i);
            addEdge(triangles[i].b, triangles[i].c, i);
            addEdge(triangles[i].c, triangles[i].a, i);
        }

        std::vector<unsigned> componentParent(triangles.size());
        std::vector<unsigned char> componentRank(triangles.size(), 0);
        for (unsigned i = 0; i < componentParent.size(); ++i)
            componentParent[i] = i;
        const auto findComponentRoot = [&componentParent](unsigned value) -> unsigned
        {
            while (componentParent[value] != value)
            {
                componentParent[value] = componentParent[componentParent[value]];
                value = componentParent[value];
            }
            return value;
        };
        const auto joinComponents = [&componentParent, &componentRank, &findComponentRoot]
            (unsigned first, unsigned second)
        {
            first = findComponentRoot(first);
            second = findComponentRoot(second);
            if (first == second)
                return;
            if (componentRank[first] < componentRank[second])
                componentParent[first] = second;
            else if (componentRank[first] > componentRank[second])
                componentParent[second] = first;
            else
            {
                componentParent[second] = first;
                ++componentRank[first];
            }
        };

        const float coplanarEdgeCosine = 0.985f;
        for (std::unordered_map<OutlineEdge, std::vector<unsigned>, OutlineEdgeHash>::const_iterator
                 edge = edgeTriangles.begin(); edge != edgeTriangles.end(); ++edge)
        {
            const std::vector<unsigned>& adjacent = edge->second;
            for (size_t i = 0; i < adjacent.size(); ++i)
            {
                for (size_t j = i + 1; j < adjacent.size(); ++j)
                {
                    const OutlineTriangle& a = triangles[adjacent[i]];
                    const OutlineTriangle& b = triangles[adjacent[j]];
                    const float alignment = fabsf(a.nx * b.nx + a.ny * b.ny + a.nz * b.nz);
                    if (alignment >= coplanarEdgeCosine)
                        joinComponents(adjacent[i], adjacent[j]);
                }
            }
        }

        std::unordered_map<unsigned, std::vector<unsigned> > patchTriangles;
        patchTriangles.reserve(triangles.size());
        for (unsigned i = 0; i < triangles.size(); ++i)
            patchTriangles[findComponentRoot(i)].push_back(i);

        const unsigned noComponent = 0xffffffffU;
        std::vector<unsigned> vertexComponents(allocated, noComponent);
        std::vector<unsigned char> vertexAmbiguous(allocated, 0);
        std::vector<float> radialNormals(static_cast<size_t>(allocated) * 3, 0.0f);

        for (std::unordered_map<unsigned, std::vector<unsigned> >::const_iterator
                 patch = patchTriangles.begin(); patch != patchTriangles.end(); ++patch)
        {
            const std::vector<unsigned>& patchTriangleIndices = patch->second;
            if (patchTriangleIndices.empty())
                continue;

            const OutlineTriangle& referenceTriangle = triangles[patchTriangleIndices[0]];
            float normalSumX = 0.0f, normalSumY = 0.0f, normalSumZ = 0.0f;
            float centroidSumX = 0.0f, centroidSumY = 0.0f, centroidSumZ = 0.0f;
            std::unordered_set<OutlinePosition, OutlinePositionHash> patchPositions;
            std::unordered_set<unsigned> patchVertexIndices;
            patchPositions.reserve(patchTriangleIndices.size() * 2);
            patchVertexIndices.reserve(patchTriangleIndices.size() * 2);

            for (size_t i = 0; i < patchTriangleIndices.size(); ++i)
            {
                const OutlineTriangle& triangle = triangles[patchTriangleIndices[i]];
                const float orientation =
                    triangle.nx * referenceTriangle.nx +
                    triangle.ny * referenceTriangle.ny +
                    triangle.nz * referenceTriangle.nz;
                const float sign = orientation < 0.0f ? -1.0f : 1.0f;
                normalSumX += triangle.nx * triangle.weight * sign;
                normalSumY += triangle.ny * triangle.weight * sign;
                normalSumZ += triangle.nz * triangle.weight * sign;

                const unsigned vertices[3] = { triangle.a, triangle.b, triangle.c };
                for (int v = 0; v < 3; ++v)
                {
                    const unsigned vertex = vertices[v];
                    if (vertex >= allocated || !validVertexPositions[vertex])
                        continue;
                    patchVertexIndices.insert(vertex);
                    if (patchPositions.insert(vertexPositions[vertex]).second)
                    {
                        const float* p = (const float*)((const unsigned char*)coord +
                                                        vertex * stride);
                        centroidSumX += p[0];
                        centroidSumY += p[1];
                        centroidSumZ += p[2];
                    }
                }
            }

            const float normalLengthSquared =
                normalSumX * normalSumX + normalSumY * normalSumY + normalSumZ * normalSumZ;
            if (patchPositions.size() < 3 || normalLengthSquared <= 1.0e-12f)
                continue;

            const float inverseNormalLength = 1.0f / sqrtf(normalLengthSquared);
            const float planeNormalX = normalSumX * inverseNormalLength;
            const float planeNormalY = normalSumY * inverseNormalLength;
            const float planeNormalZ = normalSumZ * inverseNormalLength;
            bool planarPatch = true;
            for (size_t i = 0; i < patchTriangleIndices.size(); ++i)
            {
                const OutlineTriangle& triangle = triangles[patchTriangleIndices[i]];
                const float alignment = fabsf(
                    planeNormalX * triangle.nx +
                    planeNormalY * triangle.ny +
                    planeNormalZ * triangle.nz);
                if (alignment < coplanarEdgeCosine)
                {
                    planarPatch = false;
                    break;
                }
            }
            if (!planarPatch)
                continue;

            const float inversePositionCount = 1.0f / static_cast<float>(patchPositions.size());
            const float centroidX = centroidSumX * inversePositionCount;
            const float centroidY = centroidSumY * inversePositionCount;
            const float centroidZ = centroidSumZ * inversePositionCount;

            for (std::unordered_set<unsigned>::const_iterator vertexIt =
                     patchVertexIndices.begin(); vertexIt != patchVertexIndices.end(); ++vertexIt)
            {
                const unsigned vertex = *vertexIt;
                const float* p = (const float*)((const unsigned char*)coord + vertex * stride);
                float radialX = p[0] - centroidX;
                float radialY = p[1] - centroidY;
                float radialZ = p[2] - centroidZ;
                const float normalComponent =
                    radialX * planeNormalX +
                    radialY * planeNormalY +
                    radialZ * planeNormalZ;
                radialX -= planeNormalX * normalComponent;
                radialY -= planeNormalY * normalComponent;
                radialZ -= planeNormalZ * normalComponent;

                const float radialLengthSquared =
                    radialX * radialX + radialY * radialY + radialZ * radialZ;
                if (radialLengthSquared <= 1.0e-10f)
                    continue;

                const float inverseRadialLength = 1.0f / sqrtf(radialLengthSquared);
                radialX *= inverseRadialLength;
                radialY *= inverseRadialLength;
                radialZ *= inverseRadialLength;

                if (vertexComponents[vertex] == noComponent)
                {
                    vertexComponents[vertex] = patch->first;
                    radialNormals[vertex * 3] = radialX;
                    radialNormals[vertex * 3 + 1] = radialY;
                    radialNormals[vertex * 3 + 2] = radialZ;
                }
                else if (vertexComponents[vertex] != patch->first)
                {
                    // A shared indexed vertex between distinct planes has no
                    // unique radial direction; retain its generated face normal.
                    vertexAmbiguous[vertex] = 1;
                }
            }
        }

        bool hasRadialOutlineVertices = false;
        for (unsigned i = 0; i < allocated; ++i)
        {
            if (vertexComponents[i] == noComponent || vertexAmbiguous[i])
                continue;
            outlineData[i * 3] = radialNormals[i * 3];
            outlineData[i * 3 + 1] = radialNormals[i * 3 + 1];
            outlineData[i * 3 + 2] = radialNormals[i * 3 + 2];
            hasRadialOutlineVertices = true;
        }
        if (hasRadialOutlineVertices)
        {
            environmentOutlineNormals = outlineData;
            environmentOutlineNormalsAvailable = true;
            environmentOutlineNormalsUploaded = false;
            return true;
        }
    }


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

    bool hasSmoothedOutlineNormals = false;

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
            hasSmoothedOutlineNormals = true;
            }
        }

    }

    if (!hasSmoothedOutlineNormals)
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

    if (environmentOutlineGeometryPass &&
        !(vertexType & PDDI_V_NORMAL) && !GenerateEnvironmentNormals())
        return;

    if (environmentOutlineGeometryPass && !(vertexType & PDDI_V_NORMAL))
        useCreaseAwareEnvironmentOutlineNormals = GenerateEnvironmentOutlineNormals();
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
            // The element buffer binding is VAO state. Uploads are handled
            // independently below so vertex edits do not resend unchanged indices.
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,indexBuffer);
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

    // Keep index uploads independent from vertex-buffer refreshes. SetIndices()
    // invalidates this cache; ordinary vertex Unlock() calls do not.
    if (indexCount && indices && !indexDataValid)
    {
        if (!indexBuffer)
            glGenBuffers(1, &indexBuffer);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indexBuffer);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     indexCount * sizeof(unsigned short), indices, GL_STATIC_DRAW);
        indexDataValid = true;
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

    if(indexCount && indices)
    {
        glDrawElements(primTypeTable[primType],indexCount,GL_UNSIGNED_SHORT,0);
    }
    else
    {
        glDrawArrays(primTypeTable[primType], 0, total);
    }

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
#endif
    if(program == currentProgram)
    {
#ifdef RAD_ANDROID
        if (currentProgram)
            currentProgram->SetCelShadingState(gCelShadingEnabled, gCelShadingObjectEnabled, gCelShadingOutlinePass, gSHARObjectBrightness, gSHARObjectSaturation, IsSHARRimLightingEnabled(),
            gSHAREnvironmentCelPassActive && gSHAREnvironmentCelMaterialEligible,
            gSHARCelOutlineViewportWidth, gSHARCelOutlineViewportHeight,
            gSHARFrontendMenuLightingActive,
            gSHARMoodTintRed, gSHARMoodTintGreen, gSHARMoodTintBlue);
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
            gSHARFrontendMenuLightingActive,
            gSHARMoodTintRed, gSHARMoodTintGreen, gSHARMoodTintBlue);
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
    if (currentProgram)
        currentProgram->SetCelShadingState(gCelShadingEnabled, gCelShadingObjectEnabled, gCelShadingOutlinePass, gSHARObjectBrightness, gSHARObjectSaturation, IsSHARRimLightingEnabled(),
            gSHAREnvironmentCelPassActive && gSHAREnvironmentCelMaterialEligible,
            gSHARCelOutlineViewportWidth, gSHARCelOutlineViewportHeight,
            gSHARFrontendMenuLightingActive,
            gSHARMoodTintRed, gSHARMoodTintGreen, gSHARMoodTintBlue);
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
#ifdef RAD_ANDROID
    // Alpha-tested and blended materials must retain texture sampling in their
    // outline pass. Only fully opaque texture materials can skip that fetch.
    currentProgram->SetOpaqueOutlineTexture(
        texEnv->texture != NULL &&
        texEnv->texture->IsOpaqueSourceTexture() &&
        !texEnv->alphaTest &&
        texEnv->alphaBlendMode == PDDI_BLEND_NONE);
#endif
    currentProgram->SetTextureEnvironment(texEnv);
}
