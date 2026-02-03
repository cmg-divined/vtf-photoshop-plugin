// VTF Format Plugin for Adobe Photoshop
// Loads and Saves Valve Texture Format (.vtf) files
//
// Based on Adobe Photoshop SDK File Format samples

//-------------------------------------------------------------------------------
//	Includes
//-------------------------------------------------------------------------------

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <fstream>

#include "../win/resource.h"

//-------------------------------------------------------------------------------
//	Debug Logging
//-------------------------------------------------------------------------------

static void DebugLog(const char* msg) {
    static bool firstCall = true;
    std::ofstream log;
    if (firstCall) {
        log.open("C:\\vtf_plugin_debug.log", std::ios::out | std::ios::trunc);
        firstCall = false;
    } else {
        log.open("C:\\vtf_plugin_debug.log", std::ios::out | std::ios::app);
    }
    if (log.is_open()) {
        log << msg << std::endl;
        log.close();
    }
}

static void DebugLogInt(const char* msg, int value) {
    char buf[512];
    sprintf_s(buf, "%s: %d", msg, value);
    DebugLog(buf);
}

// Adobe Photoshop SDK headers
#include "PIDefines.h"
#include "PITypes.h"
#include "PIGeneral.h"
#include "PIFormat.h"
#include "PIUtilities.h"
#include "FileUtilities.h"

// Our VTF headers
#include "VTFFormat.h"
#include "VTFLoader.h"
#include "VTFWriter.h"

//-------------------------------------------------------------------------------
//	Plugin Entry Point Declaration
//-------------------------------------------------------------------------------

DLLExport MACPASCAL void PluginMain(const int16 selector,
                                    FormatRecordPtr formatParamBlock,
                                    intptr_t* data,
                                    int16* result);

//-------------------------------------------------------------------------------
//	Global State
//-------------------------------------------------------------------------------

SPBasicSuite* sSPBasic = nullptr;
SPPluginRef gPluginRef = nullptr;
FormatRecordPtr gFormatRecord = nullptr;
intptr_t* gDataPtr = nullptr;
int16* gResult = nullptr;

// Persistent Settings (Sticky)
static VTFImageFormat s_lastFormat = IMAGE_FORMAT_DXT5;
static uint32_t s_lastFlags = TEXTUREFLAGS_NORMAL | TEXTUREFLAGS_EIGHTBITALPHA;
static bool s_lastMipmaps = true;

// Plugin data structure
struct VTFPluginData {
    VTFLoader* loader;
    VTFWriter* writer;
    std::vector<uint8_t> imageData;
    std::vector<uint8_t> fileData;
    VTFImageFormat exportFormat;
    bool generateMipmaps;
    uint32_t flags;
    
    VTFPluginData() : loader(nullptr), writer(nullptr),
                      exportFormat(IMAGE_FORMAT_DXT5),
                      generateMipmaps(true),
                      flags(TEXTUREFLAGS_NORMAL | TEXTUREFLAGS_EIGHTBITALPHA) {}
    
    ~VTFPluginData() {
        delete loader;
        delete writer;
    }
};

VTFPluginData* gData = nullptr;

//-------------------------------------------------------------------------------
//	Prototypes
//-------------------------------------------------------------------------------

static void DoAbout(void);

// Read operations
static void DoReadPrepare(void);
static void DoReadStart(void);
static void DoReadContinue(void);
static void DoReadFinish(void);

// Write operations  
static void DoWritePrepare(void);
static void DoWriteStart(void);
static void DoWriteContinue(void);
static void DoWriteFinish(void);

// Options
static void DoOptionsStart(void);
static void DoOptionsContinue(void);
static void DoOptionsFinish(void);

// Estimate
static void DoEstimatePrepare(void);
static void DoEstimateStart(void);
static void DoEstimateContinue(void);
static void DoEstimateFinish(void);

// Filter
static void DoFilterFile(void);

// Helpers
static void ReadSome(int32 count, void* buffer);
static void WriteSome(int32 count, void* buffer);
static VPoint GetFormatImageSize(void);
static void SetFormatImageSize(VPoint inPoint);

//-------------------------------------------------------------------------------
//	PluginMain
//-------------------------------------------------------------------------------

