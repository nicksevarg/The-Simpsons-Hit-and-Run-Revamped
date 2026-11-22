/*===========================================================================
    texture.cpp

    Copyright (c)2000 Radical Entertainment, Inc.
    All rights reserved.
===========================================================================*/
#include <p3d/texture.hpp>
#include <p3d/image.hpp>
#include <p3d/imagefactory.hpp>
#include <p3d/imageconverter.hpp>
#include <p3d/utility.hpp>
#include <p3d/file.hpp>
#include <p3d/chunkfile.hpp>
#include <constants/chunks.h>
#include <constants/chunkids.hpp>
#include <constants/srrchunks.h> // For SetChunk.

#ifdef RAD_ANDROID
#include <ctype.h>
#include <vector>
#include <mutex>
#include <string.h>
#include <string>
#include <math.h>

bool IsCelShadingEnabled();

namespace
{
    struct SelectedShadowAlphaState
    {
        tTexture* texture;
        std::vector< std::vector<unsigned char> > originalAlpha;
        bool sharpened;
    };

    std::vector<SelectedShadowAlphaState*> gSelectedShadowAlphaStates;
    std::mutex gSelectedShadowAlphaMutex;

    bool IsSelectedShadowAlphaTextureName(const char* name)
    {
        if (name == NULL)
        {
            return false;
        }

        char lowerName[256];
        size_t i = 0;
        for (; name[i] != '\0' && i < sizeof(lowerName) - 1; ++i)
        {
            lowerName[i] = (char)tolower((unsigned char)name[i]);
        }
        lowerName[i] = '\0';

        // Keep this allow-list narrow so character/vehicle blob shadows
        // and unrelated shadow textures retain their original appearance.
        // "treeshad" covers treeshadow, treeshadowsmall, treeshad_overcast,
        // and deadtreeshad_overcast variants.
        return strstr(lowerName, "treeshad") != NULL ||
               strstr(lowerName, "beeshadow") != NULL;
    }

    bool CaptureSelectedShadowAlpha(SelectedShadowAlphaState* state)
    {
        if (state == NULL || state->texture == NULL)
        {
            return false;
        }

        const int lastMip = state->texture->GetNumMipMaps();
        if (lastMip < 0 || lastMip > 12)
        {
            return false;
        }

        state->originalAlpha.clear();
        for (int mip = 0; mip <= lastMip; ++mip)
        {
            pddiLockInfo* lock = state->texture->Lock(mip);
            if (lock == NULL || lock->bits == NULL)
            {
                if (lock != NULL)
                {
                    state->texture->Unlock(mip);
                }
                state->originalAlpha.clear();
                return false;
            }

            // Android GLES uses 32-bit ARGB for normal textured assets.
            // Compressed or unexpected formats are deliberately left alone.
            const unsigned int alphaMask = lock->rgbaMask[3];
            int alphaShift = 0;
            while (alphaShift < 32 &&
                   ((alphaMask >> alphaShift) & 1U) == 0U)
            {
                ++alphaShift;
            }

            if (lock->format != PDDI_PIXEL_ARGB8888 ||
                lock->width <= 0 || lock->height <= 0 ||
                lock->pitch == 0 || alphaMask == 0 ||
                alphaShift >= 32 || (alphaMask >> alphaShift) != 0xffU)
            {
                state->texture->Unlock(mip);
                state->originalAlpha.clear();
                return false;
            }

            std::vector<unsigned char> alpha;
            alpha.resize((size_t)lock->width * (size_t)lock->height);

            unsigned char* row = (unsigned char*)lock->bits;
            for (int y = 0; y < lock->height; ++y)
            {
                for (int x = 0; x < lock->width; ++x)
                {
                    unsigned int pixel = 0;
                    memcpy(&pixel, row + (x * 4), sizeof(pixel));
                    alpha[(size_t)y * (size_t)lock->width + (size_t)x] =
                        (unsigned char)((pixel & alphaMask) >> alphaShift);
                }
                // A negative pitch is valid on the GLES backend.
                row += lock->pitch;
            }

            state->texture->Unlock(mip);
            state->originalAlpha.push_back(alpha);
        }

        return !state->originalAlpha.empty();
    }

    bool WriteSelectedShadowAlpha(SelectedShadowAlphaState* state, bool sharpen)
    {
        if (state == NULL || state->texture == NULL ||
            state->originalAlpha.empty())
        {
            return false;
        }

        for (size_t mip = 0; mip < state->originalAlpha.size(); ++mip)
        {
            pddiLockInfo* lock = state->texture->Lock((int)mip);
            if (lock == NULL || lock->bits == NULL)
            {
                if (lock != NULL)
                {
                    state->texture->Unlock((int)mip);
                }
                return false;
            }

            const unsigned int alphaMask = lock->rgbaMask[3];
            int alphaShift = 0;
            while (alphaShift < 32 &&
                   ((alphaMask >> alphaShift) & 1U) == 0U)
            {
                ++alphaShift;
            }

            const std::vector<unsigned char>& original =
                state->originalAlpha[mip];
            if (lock->format != PDDI_PIXEL_ARGB8888 ||
                lock->width <= 0 || lock->height <= 0 ||
                lock->pitch == 0 || alphaMask == 0 ||
                alphaShift >= 32 || (alphaMask >> alphaShift) != 0xffU ||
                original.size() != (size_t)lock->width * (size_t)lock->height)
            {
                state->texture->Unlock((int)mip);
                return false;
            }

            unsigned int maxAlpha = 0;
            for (size_t i = 0; i < original.size(); ++i)
            {
                if (original[i] > maxAlpha)
                {
                    maxAlpha = original[i];
                }
            }

            // Drop faint edge pixels, then remap the remaining range to a
            // stronger yet translucent shadow. The relative cutoff keeps
            // lower-resolution mip levels from disappearing at distance.
            const unsigned int threshold = (maxAlpha * 72U) / 100U;
            unsigned char* row = (unsigned char*)lock->bits;
            for (int y = 0; y < lock->height; ++y)
            {
                for (int x = 0; x < lock->width; ++x)
                {
                    const size_t index =
                        (size_t)y * (size_t)lock->width + (size_t)x;
                    unsigned int pixel = 0;
                    memcpy(&pixel, row + (x * 4), sizeof(pixel));

                    unsigned int newAlpha = original[index];
                    if (sharpen)
                    {
                        if (maxAlpha <= threshold || newAlpha <= threshold)
                        {
                            newAlpha = 0;
                        }
                        else
                        {
                            newAlpha = ((newAlpha - threshold) * 200U) /
                                       (maxAlpha - threshold);
                            if (newAlpha > 200U)
                            {
                                newAlpha = 200U;
                            }
                        }
                    }

                    pixel = (pixel & ~alphaMask) |
                            ((newAlpha << alphaShift) & alphaMask);
                    memcpy(row + (x * 4), &pixel, sizeof(pixel));
                }
                row += lock->pitch;
            }

            state->texture->Unlock((int)mip);
        }

        return true;
    }


