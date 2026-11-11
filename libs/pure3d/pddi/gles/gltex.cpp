//=============================================================================
// Copyright (c) 2002 Radical Games Ltd.  All rights reserved.
//=============================================================================

#include <pddi/gles/gl.hpp>
#include <pddi/gles/gldisplay.hpp>
#include <pddi/gles/gltex.hpp>
#include <pddi/gles/glcon.hpp>
#include <pddi/gles/decompress.hpp>

#include <math.h>
#include <pddi/base/debug.hpp>
#include <radmemory.hpp>
#if defined(RAD_ANDROID)
#include <SDL.h>
#include <SDL_system.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#endif

#include <microprofile.h>

#if defined(RAD_ANDROID)
static const int SHAR_ANISOTROPIC_FILTER_LEVELS[5] = { 1, 4, 8, 12, 16 };
static int gSHARAnisotropicFilteringSelection = 1; // 4x on new installs
static bool gSHARAnisotropicFilteringConfigurationLoaded = false;
static bool gSHARAnisotropyCapabilityChecked = false;
static bool gSHARAnisotropySupported = false;
static GLfloat gSHARMaximumAnisotropy = 1.0f;
static const GLenum SHAR_GL_TEXTURE_MAX_ANISOTROPY_EXT = 0x84FE;
static const GLenum SHAR_GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT = 0x84FF;

static bool BuildSHARAnisotropicConfigurationPath( char* path, size_t pathSize )
{
    const char* basePath = SDL_AndroidGetApplicationExternalFilesPath();
    if( basePath == NULL || basePath[0] == '\0' )
        return false;
    char directory[512];
    const int length = snprintf(directory, sizeof(directory), "%s/Graphics", basePath);
    if( length < 0 || length >= static_cast<int>(sizeof(directory)) )
        return false;
    struct stat info;
    if( mkdir(directory, 0777) != 0 && errno != EEXIST )
        return false;
    if( stat(directory, &info) != 0 || !S_ISDIR(info.st_mode) )
        return false;
    const int pathLength = snprintf(path, pathSize, "%s/Simpsons_anisotropic_filtering.txt", directory);
    return pathLength >= 0 && pathLength < static_cast<int>(pathSize);
}

static void WriteSHARAnisotropicFilteringConfiguration()
{
    char path[512];
    if( !BuildSHARAnisotropicConfigurationPath(path, sizeof(path)) )
        return;

    FILE* file = fopen(path, "w");
    if( file == NULL )
        return;

    fprintf(file, "# Simpsons Hit & Run - Revamped! [AF Settings]\n");
    fprintf(file, "# Improves the quality of textures at distant oblique angles.\n");
    fprintf(file, "# Change with OFF/4x/8x/12x/16x values.\n");
    fprintf(file, "# You can also use the in-game settings toggle.\n");
    fprintf(file, "# Changes to this file will be reflected upon restart.\n");
    fprintf(file, "\n");
    if (gSHARAnisotropicFilteringSelection == 0)
        fprintf(file, "anisotropy=OFF\n");
    else
        fprintf(file, "anisotropy=%dx\n", SHAR_ANISOTROPIC_FILTER_LEVELS[gSHARAnisotropicFilteringSelection]);
    fclose(file);
}
static void LoadSHARAnisotropicFilteringConfiguration()
{
    if( gSHARAnisotropicFilteringConfigurationLoaded )
        return;
    gSHARAnisotropicFilteringConfigurationLoaded = true;
    char path[512];
    if( !BuildSHARAnisotropicConfigurationPath(path, sizeof(path)) )
        return;
    FILE* file = fopen(path, "r");
    if( file == NULL )
    {
        WriteSHARAnisotropicFilteringConfiguration();
        return;
    }
    char line[256];
    while( fgets(line, sizeof(line), file) != NULL )
    {
        char* value = strstr(line, "anisotropy=");
        if( value == NULL )
            continue;
        value += strlen("anisotropy=");
        const int requested = static_cast<int>(strtol(value, NULL, 10));
        if (requested <= 1)
        {
            gSHARAnisotropicFilteringSelection = 0;
        }
        else if (requested == 2)
        {
            // Existing installations used 2x as their lowest enabled level.
            // Normalize it to the new lowest enabled choice, 4x.
            gSHARAnisotropicFilteringSelection = 1;
        }
        else
        {
            for( int i = 1; i < 5; ++i )
            {
                if( requested == SHAR_ANISOTROPIC_FILTER_LEVELS[i] )
                {
                    gSHARAnisotropicFilteringSelection = i;
                    break;
                }
            }
        }
        break;
    }
    fclose(file);
}