DLLExport MACPASCAL void PluginMain(const int16 selector,
                                    FormatRecordPtr formatParamBlock,
                                    intptr_t* data,
                                    int16* result)
{
    DebugLogInt("PluginMain called with selector", selector);
    
    try {
        // Set up globals
        gFormatRecord = formatParamBlock;
        gPluginRef = reinterpret_cast<SPPluginRef>(gFormatRecord->plugInRef);
        gResult = result;
        gDataPtr = data;
        
        // Handle About box separately
        if (selector == formatSelectorAbout) {
            AboutRecordPtr aboutRecord = reinterpret_cast<AboutRecordPtr>(formatParamBlock);
            sSPBasic = aboutRecord->sSPBasic;
            DoAbout();
            return;
        }
        
        // Set up SPBasic suite
        sSPBasic = formatParamBlock->sSPBasic;
        
        // Enable 32-bit coordinates
        if (gFormatRecord->HostSupports32BitCoordinates)
            gFormatRecord->PluginUsing32BitCoordinates = true;
        
        // Allocate plugin data if needed
        gData = reinterpret_cast<VTFPluginData*>(*data);
        if (gData == nullptr) {
            gData = new VTFPluginData();
            if (gData == nullptr) {
                *result = memFullErr;
                return;
            }
            *data = reinterpret_cast<intptr_t>(gData);
        }
        
        // Dispatch selector
        switch (selector) {
            // Read
            case formatSelectorReadPrepare:
                DoReadPrepare();
                break;
            case formatSelectorReadStart:
                DoReadStart();
                break;
            case formatSelectorReadContinue:
                DoReadContinue();
                break;
            case formatSelectorReadFinish:
                DoReadFinish();
                break;
                
            // Options
            case formatSelectorOptionsPrepare:
                gFormatRecord->maxData = 0;
                break;
            case formatSelectorOptionsStart:
                DoOptionsStart();
                break;
            case formatSelectorOptionsContinue:
                DoOptionsContinue();
                break;
            case formatSelectorOptionsFinish:
                DoOptionsFinish();
                break;
                
            // Estimate
            case formatSelectorEstimatePrepare:
                DoEstimatePrepare();
                break;
            case formatSelectorEstimateStart:
                DoEstimateStart();
                break;
            case formatSelectorEstimateContinue:
                DoEstimateContinue();
                break;
            case formatSelectorEstimateFinish:
                DoEstimateFinish();
                break;
                
            // Write
            case formatSelectorWritePrepare:
                DoWritePrepare();
                break;
            case formatSelectorWriteStart:
                DoWriteStart();
                break;
            case formatSelectorWriteContinue:
                DoWriteContinue();
                break;
            case formatSelectorWriteFinish:
                DoWriteFinish();
                break;
                
            // Filter
            case formatSelectorFilterFile:
                DoFilterFile();
                break;
                
            default:
                break;
        }
        
        // Clean up on finish selectors or error
        if (selector == formatSelectorReadFinish ||
            selector == formatSelectorWriteFinish ||
            selector == formatSelectorOptionsFinish ||
            selector == formatSelectorEstimateFinish ||
            selector == formatSelectorFilterFile ||
            *gResult != noErr) {
            
            PIUSuitesRelease();
        }
        
    } catch (...) {
        if (result)
            *result = formatBadParameters;
    }
}

//-------------------------------------------------------------------------------
//	DoAbout
//-------------------------------------------------------------------------------

static void DoAbout(void) {
    MessageBoxW(nullptr,
        L"VTF Format Plugin v1.0\n\n"
        L"Loads and saves Valve Texture Format (.vtf) files.\n\n"
        L"Supported formats:\n"
        L"  \x2022 DXT1 (BC1) - RGB, no alpha\n"
        L"  \x2022 DXT5 (BC3) - RGBA with alpha\n"
        L"  \x2022 RGB888 / BGR888 - Uncompressed\n"
        L"  \x2022 RGBA8888 / BGRA8888 - Uncompressed\n\n"
        L"For Source Engine / Garry's Mod content creation.\n\n"
        L"\xA9 2026 Divined",
        L"About VTF Format",
        MB_OK | MB_ICONINFORMATION);
}

//-------------------------------------------------------------------------------
//	File I/O Helpers
//-------------------------------------------------------------------------------

