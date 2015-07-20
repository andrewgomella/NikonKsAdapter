// -----------------------------------------------------------------------------
// KsCam.h
// -----------------------------------------------------------------------------

#ifndef __KSCAM_H_INCLUDED__
#define __KSCAM_H_INCLUDED__

#define DLLEXPORT __declspec(dllexport)
#define DLLIMPORT __declspec(dllimport)

#ifdef __cplusplus
#  define EXTERN extern "C"
#else
#  define EXTERN extern
#endif

#ifdef CAMERADRIVER_EXPORTS
#  define CAM_SDK_API EXTERN DLLEXPORT
#else
#  define CAM_SDK_API EXTERN DLLIMPORT
#endif

#define IN
#define OUT
#define INOUT

typedef void *HANDLE;

// -----------------------------------------------------------------------------
#ifndef LX_OK
   #define LX_OK				0
#endif
#ifndef LX_ERR_UNEXPECTED
   #define LX_ERR_UNEXPECTED	-1
#endif
#ifndef LX_ERR_NOTIMPL
   #define LX_ERR_NOTIMPL		-2
#endif
#ifndef LX_ERR_OUTOFMEMORY
   #define LX_ERR_OUTOFMEMORY	-3
#endif
#ifndef LX_ERR_INVALIDARG
   #define LX_ERR_INVALIDARG	-4
#endif
#ifndef LX_ERR_NOINTERFACE
   #define LX_ERR_NOINTERFACE	-5
#endif
#ifndef LX_ERR_POINTER
   #define LX_ERR_POINTER		-6
#endif
#ifndef LX_ERR_HANDLE
   #define LX_ERR_HANDLE		-7
#endif
#ifndef LX_ERR_ABORT
   #define LX_ERR_ABORT 		-8
#endif
#ifndef LX_ERR_FAIL
   #define LX_ERR_FAIL			-9
#endif
#ifndef LX_ERR_ACCESSDENIED
   #define LX_ERR_ACCESSDENIED	-10
#endif

// ------------------------------------------
typedef char					lx_char8;
typedef unsigned char			lx_uchar8;

typedef short					lx_short16;
typedef unsigned short			lx_ushort16;

#if !defined(VC2008)
typedef unsigned short			lx_wchar;
#else
typedef wchar_t 				lx_wchar;
#endif

typedef int 					lx_int32;
typedef unsigned int			lx_uint32;
typedef __int32 				lx_result;
typedef __int64 				lx_int64;
typedef unsigned __int64		lx_uint64;

#if defined (WIN64) || defined (_WIN64) || defined (__WIN64)
typedef __int64 				lx_int3264;
typedef unsigned __int64		lx_uint3264;
typedef size_t					lx_size;
#else
typedef int 					lx_int3264;
typedef unsigned int			lx_uint3264;
typedef size_t					lx_size;
#endif




// ---------------------------------------------------------------------------------------------------------------
#define CAM_DEVICE_MAX			100
#define CAM_ERRMSG_MAX			256
#define CAM_VERSION_MAX			16
#define CAM_TEXT_MAX			256
#define CAM_NAME_MAX			32

#include "KsCamFeature.h"
#include "KsCamImage.h"
#include "KsCamCommand.h"
#include "KsCamEvent.h"


// -----------------------------------------------------------------------------
enum ECamDeviceType
{
	ecdtUnknown = 0,
	eRi2,				//	Projectname Provisional
	eRi2_Simulator,		//	Projectname Provisional
	eQi2,				//	Projectname Provisional
	eQi2_Simulator,		//	Projectname Provisional
};

typedef struct CAM_Device
{
	ECamDeviceType	eCamDeviceType;
	lx_uint32		uiSerialNo;
	lx_wchar		wszFwVersion[CAM_VERSION_MAX];
	lx_wchar		wszFpgaVersion[CAM_VERSION_MAX];
	lx_wchar		wszFx3Version[CAM_VERSION_MAX];
	lx_wchar		wszUsbVersion[CAM_VERSION_MAX];
	lx_wchar		wszDriverVersion[CAM_VERSION_MAX];
	lx_wchar		wszCameraName[CAM_NAME_MAX];
public:
	CAM_Device() { memset(this, NULL, sizeof(CAM_Device)); }
} CAM_Device;


// -----------------------------------------------------------------------------
// ************ Devices **************
//	Enumerates the list of devices that are connected (simulator included).
//		Parameter:
//			uiDeviceCount - device count. all connected device.
//			ppstCamDevice - device info(work memory on SDK)
// MAX 100 include Simulator
CAM_SDK_API lx_result CAM_OpenDevices(OUT lx_uint32& uiDeviceCount, OUT CAM_Device** ppstCamDevice);

//	Release device created by CAM_GetDeviceList().
CAM_SDK_API lx_result CAM_CloseDevices(void); //When No Grabber, please call this method

//	Open camera.
//		Parameter:
//			uiDeviceIndex		- device index		(0..uiDeviceCount - 1:device index, Else:first valid device)
//			uiCameraHandle		- camera handle		(more than 0)
//			uiErrMsgMaxSize		- sizeof pwszErrMsg (need CAM_ERRMSG_MAX)
//			pwszErrMsg			- filled with human readable error description
//
CAM_SDK_API lx_result CAM_Open(IN const lx_uint32 uiDeviceIndex, OUT lx_uint32& uiCameraHandle, IN const lx_uint32 uiErrMsgMaxSize, OUT lx_wchar* pwszErrMsg);