    void DilateL7KrustSignTransparentRGB(tTexture* texture, const char* name)
    {
        if (texture == NULL || name == NULL ||
            strcmp(name, "L7krust_sign.bmp") != 0 ||
            texture->GetWidth() != 256 || texture->GetHeight() != 256)
            return;
        pddiLockInfo* lock = texture->Lock(0);
        if (lock == NULL || lock->bits == NULL)
        {
            if (lock != NULL) texture->Unlock(0);
            return;
        }
        const unsigned int alphaMask = lock->rgbaMask[3];
        const unsigned int rgbMask =
            lock->rgbaMask[0] | lock->rgbaMask[1] | lock->rgbaMask[2];
        if (lock->format != PDDI_PIXEL_ARGB8888 ||
            lock->depth != 32 || lock->width != 256 ||
            lock->height != 256 || lock->pitch == 0 ||
            alphaMask == 0 || rgbMask == 0)
        {
            texture->Unlock(0);
            return;
        }

        const size_t pixelCount = 256u * 256u;
        std::vector<unsigned int> pixels(pixelCount);
        unsigned char* row = reinterpret_cast<unsigned char*>(lock->bits);
        for (int y = 0; y < 256; ++y)
        {
            for (int x = 0; x < 256; ++x)
                memcpy(&pixels[(size_t)y * 256u + (size_t)x],
                       row + x * 4, sizeof(unsigned int));
            row += lock->pitch;
        }

        // Seed a four-way flood fill from every visible texel. Only RGB in
        // fully transparent texels is replaced; every alpha bit is preserved.
        std::vector<int> nearest(pixelCount, -1);
        std::vector<int> queue;
        queue.reserve(pixelCount);
        for (size_t i = 0; i < pixelCount; ++i)
        {
            if ((pixels[i] & alphaMask) != 0)
            {
                nearest[i] = static_cast<int>(i);
                queue.push_back(static_cast<int>(i));
            }
        }
        if (queue.empty())
        {
            texture->Unlock(0);
            return;
        }
        size_t readIndex = 0;
        while (readIndex < queue.size())
        {
            const int index = queue[readIndex++];
            const int x = index % 256, y = index / 256;
            const int neighbours[4] = {
                x > 0 ? index - 1 : -1, x < 255 ? index + 1 : -1,
                y > 0 ? index - 256 : -1, y < 255 ? index + 256 : -1
            };
            for (int n = 0; n < 4; ++n)
            {
                const int next = neighbours[n];
                if (next >= 0 && nearest[(size_t)next] < 0)
                {
                    nearest[(size_t)next] = nearest[(size_t)index];
                    queue.push_back(next);
                }
            }
        }
        int changed = 0;
        for (size_t i = 0; i < pixelCount; ++i)
        {
            if ((pixels[i] & alphaMask) == 0 && nearest[i] >= 0)
            {
                pixels[i] = (pixels[i] & alphaMask) |
                    (pixels[(size_t)nearest[i]] & rgbMask);
                ++changed;
            }
        }
        row = reinterpret_cast<unsigned char*>(lock->bits);
        for (int y = 0; y < 256; ++y)
        {
            for (int x = 0; x < 256; ++x)
                memcpy(row + x * 4,
                       &pixels[(size_t)y * 256u + (size_t)x],
                       sizeof(unsigned int));
            row += lock->pitch;
        }
        texture->Unlock(0);
    }

    // Keep original mip pixels so Full/Lite/Off switches never accumulate tinting.
    struct SHARGrassTextureState
    {
        tTexture* texture;
        std::string name;
        std::vector< std::vector<unsigned char> > originalMipData;
        std::vector<int> mipWidths;
        std::vector<int> mipHeights;
        bool mailbox;
        bool grass;
        bool road;
        bool warmBrownDitch;
        bool originalCaptured;
        bool needsRestore;
        bool tinted;
    };

    std::vector<SHARGrassTextureState*> gSHARGrassTextureStates;

    bool IsSHARTargetGrassTerrainFile(const char* fileName)
    {
        if (fileName == NULL) return false;

        const char* baseName = strrchr(fileName, '/');
        const char* backslashName = strrchr(fileName, '\\');
        if (backslashName != NULL && (baseName == NULL || backslashName > baseName))
            baseName = backslashName;
        baseName = baseName != NULL ? baseName + 1 : fileName;

        char lowerFileName[128];
        size_t i = 0;
        for (; baseName[i] != '\0' && i < sizeof(lowerFileName) - 1; ++i)
            lowerFileName[i] = (char)tolower((unsigned char)baseName[i]);
        lowerFileName[i] = '\0';

        return strcmp(lowerFileName, "l1_terra.p3d") == 0 ||
               strcmp(lowerFileName, "l4_terra.p3d") == 0 ||
               strcmp(lowerFileName, "l7_terra.p3d") == 0;
    }

    bool IsSHARTargetRoadTerrainFile(const char* fileName)
    {
        if (fileName == NULL) return false;

        const char* baseName = strrchr(fileName, '/');
        const char* backslashName = strrchr(fileName, '\\');
        if (backslashName != NULL && (baseName == NULL || backslashName > baseName))
            baseName = backslashName;
        baseName = baseName != NULL ? baseName + 1 : fileName;

        char lowerFileName[128];
        size_t i = 0;
        for (; baseName[i] != '\0' && i < sizeof(lowerFileName) - 1; ++i)
            lowerFileName[i] = (char)tolower((unsigned char)baseName[i]);
        lowerFileName[i] = '\0';
        return strcmp(lowerFileName, "l1_terra.p3d") == 0;
    }

    bool IsSHARMailboxP3DFile(const char* fileName)
    {
        if (fileName == NULL) return false;

        const char* baseName = strrchr(fileName, '/');
        const char* backslashName = strrchr(fileName, '\\');
        if (backslashName != NULL && (baseName == NULL || backslashName > baseName))
            baseName = backslashName;
        baseName = baseName != NULL ? baseName + 1 : fileName;

        char lowerFileName[128];
        size_t i = 0;
        for (; baseName[i] != '\0' && i < sizeof(lowerFileName) - 1; ++i)
            lowerFileName[i] = (char)tolower((unsigned char)baseName[i]);
        lowerFileName[i] = '\0';
        return strcmp(lowerFileName, "l1z1.p3d") == 0 ||
               strcmp(lowerFileName, "l4z1.p3d") == 0 ||
               strcmp(lowerFileName, "l7z1.p3d") == 0;
    }

    bool IsSHARMailboxTextureName(const char* textureName)
    {
        if (textureName == NULL) return false;
        char lowerName[256];
        size_t i = 0;
        for (; textureName[i] != '\0' && i < sizeof(lowerName) - 1; ++i)
            lowerName[i] = (char)tolower((unsigned char)textureName[i]);
        lowerName[i] = '\0';
        return strcmp(lowerName, "simpson_mailbox.bmp") == 0;
    }

    unsigned int GetSHARChannelShift(unsigned int mask)
    {
        unsigned int shift = 0;
        while (shift < 32 && ((mask >> shift) & 1U) == 0U) ++shift;
        return shift;
    }

    bool IsSHARGrassTextureName(const char* textureName)
    {
        if (textureName == NULL) return false;
        char lowerName[256];
        size_t i = 0;
        for (; textureName[i] != '\0' && i < sizeof(lowerName) - 1; ++i)
            lowerName[i] = (char)tolower((unsigned char)textureName[i]);
        lowerName[i] = '\0';

        return (strcmp(lowerName, "grassc.bmp") == 0 || strcmp(lowerName, "grasscbmp") == 0) ||
               strcmp(lowerName, "driveway_dirt.bmp") == 0 ||
               strcmp(lowerName, "ditch.bmp") == 0 ||
               strcmp(lowerName, "path_sticks.bmp") == 0 ||
               strcmp(lowerName, "opt_grass_path.bmp") == 0 ||
               strcmp(lowerName, "driveway.bmp") == 0 ||
               strcmp(lowerName, "rock_tan_top.bmp") == 0;
    }

    bool IsSHARRoadTextureName(const char* textureName)
    {
        if (textureName == NULL) return false;
        char lowerName[256];
        size_t i = 0;
        for (; textureName[i] != '\0' && i < sizeof(lowerName) - 1; ++i)
            lowerName[i] = (char)tolower((unsigned char)textureName[i]);
        lowerName[i] = '\0';

        return strcmp(lowerName, "rd1_norm.bmp") == 0 ||
               strcmp(lowerName, "asphalt_norm.bmp") == 0 ||
               strcmp(lowerName, "sdewlk_corner.bmp") == 0 ||
               strcmp(lowerName, "rd2_crosswalk.bmp") == 0 ||
               strcmp(lowerName, "rd2_norm.bmp") == 0 ||
               strcmp(lowerName, "rd2_parking.bmp") == 0 ||
               strcmp(lowerName, "rd2_country.bmp") == 0 ||
               strcmp(lowerName, "rd2_crosswalk_country.bmp") == 0;
    }