static void ReadSome(int32 count, void* buffer) {
    if (*gResult != noErr) return;
    
    int32 readCount = count;
    *gResult = PSSDKRead(gFormatRecord->dataFork,
                         gFormatRecord->posixFileDescriptor,
                         gFormatRecord->pluginUsingPOSIXIO,
                         &readCount,
                         buffer);
    
    if (*gResult == noErr && readCount != count) {
        *gResult = eofErr;
    }
}

static void WriteSome(int32 count, void* buffer) {
    if (*gResult != noErr) return;
    
    int32 writeCount = count;
    *gResult = PSSDKWrite(gFormatRecord->dataFork,
                          gFormatRecord->posixFileDescriptor,
                          gFormatRecord->pluginUsingPOSIXIO,
                          &writeCount,
                          buffer);
    
    if (*gResult == noErr && writeCount != count) {
        *gResult = dskFulErr;
    }
}

//-------------------------------------------------------------------------------
//	Read Operations
//-------------------------------------------------------------------------------

static void DoReadPrepare(void) {
    gFormatRecord->maxData = 0;
    *gResult = noErr;
}

static void DoReadStart(void) {
    DebugLog("DoReadStart called");
    *gResult = noErr;
    
    // Seek to start of file
    *gResult = PSSDKSetFPos(gFormatRecord->dataFork,
                            gFormatRecord->posixFileDescriptor,
                            gFormatRecord->pluginUsingPOSIXIO,
                            fsFromStart, 0);
    if (*gResult != noErr) return;
    
    // Read VTF header first
    VTFHeader header;
    ReadSome(sizeof(VTFHeader), &header);
    if (*gResult != noErr) return;
    
    DebugLog("Read VTF header");
    char sigBuf[64];
    sprintf_s(sigBuf, "Signature: %c%c%c (0x%02X)", header.signature[0], header.signature[1], header.signature[2], (unsigned char)header.signature[3]);
    DebugLog(sigBuf);
    
    // Validate signature
    if (header.signature[0] != 'V' || header.signature[1] != 'T' ||
        header.signature[2] != 'F' || header.signature[3] != '\0') {
        DebugLog("Invalid VTF signature!");
        *gResult = formatCannotRead;
        return;
    }
    DebugLog("Valid VTF signature");
    
    // Check version
    if (header.version[0] != 7 || header.version[1] > 5) {
        *gResult = formatCannotRead;
        return;
    }
    
    // Calculate total data size needed
    VTFImageFormat format = static_cast<VTFImageFormat>(header.highResImageFormat);
    int width = header.width;
    int height = header.height;
    int mipmapCount = header.mipmapCount;
    int frameCount = header.frames;
    if (frameCount < 1) frameCount = 1;
    if (mipmapCount < 1) mipmapCount = 1;
    
    // Calculate total image data size (all mipmaps, all frames)
    size_t imageDataSize = 0;
    int mipWidth = width;
    int mipHeight = height;
    for (int mip = 0; mip < mipmapCount; mip++) {
        imageDataSize += CalculateImageSize(mipWidth, mipHeight, format) * frameCount;
        mipWidth = (mipWidth > 1) ? mipWidth / 2 : 1;
        mipHeight = (mipHeight > 1) ? mipHeight / 2 : 1;
    }
    
    // Add low-res thumbnail size if present
    size_t lowResSize = 0;
    if (header.lowResImageFormat != IMAGE_FORMAT_NONE && 
        header.lowResImageWidth > 0 && header.lowResImageHeight > 0) {
        lowResSize = CalculateImageSize(header.lowResImageWidth, header.lowResImageHeight,
                                        static_cast<VTFImageFormat>(header.lowResImageFormat));
    }
    
    // Total file size = header + lowres + image data
    size_t totalSize = header.headerSize + lowResSize + imageDataSize;
    
    // Allocate and read entire file
    gData->fileData.resize(totalSize);
    
    // Seek back to start and read everything
    *gResult = PSSDKSetFPos(gFormatRecord->dataFork,
                            gFormatRecord->posixFileDescriptor,
                            gFormatRecord->pluginUsingPOSIXIO,
                            fsFromStart, 0);
    if (*gResult != noErr) return;
    
    ReadSome(static_cast<int32>(totalSize), gData->fileData.data());
    if (*gResult != noErr) {
        // Try reading what we can
        *gResult = noErr;
    }
    
    // Create loader and parse
    if (gData->loader) {
        delete gData->loader;
    }
    gData->loader = new VTFLoader();
    
    DebugLog("Calling LoadFromMemory");
    if (!gData->loader->LoadFromMemory(gData->fileData.data(), gData->fileData.size())) {
        DebugLog("LoadFromMemory FAILED");
        *gResult = formatCannotRead;
        return;
    }
    DebugLog("LoadFromMemory succeeded");
    
    // Set up document
    bool hasAlpha = gData->loader->HasAlpha();
    DebugLogInt("Width", gData->loader->GetWidth());
    DebugLogInt("Height", gData->loader->GetHeight());
    DebugLogInt("HasAlpha", hasAlpha ? 1 : 0);
    
    gFormatRecord->imageMode = plugInModeRGBColor;
    gFormatRecord->depth = 8;
    gFormatRecord->planes = hasAlpha ? 4 : 3;
    
    VPoint imageSize;
    imageSize.h = gData->loader->GetWidth();
    imageSize.v = gData->loader->GetHeight();
    SetFormatImageSize(imageSize);
    
    if (hasAlpha) {
        // Don't set transparencyPlane - let it be an Alpha Channel (Alpha 1)
        // gFormatRecord->transparencyPlane = 3;
        // gFormatRecord->transparencyMatting = 0;
    }
}

