// -----------------------------------------------------------------------------
// KsCamImage.h
// -----------------------------------------------------------------------------

#pragma once

// -----------------------------------------------------------------------------
struct CAM_Image
{
	void*				pDataBuffer;		//	set in driver (include image info)
	lx_uint32			uiDataBufferSize;	//	set by application. SDK checks whether m_uiDataBufferSize is the same as CAM_FrameSize::uiFrameSize.
	lx_uint32			uiImageSize;		//	set in SDK (from driver)
	lx_uint32			uiEndTime;			//	set in SDK (from driver)
	lx_uint64			uiFrameCount;		//	Application use (SDK is no care)
	lx_uint32			uiRefCount;			//	Application use (SDK is no care)

public:
	// constructor is needed to initialize the structure
	CAM_Image() { memset(this, NULL, sizeof(CAM_Image)); }

	// ***
	virtual lx_uint32 AddRef() { return 0; };
	virtual void Release() {};
};


// Image Info ====================================================================

typedef struct
{
	//	0
	lx_ushort16	usFrameNo;				//	0x0000 -> 0xFFFF
	lx_ushort16	usTrggerOptionNo;		//	0=not trigger mode 1 -> 65535 (TrggerOption max)
	lx_ushort16	usMultiExposureTimeNo;	//	0=not multi exposure time 1 -> 15 (MultiExposureTime max)
	lx_uchar8	ucReserve000[2];		//	
	//	8
	lx_uint32	uiExposureTime;			//	100 -> 120000000 (usec)
	lx_uchar8	ucReserve008[4];		//	
	//	16
	lx_uchar8	ucReserve016[48];		//	
	//	64
	lx_uchar8	ucCameraType;			//	0=DS-Ri2, 1=DS-Qi2
	lx_uchar8	ucImageMode;			//	1 -> 7
	lx_uchar8	ucImageColor;			//	1=RGB24, 2=YUV444, 3=RAW(Mono16)
	lx_uchar8	ucTriggerMode;			//	0=OFF, 1=Hard, 2=Soft
	lx_uint32	uiSerialNo;				//	
	//	72
	lx_uint32	uiFwVersion;			//	
	lx_uint32	uiFpgaVersion;			//	
	//	80
	lx_ushort16	usFx3Version;			//	
	lx_uchar8	ucReserve080[2];		//	
	lx_ushort16	usImageWidth;			//	
	lx_ushort16	usImageHeight;			//	
	//	88
	lx_ushort16	usRoiLeft;				//	
	lx_ushort16	usRoiTop;				//	
	lx_uint32	uiImageSize;			//	
	//	96
	lx_uchar8	ucExposureMode;			//	0=AE, 1=OnePushAE, 2=Manual, 3=MultiExposureTime
	lx_char8	cExposureBias;			//	-6 -> 6
	lx_uchar8	ucTone;					//		(...not supported)
	lx_uchar8	ucScene;				//	Scene mode code (DS-Ri2 only:DS-Qi2=0)
	lx_uchar8	ucReserve096[4];		//	
	//	104
	lx_ushort16	usGain;					//	100 -> 6400
	lx_short16	sBrightness;			//	-50 -> 50
	lx_char8	cSharpness;				//	-3 -> 5 (DS-Ri2 only:DS-Qi2=0)
	lx_uchar8	ucCaptureMode;			//	0=OFF, 1=ON
	lx_uchar8	ucAeStay;				//	0=Running, 1=Stay or Manual, 2=Disable
	lx_uchar8	ucMeteringMode;			//	0=Average, 1=Peak
	//	112
	lx_ushort16	usMeteringAreaLeft;		//	
	lx_ushort16	usMeteringAreaTop;		//	
	lx_ushort16	usMeteringAreaWidth;	//	
	lx_ushort16	usMeteringAreaHeight;	//	
	//	120
	lx_short16	sHue;					//	-50 -> 50 (DS-Ri2 only:DS-Qi2=0)
	lx_short16	sSaturation;			//	-50 -> 50 (DS-Ri2 only:DS-Qi2=0)
	lx_ushort16	usWhiteBalanceRed;		//	0 -> 799 (DS-Ri2 only:DS-Qi2=0)
	lx_ushort16	usWhiteBalanceBlue;		//	0 -> 799 (DS-Ri2 only:DS-Qi2=0)
	//	128
	lx_ushort16	usDefect;				//		(...not supported)
	lx_uchar8	ucReserve128[6];		//	
	//	136
	lx_uchar8	ucReserve136[120];		//	
} CAM_ImageInfo;

#define CAM_IMG_INFO_SIZE	sizeof(CAM_ImageInfo)

struct CAM_ImageInfoEx
{
	union {
		lx_uchar8			ucInfo[CAM_IMG_INFO_SIZE];
		CAM_ImageInfo		stInfo;
	};

public:
	CAM_ImageInfoEx()				{ memset(this, NULL, sizeof(CAM_ImageInfoEx)); }
	void CopyInto(lx_uchar8* pInfo)	{ memcpy(ucInfo, pInfo, CAM_IMG_INFO_SIZE); }
	CAM_ImageInfo* GetInfo(CAM_Image& stImage)
	{
		CopyInto(&((lx_uchar8*)stImage.pDataBuffer)[stImage.uiImageSize]);
		return &stInfo;
	}
};