int GetSHARAnisotropicFilteringSelection()
{
    LoadSHARAnisotropicFilteringConfiguration();
    return gSHARAnisotropicFilteringSelection;
}

void SetSHARAnisotropicFilteringSelection( int selection )
{
    LoadSHARAnisotropicFilteringConfiguration();
    if( selection < 0 ) selection = 0;
    if( selection > 4 ) selection = 4;
    if( gSHARAnisotropicFilteringSelection == selection )
        return;
    gSHARAnisotropicFilteringSelection = selection;
    WriteSHARAnisotropicFilteringConfiguration();
}

static void ApplySHARAnisotropicFilteringToBoundTexture( int selection )
{
    if( !gSHARAnisotropyCapabilityChecked )
    {
        gSHARAnisotropyCapabilityChecked = true;
        if( SDL_GL_ExtensionSupported("GL_EXT_texture_filter_anisotropic") )
        {
            GLfloat maximum = 1.0f;
            glGetFloatv(SHAR_GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maximum);
            if( maximum >= 1.0f )
            {
                gSHARMaximumAnisotropy = maximum;
                gSHARAnisotropySupported = true;
            }
        }
    }
    if( !gSHARAnisotropySupported )
        return;
    if( selection < 0 ) selection = 0;
    if( selection > 4 ) selection = 4;
    GLfloat requested = selection == 0 ? 1.0f :
        static_cast<GLfloat>(SHAR_ANISOTROPIC_FILTER_LEVELS[selection]);
    if( requested > gSHARMaximumAnisotropy )
        requested = gSHARMaximumAnisotropy;
    glTexParameterf(GL_TEXTURE_2D, SHAR_GL_TEXTURE_MAX_ANISOTROPY_EXT, requested);
}
#endif

static inline GLenum PickPixelFormat(pddiPixelFormat format)
{
    switch (format)
    {
    case PDDI_PIXEL_RGB888: return GL_BGRA_EXT;
    case PDDI_PIXEL_ARGB8888: return GL_BGRA_EXT;
#ifdef RAD_VITAGL
    case PDDI_PIXEL_DXT1: return GL_COMPRESSED_RGB_S3TC_DXT1_EXT;
    case PDDI_PIXEL_DXT3: return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
    case PDDI_PIXEL_DXT5: return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
#else
    case PDDI_PIXEL_DXT1: return GL_RGBA;
    case PDDI_PIXEL_DXT3: return GL_RGBA;
    case PDDI_PIXEL_DXT5: return GL_RGBA;
#endif
    }
    PDDIASSERT(false);
    return GL_INVALID_ENUM;
};

static inline pddiPixelFormat PickPixelFormat(pddiTextureType type, int bitDepth, int alphaDepth)
{
    switch (type)
    {
    case PDDI_TEXTYPE_RGB:
        switch (alphaDepth)
        {
        case 0:
            return (bitDepth <= 16) ? PDDI_PIXEL_RGB565 : PDDI_PIXEL_RGB888;
        case 1:
            return (bitDepth <= 16) ? PDDI_PIXEL_ARGB1555 : PDDI_PIXEL_ARGB8888;
        default:
            return (bitDepth <= 16) ? PDDI_PIXEL_ARGB4444 : PDDI_PIXEL_ARGB8888;
        }
        break;

    case PDDI_TEXTYPE_PALETTIZED:
        return PDDI_PIXEL_PAL8;

    case PDDI_TEXTYPE_LUMINANCE:
        return PDDI_PIXEL_LUM8;

    case PDDI_TEXTYPE_BUMPMAP:
        return PDDI_PIXEL_DUDV88;

    case PDDI_TEXTYPE_DXT1:
        return PDDI_PIXEL_DXT1;

    case PDDI_TEXTYPE_DXT2:
        return PDDI_PIXEL_DXT2;

    case PDDI_TEXTYPE_DXT3:
        return PDDI_PIXEL_DXT3;

    case PDDI_TEXTYPE_DXT4:
        return PDDI_PIXEL_DXT4;

    case PDDI_TEXTYPE_DXT5:
        return PDDI_PIXEL_DXT5;

    case PDDI_TEXTYPE_YUV:
        return PDDI_PIXEL_YUV;
    }
    PDDIASSERT(false);
    return PDDI_PIXEL_UNKNOWN;
};