static void DoReadContinue(void) {
    DebugLog("DoReadContinue called");
    *gResult = noErr;
    
    if (!gData->loader) {
        DebugLog("ERROR: loader is null");
        *gResult = formatCannotRead;
        return;
    }
    
    VPoint imageSize = GetFormatImageSize();
    int width = imageSize.h;
    int height = imageSize.v;
    int planes = gFormatRecord->planes;
    
    const uint8_t* rgbaData = gData->loader->GetRGBAData();
    if (!rgbaData) {
        *gResult = formatCannotRead;
        return;
    }
    
    // Set up the rectangle to write
    VRect theRect;
    theRect.left = 0;
    theRect.top = 0;
    theRect.right = width;
    theRect.bottom = height;
    
    if (gFormatRecord->PluginUsing32BitCoordinates) {
        gFormatRecord->theRect32 = theRect;
    } else {
        gFormatRecord->theRect.left = static_cast<int16>(theRect.left);
        gFormatRecord->theRect.top = static_cast<int16>(theRect.top);
        gFormatRecord->theRect.right = static_cast<int16>(theRect.right);
        gFormatRecord->theRect.bottom = static_cast<int16>(theRect.bottom);
    }
    
    gFormatRecord->loPlane = 0;
    gFormatRecord->hiPlane = planes - 1;
    gFormatRecord->colBytes = planes;
    gFormatRecord->rowBytes = width * planes;
    gFormatRecord->planeBytes = 1;
    
    // Allocate buffer and copy data
    size_t bufferSize = static_cast<size_t>(width) * height * planes;
    gData->imageData.resize(bufferSize);
    
    // Convert from RGBA to interleaved
    uint8_t* dst = gData->imageData.data();
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int srcIdx = (y * width + x) * 4;
            int dstIdx = (y * width + x) * planes;
            
            dst[dstIdx + 0] = rgbaData[srcIdx + 0]; // R
            dst[dstIdx + 1] = rgbaData[srcIdx + 1]; // G
            dst[dstIdx + 2] = rgbaData[srcIdx + 2]; // B
            if (planes > 3) {
                dst[dstIdx + 3] = rgbaData[srcIdx + 3]; // A
            }
        }
    }
    
    gFormatRecord->data = gData->imageData.data();
    
    DebugLog("Calling advanceState");
    // Advance state to write data to Photoshop
    *gResult = gFormatRecord->advanceState();
    DebugLogInt("advanceState returned", *gResult);
    
    // Signal we're done
    if (gFormatRecord->PluginUsing32BitCoordinates) {
        gFormatRecord->theRect32.left = 0;
        gFormatRecord->theRect32.top = 0;
        gFormatRecord->theRect32.right = 0;
        gFormatRecord->theRect32.bottom = 0;
    } else {
        gFormatRecord->theRect.left = 0;
        gFormatRecord->theRect.top = 0;
        gFormatRecord->theRect.right = 0;
        gFormatRecord->theRect.bottom = 0;
    }
    
    gFormatRecord->data = nullptr;
}

