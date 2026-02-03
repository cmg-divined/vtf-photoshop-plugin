#pragma once

#include <cstdint>

// VTF File Format Definitions
// Based on Valve's VTF specification

#pragma pack(push, 1)

// VTF Header (version 7.0-7.5)
struct VTFHeader {
    char        signature[4];       // "VTF\0"
    uint32_t    version[2];         // Version (major, minor)
    uint32_t    headerSize;         // Size of header
    uint16_t    width;              // Width of largest mipmap
    uint16_t    height;             // Height of largest mipmap
    uint32_t    flags;              // VTF flags
    uint16_t    frames;             // Number of frames (animated textures)
    uint16_t    firstFrame;         // First frame index
    uint8_t     padding0[4];
    float       reflectivity[3];    // Reflectivity vector
    uint8_t     padding1[4];
    float       bumpmapScale;       // Bumpmap scale
    uint32_t    highResImageFormat; // High-res image format
    uint8_t     mipmapCount;        // Number of mipmaps
    uint32_t    lowResImageFormat;  // Low-res (thumbnail) format
    uint8_t     lowResImageWidth;   // Low-res width
    uint8_t     lowResImageHeight;  // Low-res height
    
    // Version 7.2+
    uint16_t    depth;              // Depth of texture (for 3D textures)
    
    // Version 7.3+
    uint8_t     padding2[3];
    uint32_t    numResources;       // Number of resources
    uint8_t     padding3[8];
};

#pragma pack(pop)

// VTF Image Formats
enum VTFImageFormat {
    IMAGE_FORMAT_NONE = -1,
    IMAGE_FORMAT_RGBA8888 = 0,
    IMAGE_FORMAT_ABGR8888,
    IMAGE_FORMAT_RGB888,
    IMAGE_FORMAT_BGR888,
    IMAGE_FORMAT_RGB565,
    IMAGE_FORMAT_I8,
    IMAGE_FORMAT_IA88,
    IMAGE_FORMAT_P8,
    IMAGE_FORMAT_A8,
    IMAGE_FORMAT_RGB888_BLUESCREEN,
    IMAGE_FORMAT_BGR888_BLUESCREEN,
    IMAGE_FORMAT_ARGB8888,
    IMAGE_FORMAT_BGRA8888,
    IMAGE_FORMAT_DXT1,
    IMAGE_FORMAT_DXT3,
    IMAGE_FORMAT_DXT5,
    IMAGE_FORMAT_BGRX8888,
    IMAGE_FORMAT_BGR565,
    IMAGE_FORMAT_BGRX5551,
    IMAGE_FORMAT_BGRA4444,
    IMAGE_FORMAT_DXT1_ONEBITALPHA,
    IMAGE_FORMAT_BGRA5551,
    IMAGE_FORMAT_UV88,
    IMAGE_FORMAT_UVWQ8888,
    IMAGE_FORMAT_RGBA16161616F,
    IMAGE_FORMAT_RGBA16161616,
    IMAGE_FORMAT_UVLX8888,
    IMAGE_FORMAT_COUNT
};