    // Match one specifically named texture in one specifically named P3D.
    // This keeps supplemental Level 1 adjustments from widening the existing
    // terrain allow-lists or touching similarly named assets elsewhere.
    bool IsSHARTextureInP3D(const char* fileName, const char* textureName,
                            const char* expectedFile, const char* expectedTexture)
    {
        if (fileName == NULL || textureName == NULL ||
            expectedFile == NULL || expectedTexture == NULL)
            return false;

        const char* baseName = strrchr(fileName, '/');
        const char* backslashName = strrchr(fileName, '\\');
        if (backslashName != NULL && (baseName == NULL || backslashName > baseName))
            baseName = backslashName;
        baseName = baseName != NULL ? baseName + 1 : fileName;

        char lowerFileName[128];
        size_t fileLength = 0;
        for (; baseName[fileLength] != '\0' && fileLength < sizeof(lowerFileName) - 1; ++fileLength)
            lowerFileName[fileLength] = (char)tolower((unsigned char)baseName[fileLength]);
        lowerFileName[fileLength] = '\0';

        char lowerTextureName[256];
        size_t textureLength = 0;
        for (; textureName[textureLength] != '\0' && textureLength < sizeof(lowerTextureName) - 1; ++textureLength)
            lowerTextureName[textureLength] = (char)tolower((unsigned char)textureName[textureLength]);
        lowerTextureName[textureLength] = '\0';

        return strcmp(lowerFileName, expectedFile) == 0 &&
               strcmp(lowerTextureName, expectedTexture) == 0;
    }

    // A restrained warm balance applied after each existing colour treatment.
    // RGB detail and alpha remain intact; grass texels are explicitly excluded.
    void ApplySHARLevelOneWarmBalance(unsigned int& pixel,
                                      unsigned int redMask,
                                      unsigned int greenMask,
                                      unsigned int blueMask,
                                      unsigned int redShift,
                                      unsigned int greenShift,
                                      unsigned int blueShift)
    {
        const float red = (float)((pixel & redMask) >> redShift);
        const float green = (float)((pixel & greenMask) >> greenShift);
        const float blue = (float)((pixel & blueMask) >> blueShift);

        const unsigned int newRed = (unsigned int)fminf(255.0f, red * 1.02f + 0.5f);
        const unsigned int newGreen = (unsigned int)(green + 0.5f);
        const unsigned int newBlue = (unsigned int)(blue * 0.97f + 0.5f);

        pixel = (pixel & ~(redMask | greenMask | blueMask)) |
            ((newRed << redShift) & redMask) |
            ((newGreen << greenShift) & greenMask) |
            ((newBlue << blueShift) & blueMask);
    }

    // Compensate only the light, near-neutral road-paint pixels in the three
    // Level 1 marking maps. The measured in-game shift #A0A773 versus the
    // source marking colour #A8ABAE requires modest red/green gains and a
    // blue gain limited to a net +15% after warm balance. Alpha is preserved.
    void ApplySHARLevelOneWhiteMarkingCompensation(unsigned int& pixel,
                                                   unsigned int redMask,
                                                   unsigned int greenMask,
                                                   unsigned int blueMask,
                                                   unsigned int redShift,
                                                   unsigned int greenShift,
                                                   unsigned int blueShift)
    {
        const float red = (float)((pixel & redMask) >> redShift);
        const float green = (float)((pixel & greenMask) >> greenShift);
        const float blue = (float)((pixel & blueMask) >> blueShift);

        const unsigned int newRed = (unsigned int)fminf(255.0f, red * (168.0f / 160.0f) + 0.5f);
        const unsigned int newGreen = (unsigned int)fminf(255.0f, green * (171.0f / 167.0f) + 0.5f);
        const unsigned int newBlue = (unsigned int)fminf(255.0f, blue * (1.15f / 0.97f) + 0.5f);

        pixel = (pixel & ~(redMask | greenMask | blueMask)) |
            ((newRed << redShift) & redMask) |
            ((newGreen << greenShift) & greenMask) |
            ((newBlue << blueShift) & blueMask);
    }

    // The new striped-road maps have brighter source paint than the original
    // three marking textures. Use the same net +15% blue correction after
    // warm balance, scaling all channels together only if a channel would clip.
    void ApplySHARLevelOneBrightWhiteMarkingCompensation(unsigned int& pixel,
                                                         unsigned int redMask,
                                                         unsigned int greenMask,
                                                         unsigned int blueMask,
                                                         unsigned int redShift,
                                                         unsigned int greenShift,
                                                         unsigned int blueShift)
    {
        const float red = (float)((pixel & redMask) >> redShift) * (168.0f / 160.0f);
        const float green = (float)((pixel & greenMask) >> greenShift) * (171.0f / 167.0f);
        const float blue = (float)((pixel & blueMask) >> blueShift) * (1.15f / 0.97f);
        const float maximum = fmaxf(red, fmaxf(green, blue));
        const float scale = maximum > 255.0f ? 255.0f / maximum : 1.0f;
        const unsigned int newRed = (unsigned int)(red * scale + 0.5f);
        const unsigned int newGreen = (unsigned int)(green * scale + 0.5f);
        const unsigned int newBlue = (unsigned int)(blue * scale + 0.5f);

        pixel = (pixel & ~(redMask | greenMask | blueMask)) |
            ((newRed << redShift) & redMask) |
            ((newGreen << greenShift) & greenMask) |
            ((newBlue << blueShift) & blueMask);
    }

    bool IsSHARMainGrassTexture(const char* textureName)
    {
        if (textureName == NULL) return false;
        char lowerName[32];
        size_t i = 0;
        for (; textureName[i] != '\0' && i < sizeof(lowerName) - 1; ++i)
            lowerName[i] = (char)tolower((unsigned char)textureName[i]);
        lowerName[i] = '\0';
        return strcmp(lowerName, "grassc.bmp") == 0 ||
               strcmp(lowerName, "grasscbmp") == 0;
    }

