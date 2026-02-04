#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include "VTFFormat.h"
#include "DXTDecompress.h"

class VTFLoader {
public:
    VTFLoader();
    ~VTFLoader();
    
    // Load a VTF file
    bool Load(const char* filename);
    bool Load(const wchar_t* filename);
    bool LoadFromMemory(const uint8_t* data, size_t size);
    
    // Get image properties
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    int GetFrameCount() const { return m_frameCount; }
    int GetMipmapCount() const { return m_mipmapCount; }
    bool HasAlpha() const { return m_hasAlpha; }
    VTFImageFormat GetFormat() const { return m_format; }
    
    // Get decoded RGBA data (always returns RGBA8888)
    // Returns pointer to internal buffer, valid until next Load() or destruction
    const uint8_t* GetRGBAData(int frame = 0, int mipmap = 0);
    
    // Get last error message
    const std::string& GetError() const { return m_error; }
    
private:
    bool ParseHeader(const uint8_t* data, size_t size);
    bool DecodeImage(const uint8_t* srcData, size_t srcSize);
    void ConvertToRGBA(const uint8_t* src, uint8_t* dst, int width, int height, VTFImageFormat format);
    
    // Image properties
    int m_width = 0;
    int m_height = 0;
    int m_frameCount = 0;
    int m_mipmapCount = 0;
    bool m_hasAlpha = false;
    VTFImageFormat m_format = IMAGE_FORMAT_NONE;
    
    // Version info
    int m_versionMajor = 0;
    int m_versionMinor = 0;
    
    // Raw file data
    std::vector<uint8_t> m_fileData;
    
    // Decoded RGBA data
    std::vector<uint8_t> m_rgbaData;
    
    // Error message
    std::string m_error;
};

// Implementation
inline VTFLoader::VTFLoader() {}
inline VTFLoader::~VTFLoader() {}

inline bool VTFLoader::Load(const char* filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        m_error = "Failed to open file";
        return false;
    }
    
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    m_fileData.resize(size);
    if (!file.read(reinterpret_cast<char*>(m_fileData.data()), size)) {
        m_error = "Failed to read file";
        return false;
    }
    
    return LoadFromMemory(m_fileData.data(), m_fileData.size());
}

inline bool VTFLoader::Load(const wchar_t* filename) {
    std::ifstream file(filename, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        m_error = "Failed to open file";
        return false;
    }
    
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    m_fileData.resize(size);
    if (!file.read(reinterpret_cast<char*>(m_fileData.data()), size)) {
        m_error = "Failed to read file";
        return false;
    }
    
    return LoadFromMemory(m_fileData.data(), m_fileData.size());
}

inline bool VTFLoader::LoadFromMemory(const uint8_t* data, size_t size) {
    if (!ParseHeader(data, size)) {
        return false;
    }
    
    return DecodeImage(data, size);
}

inline bool VTFLoader::ParseHeader(const uint8_t* data, size_t size) {
    if (size < sizeof(VTFHeader)) {
        m_error = "File too small for VTF header";
        return false;
    }
    
    const VTFHeader* header = reinterpret_cast<const VTFHeader*>(data);
    
    // Check signature
    if (header->signature[0] != 'V' || header->signature[1] != 'T' ||
        header->signature[2] != 'F' || header->signature[3] != '\0') {
        m_error = "Invalid VTF signature";
        return false;
    }
    
    // Check version
    m_versionMajor = header->version[0];
    m_versionMinor = header->version[1];
    
    if (m_versionMajor != 7 || m_versionMinor > 5) {
        m_error = "Unsupported VTF version: " + std::to_string(m_versionMajor) + "." + std::to_string(m_versionMinor);
        return false;
    }
    
    // Store properties
    m_width = header->width;
    m_height = header->height;
    m_frameCount = header->frames;
    m_mipmapCount = header->mipmapCount;
    m_format = static_cast<VTFImageFormat>(header->highResImageFormat);
    m_hasAlpha = FormatHasAlpha(m_format);
    
    if (m_frameCount < 1) m_frameCount = 1;
    if (m_mipmapCount < 1) m_mipmapCount = 1;
    
    return true;
}

inline bool VTFLoader::DecodeImage(const uint8_t* srcData, size_t srcSize) {
    const VTFHeader* header = reinterpret_cast<const VTFHeader*>(srcData);
    
    // Calculate data offset
    size_t dataOffset = header->headerSize;
    
    // Skip low-res thumbnail if present
    if (header->lowResImageFormat != IMAGE_FORMAT_NONE && 
        header->lowResImageWidth > 0 && header->lowResImageHeight > 0) {
        dataOffset += CalculateImageSize(header->lowResImageWidth, header->lowResImageHeight, 
                                         static_cast<VTFImageFormat>(header->lowResImageFormat));
    }
    
    // Calculate size of high-res image data (all mipmaps, all frames)
    size_t imageDataSize = 0;
    int mipWidth = m_width;
    int mipHeight = m_height;
    for (int mip = m_mipmapCount - 1; mip >= 0; mip--) {
        imageDataSize += CalculateImageSize(mipWidth, mipHeight, m_format) * m_frameCount;
        mipWidth = (mipWidth > 1) ? mipWidth / 2 : 1;
        mipHeight = (mipHeight > 1) ? mipHeight / 2 : 1;
    }
    
    if (dataOffset + imageDataSize > srcSize) {
        m_error = "File truncated - not enough image data";
        return false;
    }
    
    // Allocate output buffer (RGBA8888)
    m_rgbaData.resize(m_width * m_height * 4);
    
    // Find offset to mipmap 0, frame 0 (stored last in VTF files)
    // Mipmaps are stored smallest to largest
    size_t offset = dataOffset;
    mipWidth = m_width;
    mipHeight = m_height;
    for (int mip = m_mipmapCount - 1; mip > 0; mip--) {
        mipWidth = (mipWidth > 1) ? mipWidth / 2 : 1;
        mipHeight = (mipHeight > 1) ? mipHeight / 2 : 1;
        offset += CalculateImageSize(mipWidth, mipHeight, m_format) * m_frameCount;
    }
    
    // Decode the largest mipmap (mip 0)
    const uint8_t* imageData = srcData + offset;
    ConvertToRGBA(imageData, m_rgbaData.data(), m_width, m_height, m_format);
    
    return true;
}

