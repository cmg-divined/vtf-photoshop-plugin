#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <fstream>
#include "VTFFormat.h"

// DXT Compression (simplified - for production, consider using a library like stb_dxt)
namespace DXTCompress {

// Compress a 4x4 block to DXT1
inline void CompressDXT1Block(const uint8_t* rgba, uint8_t* output) {
    // Find min/max colors for the block
    uint8_t minColor[3] = {255, 255, 255};
    uint8_t maxColor[3] = {0, 0, 0};
    
    for (int i = 0; i < 16; i++) {
        for (int c = 0; c < 3; c++) {
            if (rgba[i*4 + c] < minColor[c]) minColor[c] = rgba[i*4 + c];
            if (rgba[i*4 + c] > maxColor[c]) maxColor[c] = rgba[i*4 + c];
        }
    }
    
    // Convert to 565 format
    uint16_t color0 = ((maxColor[0] >> 3) << 11) | ((maxColor[1] >> 2) << 5) | (maxColor[2] >> 3);
    uint16_t color1 = ((minColor[0] >> 3) << 11) | ((minColor[1] >> 2) << 5) | (minColor[2] >> 3);
    
    // Ensure color0 > color1 for 4-color mode
    if (color0 < color1) {
        std::swap(color0, color1);
        std::swap(minColor, maxColor);
    }
    
    // Write colors
    *reinterpret_cast<uint16_t*>(output) = color0;
    *reinterpret_cast<uint16_t*>(output + 2) = color1;
    
    // Build palette
    uint8_t palette[4][3];
    for (int c = 0; c < 3; c++) {
        palette[0][c] = maxColor[c];
        palette[1][c] = minColor[c];
        palette[2][c] = (2 * maxColor[c] + minColor[c]) / 3;
        palette[3][c] = (maxColor[c] + 2 * minColor[c]) / 3;
    }
    
    // Find best index for each pixel
    uint32_t indices = 0;
    for (int i = 0; i < 16; i++) {
        int bestIdx = 0;
        int bestDist = INT_MAX;
        
        for (int j = 0; j < 4; j++) {
            int dist = 0;
            for (int c = 0; c < 3; c++) {
                int diff = rgba[i*4 + c] - palette[j][c];
                dist += diff * diff;
            }
            if (dist < bestDist) {
                bestDist = dist;
                bestIdx = j;
            }
        }
        
        indices |= (bestIdx << (i * 2));
    }
    
    *reinterpret_cast<uint32_t*>(output + 4) = indices;
}

// Compress a 4x4 block to DXT5 (with alpha)
inline void CompressDXT5Block(const uint8_t* rgba, uint8_t* output) {
    // Find min/max alpha
    uint8_t minAlpha = 255, maxAlpha = 0;
    for (int i = 0; i < 16; i++) {
        if (rgba[i*4 + 3] < minAlpha) minAlpha = rgba[i*4 + 3];
        if (rgba[i*4 + 3] > maxAlpha) maxAlpha = rgba[i*4 + 3];
    }
    
    // Write alpha endpoints
    output[0] = maxAlpha;
    output[1] = minAlpha;
    
    // Build alpha palette
    uint8_t alphaPalette[8];
    alphaPalette[0] = maxAlpha;
    alphaPalette[1] = minAlpha;
    if (maxAlpha > minAlpha) {
        for (int i = 0; i < 6; i++) {
            alphaPalette[i + 2] = ((6 - i) * maxAlpha + (i + 1) * minAlpha) / 7;
        }
    } else {
        for (int i = 0; i < 4; i++) {
            alphaPalette[i + 2] = ((4 - i) * maxAlpha + (i + 1) * minAlpha) / 5;
        }
        alphaPalette[6] = 0;
        alphaPalette[7] = 255;
    }
    
    // Find best alpha index for each pixel
    uint64_t alphaIndices = 0;
    for (int i = 0; i < 16; i++) {
        int bestIdx = 0;
        int bestDist = INT_MAX;
        
        for (int j = 0; j < 8; j++) {
            int dist = abs(rgba[i*4 + 3] - alphaPalette[j]);
            if (dist < bestDist) {
                bestDist = dist;
                bestIdx = j;
            }
        }
        
        alphaIndices |= (static_cast<uint64_t>(bestIdx) << (i * 3));
    }
    
    // Write alpha indices (6 bytes)
    for (int i = 0; i < 6; i++) {
        output[2 + i] = (alphaIndices >> (i * 8)) & 0xFF;
    }
    
    // Compress color part (same as DXT1)
    CompressDXT1Block(rgba, output + 8);
}

} // namespace DXTCompress