    bool TransformSHARGrassMip(tTexture* texture, const char* textureName, int mipLevel,
                               bool warmBrownDitch)
    {
        pddiLockInfo* lock = texture->Lock(mipLevel);
        if (lock == NULL || lock->bits == NULL)
        {
            if (lock != NULL) texture->Unlock(mipLevel);
            return false;
        }

        const unsigned int redMask = lock->rgbaMask[0];
        const unsigned int greenMask = lock->rgbaMask[1];
        const unsigned int blueMask = lock->rgbaMask[2];
        if (lock->depth != 32 ||
            (lock->format != PDDI_PIXEL_RGB888 && lock->format != PDDI_PIXEL_ARGB8888) ||
            lock->width <= 0 || lock->height <= 0 || lock->pitch == 0 ||
            redMask == 0 || greenMask == 0 || blueMask == 0 ||
            (redMask >> GetSHARChannelShift(redMask)) != 0xffU ||
            (greenMask >> GetSHARChannelShift(greenMask)) != 0xffU ||
            (blueMask >> GetSHARChannelShift(blueMask)) != 0xffU)
        {
            texture->Unlock(mipLevel);
            return false;
        }

        const bool mainGrass = IsSHARMainGrassTexture(textureName);
        const unsigned int redShift = GetSHARChannelShift(redMask);
        const unsigned int greenShift = GetSHARChannelShift(greenMask);
        const unsigned int blueShift = GetSHARChannelShift(blueMask);
        const unsigned int rgbMask = redMask | greenMask | blueMask;
        const float hueShift = 59.0f;
        const float saturationScale = 0.67f;

        unsigned char* row = (unsigned char*)lock->bits;
        for (int y = 0; y < lock->height; ++y)
        {
            for (int x = 0; x < lock->width; ++x)
            {
                unsigned int pixel = 0;
                memcpy(&pixel, row + (x * 4), sizeof(pixel));
                const float red = (float)((pixel & redMask) >> redShift) / 255.0f;
                const float green = (float)((pixel & greenMask) >> greenShift) / 255.0f;
                const float blue = (float)((pixel & blueMask) >> blueShift) / 255.0f;
                const float maxValue = fmaxf(red, fmaxf(green, blue));
                const float minValue = fminf(red, fminf(green, blue));
                const float delta = maxValue - minValue;
                float hue = 0.0f;
                float saturation = maxValue > 0.0f ? delta / maxValue : 0.0f;

                if (delta > 0.00001f)
                {
                    if (maxValue == red)
                        hue = 60.0f * fmodf((green - blue) / delta, 6.0f);
                    else if (maxValue == green)
                        hue = 60.0f * (((blue - red) / delta) + 2.0f);
                    else
                        hue = 60.0f * (((red - green) / delta) + 4.0f);
                    if (hue < 0.0f) hue += 360.0f;
                }

                // Only recolour grass pixels. Non-grass pixels sharing a
                // mixed terrain bitmap (such as pavement/path pieces) remain
                // byte-for-byte identical to the source texture.
                const bool grassPixel = mainGrass ||
                    (hue >= 65.0f && hue <= 115.0f && saturation >= 0.10f);
                if (!grassPixel)
                {
                    // The Level 1 ditch map mixes grass with the same purple
                    // tarmac used by the country-road maps. Recolour only that
                    // purple/burgundy region; the grass path above is unchanged.
                    if (warmBrownDitch &&
                        hue >= 285.0f && hue <= 350.0f && saturation >= 0.05f)
                    {
                        // Match the country-road target #685F59 while preserving
                        // the source pixel's brightness variation across the mip.
                        const float targetValue =
                            fminf(1.0f, maxValue * (104.0f / 116.0f));
                        const float chroma = targetValue * (15.0f / 104.0f);
                        const float secondary = chroma * 0.4f; // Hue 24 degrees.
                        const float offset = targetValue - chroma;
                        const unsigned int newRed =
                            (unsigned int)((chroma + offset) * 255.0f + 0.5f);
                        const unsigned int newGreen =
                            (unsigned int)((secondary + offset) * 255.0f + 0.5f);
                        const unsigned int newBlue =
                            (unsigned int)(offset * 255.0f + 0.5f);

                        pixel = (pixel & ~rgbMask) |
                            ((newRed << redShift) & redMask) |
                            ((newGreen << greenShift) & greenMask) |
                            ((newBlue << blueShift) & blueMask);
                        memcpy(row + (x * 4), &pixel, sizeof(pixel));
                    }
                    continue;
                }

                hue += hueShift;
                if (hue >= 360.0f) hue -= 360.0f;
                saturation *= saturationScale;
                const float chroma = maxValue * saturation;
                const float hueSector = hue / 60.0f;
                const float secondary = chroma * (1.0f - fabsf(fmodf(hueSector, 2.0f) - 1.0f));
                float outputRed = 0.0f, outputGreen = 0.0f, outputBlue = 0.0f;

                if (hue < 60.0f) { outputRed = chroma; outputGreen = secondary; }
                else if (hue < 120.0f) { outputRed = secondary; outputGreen = chroma; }
                else if (hue < 180.0f) { outputGreen = chroma; outputBlue = secondary; }
                else if (hue < 240.0f) { outputGreen = secondary; outputBlue = chroma; }
                else if (hue < 300.0f) { outputRed = secondary; outputBlue = chroma; }
                else { outputRed = chroma; outputBlue = secondary; }

                const float offset = maxValue - chroma;
                const unsigned int newRed = (unsigned int)((outputRed + offset) * 255.0f + 0.5f);
                const unsigned int newGreen = (unsigned int)((outputGreen + offset) * 255.0f + 0.5f);
                const unsigned int newBlue = (unsigned int)((outputBlue + offset) * 255.0f + 0.5f);
                pixel = (pixel & ~rgbMask) |
                    ((newRed << redShift) & redMask) |
                    ((newGreen << greenShift) & greenMask) |
                    ((newBlue << blueShift) & blueMask);
                memcpy(row + (x * 4), &pixel, sizeof(pixel));
            }
            row += lock->pitch;
        }
        texture->Unlock(mipLevel);
        return true;
    }