static void DoReadFinish(void) {
    gData->imageData.clear();
    gData->imageData.shrink_to_fit();
    gData->fileData.clear();
    gData->fileData.shrink_to_fit();
    
    if (gData->loader) {
        delete gData->loader;
        gData->loader = nullptr;
    }
    
    *gResult = noErr;
}

//-------------------------------------------------------------------------------
//	Write Operations
//-------------------------------------------------------------------------------

static void DoWritePrepare(void) {
    gFormatRecord->maxData = 0;
    *gResult = noErr;
}

static void DoWriteStart(void) {
    *gResult = noErr;
    
    VPoint imageSize = GetFormatImageSize();
    int width = imageSize.h;
    int height = imageSize.v;
    int planes = gFormatRecord->planes;
    
    // Request data from Photoshop
    VRect theRect;
    theRect.left = 0;
    theRect.top = 0;
    theRect.right = width;
    theRect.bottom = height;
    
    if (gFormatRecord->PluginUsing32BitCoordinates) {
        gFormatRecord->theRect32 = theRect;
    } else {
        gFormatRecord->theRect.left = static_cast<int16>(theRect.left);
        gFormatRecord->theRect.top = static_cast<int16>(theRect.top);
        gFormatRecord->theRect.right = static_cast<int16>(theRect.right);
        gFormatRecord->theRect.bottom = static_cast<int16>(theRect.bottom);
    }
    
    gFormatRecord->loPlane = 0;
    gFormatRecord->hiPlane = (planes > 3) ? 3 : planes - 1;
    gFormatRecord->colBytes = planes;
    gFormatRecord->rowBytes = width * planes;
    gFormatRecord->planeBytes = 1;
    
    // Allocate buffer
    size_t bufferSize = static_cast<size_t>(width) * height * planes;
    gData->imageData.resize(bufferSize);
    gFormatRecord->data = gData->imageData.data();
}

static void DoWriteContinue(void) {
    *gResult = noErr;
    
    VPoint imageSize = GetFormatImageSize();
    int width = imageSize.h;
    int height = imageSize.v;
    int planes = gFormatRecord->planes;
    bool hasAlpha = planes > 3;
    
    // Get data from Photoshop
    *gResult = gFormatRecord->advanceState();
    if (*gResult != noErr) return;
    
    // Create writer
    if (gData->writer) {
        delete gData->writer;
    }
    gData->writer = new VTFWriter();
    
    // Convert from interleaved to RGBA
    std::vector<uint8_t> rgbaData(width * height * 4);
    const uint8_t* src = gData->imageData.data();
    
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int srcIdx = (y * width + x) * planes;
            int dstIdx = (y * width + x) * 4;
            
            rgbaData[dstIdx + 0] = src[srcIdx + 0]; // R
            rgbaData[dstIdx + 1] = src[srcIdx + 1]; // G
            rgbaData[dstIdx + 2] = src[srcIdx + 2]; // B
            rgbaData[dstIdx + 3] = hasAlpha ? src[srcIdx + 3] : 255; // A
        }
    }
    
    // Set up writer
    gData->writer->SetImageData(rgbaData.data(), width, height, hasAlpha);
    gData->writer->SetFormat(gData->exportFormat);
    gData->writer->SetGenerateMipmaps(gData->generateMipmaps);
    gData->writer->SetFlags(gData->flags);
    
    // Generate VTF data
    std::vector<uint8_t> vtfData;
    if (!gData->writer->WriteToMemory(vtfData)) {
        *gResult = writErr;
        return;
    }
    
    // Seek to start and write
    *gResult = PSSDKSetFPos(gFormatRecord->dataFork,
                            gFormatRecord->posixFileDescriptor,
                            gFormatRecord->pluginUsingPOSIXIO,
                            fsFromStart, 0);
    if (*gResult != noErr) return;
    
    WriteSome(static_cast<int32>(vtfData.size()), vtfData.data());
    
    // Signal done
    if (gFormatRecord->PluginUsing32BitCoordinates) {
        gFormatRecord->theRect32.left = 0;
        gFormatRecord->theRect32.top = 0;
        gFormatRecord->theRect32.right = 0;
        gFormatRecord->theRect32.bottom = 0;
    } else {
        gFormatRecord->theRect.left = 0;
        gFormatRecord->theRect.top = 0;
        gFormatRecord->theRect.right = 0;
        gFormatRecord->theRect.bottom = 0;
    }
    
    gFormatRecord->data = nullptr;
}