class VTFWriter {
public:
    VTFWriter();
    ~VTFWriter();
    
    // Set image data (RGBA format, 8 bits per channel)
    void SetImageData(const uint8_t* rgba, int width, int height, bool hasAlpha);
    
    // Set output format
    void SetFormat(VTFImageFormat format) { m_format = format; }
    
    // Set flags
    void SetFlags(uint32_t flags) { m_flags = flags; }
    
    // Generate mipmaps
    void SetGenerateMipmaps(bool generate) { m_generateMipmaps = generate; }
    
    // Write to file
    bool Write(const char* filename);
    bool Write(const wchar_t* filename);
    
    // Write to memory buffer
    bool WriteToMemory(std::vector<uint8_t>& output);
    
    // Get error
    const std::string& GetError() const { return m_error; }
    
private:
    void GenerateMipmaps();
    void CompressImage(const uint8_t* rgba, int width, int height, std::vector<uint8_t>& output);
    void ConvertFromRGBA(const uint8_t* rgba, uint8_t* dst, int width, int height);
    int CalculateMipmapCount(int width, int height);
    
    // Source image
    std::vector<uint8_t> m_sourceRGBA;
    int m_width = 0;
    int m_height = 0;
    bool m_hasAlpha = false;
    
    // Mipmaps (including original)
    std::vector<std::vector<uint8_t>> m_mipmaps;
    
    // Output settings
    VTFImageFormat m_format = IMAGE_FORMAT_DXT5;
    uint32_t m_flags = TEXTUREFLAGS_NORMAL;
    bool m_generateMipmaps = true;
    
    std::string m_error;
};

// Implementation
inline VTFWriter::VTFWriter() {}
inline VTFWriter::~VTFWriter() {}

inline void VTFWriter::SetImageData(const uint8_t* rgba, int width, int height, bool hasAlpha) {
    m_width = width;
    m_height = height;
    m_hasAlpha = hasAlpha;
    
    size_t size = width * height * 4;
    m_sourceRGBA.resize(size);
    memcpy(m_sourceRGBA.data(), rgba, size);
    
    // Auto-select format based on alpha
    if (!hasAlpha && m_format == IMAGE_FORMAT_DXT5) {
        m_format = IMAGE_FORMAT_DXT1;
    }
}

inline int VTFWriter::CalculateMipmapCount(int width, int height) {
    int count = 1;
    while (width > 1 || height > 1) {
        width = (width > 1) ? width / 2 : 1;
        height = (height > 1) ? height / 2 : 1;
        count++;
    }
    return count;
}

inline void VTFWriter::GenerateMipmaps() {
    m_mipmaps.clear();
    
    // Start with original
    m_mipmaps.push_back(m_sourceRGBA);
    
    if (!m_generateMipmaps) return;
    
    int mipWidth = m_width;
    int mipHeight = m_height;
    
    while (mipWidth > 1 || mipHeight > 1) {
        int newWidth = (mipWidth > 1) ? mipWidth / 2 : 1;
        int newHeight = (mipHeight > 1) ? mipHeight / 2 : 1;
        
        const std::vector<uint8_t>& src = m_mipmaps.back();
        std::vector<uint8_t> dst(newWidth * newHeight * 4);
        
        // Simple box filter downscale
        for (int y = 0; y < newHeight; y++) {
            for (int x = 0; x < newWidth; x++) {
                int srcX = x * 2;
                int srcY = y * 2;
                
                // Average 2x2 block
                for (int c = 0; c < 4; c++) {
                    int sum = 0;
                    int count = 0;
                    
                    for (int dy = 0; dy < 2 && srcY + dy < mipHeight; dy++) {
                        for (int dx = 0; dx < 2 && srcX + dx < mipWidth; dx++) {
                            sum += src[((srcY + dy) * mipWidth + (srcX + dx)) * 4 + c];
                            count++;
                        }
                    }
                    
                    dst[(y * newWidth + x) * 4 + c] = sum / count;
                }
            }
        }
        
        m_mipmaps.push_back(std::move(dst));
        mipWidth = newWidth;
        mipHeight = newHeight;
    }
}