    bool TransformSHARRoadMip(tTexture* texture, const char* textureName, int mipLevel)
    {
        pddiLockInfo* lock = texture->Lock(mipLevel);
        if (lock == NULL || lock->bits == NULL)
        {
            if (lock != NULL) texture->Unlock(mipLevel);
            return false;
        }

        const unsigned int redMask = lock->rgbaMask[0];
        const unsigned int greenMask = lock->rgbaMask[1];
        const unsigned int blueMask = lock->rgbaMask[2];
        if (lock->depth != 32 ||
            (lock->format != PDDI_PIXEL_RGB888 && lock->format != PDDI_PIXEL_ARGB8888) ||
            lock->width <= 0 || lock->height <= 0 || lock->pitch == 0 ||
            redMask == 0 || greenMask == 0 || blueMask == 0 ||
            (redMask >> GetSHARChannelShift(redMask)) != 0xffU ||
            (greenMask >> GetSHARChannelShift(greenMask)) != 0xffU ||
            (blueMask >> GetSHARChannelShift(blueMask)) != 0xffU)
        {
            texture->Unlock(mipLevel);
            return false;
        }

        char lowerName[256];
        size_t nameLength = 0;
        if (textureName != NULL)
        {
            for (; textureName[nameLength] != '\0' && nameLength < sizeof(lowerName) - 1; ++nameLength)
                lowerName[nameLength] = (char)tolower((unsigned char)textureName[nameLength]);
        }
        lowerName[nameLength] = '\0';

        float minimumHue = 210.0f;
        float maximumHue = 300.0f;
        float minimumSaturation = 0.18f;
        // The previous per-texture value scales targeted V=69 (#353F45).
        // Raising them by 91/69 retargets the same source brightness detail
        // to a neutral-grey median of V=91 (#5B5B5B).
        float valueScale = 0.79130435f;

        if (strcmp(lowerName, "rd1_norm.bmp") == 0)
        {
            minimumSaturation = 0.12f;
            valueScale = 0.77118644f;
        }
        else if (strcmp(lowerName, "asphalt_norm.bmp") == 0)
        {
            minimumSaturation = 0.18f;
            valueScale = 0.79130435f;
        }
        else if (strcmp(lowerName, "sdewlk_corner.bmp") == 0)
        {
            // The narrower mask excludes beige kerbs and low-chroma pavement.
            minimumHue = 230.0f;
            maximumHue = 285.0f;
            minimumSaturation = 0.18f;
            valueScale = 0.80530974f;
        }
        else if (strcmp(lowerName, "rd2_crosswalk.bmp") == 0)
        {
            minimumSaturation = 0.18f;
            valueScale = 0.83486238f;
        }
        else if (strcmp(lowerName, "rd2_norm.bmp") == 0)
        {
            minimumSaturation = 0.18f;
            valueScale = 0.78448276f;
        }
        else if (strcmp(lowerName, "rd2_parking.bmp") == 0)
        {
            minimumSaturation = 0.20f;
            valueScale = 0.81250000f;
        }
        else if (strcmp(lowerName, "rd2_striped.bmp") == 0)
        {
            minimumSaturation = 0.18f;
            valueScale = 0.79824562f;
        }
        else if (strcmp(lowerName, "rd2_crosswalk_country.bmp") == 0)
        {
            minimumSaturation = 0.18f;
            valueScale = 0.79130435f;
        }

        const bool hasWhiteRoadMarkings =
            strcmp(lowerName, "rd2_crosswalk.bmp") == 0 ||
            strcmp(lowerName, "rd2_norm.bmp") == 0 ||
            strcmp(lowerName, "rd2_parking.bmp") == 0 ||
            strcmp(lowerName, "rd2_striped.bmp") == 0 ||
            strcmp(lowerName, "rd2_country.bmp") == 0 ||
            strcmp(lowerName, "rd2_crosswalk_country.bmp") == 0;

        // These two country-road maps retain a purple source palette under
        // Level 1 lighting. Re-map only their purple/burgundy road pixels to
        // the requested warm brown, preserving per-pixel value variation.
        const bool warmBrownCountryRoad =
            strcmp(lowerName, "rd2_country.bmp") == 0 ||
            strcmp(lowerName, "rd2_crosswalk_country.bmp") == 0;

        // Target tarmac tone is neutral grey #5B5B5B (RGB 91,91,91).
        // Source value is scaled per pixel to retain original light/dark detail,
        // while setting all three output channels equally to remove blue chroma.

        const unsigned int redShift = GetSHARChannelShift(redMask);
        const unsigned int greenShift = GetSHARChannelShift(greenMask);
        const unsigned int blueShift = GetSHARChannelShift(blueMask);
        const unsigned int rgbMask = redMask | greenMask | blueMask;

        unsigned char* row = (unsigned char*)lock->bits;
        for (int y = 0; y < lock->height; ++y)
        {
            for (int x = 0; x < lock->width; ++x)
            {
                unsigned int pixel = 0;
                memcpy(&pixel, row + (x * 4), sizeof(pixel));

                const float red = (float)((pixel & redMask) >> redShift) / 255.0f;
                const float green = (float)((pixel & greenMask) >> greenShift) / 255.0f;
                const float blue = (float)((pixel & blueMask) >> blueShift) / 255.0f;
                const float maxValue = fmaxf(red, fmaxf(green, blue));
                const float minValue = fminf(red, fminf(green, blue));
                const float delta = maxValue - minValue;

                if (delta <= 0.00001f || maxValue <= 0.0f)
                    continue;

                const float initialSaturation = delta / maxValue;
                const float minimumChannel = minValue;
                // Bright low-chroma marking pixels and their antialiased edges
                // are deliberately left in their original colours.
                if (hasWhiteRoadMarkings &&
                    minimumChannel >= (115.0f / 255.0f) &&
                    initialSaturation <= 0.30f)
                    continue;

                float hue = 0.0f;
                if (maxValue == red)
                    hue = 60.0f * fmodf((green - blue) / delta, 6.0f);
                else if (maxValue == green)
                    hue = 60.0f * (((blue - red) / delta) + 2.0f);
                else
                    hue = 60.0f * (((red - green) / delta) + 4.0f);
                if (hue < 0.0f) hue += 360.0f;

                if (warmBrownCountryRoad)
                {
                    // Source average #74616D -> target #685F59:
                    // hue 24 degrees, saturation 15/104 and value 104/116.
                    // The hue/saturation are held constant while each pixel's
                    // original brightness variation is scaled independently.
                    if (hue < 285.0f || hue > 350.0f ||
                        initialSaturation < 0.05f)
                        continue;

                    const float targetValue =
                        fminf(1.0f, maxValue * (104.0f / 116.0f));
                    const float chroma = targetValue * (15.0f / 104.0f);
                    const float secondary = chroma * 0.4f; // Hue 24 degrees.
                    const float offset = targetValue - chroma;
                    const unsigned int newRed =
                        (unsigned int)((chroma + offset) * 255.0f + 0.5f);
                    const unsigned int newGreen =
                        (unsigned int)((secondary + offset) * 255.0f + 0.5f);
                    const unsigned int newBlue =
                        (unsigned int)(offset * 255.0f + 0.5f);

                    pixel = (pixel & ~rgbMask) |
                        ((newRed << redShift) & redMask) |
                        ((newGreen << greenShift) & greenMask) |
                        ((newBlue << blueShift) & blueMask);
                }
                else
                {
                    // Other selected Level 1 tarmac stays neutral #5B5B5B.
                    // Concrete, neutral paths, kerbs and road markings bypass it.
                    if (hue < minimumHue || hue > maximumHue ||
                        initialSaturation < minimumSaturation)
                        continue;

                    const float targetValue = fminf(1.0f, maxValue * valueScale);
                    const unsigned int newGrey =
                        (unsigned int)(targetValue * 255.0f + 0.5f);

                    pixel = (pixel & ~rgbMask) |
                        ((newGrey << redShift) & redMask) |
                        ((newGrey << greenShift) & greenMask) |
                        ((newGrey << blueShift) & blueMask);
                }
                memcpy(row + (x * 4), &pixel, sizeof(pixel));
            }
            row += lock->pitch;
        }

        texture->Unlock(mipLevel);
        return true;
    }

    bool TransformSHARMailboxMip(tTexture* texture, int mipLevel)
    {
        pddiLockInfo* lock = texture->Lock(mipLevel);
        if (lock == NULL || lock->bits == NULL)
        {
            if (lock != NULL) texture->Unlock(mipLevel);
            return false;
        }

        const unsigned int redMask = lock->rgbaMask[0];
        const unsigned int greenMask = lock->rgbaMask[1];
        const unsigned int blueMask = lock->rgbaMask[2];
        if (lock->depth != 32 ||
            (lock->format != PDDI_PIXEL_RGB888 && lock->format != PDDI_PIXEL_ARGB8888) ||
            lock->width <= 0 || lock->height <= 0 || lock->pitch == 0 ||
            redMask == 0 || greenMask == 0 || blueMask == 0 ||
            (redMask >> GetSHARChannelShift(redMask)) != 0xffU ||
            (greenMask >> GetSHARChannelShift(greenMask)) != 0xffU ||
            (blueMask >> GetSHARChannelShift(blueMask)) != 0xffU)
        {
            texture->Unlock(mipLevel);
            return false;
        }

        const unsigned int redShift = GetSHARChannelShift(redMask);
        const unsigned int greenShift = GetSHARChannelShift(greenMask);
        const unsigned int blueShift = GetSHARChannelShift(blueMask);
        const unsigned int rgbMask = redMask | greenMask | blueMask;

        // The mailbox artwork uses a narrow peach/orange hue range for its
        // body. Yellow lettering and the dark reddish lettering outline fall
        // outside these limits and are deliberately left unchanged.
        // Preserve the source's subtle shading while centring the blue around
        // the requested #3177A2 colour.
        const float targetHue = 205.0f;
        const float targetSaturation = 0.69753086f;
        const float targetValueScale = 0.63529412f / 0.76f;

        unsigned char* row = (unsigned char*)lock->bits;
        for (int y = 0; y < lock->height; ++y)
        {
            for (int x = 0; x < lock->width; ++x)
            {
                unsigned int pixel = 0;
                memcpy(&pixel, row + (x * 4), sizeof(pixel));
                const float red = (float)((pixel & redMask) >> redShift) / 255.0f;
                const float green = (float)((pixel & greenMask) >> greenShift) / 255.0f;
                const float blue = (float)((pixel & blueMask) >> blueShift) / 255.0f;
                const float maxValue = fmaxf(red, fmaxf(green, blue));
                const float minValue = fminf(red, fminf(green, blue));
                const float delta = maxValue - minValue;
                float hue = 0.0f;
                const float saturation = maxValue > 0.0f ? delta / maxValue : 0.0f;

                if (delta > 0.00001f)
                {
                    if (maxValue == red)
                        hue = 60.0f * fmodf((green - blue) / delta, 6.0f);
                    else if (maxValue == green)
                        hue = 60.0f * (((blue - red) / delta) + 2.0f);
                    else
                        hue = 60.0f * (((red - green) / delta) + 4.0f);
                    if (hue < 0.0f) hue += 360.0f;
                }

                // Original mailbox-body pixels are light orange (about 4-10
                // degrees). Keep low-value outline pixels and yellow details.
                if (hue < 3.0f || hue > 13.0f ||
                    saturation < 0.30f || maxValue < 0.65f)
                    continue;

                const float value = fminf(1.0f, maxValue * targetValueScale);
                const float chroma = value * targetSaturation;
                const float hueSector = targetHue / 60.0f;
                const float secondary = chroma *
                    (1.0f - fabsf(fmodf(hueSector, 2.0f) - 1.0f));
                float outputRed = 0.0f, outputGreen = 0.0f, outputBlue = 0.0f;

                if (targetHue < 60.0f) { outputRed = chroma; outputGreen = secondary; }
                else if (targetHue < 120.0f) { outputRed = secondary; outputGreen = chroma; }
                else if (targetHue < 180.0f) { outputGreen = chroma; outputBlue = secondary; }
                else if (targetHue < 240.0f) { outputGreen = secondary; outputBlue = chroma; }
                else if (targetHue < 300.0f) { outputRed = secondary; outputBlue = chroma; }
                else { outputRed = chroma; outputBlue = secondary; }

                const float offset = value - chroma;
                const unsigned int newRed = (unsigned int)((outputRed + offset) * 255.0f + 0.5f);
                const unsigned int newGreen = (unsigned int)((outputGreen + offset) * 255.0f + 0.5f);
                const unsigned int newBlue = (unsigned int)((outputBlue + offset) * 255.0f + 0.5f);
                pixel = (pixel & ~rgbMask) |
                    ((newRed << redShift) & redMask) |
                    ((newGreen << greenShift) & greenMask) |
                    ((newBlue << blueShift) & blueMask);
                ApplySHARLevelOneWarmBalance(
                    pixel, redMask, greenMask, blueMask,
                    redShift, greenShift, blueShift);
                memcpy(row + (x * 4), &pixel, sizeof(pixel));
            }
            row += lock->pitch;
        }

        texture->Unlock(mipLevel);
        return true;
    }

