// VTF Format Plugin Resource File
// Based on Adobe Photoshop SDK SimpleFormat sample

//-------------------------------------------------------------------------------
//	Definitions -- Required by include files.
//-------------------------------------------------------------------------------

#define plugInName			"VTF Format"
#define plugInCopyrightYear	"2024"
#define plugInDescription \
	"Valve Texture Format (VTF) file format plugin for Adobe Photoshop."

//-------------------------------------------------------------------------------
//	Definitions -- Required by other resources in this rez file.
//-------------------------------------------------------------------------------

#define vendorName			"Softlamps"
#define plugInAETEComment 	"VTF format file format module"

#define plugInSuiteID		'VTFF'
#define plugInClassID		'vtfF'
#define plugInEventID		typeNull

//-------------------------------------------------------------------------------
//	Set up included files for Windows.
//-------------------------------------------------------------------------------

#include "PIDefines.h"
#include "PIResourceDefines.h"

#define Rez
#include "PIGeneral.h"
#include "PIUtilities.r"

#include "PITerminology.h"
#include "PIActions.h"

//-------------------------------------------------------------------------------
//	PiPL resource
//-------------------------------------------------------------------------------

resource 'PiPL' (16000, plugInName " PiPL", purgeable)
{
	{
		Kind { ImageFormat },
		Name { plugInName },
		Version { (latestFormatVersion << 16) | latestFormatSubVersion },

		#ifdef MSWindows
			CodeEntryPointWin64 { "PluginMain" },
		#endif

		// File type and extension
		FmtFileType { 'VTF ', '8BIM' },
		ReadExtensions { { 'VTF ' } },
		WriteExtensions { { 'VTF ' } },
		FilteredExtensions { { 'VTF ' } },

		// Capabilities (must specify one from each pair, in order)
		FormatFlags { fmtDoesNotSaveImageResources,
		              fmtCanRead,
		              fmtCanWrite,
		              fmtCanWriteIfRead,
		              fmtCannotWriteTransparency,
		              fmtCannotCreateThumbnail },

		// Supported color modes
		SupportedModes
		{
			noBitmap, noGrayScale,
			noIndexedColor, doesSupportRGBColor,
			noCMYKColor, noHSLColor,
			noHSBColor, noMultichannel,
			noDuotone, noLABColor
		},

		// Enable info - always enabled
		EnableInfo { "true" },

		// Maximum image size
		PlugInMaxSize { 16384, 16384 },
		FormatMaxSize { { 16384, 16384 } },

		// Max channels per mode (bitmap, gray, indexed, RGB, CMYK, HSL, HSB, multi, duo, LAB, gray16, RGB48)
		FormatMaxChannels { { 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0 } }
	}
};

//-------------------------------------------------------------------------------
// end VTFFormat.r
