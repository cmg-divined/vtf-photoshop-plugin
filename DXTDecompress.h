#pragma once

#include <cstdint>
#include <cstring>

// DXT/BC Texture Decompression Functions
namespace DXT {

// Decode a 565 color to RGB
inline void DecodeColor565(uint16_t color, uint8_t* r, uint8_t* g, uint8_t* b) {
    *r = (color >> 11) << 3;
    *g = ((color >> 5) & 0x3F) << 2;
    *b = (color & 0x1F) << 3;
    // Replicate high bits to low bits for accuracy
    *r |= *r >> 5;
    *g |= *g >> 6;
    *b |= *b >> 5;
}

// Decompress a single DXT1 4x4 block
inline void DecompressDXT1Block(const uint8_t* src, uint8_t* dst, int dstPitch, bool hasAlpha = false) {
    uint16_t color0 = *reinterpret_cast<const uint16_t*>(src);
    uint16_t color1 = *reinterpret_cast<const uint16_t*>(src + 2);
    uint32_t indices = *reinterpret_cast<const uint32_t*>(src + 4);
    
    uint8_t palette[4][4];
    
    DecodeColor565(color0, &palette[0][0], &palette[0][1], &palette[0][2]);
    palette[0][3] = 255;
    
    DecodeColor565(color1, &palette[1][0], &palette[1][1], &palette[1][2]);
    palette[1][3] = 255;
    
    if (color0 > color1 || !hasAlpha) {
        // 4-color mode (standard, or forced for opaque to avoid black artifacts)
        for (int c = 0; c < 3; c++) {
            palette[2][c] = (2 * palette[0][c] + palette[1][c]) / 3;
            palette[3][c] = (palette[0][c] + 2 * palette[1][c]) / 3;
        }
        palette[2][3] = 255;
        palette[3][3] = 255;
    } else {
        // 3-color + transparent mode
        for (int c = 0; c < 3; c++) {
            palette[2][c] = (palette[0][c] + palette[1][c]) / 2;
            palette[3][c] = 0;
        }
        palette[2][3] = 255;
        palette[3][3] = hasAlpha ? 0 : 255;
    }
    
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int idx = (indices >> ((y * 4 + x) * 2)) & 0x3;
            uint8_t* pixel = dst + y * dstPitch + x * 4;
            pixel[0] = palette[idx][0]; // R
            pixel[1] = palette[idx][1]; // G
            pixel[2] = palette[idx][2]; // B
            pixel[3] = palette[idx][3]; // A
        }
    }
}

// Decompress a single DXT3 4x4 block
inline void DecompressDXT3Block(const uint8_t* src, uint8_t* dst, int dstPitch) {
    // First 8 bytes are explicit alpha (4 bits per pixel)
    const uint8_t* alphaSrc = src;
    
    // Decompress color part (same as DXT1)
    DecompressDXT1Block(src + 8, dst, dstPitch, false);
    
    // Apply alpha
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int alphaIdx = y * 4 + x;
            uint8_t alpha;
            if (alphaIdx & 1) {
                alpha = (alphaSrc[alphaIdx / 2] >> 4) & 0xF;
            } else {
                alpha = alphaSrc[alphaIdx / 2] & 0xF;
            }
            alpha = alpha | (alpha << 4); // Expand 4 bits to 8 bits
            
            uint8_t* pixel = dst + y * dstPitch + x * 4;
            pixel[3] = alpha;
        }
    }
}

// Decompress a single DXT5 4x4 block
inline void DecompressDXT5Block(const uint8_t* src, uint8_t* dst, int dstPitch) {
    // First 8 bytes are alpha block
    uint8_t alpha0 = src[0];
    uint8_t alpha1 = src[1];
    
    // Build alpha palette
    uint8_t alphaPalette[8];
    alphaPalette[0] = alpha0;
    alphaPalette[1] = alpha1;
    
    if (alpha0 > alpha1) {
        // 8-alpha mode
        for (int i = 0; i < 6; i++) {
            alphaPalette[i + 2] = ((6 - i) * alpha0 + (i + 1) * alpha1) / 7;
        }
    } else {
        // 6-alpha + 0 + 255 mode
        for (int i = 0; i < 4; i++) {
            alphaPalette[i + 2] = ((4 - i) * alpha0 + (i + 1) * alpha1) / 5;
        }
        alphaPalette[6] = 0;
        alphaPalette[7] = 255;
    }
    
    // Read 48 bits of alpha indices
    uint64_t alphaIndices = 0;
    for (int i = 0; i < 6; i++) {
        alphaIndices |= static_cast<uint64_t>(src[2 + i]) << (i * 8);
    }
    
    // Decompress color part
    DecompressDXT1Block(src + 8, dst, dstPitch, false);
    
    // Apply alpha
    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int pixelIdx = y * 4 + x;
            int alphaIdx = (alphaIndices >> (pixelIdx * 3)) & 0x7;
            
            uint8_t* pixel = dst + y * dstPitch + x * 4;
            pixel[3] = alphaPalette[alphaIdx];
        }
    }
}

// Decompress a full DXT image to RGBA
inline void DecompressDXT(const uint8_t* src, uint8_t* dst, int width, int height, int format) {
    int blocksX = (width + 3) / 4;
    int blocksY = (height + 3) / 4;
    int dstPitch = width * 4;
    
    // Create a temp buffer for partial blocks at edges
    uint8_t tempBlock[4 * 4 * 4];
    
    for (int by = 0; by < blocksY; by++) {
        for (int bx = 0; bx < blocksX; bx++) {
            int blockX = bx * 4;
            int blockY = by * 4;
            
            // Check if this is a partial block at the edge
            bool isPartial = (blockX + 4 > width) || (blockY + 4 > height);
            
            uint8_t* dstBlock;
            int dstBlockPitch;
            
            if (isPartial) {
                dstBlock = tempBlock;
                dstBlockPitch = 16; // 4 pixels * 4 bytes
            } else {
                dstBlock = dst + blockY * dstPitch + blockX * 4;
                dstBlockPitch = dstPitch;
            }
            
            // Decompress based on format
            switch (format) {
                case 13: // IMAGE_FORMAT_DXT1
                case 20: // IMAGE_FORMAT_DXT1_ONEBITALPHA
                    DecompressDXT1Block(src, dstBlock, dstBlockPitch, format == 20);
                    src += 8;
                    break;
                case 14: // IMAGE_FORMAT_DXT3
                    DecompressDXT3Block(src, dstBlock, dstBlockPitch);
                    src += 16;
                    break;
                case 15: // IMAGE_FORMAT_DXT5
                    DecompressDXT5Block(src, dstBlock, dstBlockPitch);
                    src += 16;
                    break;
            }
            
            // Copy partial block to destination
            if (isPartial) {
                int copyWidth = (blockX + 4 <= width) ? 4 : (width - blockX);
                int copyHeight = (blockY + 4 <= height) ? 4 : (height - blockY);
                
                for (int y = 0; y < copyHeight; y++) {
                    memcpy(dst + (blockY + y) * dstPitch + blockX * 4,
                           tempBlock + y * 16,
                           copyWidth * 4);
                }
            }
        }
    }
}

} // namespace DXT