static void DoWriteFinish(void) {
    gData->imageData.clear();
    gData->imageData.shrink_to_fit();
    
    if (gData->writer) {
        delete gData->writer;
        gData->writer = nullptr;
    }
    
    *gResult = noErr;
}

//-------------------------------------------------------------------------------
//	Options
//-------------------------------------------------------------------------------

// Dialog Procedure
static INT_PTR CALLBACK VTFOptionsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_INITDIALOG:
        {
            // Populate Format Combobox
            HWND hCombo = GetDlgItem(hDlg, IDC_FORMAT);
            int idx;
            
            idx = (int)SendMessageA(hCombo, CB_ADDSTRING, 0, (LPARAM)"DXT1 (No Alpha)");
            SendMessageA(hCombo, CB_SETITEMDATA, idx, IMAGE_FORMAT_DXT1);
            
            idx = (int)SendMessageA(hCombo, CB_ADDSTRING, 0, (LPARAM)"DXT5 (Alpha)");
            SendMessageA(hCombo, CB_SETITEMDATA, idx, IMAGE_FORMAT_DXT5);
            
            idx = (int)SendMessageA(hCombo, CB_ADDSTRING, 0, (LPARAM)"RGBA8888 (Uncompressed)");
            SendMessageA(hCombo, CB_SETITEMDATA, idx, IMAGE_FORMAT_RGBA8888);
            
            idx = (int)SendMessageA(hCombo, CB_ADDSTRING, 0, (LPARAM)"BGRA8888 (Uncompressed)");
            SendMessageA(hCombo, CB_SETITEMDATA, idx, IMAGE_FORMAT_BGRA8888);

            // Set Default Selection (from persistent settings)
            int comboIndex = 1; // Default DXT5
            switch (s_lastFormat) {
                case IMAGE_FORMAT_DXT1: comboIndex = 0; break;
                case IMAGE_FORMAT_DXT5: comboIndex = 1; break;
                case IMAGE_FORMAT_RGBA8888: comboIndex = 2; break;
                case IMAGE_FORMAT_BGRA8888: comboIndex = 3; break;
            }
            SendMessageA(hCombo, CB_SETCURSEL, comboIndex, 0);
            
            // Set Checkboxes from persistent flags
            if (s_lastMipmaps) CheckDlgButton(hDlg, IDC_CHK_MIPMAPS, BST_CHECKED);
            
            if (s_lastFlags & TEXTUREFLAGS_POINTSAMPLE) CheckDlgButton(hDlg, IDC_CHK_POINTSAMPLE, BST_CHECKED);
            if (s_lastFlags & TEXTUREFLAGS_TRILINEAR) CheckDlgButton(hDlg, IDC_CHK_TRILINEAR, BST_CHECKED);
            if (s_lastFlags & TEXTUREFLAGS_CLAMPS) CheckDlgButton(hDlg, IDC_CHK_CLAMPS, BST_CHECKED);
            if (s_lastFlags & TEXTUREFLAGS_CLAMPT) CheckDlgButton(hDlg, IDC_CHK_CLAMPT, BST_CHECKED);
            if (s_lastFlags & TEXTUREFLAGS_ANISOTROPIC) CheckDlgButton(hDlg, IDC_CHK_ANISOTROPIC, BST_CHECKED);
            if (s_lastFlags & TEXTUREFLAGS_HINT_DXT5) CheckDlgButton(hDlg, IDC_CHK_HINTDXT5, BST_CHECKED);
            if (s_lastFlags & TEXTUREFLAGS_NORMAL) CheckDlgButton(hDlg, IDC_CHK_NORMAL, BST_CHECKED);
            if (s_lastFlags & TEXTUREFLAGS_NOMIP) CheckDlgButton(hDlg, IDC_CHK_NOMIP, BST_CHECKED);
            if (s_lastFlags & TEXTUREFLAGS_NOLOD) CheckDlgButton(hDlg, IDC_CHK_NOLOD, BST_CHECKED);
            if (s_lastFlags & TEXTUREFLAGS_ALL_MIPS) CheckDlgButton(hDlg, IDC_CHK_MINMIP, BST_CHECKED);
            if (s_lastFlags & TEXTUREFLAGS_PRE_SRGB) CheckDlgButton(hDlg, IDC_CHK_SRGB, BST_CHECKED);
        }
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            // Get Format
            HWND hCombo = GetDlgItem(hDlg, IDC_FORMAT);
            int idx = (int)SendMessageA(hCombo, CB_GETCURSEL, 0, 0);
            VTFImageFormat fmt = (VTFImageFormat)SendMessage(hCombo, CB_GETITEMDATA, idx, 0);
            if (idx == CB_ERR) fmt = IMAGE_FORMAT_DXT5; // Fallback
            
            gData->exportFormat = fmt;
            
            // Get Flags
            uint32_t flags = 0; // Initialize to 0, let checkboxes drive it
            if (IsDlgButtonChecked(hDlg, IDC_CHK_POINTSAMPLE)) flags |= TEXTUREFLAGS_POINTSAMPLE;
            if (IsDlgButtonChecked(hDlg, IDC_CHK_TRILINEAR)) flags |= TEXTUREFLAGS_TRILINEAR;
            if (IsDlgButtonChecked(hDlg, IDC_CHK_CLAMPS)) flags |= TEXTUREFLAGS_CLAMPS;
            if (IsDlgButtonChecked(hDlg, IDC_CHK_CLAMPT)) flags |= TEXTUREFLAGS_CLAMPT;
            if (IsDlgButtonChecked(hDlg, IDC_CHK_ANISOTROPIC)) flags |= TEXTUREFLAGS_ANISOTROPIC;
            if (IsDlgButtonChecked(hDlg, IDC_CHK_HINTDXT5)) flags |= TEXTUREFLAGS_HINT_DXT5;
            if (IsDlgButtonChecked(hDlg, IDC_CHK_NORMAL)) flags |= TEXTUREFLAGS_NORMAL;
            if (IsDlgButtonChecked(hDlg, IDC_CHK_NOMIP)) flags |= TEXTUREFLAGS_NOMIP;
            if (IsDlgButtonChecked(hDlg, IDC_CHK_NOLOD)) flags |= TEXTUREFLAGS_NOLOD;
            if (IsDlgButtonChecked(hDlg, IDC_CHK_MINMIP)) flags |= TEXTUREFLAGS_ALL_MIPS; // 0x00000400
            if (IsDlgButtonChecked(hDlg, IDC_CHK_SRGB)) flags |= TEXTUREFLAGS_PRE_SRGB; // 0x00080000
            
            if (fmt == IMAGE_FORMAT_DXT5 || fmt == IMAGE_FORMAT_RGBA8888) {
                flags |= TEXTUREFLAGS_EIGHTBITALPHA;
            } else if (fmt == IMAGE_FORMAT_DXT1) {
                flags |= TEXTUREFLAGS_ONEBITALPHA;
            }
            
            gData->flags = flags;
            gData->generateMipmaps = !IsDlgButtonChecked(hDlg, IDC_CHK_NOMIP); // If No Mipmap is checked, don't generate

            // Update persistent settings
            s_lastFormat = fmt;
            s_lastFlags = flags;
            s_lastMipmaps = gData->generateMipmaps;

            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        else if (LOWORD(wParam) == IDCANCEL) {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}