#if defined(RAD_ANDROID)
// The compressed-texture extensions are optional on Android. Cache their
// availability for the current SDL GL context and keep the RGBA decoder as
// the compatibility path for unsupported formats.
static SDL_GLContext gSHARDxtCompressionContext = NULL;
static bool gSHARDxtCompressionCapabilitiesChecked = false;
static bool gSHARDxt1CompressionSupported = false;
static bool gSHARDxt3CompressionSupported = false;
static bool gSHARDxt5CompressionSupported = false;

static bool IsSHARGlExtensionSupported(const char* extension)
{
    return SDL_GL_ExtensionSupported(extension) == SDL_TRUE;
}

static void CheckSHARDxtCompressionCapabilities()
{
    const SDL_GLContext currentContext = SDL_GL_GetCurrentContext();
    if (currentContext == NULL)
        return;

    if (gSHARDxtCompressionCapabilitiesChecked &&
        gSHARDxtCompressionContext == currentContext)
    {
        return;
    }

    gSHARDxtCompressionContext = currentContext;
    gSHARDxtCompressionCapabilitiesChecked = true;

    // The full S3TC extension includes DXT1, DXT3 and DXT5. Some drivers
    // advertise the formats separately, so check each format independently.
    const bool fullS3tcSupport =
        GLAD_GL_EXT_texture_compression_s3tc != 0 ||
        IsSHARGlExtensionSupported("GL_EXT_texture_compression_s3tc") ||
        IsSHARGlExtensionSupported("GL_OES_texture_compression_S3TC");

    gSHARDxt1CompressionSupported =
        fullS3tcSupport ||
        IsSHARGlExtensionSupported("GL_EXT_texture_compression_dxt1");
    gSHARDxt3CompressionSupported =
        fullS3tcSupport ||
        IsSHARGlExtensionSupported("GL_EXT_texture_compression_dxt3");
    gSHARDxt5CompressionSupported =
        fullS3tcSupport ||
        IsSHARGlExtensionSupported("GL_EXT_texture_compression_dxt5");

}

static bool IsSHARDxtCompressionSupported(pddiTextureType type)
{
    CheckSHARDxtCompressionCapabilities();

    switch (type)
    {
    case PDDI_TEXTYPE_DXT1:
        return gSHARDxt1CompressionSupported;
    case PDDI_TEXTYPE_DXT3:
        return gSHARDxt3CompressionSupported;
    case PDDI_TEXTYPE_DXT5:
        return gSHARDxt5CompressionSupported;
    default:
        return false;
    }
}

static GLenum GetSHARDxtInternalFormat(pddiTextureType type)
{
    switch (type)
    {
    case PDDI_TEXTYPE_DXT1:
        // Use the RGBA variant because DXT1 assets can contain 1-bit alpha.
        return GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
    case PDDI_TEXTYPE_DXT3:
        return GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
    case PDDI_TEXTYPE_DXT5:
        return GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
    default:
        return GL_INVALID_ENUM;
    }
}

static GLenum DrainSHARTextureUploadErrors()
{
    GLenum firstError = GL_NO_ERROR;
    for (int i = 0; i < 16; ++i)
    {
        const GLenum error = glGetError();
        if (error == GL_NO_ERROR)
            break;
        if (firstError == GL_NO_ERROR)
            firstError = error;
    }
    return firstError;
}