inline void VTFWriter::CompressImage(const uint8_t* rgba, int width, int height, std::vector<uint8_t>& output) {
    if (m_format == IMAGE_FORMAT_DXT1 || m_format == IMAGE_FORMAT_DXT1_ONEBITALPHA) {
        int blocksX = (width + 3) / 4;
        int blocksY = (height + 3) / 4;
        output.resize(blocksX * blocksY * 8);
        
        uint8_t block[64]; // 4x4 pixels * 4 bytes
        
        for (int by = 0; by < blocksY; by++) {
            for (int bx = 0; bx < blocksX; bx++) {
                // Extract 4x4 block
                for (int y = 0; y < 4; y++) {
                    for (int x = 0; x < 4; x++) {
                        int srcX = bx * 4 + x;
                        int srcY = by * 4 + y;
                        if (srcX < width && srcY < height) {
                            memcpy(&block[(y * 4 + x) * 4], &rgba[(srcY * width + srcX) * 4], 4);
                        } else {
                            memset(&block[(y * 4 + x) * 4], 0, 4);
                        }
                    }
                }
                
                DXTCompress::CompressDXT1Block(block, &output[(by * blocksX + bx) * 8]);
            }
        }
    }
    else if (m_format == IMAGE_FORMAT_DXT5) {
        int blocksX = (width + 3) / 4;
        int blocksY = (height + 3) / 4;
        output.resize(blocksX * blocksY * 16);
        
        uint8_t block[64];
        
        for (int by = 0; by < blocksY; by++) {
            for (int bx = 0; bx < blocksX; bx++) {
                // Extract 4x4 block
                for (int y = 0; y < 4; y++) {
                    for (int x = 0; x < 4; x++) {
                        int srcX = bx * 4 + x;
                        int srcY = by * 4 + y;
                        if (srcX < width && srcY < height) {
                            memcpy(&block[(y * 4 + x) * 4], &rgba[(srcY * width + srcX) * 4], 4);
                        } else {
                            memset(&block[(y * 4 + x) * 4], 0, 4);
                        }
                    }
                }
                
                DXTCompress::CompressDXT5Block(block, &output[(by * blocksX + bx) * 16]);
            }
        }
    }
    else {
        // Uncompressed formats
        ConvertFromRGBA(rgba, nullptr, width, height);
        output.resize(width * height * GetBytesPerPixel(m_format));
        ConvertFromRGBA(rgba, output.data(), width, height);
    }
}

inline void VTFWriter::ConvertFromRGBA(const uint8_t* rgba, uint8_t* dst, int width, int height) {
    int pixelCount = width * height;
    
    switch (m_format) {
        case IMAGE_FORMAT_RGBA8888:
            if (dst) memcpy(dst, rgba, pixelCount * 4);
            break;
            
        case IMAGE_FORMAT_BGRA8888:
            if (dst) {
                for (int i = 0; i < pixelCount; i++) {
                    dst[i*4 + 0] = rgba[i*4 + 2]; // B
                    dst[i*4 + 1] = rgba[i*4 + 1]; // G
                    dst[i*4 + 2] = rgba[i*4 + 0]; // R
                    dst[i*4 + 3] = rgba[i*4 + 3]; // A
                }
            }
            break;
            
        case IMAGE_FORMAT_RGB888:
            if (dst) {
                for (int i = 0; i < pixelCount; i++) {
                    dst[i*3 + 0] = rgba[i*4 + 0];
                    dst[i*3 + 1] = rgba[i*4 + 1];
                    dst[i*3 + 2] = rgba[i*4 + 2];
                }
            }
            break;
            
        case IMAGE_FORMAT_BGR888:
            if (dst) {
                for (int i = 0; i < pixelCount; i++) {
                    dst[i*3 + 0] = rgba[i*4 + 2]; // B
                    dst[i*3 + 1] = rgba[i*4 + 1]; // G
                    dst[i*3 + 2] = rgba[i*4 + 0]; // R
                }
            }
            break;
            
        default:
            break;
    }
}

inline bool VTFWriter::Write(const char* filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        m_error = "Failed to open file for writing";
        return false;
    }
    
    // Generate mipmaps
    GenerateMipmaps();
    
    // Build VTF header
    VTFHeader header = {};
    header.signature[0] = 'V';
    header.signature[1] = 'T';
    header.signature[2] = 'F';
    header.signature[3] = '\0';
    header.version[0] = 7;
    header.version[1] = 2;
    header.headerSize = 80; // Version 7.2 require 80 bytes header (padded)
    header.width = static_cast<uint16_t>(m_width);
    header.height = static_cast<uint16_t>(m_height);
    header.flags = m_flags;
    header.frames = 1;
    header.firstFrame = 0;
    header.reflectivity[0] = 0.5f;
    header.reflectivity[1] = 0.5f;
    header.reflectivity[2] = 0.5f;
    header.bumpmapScale = 1.0f;
    header.highResImageFormat = static_cast<uint32_t>(m_format);
    header.mipmapCount = static_cast<uint8_t>(m_mipmaps.size());
    header.lowResImageFormat = IMAGE_FORMAT_NONE;
    header.lowResImageWidth = 0;
    header.lowResImageHeight = 0;
    header.depth = 1;
    
    // Write header (full struct is 80 bytes padded)
    file.write(reinterpret_cast<const char*>(&header), sizeof(VTFHeader));
    
    // Write mipmaps (smallest to largest, as per VTF spec)
    for (int mip = static_cast<int>(m_mipmaps.size()) - 1; mip >= 0; mip--) {
        int mipWidth = m_width >> mip;
        int mipHeight = m_height >> mip;
        if (mipWidth < 1) mipWidth = 1;
        if (mipHeight < 1) mipHeight = 1;
        
        std::vector<uint8_t> compressed;
        CompressImage(m_mipmaps[mip].data(), mipWidth, mipHeight, compressed);
        file.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());
    }
    
    return true;
}