static void DoOptionsStart(void) {
    *gResult = noErr;
    
    // Get correct module handle for the DLL
    HMODULE hModule = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCSTR)&PluginMain,
                       &hModule);

    // Get PICA UI Suite for Window Handle if possible, or use NULL
    HWND parent = GetActiveWindow();
    
    INT_PTR result = DialogBoxParamA(hModule, 
                                   MAKEINTRESOURCEA(IDD_OPTIONS), 
                                   parent, 
                                   VTFOptionsDlgProc, 
                                   0);
                                   
    if (result == -1) {
        // Resource not found or other error
        char buf[256];
        sprintf_s(buf, "Failed to open Options Dialog. Error: %lu\nMake sure resources are linked correctly.", GetLastError());
        MessageBoxA(nullptr, buf, "VTF Plugin Error", MB_OK | MB_ICONERROR);
        
        // Fallback to defaults if dialog fails
        gData->exportFormat = IMAGE_FORMAT_DXT5;
        gData->flags = TEXTUREFLAGS_NORMAL | TEXTUREFLAGS_EIGHTBITALPHA;
    }
    else if (result == IDCANCEL) {
        *gResult = userCanceledErr;
    }
}

static void DoOptionsContinue(void) {
    *gResult = noErr;
}

static void DoOptionsFinish(void) {
    *gResult = noErr;
}