static bool TryUploadSHARDxtMipChain(
    pddiTextureType type, int width, int height, int lastMipLevel, char** mipBits)
{
    // Do not mistake an earlier renderer error for a failure of the compressed
    // upload. Report and clear any pending GL errors before testing this path.
    const GLenum pendingError = DrainSHARTextureUploadErrors();
    if (pendingError != GL_NO_ERROR)
    {
        SDL_LogError(SDL_LOG_CATEGORY_RENDER,
            "[GRAPHICS][TEXTURES] Pending GL error 0x%04X before DXT upload; using RGBA fallback",
            static_cast<unsigned int>(pendingError));
        return false;
    }

    const GLenum internalFormat = GetSHARDxtInternalFormat(type);
    const unsigned int blockSize = type == PDDI_TEXTYPE_DXT1 ? 8u : 16u;

    for (int mipLevel = 0; mipLevel <= lastMipLevel; ++mipLevel)
    {
        const int mipWidth = width >> mipLevel;
        const int mipHeight = height >> mipLevel;
        const size_t blocksWide = (static_cast<size_t>(mipWidth) + 3u) / 4u;
        const size_t blocksHigh = (static_cast<size_t>(mipHeight) + 3u) / 4u;
        const size_t imageBytes = blocksWide * blocksHigh * blockSize;

        if (imageBytes == 0 || imageBytes > static_cast<size_t>(0x7fffffff))
        {
            SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                "[GRAPHICS][TEXTURES] Invalid DXT mip size (%dx%d); using RGBA fallback",
                mipWidth, mipHeight);
            return false;
        }

        glCompressedTexImage2D(
            GL_TEXTURE_2D, mipLevel, internalFormat,
            mipWidth, mipHeight, 0, static_cast<GLsizei>(imageBytes),
            (const GLvoid*)mipBits[mipLevel]);

        const GLenum uploadError = DrainSHARTextureUploadErrors();
        if (uploadError != GL_NO_ERROR)
        {
            SDL_LogError(SDL_LOG_CATEGORY_RENDER,
                "[GRAPHICS][TEXTURES] DXT upload failed at mip %d (GL error 0x%04X); using RGBA fallback",
                mipLevel, static_cast<unsigned int>(uploadError));
            return false;
        }
    }

    return true;
}

static void UploadSHARDecompressedDxtMip(
    pddiTextureType type, pddiPixelFormat pixelFormat,
    int width, int height, int mipLevel, const char* compressedBits)
{
    const size_t imageBytes =
        static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
    unsigned char* image = new unsigned char[imageBytes];

    if (type == PDDI_TEXTYPE_DXT1)
        BlockDecompressImageBC1(width, height,
            (const uint8_t*)compressedBits, image);
    else if (type == PDDI_TEXTYPE_DXT3)
        BlockDecompressImageBC2(width, height,
            (const uint8_t*)compressedBits, image);
    else
        BlockDecompressImageBC3(width, height,
            (const uint8_t*)compressedBits, image);

    glTexImage2D(GL_TEXTURE_2D, mipLevel, PickPixelFormat(pixelFormat),
        width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image);
    delete [] image;
}
#endif