    bool CaptureSHARGrassTexture(SHARGrassTextureState* state)
    {
        if (state == NULL || state->texture == NULL) return false;
        if (state->originalCaptured) return true;

        // Back up pixel data only when cel shading first needs to recolour this
        // texture. Most users leave cel shading off, so this avoids retaining
        // duplicate mip chains for textures that are never transformed.
        state->originalMipData.clear();
        state->mipWidths.clear();
        state->mipHeights.clear();

        const int lastMip = state->texture->GetNumMipMaps();
        if (lastMip < 0 || lastMip > 12) return false;

        for (int mip = 0; mip <= lastMip; ++mip)
        {
            pddiLockInfo* lock = state->texture->Lock(mip);
            if (lock == NULL || lock->bits == NULL)
            {
                if (lock != NULL) state->texture->Unlock(mip);
                state->originalMipData.clear();
                state->mipWidths.clear();
                state->mipHeights.clear();
                return false;
            }
            if (lock->depth != 32 ||
                (lock->format != PDDI_PIXEL_RGB888 && lock->format != PDDI_PIXEL_ARGB8888) ||
                lock->width <= 0 || lock->height <= 0 || lock->pitch == 0)
            {
                state->texture->Unlock(mip);
                state->originalMipData.clear();
                state->mipWidths.clear();
                state->mipHeights.clear();
                return false;
            }

            const int mipWidth = lock->width;
            const int mipHeight = lock->height;
            std::vector<unsigned char> pixels((size_t)mipWidth * (size_t)mipHeight * 4);
            unsigned char* row = (unsigned char*)lock->bits;
            for (int y = 0; y < lock->height; ++y)
            {
                memcpy(&pixels[(size_t)y * (size_t)mipWidth * 4],
                       row, (size_t)mipWidth * 4);
                row += lock->pitch;
            }
            state->texture->Unlock(mip);
            state->mipWidths.push_back(mipWidth);
            state->mipHeights.push_back(mipHeight);
            state->originalMipData.push_back(pixels);
        }

        state->originalCaptured = !state->originalMipData.empty();
        return state->originalCaptured;
    }

    bool RestoreSHARGrassTexture(SHARGrassTextureState* state)
    {
        if (state == NULL || state->texture == NULL) return false;
        if (!state->originalCaptured) return true;
        for (size_t mip = 0; mip < state->originalMipData.size(); ++mip)
        {
            pddiLockInfo* lock = state->texture->Lock((int)mip);
            if (lock == NULL || lock->bits == NULL)
            {
                if (lock != NULL) state->texture->Unlock((int)mip);
                return false;
            }
            if (lock->width != state->mipWidths[mip] ||
                lock->height != state->mipHeights[mip] || lock->pitch == 0)
            {
                state->texture->Unlock((int)mip);
                return false;
            }
            unsigned char* row = (unsigned char*)lock->bits;
            for (int y = 0; y < lock->height; ++y)
            {
                memcpy(row, &state->originalMipData[mip][(size_t)y * (size_t)lock->width * 4],
                       (size_t)lock->width * 4);
                row += lock->pitch;
            }
            state->texture->Unlock((int)mip);
        }
        return true;
    }

    bool SetSHARGrassTextureTint(SHARGrassTextureState* state, bool enable)
    {
        if (state == NULL) return false;

        if (enable)
        {
            if (state->tinted && !state->needsRestore) return true;

            // The first enable captures the pristine mip chain. After an
            // ordinary disable, the texture is already restored, so don't copy
            // the entire texture back over itself before recolouring it.
            if (!CaptureSHARGrassTexture(state)) return false;
            if (state->needsRestore)
            {
                if (!RestoreSHARGrassTexture(state)) return false;
                state->needsRestore = false;
                state->tinted = false;
            }

            const int lastMip = state->texture->GetNumMipMaps();
            for (int mip = 0; mip <= lastMip; ++mip)
            {
                bool transformed = false;
                if (state->mailbox)
                    transformed = TransformSHARMailboxMip(state->texture, mip);
                else if (state->road)
                    transformed = TransformSHARRoadMip(
                        state->texture, state->name.c_str(), mip);
                else if (state->grass)
                    transformed = TransformSHARGrassMip(
                        state->texture, state->name.c_str(), mip,
                        state->warmBrownDitch);
                if (!transformed)
                {
                    // Do not leave a partially tinted mip chain behind. If a
                    // restore cannot complete, remember to retry before any
                    // subsequent transform or disable operation.
                    if (RestoreSHARGrassTexture(state))
                    {
                        state->tinted = false;
                        state->needsRestore = false;
                    }
                    else
                    {
                        state->needsRestore = true;
                    }
                    return false;
                }
            }

            state->tinted = true;
            state->needsRestore = false;
            return true;
        }

        // A never-enabled texture has no backup and is still untouched.
        if (!state->originalCaptured)
        {
            state->tinted = false;
            state->needsRestore = false;
            return true;
        }
        if (!state->tinted && !state->needsRestore) return true;

        if (!RestoreSHARGrassTexture(state))
        {
            state->needsRestore = true;
            return false;
        }

        state->tinted = false;
        state->needsRestore = false;
        return true;
    }

    void RemoveSHARGrassTextureState(tTexture* texture)
    {
        std::lock_guard<std::mutex> guard(gSelectedShadowAlphaMutex);
        for (std::vector<SHARGrassTextureState*>::iterator it = gSHARGrassTextureStates.begin();
             it != gSHARGrassTextureStates.end(); ++it)
        {
            if (*it != NULL && (*it)->texture == texture)
            {
                delete *it;
                gSHARGrassTextureStates.erase(it);
                return;
            }
        }
    }