//	Close camera.
//
CAM_SDK_API lx_result CAM_Close(IN const lx_uint32 uiCameraHandle);

// ************ Features ************		(ref. KsCamFeature.h)		See ECamFeatureId in KsCamFeature.h for iFeature ID
//	Gets all features
//		no need to specify (vectFeatureValue.ppFeatureValue[i])->iFeatureId
//  
CAM_SDK_API lx_result CAM_GetAllFeatures(IN const lx_uint32 uiCameraHandle, OUT Vector_CAM_FeatureValue& vectFeatureValue);

//	Gets selected set of features
//		requested set of features is defined by on input : (vectFeatureValue.ppFeatureValue[i])->iFeatureId
//
CAM_SDK_API lx_result CAM_GetFeatures(IN const lx_uint32 uiCameraHandle, INOUT Vector_CAM_FeatureValue& vectFeatureValue);

// ----> CAM_GetAllFeatures() get after reloading all features from CCU. but CAM_GetFeatures() get from SDK memory.

//	Sets selected set of features
//		requested set of features is defined by on input : (vectFeatureValue.ppFeatureValue[i])->iFeatureId
//
CAM_SDK_API lx_result CAM_SetFeatures(IN const lx_uint32 uiCameraHandle, INOUT Vector_CAM_FeatureValue& vectFeatureValue);

// feature description allocated on application side
//
CAM_SDK_API lx_result CAM_GetFeatureDesc(IN const lx_uint32 uiCameraHandle, IN lx_uint32 uiFeatureId, OUT CAM_FeatureDesc& stFeatureDesc);

// ************ Images ************			(ref. KsCamImage.h)
//	Get image data.
//		Nikon copies the image data to buffer allocated by NIS-E and passed as argument 'sImage'
//		During the acquisition callbacks are generated e.g. eExposureEnd, eDataReady
//
CAM_SDK_API lx_result CAM_GetImage(IN const lx_uint32 uiCameraHandle, IN bool bNewestRequired, INOUT CAM_Image& stImage, OUT lx_uint32& uiRemained);

// ************ Commands ************		(ref. KsCamCommand.h)
//	Request function command.
//		CAM_CMD_ONEPUSH_AE
//		CAM_CMD_ONEPUSH_WHITEBALANCE
//		CAM_CMD_ONEPUSH_SOFTTRIGGER
//		CAM_CMD_GET_FRAMESIZE
//		CAM_CMD_START_FRAMETRANSFER
//		CAM_CMD_STOP_FRAMETRANSFER
//
CAM_SDK_API lx_result CAM_Command(IN const lx_uint32 uiCameraHandle, IN const lx_wchar* pwszCommand, INOUT void* pData);


// -----------------------------------------------------------------------------
// ************ Events / Callbacks ************		(ref. KsCamEvent.h)

//	Polling drivers events.
//		Parameter:
//			uiCameraHandle		- camera handle
//			hStopEvent			- NULL=NonBlocking[return soon], !NULL=Blocking=[Handle of CreateEvent(). Applecation set this handle then return.]
//			eEventType			- event type (Set one of ECamEventType)
//			pstEvent			- detail event data
//
CAM_SDK_API lx_result CAM_EventPolling(IN const lx_uint32 uiCameraHandle, IN const HANDLE hStopEvent, IN ECamEventType eEventType, OUT CAM_Event* pstEvent);

//	Set event callback
//		Parameter:
//			uiCameraHandle		- camera handle
//			pstEvent			- detail event data  (It is SDK's memory. Application don't delete this.)
//			pTransData			- Intended for application side, the SDK should only copy it to the callback parameter.
//								  Application will use it to get the 'camera object' instance pointer into the static callback function.
//	* Application use CAM_SetEventCallback() or CAM_EventPolling().
//
typedef void (*FCAM_EventCallback)(IN const lx_uint32 uiCameraHandle, IN CAM_Event* pstEvent, IN void* pTransData);
CAM_SDK_API lx_result CAM_SetEventCallback(IN const lx_uint32 uiCameraHandle, IN FCAM_EventCallback fCAM_EventCallback, IN void* pTransData);

//Reserved,
//	Set notice callback
//		Parameter:
//			uiCameraHandle		- camera handle
//			pstNotice			- detail notice data  (It is SDK's memory. Application don't delete this.)
//			pTransData			- Intended for application side, the SDK should only copy it to the callback parameter.
//								  Application will use it to get the 'camera object' instance pointer into the static callback function.
//	* don't use now. (for future)
//
typedef void (*FCAM_NoticeCallback)(IN const lx_uint32 uiCameraHandle, IN CAM_Notice* pstNotice, IN void* pTransData);
CAM_SDK_API lx_result CAM_SetNoticeCallback(IN const lx_uint32 uiCameraHandle, IN FCAM_NoticeCallback fCAM_NoticeCallback, IN void* pTransData);

//#undef CAM_SDK_API
#endif __KSCAM_H_INCLUDED__