void pglTexture::SetGLState(void)
{
    if(context->contextID != contextID)
    {
        contextID = context->contextID;
        gltexture = 0;
        valid = false;
        samplerParamsValid = false;
#if defined(RAD_ANDROID)
        mipmapsAvailable = false;
        mipmapGenerationAttempted = false;
        anisotropySelectionApplied = -1;
#endif
    }

    MICROPROFILE_SCOPEI("PDDI", "pglTexture::SetGLState", MP_RED);

    if(!valid)
    {
        samplerParamsValid = false;
        glDeleteTextures(1, &gltexture);
        glGenTextures(1,&gltexture);
        glBindTexture(GL_TEXTURE_2D, gltexture);

#if defined(RAD_ANDROID)
        // Use authored mip levels only when they form a complete square chain.
        // Lock() cannot safely expose levels beyond a non-square texture's
        // shorter dimension, so generate those chains from level 0 on demand.
        mipmapsAvailable = false;
        mipmapGenerationAttempted = false;
        const bool hasCompleteSourceMipChain =
            (log2X == log2Y && nMipMap >= log2X);
        const int lastSourceMipLevel = hasCompleteSourceMipChain ? log2X : 0;

        const bool isDxtTexture =
            type == PDDI_TEXTYPE_DXT1 || type == PDDI_TEXTYPE_DXT3 ||
            type == PDDI_TEXTYPE_DXT5;

        if (isDxtTexture)
        {
            // glGenerateMipmap is not valid for a compressed base level on
            // GLES. Use native DXT only when every authored mip is available;
            // otherwise retain RGBA so the existing lazy mipmap generation works.
            const bool nativeDxtAvailable =
                hasCompleteSourceMipChain && IsSHARDxtCompressionSupported(type);
            const bool nativeDxtUploaded = nativeDxtAvailable &&
                TryUploadSHARDxtMipChain(type, xSize, ySize,
                    lastSourceMipLevel, bits);

            if (!nativeDxtUploaded)
            {
                // If a driver advertised support but rejected an upload, discard
                // the partially defined compressed texture before retrying the
                // complete chain through the established RGBA decoder.
                if (nativeDxtAvailable)
                {
                    glDeleteTextures(1, &gltexture);
                    glGenTextures(1, &gltexture);
                    glBindTexture(GL_TEXTURE_2D, gltexture);
                }

                for (int mipLevel = 0;
                     mipLevel <= lastSourceMipLevel; ++mipLevel)
                {
                    const int mipWidth = xSize >> mipLevel;
                    const int mipHeight = ySize >> mipLevel;
                    UploadSHARDecompressedDxtMip(
                        type, lock.format, mipWidth, mipHeight,
                        mipLevel, bits[mipLevel]);
                }
            }
        }

        for (int mipLevel = 0; mipLevel <= lastSourceMipLevel; ++mipLevel)
        {
            // DXT textures were uploaded above, either natively or through the
            // RGBA fallback. Keep the ordinary texture/YUV upload loop separate.
            if (isDxtTexture)
                continue;

            const int mipWidth = xSize >> mipLevel;
            const int mipHeight = ySize >> mipLevel;

#ifdef RAD_VITAGL
            if (type == PDDI_TEXTYPE_YUV)
            {
                glCompressedTexImage2D(GL_TEXTURE_2D, mipLevel, VGL_YUV420P_BT601,
                    mipWidth, mipHeight, 0,
                    (mipWidth * mipHeight * 3) / 2, (GLvoid*)bits[mipLevel]);
            }
            else
#endif
            {
                glTexImage2D(GL_TEXTURE_2D, mipLevel, PickPixelFormat(lock.format),
                    mipWidth, mipHeight, 0,
                    lock.native ? GL_BGRA_EXT : GL_RGBA, GL_UNSIGNED_BYTE,
                    (GLvoid*)bits[mipLevel]);
            }
        }

        mipmapsAvailable = hasCompleteSourceMipChain;
        mipmapGenerationAttempted = hasCompleteSourceMipChain;
#else
        if (type == PDDI_TEXTYPE_DXT1 || type == PDDI_TEXTYPE_DXT3 ||
            type == PDDI_TEXTYPE_DXT5)
        {
#ifdef RAD_VITAGL
            unsigned int blocksize = lock.format == PDDI_PIXEL_DXT1 ? 8 : 16;
            GLenum internalFormat = lock.format == PDDI_PIXEL_DXT5 ? GL_COMPRESSED_RGBA_S3TC_DXT5_EXT :
                lock.format == PDDI_PIXEL_DXT3 ? GL_COMPRESSED_RGBA_S3TC_DXT3_EXT : GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
            glCompressedTexImage2D(GL_TEXTURE_2D, 0, internalFormat, xSize,
                ySize, 0, ceil(xSize/4.0)*ceil(ySize/4.0)*blocksize, (GLvoid*)bits[0]);
#else
            unsigned char* image = new unsigned char[xSize * ySize * 4];
            if (type == PDDI_TEXTYPE_DXT1)
                BlockDecompressImageBC1(xSize, ySize, (const uint8_t*)bits[0], image);
            else if (type == PDDI_TEXTYPE_DXT3)
                BlockDecompressImageBC2(xSize, ySize, (const uint8_t*)bits[0], image);
            else if( type == PDDI_TEXTYPE_DXT5)
                BlockDecompressImageBC3(xSize, ySize, (const uint8_t*)bits[0], image);
            glTexImage2D(GL_TEXTURE_2D, 0, PickPixelFormat(lock.format), xSize,
                ySize, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                image);
            delete [] image;
#endif
        }
#ifdef RAD_VITAGL
        else if (type == PDDI_TEXTYPE_YUV)
        {
            glCompressedTexImage2D(GL_TEXTURE_2D, 0, VGL_YUV420P_BT601, xSize,
                ySize, 0, (xSize * ySize * 3) / 2, (GLvoid *)bits[0]);
        }
#endif
        else
        {
            glTexImage2D(GL_TEXTURE_2D, 0, PickPixelFormat(lock.format), xSize,
                ySize, 0, lock.native ? GL_BGRA_EXT : GL_RGBA, GL_UNSIGNED_BYTE,
                (GLvoid *)bits[0]);
        }
#endif

        valid = true;
    }
    else
    {
        glBindTexture(GL_TEXTURE_2D, gltexture);
    }

#if defined(RAD_ANDROID)
    const int anisotropySelection = GetSHARAnisotropicFilteringSelection();
    if( anisotropySelectionApplied != anisotropySelection )
    {
        // Only touch the texture parameter when the user has changed the level.
        anisotropySelectionApplied = anisotropySelection;
        ApplySHARAnisotropicFilteringToBoundTexture(anisotropySelection);
    }
#endif
}