    void RegisterSHARGrassTexture(tTexture* texture, const char* textureName,
                                  const char* sourceFileName)
    {
        const bool isGrassTexture =
            (IsSHARTargetGrassTerrainFile(sourceFileName) &&
             IsSHARGrassTextureName(textureName)) ||
            IsSHARTextureInP3D(sourceFileName, textureName, "l1r6.p3d", "field.bmp") ||
            IsSHARTextureInP3D(sourceFileName, textureName, "l1r7.p3d", "path_sticks_dark.bmp") ||
            IsSHARTextureInP3D(sourceFileName, textureName, "l1z7.p3d", "driveway_dirt_entr.bmp");
        const bool isRoadTexture =
            (IsSHARTargetRoadTerrainFile(sourceFileName) &&
             IsSHARRoadTextureName(textureName)) ||
            IsSHARTextureInP3D(sourceFileName, textureName, "l1r6.p3d", "rd2_striped.bmp");
        const bool isMailboxTexture =
            IsSHARMailboxP3DFile(sourceFileName) &&
            IsSHARMailboxTextureName(textureName);
        const bool isWarmBrownDitch =
            IsSHARTextureInP3D(sourceFileName, textureName,
                               "l1_terra.p3d", "ditch.bmp");
        if (texture == NULL || (!isMailboxTexture && !isGrassTexture && !isRoadTexture))
            return;

        std::lock_guard<std::mutex> guard(gSelectedShadowAlphaMutex);
        for (size_t i = 0; i < gSHARGrassTextureStates.size(); ++i)
            if (gSHARGrassTextureStates[i] != NULL &&
                gSHARGrassTextureStates[i]->texture == texture)
                return;

        SHARGrassTextureState* state = new SHARGrassTextureState;
        state->texture = texture;
        state->name = textureName;
        state->mailbox = isMailboxTexture;
        state->grass = isGrassTexture;
        state->road = isRoadTexture;
        state->warmBrownDitch = isWarmBrownDitch;
        state->originalCaptured = false;
        state->needsRestore = false;
        state->tinted = false;
        gSHARGrassTextureStates.push_back(state);
        SetSHARGrassTextureTint(state, IsCelShadingEnabled());
    }

    void RemoveSelectedShadowAlphaState(tTexture* texture)
    {
        std::lock_guard<std::mutex> guard(gSelectedShadowAlphaMutex);
        for (std::vector<SelectedShadowAlphaState*>::iterator it =
                 gSelectedShadowAlphaStates.begin();
             it != gSelectedShadowAlphaStates.end(); ++it)
        {
            if (*it != NULL && (*it)->texture == texture)
            {
                delete *it;
                gSelectedShadowAlphaStates.erase(it);
                return;
            }
        }
    }

    void RegisterSelectedShadowTexture(tTexture* texture, const char* name)
    {
        if (texture == NULL || !IsSelectedShadowAlphaTextureName(name))
        {
            return;
        }

        std::lock_guard<std::mutex> guard(gSelectedShadowAlphaMutex);
        for (size_t i = 0; i < gSelectedShadowAlphaStates.size(); ++i)
        {
            if (gSelectedShadowAlphaStates[i] != NULL &&
                gSelectedShadowAlphaStates[i]->texture == texture)
            {
                return;
            }
        }

        SelectedShadowAlphaState* state = new SelectedShadowAlphaState;
        state->texture = texture;
        state->sharpened = false;

        if (!CaptureSelectedShadowAlpha(state))
        {
            delete state;
            return;
        }

        gSelectedShadowAlphaStates.push_back(state);
    }
}

void UpdateSelectedShadowAlphaForCelState()
{
    std::lock_guard<std::mutex> guard(gSelectedShadowAlphaMutex);
    const bool celEnabled = IsCelShadingEnabled();

    for (size_t i = 0; i < gSelectedShadowAlphaStates.size(); ++i)
    {
        SelectedShadowAlphaState* state = gSelectedShadowAlphaStates[i];
        if (state == NULL || state->sharpened == celEnabled)
        {
            continue;
        }

        if (WriteSelectedShadowAlpha(state, celEnabled))
        {
            state->sharpened = celEnabled;
        }
    }

    for (size_t i = 0; i < gSHARGrassTextureStates.size(); ++i)
    {
        SHARGrassTextureState* state = gSHARGrassTextureStates[i];
        if (state != NULL)
            SetSHARGrassTextureTint(state, celEnabled);
    }
}
#endif


tTexture::tTexture() : texture(NULL)
{
}

tTexture::~tTexture()
{
#ifdef RAD_ANDROID
    RemoveSelectedShadowAlphaState(this);
    RemoveSHARGrassTextureState(this);
#endif
    tRefCounted::Release(texture);
}

void tTexture::SetTexture(pddiTexture* t)
{
    tRefCounted::Assign(texture, t);
}

bool tTexture::Create(int xSize, int ySize, int bpp, int alphaDepth, int nMip,
                pddiTextureType textureType,
                pddiTextureUsageHint usageHint)
    {  
        pddiTextureDesc desc;
        desc.SetSizeX(xSize);
        desc.SetSizeY(ySize);
        desc.SetBitDepth(bpp);
        desc.SetAlphaDepth(alphaDepth);
        desc.SetMipMapCount(nMip);
        desc.SetType(textureType);
        desc.SetUsage(usageHint);
        texture = p3d::device->NewTexture(&desc);

        if(!texture)
        {
            return false;
        }

        texture->AddRef();
        return true;
    }

bool tTexture::CreateVolume(int xSize, int ySize, int zSize, int bpp, int alphaDepth, int nMip,
                        pddiTextureType textureType, 
                        pddiTextureUsageHint usageHint)
    {  
        pddiTextureDesc desc;
        desc.SetSizeX(xSize);
        desc.SetSizeY(ySize);
        desc.SetSizeZ(zSize);
        desc.SetBitDepth(bpp);
        desc.SetAlphaDepth(alphaDepth);
        desc.SetMipMapCount(nMip);
        desc.SetType(textureType);
        desc.SetUsage(usageHint);
        desc.SetVolume(true);
        texture = p3d::device->NewTexture(&desc);

        if(!texture)
        {
            return false;
        }

        texture->AddRef();
        return true;
    }

//-------------------------------------------------------------------
rmt::Randomizer tSetLoader::m_Random( 0x1A30E31E );

tSetLoader::tSetLoader()
{}

tSetLoader::~tSetLoader()
{}

void tSetLoader::SetRandomSeed( unsigned NewSeed )
{
    m_Random.Seed( NewSeed );
}

tLoadStatus tSetLoader::Load( tChunkFile* InFile, tEntityStore* TempLoadingStore )
{
    if( InFile->GetCurrentID() != SRR2::ChunkID::CHUNK_SET )
    {
        return LOAD_ERROR;
    }

    // Go through the set and pick on child at random.
    char name[256];
    InFile->GetPString( name );
    int version = InFile->GetLong();
    int childCount = InFile->GetUChar();
    int childIndex = m_Random.IntRanged( 0, childCount - 1 );
    // Skip some of the children.
    for( int i = 0; i < childIndex; ++i )
    {
        InFile->BeginChunk();
        // Do nothing with the chunk.
        InFile->EndChunk();
    }
    unsigned chunkID = InFile->BeginChunk();
    tChunkHandler* chunkHandler = p3d::loadManager->GetP3DHandler()->GetHandler( chunkID );
    tLoadStatus status = LOAD_ERROR;
    if( chunkHandler )
    {
        chunkHandler->SetNameOverride( name );
        status = chunkHandler->Load( InFile, TempLoadingStore );
    }
    InFile->EndChunk();
    return status;
}