// VTF Flags
enum VTFFlags {
    TEXTUREFLAGS_POINTSAMPLE        = 0x00000001,
    TEXTUREFLAGS_TRILINEAR          = 0x00000002,
    TEXTUREFLAGS_CLAMPS             = 0x00000004,
    TEXTUREFLAGS_CLAMPT             = 0x00000008,
    TEXTUREFLAGS_ANISOTROPIC        = 0x00000010,
    TEXTUREFLAGS_HINT_DXT5          = 0x00000020,
    TEXTUREFLAGS_PWL_CORRECTED      = 0x00000040,
    TEXTUREFLAGS_NORMAL             = 0x00000080,
    TEXTUREFLAGS_NOMIP              = 0x00000100,
    TEXTUREFLAGS_NOLOD              = 0x00000200,
    TEXTUREFLAGS_ALL_MIPS           = 0x00000400,
    TEXTUREFLAGS_PROCEDURAL         = 0x00000800,
    TEXTUREFLAGS_ONEBITALPHA        = 0x00001000,
    TEXTUREFLAGS_EIGHTBITALPHA      = 0x00002000,
    TEXTUREFLAGS_ENVMAP             = 0x00004000,
    TEXTUREFLAGS_RENDERTARGET       = 0x00008000,
    TEXTUREFLAGS_DEPTHRENDERTARGET  = 0x00010000,
    TEXTUREFLAGS_NODEBUGOVERRIDE    = 0x00020000,
    TEXTUREFLAGS_SINGLECOPY         = 0x00040000,
    TEXTUREFLAGS_PRE_SRGB           = 0x00080000,
    TEXTUREFLAGS_CLAMPU             = 0x02000000,
    TEXTUREFLAGS_VERTEXTEXTURE      = 0x04000000,
    TEXTUREFLAGS_SSBUMP             = 0x08000000,
    TEXTUREFLAGS_BORDER             = 0x20000000,
};

// Get bytes per pixel for a format
inline int GetBytesPerPixel(VTFImageFormat format) {
    switch (format) {
        case IMAGE_FORMAT_RGBA8888:
        case IMAGE_FORMAT_ABGR8888:
        case IMAGE_FORMAT_ARGB8888:
        case IMAGE_FORMAT_BGRA8888:
        case IMAGE_FORMAT_BGRX8888:
        case IMAGE_FORMAT_UVWQ8888:
        case IMAGE_FORMAT_UVLX8888:
            return 4;
        case IMAGE_FORMAT_RGB888:
        case IMAGE_FORMAT_BGR888:
        case IMAGE_FORMAT_RGB888_BLUESCREEN:
        case IMAGE_FORMAT_BGR888_BLUESCREEN:
            return 3;
        case IMAGE_FORMAT_RGB565:
        case IMAGE_FORMAT_BGR565:
        case IMAGE_FORMAT_BGRX5551:
        case IMAGE_FORMAT_BGRA5551:
        case IMAGE_FORMAT_BGRA4444:
        case IMAGE_FORMAT_IA88:
        case IMAGE_FORMAT_UV88:
            return 2;
        case IMAGE_FORMAT_I8:
        case IMAGE_FORMAT_P8:
        case IMAGE_FORMAT_A8:
            return 1;
        case IMAGE_FORMAT_RGBA16161616F:
        case IMAGE_FORMAT_RGBA16161616:
            return 8;
        default:
            return 0; // Compressed formats handled separately
    }
}

// Calculate image size for compressed formats
inline size_t CalculateImageSize(int width, int height, VTFImageFormat format) {
    if (width < 1) width = 1;
    if (height < 1) height = 1;
    
    switch (format) {
        case IMAGE_FORMAT_DXT1:
        case IMAGE_FORMAT_DXT1_ONEBITALPHA:
            return ((width + 3) / 4) * ((height + 3) / 4) * 8;
        case IMAGE_FORMAT_DXT3:
        case IMAGE_FORMAT_DXT5:
            return ((width + 3) / 4) * ((height + 3) / 4) * 16;
        default:
            return width * height * GetBytesPerPixel(format);
    }
}

// Check if format has alpha
inline bool FormatHasAlpha(VTFImageFormat format) {
    switch (format) {
        case IMAGE_FORMAT_RGBA8888:
        case IMAGE_FORMAT_ABGR8888:
        case IMAGE_FORMAT_ARGB8888:
        case IMAGE_FORMAT_BGRA8888:
        case IMAGE_FORMAT_BGRA5551:
        case IMAGE_FORMAT_BGRA4444:
        case IMAGE_FORMAT_DXT1_ONEBITALPHA:
        case IMAGE_FORMAT_DXT3:
        case IMAGE_FORMAT_DXT5:
        case IMAGE_FORMAT_A8:
        case IMAGE_FORMAT_IA88:
        case IMAGE_FORMAT_RGBA16161616F:
        case IMAGE_FORMAT_RGBA16161616:
            return true;
        default:
            return false;
    }
}