//-------------------------------------------------------------------------------
//	Estimate
//-------------------------------------------------------------------------------

static void DoEstimatePrepare(void) {
    gFormatRecord->maxData = 0;
    *gResult = noErr;
}

static void DoEstimateStart(void) {
    *gResult = noErr;
    
    VPoint imageSize = GetFormatImageSize();
    int width = imageSize.h;
    int height = imageSize.v;
    
    // Estimate file size (header + mipmaps)
    int32 estimate = 80; // VTF header
    
    int mipWidth = width;
    int mipHeight = height;
    
    while (mipWidth >= 1 && mipHeight >= 1) {
        if (gData->exportFormat == IMAGE_FORMAT_DXT1) {
            estimate += ((mipWidth + 3) / 4) * ((mipHeight + 3) / 4) * 8;
        } else {
            estimate += ((mipWidth + 3) / 4) * ((mipHeight + 3) / 4) * 16;
        }
        
        if (mipWidth == 1 && mipHeight == 1) break;
        mipWidth = (mipWidth > 1) ? mipWidth / 2 : 1;
        mipHeight = (mipHeight > 1) ? mipHeight / 2 : 1;
    }
    
    gFormatRecord->minDataBytes = estimate;
    gFormatRecord->maxDataBytes = estimate;
}

static void DoEstimateContinue(void) {
    *gResult = noErr;
}

static void DoEstimateFinish(void) {
    *gResult = noErr;
}

//-------------------------------------------------------------------------------
//	Filter
//-------------------------------------------------------------------------------

static void DoFilterFile(void) {
    DebugLog("DoFilterFile called");
    *gResult = noErr;
    
    // Read first 4 bytes to check for VTF signature
    uint8_t header[4];
    
    *gResult = PSSDKSetFPos(gFormatRecord->dataFork,
                            gFormatRecord->posixFileDescriptor,
                            gFormatRecord->pluginUsingPOSIXIO,
                            fsFromStart, 0);
    if (*gResult != noErr) return;
    
    ReadSome(4, header);
    
    if (*gResult == noErr) {
        // Check for "VTF\0" signature
        if (header[0] != 'V' || header[1] != 'T' || header[2] != 'F' || header[3] != '\0') {
            DebugLog("DoFilterFile: Not a VTF file");
            *gResult = formatCannotRead;
        } else {
            DebugLog("DoFilterFile: Valid VTF file");
        }
    } else {
        DebugLogInt("DoFilterFile: Read failed with error", *gResult);
    }
}

//-------------------------------------------------------------------------------
//	Helpers
//-------------------------------------------------------------------------------

static VPoint GetFormatImageSize(void) {
    VPoint returnPoint = { 0, 0 };
    if (gFormatRecord->HostSupports32BitCoordinates && gFormatRecord->PluginUsing32BitCoordinates) {
        returnPoint.v = gFormatRecord->imageSize32.v;
        returnPoint.h = gFormatRecord->imageSize32.h;
    } else {
        returnPoint.v = gFormatRecord->imageSize.v;
        returnPoint.h = gFormatRecord->imageSize.h;
    }
    return returnPoint;
}

static void SetFormatImageSize(VPoint inPoint) {
    if (gFormatRecord->HostSupports32BitCoordinates && gFormatRecord->PluginUsing32BitCoordinates) {
        gFormatRecord->imageSize32.v = inPoint.v;
        gFormatRecord->imageSize32.h = inPoint.h;
    } else {
        gFormatRecord->imageSize.v = static_cast<int16>(inPoint.v);
        gFormatRecord->imageSize.h = static_cast<int16>(inPoint.h);
    }
}

// end VTFPlugin.cpp