bool tSetLoader::CheckChunkID( unsigned ID )
{
    return ID == SRR2::ChunkID::CHUNK_SET;
}

unsigned int tSetLoader::GetChunkID()
{
	return SRR2::ChunkID::CHUNK_SET;
}

//-------------------------------------------------------------------
static const int TEXTURE_VERSION_V13 = 0;
static const int IMAGE_VERSION_V13 = 0;

static const int TEXTURE_VERSION = 14000;
static const int IMAGE_VERSION = 14000;
static const int VOLUME_IMAGE_VERSION = 14000;

tTextureLoader::tTextureLoader() : tSimpleChunkHandler(Pure3D::Texture::TEXTURE) 
{
    imageFactory = new tImageFactory;
    imageConverter = new tImageConverter;
};

tTextureLoader::~tTextureLoader() 
{
    delete imageFactory;
    delete imageConverter;
};

tEntity* tTextureLoader::LoadObject(tChunkFile* f, tEntityStore* store)
{
    return LoadTexture(f);
}

tTexture* tTextureLoader::LoadTexture(tChunkFile* f)
{
    char name[128];
    f->GetPString(name);

    int version = f->GetLong();
    P3DASSERT(version == TEXTURE_VERSION);   

    int width = f->GetLong();
    int height = f->GetLong();
    int bpp = f->GetLong();
    int alphaDepth = f->GetLong();
    int numMipMaps = f->GetLong();
    numMipMaps = numMipMaps ? numMipMaps - 1 : numMipMaps;
    pddiTextureType textureType = (pddiTextureType)f->GetLong();
    pddiTextureUsageHint usage = (pddiTextureUsageHint)f->GetLong();
    int priority = f->GetLong();

    imageFactory->SetDesiredDepth(bpp);
    imageFactory->SetTextureHints(alphaDepth, numMipMaps, textureType, usage);

    tTexture* texture = NULL;

    int mipmap = 0;
    bool volume = false;
    bool image = false;
    while( (f->ChunksRemaining()) && (mipmap<=numMipMaps) )
    {           
        switch ( f->BeginChunk() )
        {
            case Pure3D::Texture::IMAGE:
            {
                if (!volume) 
                {
                    texture = LoadImage(f, imageFactory, texture, mipmap);
                    volume = false;
                    image = true;
                    mipmap++;
                }
                break;
            }
            case Pure3D::Texture::VOLUME_IMAGE:
            {
                if (!image)
                {
                    texture = LoadVolumeImage(f, imageFactory, texture, mipmap, numMipMaps);
                    volume = true;
                    image = false;
                    mipmap++;
                }
                break;
            }
            default:
                break;    
        }
        f->EndChunk();
    }
    if (texture != NULL)
    {
        texture->SetName(name);
        texture->SetPriority(priority);
#ifdef RAD_ANDROID
        RegisterSHARGrassTexture(texture, name, f->GetFilename());
        // RGB dilation changes only fully transparent sign texels. The original
        // alpha channel (including partial edge coverage) is left untouched.
        DilateL7KrustSignTransparentRGB(texture, name);
        RegisterSelectedShadowTexture(texture, name);
#endif
    }
    return texture;
}

tTexture* tTextureLoader::LoadImage(tChunkFile* f, tImageFactory* factory, tTexture* buildTexture, int mipLevel)
{
    if(!factory)
        factory = imageFactory;

    char name[128];
    f->GetPString(name);
    int version = f->GetLong();
    P3DASSERT(version == IMAGE_VERSION);   
    int width = f->GetLong();
    int height = f->GetLong();
    int bpp = f->GetLong();
    bool palettized = f->GetLong() == 1;
    bool alpha = f->GetLong() == 1;
    unsigned format = f->GetLong();

    tTexture* texture = buildTexture;

    while(f->ChunksRemaining())
    {
        switch(f->BeginChunk())
        {
            case Pure3D::Texture::IMAGE_DATA:
            {
                unsigned size = f->GetLong();

                if (texture == NULL)
                {
                    tFile* file = f->BeginInset();
                    texture = factory->ParseAsTexture( file, name, size, (tImageHandler::Format)format);
                    f->EndInset(file);
                }
                else
                {
                    tFile* file = f->BeginInset();
                    factory->ParseIntoTexture(file, texture, (tImageHandler::Format)format, mipLevel);
                    f->EndInset(file);
                }

                break;
            }

            case Pure3D::Texture::IMAGE_FILENAME:
            {
                char fileName[255];
                f->GetPString(fileName);
                if (texture == NULL)
                {
                    texture = factory->LoadAsTexture(fileName, name);
                }
                else
                {
                    factory->LoadIntoTexture(fileName, texture, mipLevel);
                }
                break;
            }

            default:
                break;
        }
        f->EndChunk();
    }

    return texture;
}

tTexture* tTextureLoader::LoadVolumeImage(tChunkFile* f, tImageFactory* factory, tTexture* buildTexture, int mipLevel, int numMipMaps)
{
    if(!factory)
        factory = imageFactory;

    char name[128];
    f->GetPString(name);
    int version = f->GetLong();
    P3DASSERT(version == VOLUME_IMAGE_VERSION);   
    int width = f->GetLong();
    int height = f->GetLong();
    int depth = f->GetLong();
    int bpp = f->GetLong();
    bool palettized = f->GetLong() == 1;
    bool alpha = f->GetLong() == 1;
    unsigned format = f->GetLong();

#ifdef RAD_XBOX
    P3DASSERT(depth>0);
    tTexture* texture = buildTexture;
    tImage** images = new tImage*[depth];
    bool autoStore = factory->GetAutoStore();
    factory->SetAutoStore(false);

    int imageNum = 0;

    while(f->ChunksRemaining())
    {
        switch(f->BeginChunk())
        {
            case Pure3D::Texture::IMAGE:
            {
                if (imageNum<depth)
                {              
                    bool found = false;
                    char buf[256];
                    sprintf(buf,"%s_%i",name,imageNum);
                    while((f->ChunksRemaining()) && (!found))
                    {
                        switch(f->BeginChunk())
                        {
                            case Pure3D::Texture::IMAGE_DATA:
                            {
                                unsigned size = f->GetLong();
                                tFile* file = f->BeginInset();
                                images[imageNum] = factory->ParseAsImage(file, buf, (tImageHandler::Format)format);
                                images[imageNum]->AddRef();
                                imageNum++;
                                f->EndInset(file);
                                found = true;
                                break;
                            }

                            case Pure3D::Texture::IMAGE_FILENAME:
                            {
                                char fileName[255];
                                f->GetPString(fileName);
                                images[imageNum] = factory->LoadAsImage(fileName, buf);
                                images[imageNum]->AddRef(); 
                                imageNum++;
                                found = true;
                                break;
                            }

                            default:
                                break;
                        }
                        f->EndChunk();
                    }
                }
                break;
            }
            default:
                break;
        }
        f->EndChunk();
    }

    if (imageNum>0)
    {
        if (texture == NULL)
        {
            texture = imageConverter->ImageToVolumeTexture(images, imageNum, numMipMaps);
        }
        else
        {
            texture = imageConverter->ImageInToVolumeTexture(images, imageNum, texture, mipLevel);
        }
    }

    factory->SetAutoStore(autoStore);

    for (int i = 0; i < imageNum; i++)
    {
        images[i]->Release();
    }

    delete [] images;
    return texture;

#else
    tTexture* texture = buildTexture;
    while(f->ChunksRemaining())
    {
        switch(f->BeginChunk())
        {
            case Pure3D::Texture::IMAGE:
            {
                if (texture==NULL)
                {
                    texture = LoadImage(f, factory, texture, 0);
                }
                break;
            }
            default:
                break;
        }
        f->EndChunk();
    }
    return texture;
#endif
}