int fastlog2(int x)
{
    int r = 0;
    int tmp = x;
    while(tmp > 1)
    {
        r++;
        tmp = tmp >> 1;

        if((tmp << r) != x)
            // not power of 2
            return -1;
    }
    return r;
}

bool pglTexture::Create(int x, int y, int bpp, int alphaDepth, int nMip, pddiTextureType textureType, pddiTextureUsageHint usageHint)
{
    xSize = x;
    ySize = y;
    nMipMap = nMip;
    type = textureType;
    sourceAlphaDepth = alphaDepth;

    log2X = fastlog2(xSize);
    log2Y = fastlog2(ySize);

#ifndef RAD_VITA
    if((log2X == -1) || (log2Y == -1))
    {
        lastError = PDDI_TEX_NOT_POW_2;
        return false;
    }
#endif

    if ((xSize > context->GetMaxTextureDimension()) ||
        (ySize > context->GetMaxTextureDimension()))
    {
        lastError = PDDI_TEX_TOO_BIG;
        return false;
    }

    // TODO palletized
    if (textureType == PDDI_TEXTYPE_PALETTIZED)
    {
        textureType = PDDI_TEXTYPE_RGB;
        bpp = 32;
    }

    bits = new char* [nMipMap + 1];
    if (type == PDDI_TEXTYPE_DXT1 || type == PDDI_TEXTYPE_DXT3 || type == PDDI_TEXTYPE_DXT5)
    {
        unsigned int blocksize = type == PDDI_TEXTYPE_DXT1 ? 8 : 16;
        for(int i = 0; i < nMipMap+1; i++)
            bits[i] = (char*)radMemoryAllocAligned(radMemoryGetCurrentAllocator(), size_t(ceil(double(xSize>>i)/4)*ceil(double(ySize>>i)/4)*blocksize), 16);
    }
    else
    {
        for(int i = 0; i < nMipMap+1; i++)
            bits[i] = (char*)radMemoryAllocAligned(radMemoryGetCurrentAllocator(), ((xSize>>i)*(ySize>>i)*bpp)/8, 16);
    }

    lock.depth = bpp;
    lock.format = PickPixelFormat(textureType, bpp, alphaDepth);

    if(context->GetDisplay()->ExtBGRA())
    {
        lock.native = true;
        lock.rgbaLShift[0] = lock.rgbaRShift[0] =
        lock.rgbaLShift[1] = lock.rgbaRShift[1] =
        lock.rgbaLShift[2] = lock.rgbaRShift[2] =
        lock.rgbaLShift[3] = lock.rgbaRShift[3] = 0;

        lock.rgbaMask[0] = 0x00ff0000;
        lock.rgbaMask[1] = 0x0000ff00;
        lock.rgbaMask[2] = 0x000000ff;
        lock.rgbaMask[3] = 0xff000000;
    }
    else
    {
        lock.native = false;
        lock.rgbaRShift[0] = 16;
        lock.rgbaLShift[2] = 16;

        lock.rgbaLShift[0] = 
        lock.rgbaLShift[1] = lock.rgbaRShift[1] =
        lock.rgbaRShift[2] =
        lock.rgbaLShift[3] = lock.rgbaRShift[3] = 0;

        lock.rgbaMask[0] = 0x000000ff;
        lock.rgbaMask[1] = 0x0000ff00;
        lock.rgbaMask[2] = 0x00ff0000;
        lock.rgbaMask[3] = 0xff000000;
    }

    context->ADD_STAT(PDDI_STAT_TEXTURE_ALLOC_32BIT, (float)((xSize * ySize * lock.depth) / 8192));
    context->ADD_STAT(PDDI_STAT_TEXTURE_COUNT_32BIT, 1);

    return true;
}

pglTexture::pglTexture(pglContext* c)
{
    context = c;
    contextID = c->contextID;
    sourceAlphaDepth = -1;
    bits = NULL;
    gltexture = 0;
    priority = 15;
    valid = false;
    samplerParamsValid = false;
#if defined(RAD_ANDROID)
    anisotropySelectionApplied = -1;
    mipmapsAvailable = false;
    mipmapGenerationAttempted = false;
#endif
}