inline bool VTFWriter::Write(const wchar_t* filename) {
    std::ofstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        m_error = "Failed to open file for writing";
        return false;
    }
    
    // Same implementation as char* version
    GenerateMipmaps();
    
    VTFHeader header = {};
    header.signature[0] = 'V';
    header.signature[1] = 'T';
    header.signature[2] = 'F';
    header.signature[3] = '\0';
    header.version[0] = 7;
    header.version[1] = 2;
    header.headerSize = 80; // Version 7.2 require 80 bytes header (padded)
    header.width = static_cast<uint16_t>(m_width);
    header.height = static_cast<uint16_t>(m_height);
    header.flags = m_flags;
    header.frames = 1;
    header.firstFrame = 0;
    header.reflectivity[0] = 0.5f;
    header.reflectivity[1] = 0.5f;
    header.reflectivity[2] = 0.5f;
    header.bumpmapScale = 1.0f;
    header.highResImageFormat = static_cast<uint32_t>(m_format);
    header.mipmapCount = static_cast<uint8_t>(m_mipmaps.size());
    header.lowResImageFormat = IMAGE_FORMAT_NONE;
    header.lowResImageWidth = 0;
    header.lowResImageHeight = 0;
    header.depth = 1;
    
    file.write(reinterpret_cast<const char*>(&header), sizeof(VTFHeader));
    
    for (int mip = static_cast<int>(m_mipmaps.size()) - 1; mip >= 0; mip--) {
        int mipWidth = m_width >> mip;
        int mipHeight = m_height >> mip;
        if (mipWidth < 1) mipWidth = 1;
        if (mipHeight < 1) mipHeight = 1;
        
        std::vector<uint8_t> compressed;
        CompressImage(m_mipmaps[mip].data(), mipWidth, mipHeight, compressed);
        file.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());
    }
    
    return true;
}

inline bool VTFWriter::WriteToMemory(std::vector<uint8_t>& output) {
    output.clear();
    
    // Generate mipmaps
    GenerateMipmaps();
    
    // Build VTF header
    VTFHeader header = {};
    header.signature[0] = 'V';
    header.signature[1] = 'T';
    header.signature[2] = 'F';
    header.signature[3] = '\0';
    header.version[0] = 7;
    header.version[1] = 2;
    header.headerSize = 80; // Version 7.2 requires 80 bytes header (padded)
    header.width = static_cast<uint16_t>(m_width);
    header.height = static_cast<uint16_t>(m_height);
    header.flags = m_flags;
    header.frames = 1;
    header.firstFrame = 0;
    header.reflectivity[0] = 0.5f;
    header.reflectivity[1] = 0.5f;
    header.reflectivity[2] = 0.5f;
    header.bumpmapScale = 1.0f;
    header.highResImageFormat = static_cast<uint32_t>(m_format);
    header.mipmapCount = static_cast<uint8_t>(m_mipmaps.size());
    header.lowResImageFormat = IMAGE_FORMAT_NONE;
    header.lowResImageWidth = 0;
    header.lowResImageHeight = 0;
    header.depth = 1;
    
    // Write header to output
    output.resize(sizeof(VTFHeader)); // Use full struct size (80)
    memcpy(output.data(), &header, sizeof(VTFHeader));
    
    // Write mipmaps (smallest to largest)
    for (int mip = static_cast<int>(m_mipmaps.size()) - 1; mip >= 0; mip--) {
        int mipWidth = m_width >> mip;
        int mipHeight = m_height >> mip;
        if (mipWidth < 1) mipWidth = 1;
        if (mipHeight < 1) mipHeight = 1;
        
        std::vector<uint8_t> compressed;
        CompressImage(m_mipmaps[mip].data(), mipWidth, mipHeight, compressed);
        
        size_t offset = output.size();
        output.resize(offset + compressed.size());
        memcpy(output.data() + offset, compressed.data(), compressed.size());
    }
    
    return true;
}