inline void VTFLoader::ConvertToRGBA(const uint8_t* src, uint8_t* dst, int width, int height, VTFImageFormat format) {
    int pixelCount = width * height;
    
    switch (format) {
        case IMAGE_FORMAT_RGBA8888:
            // Already RGBA, just copy
            memcpy(dst, src, pixelCount * 4);
            break;
            
        case IMAGE_FORMAT_ABGR8888:
            // ABGR -> RGBA
            for (int i = 0; i < pixelCount; i++) {
                dst[i*4 + 0] = src[i*4 + 3]; // R
                dst[i*4 + 1] = src[i*4 + 2]; // G
                dst[i*4 + 2] = src[i*4 + 1]; // B
                dst[i*4 + 3] = src[i*4 + 0]; // A
            }
            break;
            
        case IMAGE_FORMAT_RGB888:
            // RGB -> RGBA
            for (int i = 0; i < pixelCount; i++) {
                dst[i*4 + 0] = src[i*3 + 0];
                dst[i*4 + 1] = src[i*3 + 1];
                dst[i*4 + 2] = src[i*3 + 2];
                dst[i*4 + 3] = 255;
            }
            break;
            
        case IMAGE_FORMAT_BGR888:
            // BGR -> RGBA
            for (int i = 0; i < pixelCount; i++) {
                dst[i*4 + 0] = src[i*3 + 2]; // R
                dst[i*4 + 1] = src[i*3 + 1]; // G
                dst[i*4 + 2] = src[i*3 + 0]; // B
                dst[i*4 + 3] = 255;          // A
            }
            break;
            
        case IMAGE_FORMAT_ARGB8888:
            // ARGB -> RGBA
            for (int i = 0; i < pixelCount; i++) {
                dst[i*4 + 0] = src[i*4 + 1]; // R
                dst[i*4 + 1] = src[i*4 + 2]; // G
                dst[i*4 + 2] = src[i*4 + 3]; // B
                dst[i*4 + 3] = src[i*4 + 0]; // A
            }
            break;
            
        case IMAGE_FORMAT_BGRA8888:
            // BGRA -> RGBA
            for (int i = 0; i < pixelCount; i++) {
                dst[i*4 + 0] = src[i*4 + 2]; // R
                dst[i*4 + 1] = src[i*4 + 1]; // G
                dst[i*4 + 2] = src[i*4 + 0]; // B
                dst[i*4 + 3] = src[i*4 + 3]; // A
            }
            break;
            
        case IMAGE_FORMAT_BGRX8888:
            // BGRX -> RGBA (X = unused)
            for (int i = 0; i < pixelCount; i++) {
                dst[i*4 + 0] = src[i*4 + 2]; // R
                dst[i*4 + 1] = src[i*4 + 1]; // G
                dst[i*4 + 2] = src[i*4 + 0]; // B
                dst[i*4 + 3] = 255;          // A
            }
            break;
            
        case IMAGE_FORMAT_DXT1:
        case IMAGE_FORMAT_DXT1_ONEBITALPHA:
        case IMAGE_FORMAT_DXT3:
        case IMAGE_FORMAT_DXT5:
            DXT::DecompressDXT(src, dst, width, height, static_cast<int>(format));
            break;
            
        case IMAGE_FORMAT_I8:
            // Grayscale -> RGBA
            for (int i = 0; i < pixelCount; i++) {
                dst[i*4 + 0] = src[i];
                dst[i*4 + 1] = src[i];
                dst[i*4 + 2] = src[i];
                dst[i*4 + 3] = 255;
            }
            break;
            
        case IMAGE_FORMAT_IA88:
            // Grayscale+Alpha -> RGBA
            for (int i = 0; i < pixelCount; i++) {
                dst[i*4 + 0] = src[i*2 + 0];
                dst[i*4 + 1] = src[i*2 + 0];
                dst[i*4 + 2] = src[i*2 + 0];
                dst[i*4 + 3] = src[i*2 + 1];
            }
            break;
            
        case IMAGE_FORMAT_A8:
            // Alpha only -> RGBA (white with alpha)
            for (int i = 0; i < pixelCount; i++) {
                dst[i*4 + 0] = 255;
                dst[i*4 + 1] = 255;
                dst[i*4 + 2] = 255;
                dst[i*4 + 3] = src[i];
            }
            break;
            
        default:
            // Unsupported format - fill with magenta
            m_error = "Unsupported image format: " + std::to_string(static_cast<int>(format));
            for (int i = 0; i < pixelCount; i++) {
                dst[i*4 + 0] = 255;
                dst[i*4 + 1] = 0;
                dst[i*4 + 2] = 255;
                dst[i*4 + 3] = 255;
            }
            break;
    }
}

inline const uint8_t* VTFLoader::GetRGBAData(int frame, int mipmap) {
    // TODO: Support multiple frames and mipmaps
    return m_rgbaData.data();
}