pglTexture::~pglTexture()
{
    if(gltexture) glDeleteTextures(1, &gltexture);

    if(bits)
    {
        for(int i = 0; i < nMipMap + 1; i++)
            radMemoryFreeAligned(bits[i]);
        delete [] bits;
    }

    context->ADD_STAT(PDDI_STAT_TEXTURE_ALLOC_32BIT, -(float)((xSize * ySize * lock.depth) / 8192));
    context->ADD_STAT(PDDI_STAT_TEXTURE_COUNT_32BIT, -1);
}

pddiPixelFormat pglTexture::GetPixelFormat()
{
    return PDDI_PIXEL_ARGB8888;
}

int   pglTexture::GetWidth()
{
    return xSize;
}

int   pglTexture::GetHeight()
{
    return ySize;
}

int   pglTexture::GetDepth()
{
    return 32;
}

int   pglTexture::GetNumMipMaps()
{
    return nMipMap;
}

int pglTexture::GetAlphaDepth()
{
    return 8;
}

pddiLockInfo* pglTexture::Lock(int mipMap, pddiRect* rect)
{
    PDDIASSERT(mipMap <= nMipMap);

    lock.width = 1 << (log2X-mipMap);
    lock.height = 1 << (log2Y-mipMap);
    if (lock.format == PDDI_PIXEL_DXT1 || lock.format == PDDI_PIXEL_DXT3 || lock.format == PDDI_PIXEL_DXT5)
    {
        unsigned int blocksize = lock.format == PDDI_PIXEL_DXT1 ? 8 : 16;
        lock.pitch = ceil( double( xSize >> mipMap ) / 4 ) * blocksize;
        lock.bits = bits[mipMap];
    }
    else if (lock.format == PDDI_PIXEL_YUV)
    {
        lock.pitch = (lock.width * lock.depth) / 8;
        lock.bits = bits[mipMap];
    }
    else
    {
        lock.pitch = -(lock.width * 4);
        lock.bits = bits[mipMap] + (lock.width * (lock.height - 1) * 4);
    }

    return &lock;
}

void pglTexture::Unlock(int mipLevel)
{
    valid = false;
}

void pglTexture::SetPriority(int p)
{
    priority = p;
}

int pglTexture::GetPriority(void)
{
    return priority;
}

// paging control

void pglTexture::SetSamplerParameters(unsigned int magFilter,unsigned int minFilter,
                                      unsigned int wrapS,unsigned int wrapT)
{
#if defined(RAD_ANDROID)
    const bool requestsMipmaps =
        minFilter == GL_NEAREST_MIPMAP_NEAREST ||
        minFilter == GL_LINEAR_MIPMAP_NEAREST ||
        minFilter == GL_LINEAR_MIPMAP_LINEAR;

    // Avoid allocating mip levels for textures whose materials never request
    // mipmapped filtering. Generate a complete chain once, on the first request.
    if (requestsMipmaps && !mipmapsAvailable && !mipmapGenerationAttempted)
    {
        mipmapGenerationAttempted = true;
        glGenerateMipmap(GL_TEXTURE_2D);
        mipmapsAvailable = (glGetError() == GL_NO_ERROR);
    }

    // If a driver rejects generation for this texture, keep it complete and
    // sample its base level instead of leaving a mipmapped-incomplete texture.
    if (requestsMipmaps && !mipmapsAvailable)
    {
        minFilter = minFilter == GL_NEAREST_MIPMAP_NEAREST ? GL_NEAREST : GL_LINEAR;
    }
#endif

    if(!samplerParamsValid||samplerMagFilter!=magFilter) glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,magFilter);
    if(!samplerParamsValid||samplerMinFilter!=minFilter) glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,minFilter);
    if(!samplerParamsValid||samplerWrapS!=wrapS) glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,wrapS);
    if(!samplerParamsValid||samplerWrapT!=wrapT) glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,wrapT);
    samplerMagFilter=magFilter; samplerMinFilter=minFilter; samplerWrapS=wrapS; samplerWrapT=wrapT;
    samplerParamsValid=true;
}

void pglTexture::Prefetch(void)
{
}

void pglTexture::Discard(void)
{
}

// palette managment
int pglTexture::GetNumPaletteEntries(void)
{
    return 0;
}

void pglTexture::SetPalette(int nEntries, pddiColour* palette)
{
}

int pglTexture::GetPalette(pddiColour* palette)
{
    return 0;
}


