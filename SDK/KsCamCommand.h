// -----------------------------------------------------------------------------
// KsCamCommand.h
// -----------------------------------------------------------------------------

#pragma once

//	OnePush function
#define CAM_CMD_ONEPUSH_AE				L"CAM_CMD_ONEPUSH_AE"
#define CAM_CMD_ONEPUSH_WHITEBALANCE	L"CAM_CMD_ONEPUSH_WHITEBALANCE"
#define CAM_CMD_ONEPUSH_SOFTTRIGGER		L"CAM_CMD_ONEPUSH_SOFTTRIGGER"
#define CAM_CMD_ONEPUSH_TRIGGERCANCEL	L"CAM_CMD_ONEPUSH_TRIGGERCANCEL"

//	for receive image
#define CAM_CMD_GET_FRAMESIZE			L"CAM_CMD_GET_FRAMESIZE"			//	Get frame size(include ImageInfo)
#define CAM_CMD_START_FRAMETRANSFER		L"CAM_CMD_START_FRAMETRANSFER"		//	Start frame transfer
#define CAM_CMD_STOP_FRAMETRANSFER		L"CAM_CMD_STOP_FRAMETRANSFER"		//	Stop frame transfer
#define CAM_CMD_IS_TRANSFER_STARTED		L"CAM_CMD_IS_TRANSFER_STARTED"		//	Check transfer started

//	Frame dropless mode
#define CAM_CMD_FRAME_DROPLESS			L"CAM_CMD_FRAME_DROPLESS"			//	Frame dropless mode

//	Group camera
#define CAM_CMD_GROUPING				L"CAM_CMD_GROUPING"					//	Grouping settings

//	Others
#define CAM_CMD_GET_SDKVERSION			L"CAM_CMD_GET_SDKVERSION"			//	Get SDK's version

#define CAM_CMD_STRING_MAX			64


//-------------------------------------------------------
//	CAM_CMD_ONEPUSH_AE
//	CAM_CMD_ONEPUSH_WHITEBALANCE
//	CAM_CMD_ONEPUSH_SOFTTRIGGER
//	CAM_CMD_ONEPUSH_TRIGGERCANCEL
//
//		... no parameter


//-------------------------------------------------------
//	CAM_CMD_GET_FRAMESIZE
//
struct CAM_CMD_GetFrameSize
{
	lx_uint32		uiFrameSize;		//	frame buffer size (include ImageInfo)
	lx_uint32		uiFrameInterval;	//	frame interval
	lx_uint32		uiRShutterDelay;	//	RSutter delay (usec)
};


//-------------------------------------------------------
//	CAM_CMD_START_FRAMETRANSFER
//		this struct is INOUT. if return LX_ERR_OUTOFMEMORY then uiImageBufferNum was allocatable num. 
struct CAM_CMD_StartFrameTransfer
{
	lx_uint32		uiImageBufferNum;		//	Driver allocates memory by this parameter. (1 - 128)
};

//	CAM_CMD_STOP_FRAMETRANSFER
//
//		... no parameter

//-------------------------------------------------------
//	CAM_CMD_IS_TRANSFER_STARTED
struct CAM_CMD_IsTransferStarted
{
	bool	bStarted;		//	true:Started, false:Stopped
};

//-------------------------------------------------------
//	CAM_CMD_FRAME_DROPLESS
struct CAM_CMD_FrameDropless
{
	bool	bSet;			//	true:Set, false:Get
	bool	bOnOff;			//	true:ON, false:OFF
};

//-------------------------------------------------------
//	CAM_CMD_GROUPING
//
enum ECamGroupCaptureMode
{
	egcmNoGroup = 0x00,
	egcmSoftHard = 0x10,
	egcmSoftSoft = 0x20,
};

struct CAM_CMD_Grouping
{
	bool				bSet;				//	true:Set, false:Get
	lx_uchar8			ucGroup[CAM_DEVICE_MAX];
};

//-------------------------------------------------------
//	CAM_CMD_GET_SDKVERSION
//
struct CAM_CMD_GetSdkVersion
{
	lx_wchar		wszSdkVersion[CAM_VERSION_MAX];
};


// -----------------------------------------------------------------------------
struct CAM_Command
{
	//	for supported command set see CAM_CMD_...
	lx_wchar		wszString[CAM_CMD_STRING_MAX];
	void*			pParameter;

public:
	// constructor is needed to initialize the structure
	CAM_Command() { memset(this, NULL, sizeof(CAM_Command)); }
};
