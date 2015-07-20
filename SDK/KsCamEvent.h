// -----------------------------------------------------------------------------
// KsCamEvent.h
// -----------------------------------------------------------------------------

#pragma once

enum ECamEventType
{
	ecetUnknown = -1,
	ecetImageReceived = 0,
	ecetFeatureChanged,
	ecetExposureEnd,
	ecetTriggerReady,
	ecetDeviceCapture,
	ecetAeStay,
	ecetAeRunning,
	ecetAeDisable,
	ecetTransError,
	ecetBusReset,
	ecetEventTypeMax,
};

enum ECamEventBusResetCode
{
	ecebrcHappened = 1,
	ecebrcRestored = 2,
	ecebrcFailed = 3,
};

//-------------------------------------------------------
//	ecetImageReceived
//
struct CAM_EventImageReceived
{
	lx_uint32				uiTick;
	lx_uint32				uiFrameNo;
	lx_uint32				uiRemained;
};

//-------------------------------------------------------
//	ecetFeatureChanged
//
struct CAM_EventFeatureChanged
{
	lx_uint32				uiTick;
	lx_uint32				uiFeatureId;
	CAM_Variant				stVariant;
};

//-------------------------------------------------------
//	ecetExposureEnd / ecetTriggerReady / ecetDeviceCapture / ecetAeStay / ecetAeRunning / ecetAeDisable
//

struct CAM_EventSignal
{
	lx_uint32				uiTick;
	ECamEventType			eEventType;
};

//-------------------------------------------------------
//	ecetTransError
//
struct CAM_EventTransError
{
	lx_uint32				uiTick;
	lx_uint32				uiUsbErrorCode;		//	USB error code
	lx_uint32				uiDriverErrorCode;	//	Driver error code
	lx_uint32				uiReceivedSize;		//	Set when received image size error
	lx_uint32				uiSettingSize;		//	Set when received image size error
};

//-------------------------------------------------------
//	ecetBusReset
//
struct CAM_EventBusReset
{
	lx_uint32				uiTick;
	ECamEventBusResetCode	eBusResetCode;		//	BusReset code(ref. ECamEventBusResetCode)
	bool					bImageCleared;		//	Flag of cleared images in driver by BusReset.
};


//-------------------------------------------------------
//	CAM_Event : output event to application
//

struct CAM_Event
{
	ECamEventType	eEventType;
	union
	{
		CAM_EventImageReceived		stImageReceived;	//	ecetImageReceived
		CAM_EventFeatureChanged		stFeatureChanged;	//	ecetFeatureChanged
		CAM_EventSignal				stSignal;			//	ecetExposureEnd
														//	ecetTriggerReady
														//	ecetDeviceCapture
														//	ecetAeStay
														//	ecetAeRunning
														//	ecetAeDisable
		CAM_EventTransError			stTransError;		//	ecetTransError
		CAM_EventBusReset			stBusReset;			//	ecetBusReset
	};
public:
	CAM_Event() { memset(this, NULL, sizeof(CAM_Event)); }
	CAM_Event& operator = (CAM_Event const& vec)
	{
		eEventType = vec.eEventType;
		switch(eEventType) {
			case	ecetImageReceived:
				stImageReceived = vec.stImageReceived;
				break;
			case	ecetFeatureChanged:
				stFeatureChanged = vec.stFeatureChanged;
				break;
			case	ecetExposureEnd:
			case	ecetTriggerReady:
			case	ecetDeviceCapture:
			case	ecetAeStay:
			case	ecetAeRunning:
			case	ecetAeDisable:
				stSignal = vec.stSignal;
				break;
			case	ecetTransError:
				stTransError = vec.stTransError;
				break;
			case	ecetBusReset:
				stBusReset = vec.stBusReset;
				break;
		}
		return *this;
	}
};


//---------------------------------------------------------------------------------------------------------
//	CAM_Notice
enum ECamNoticeType
{
	ecntUnknown = -1,
	ecntTransError = 0,
	ecntGroup,
	ecntInfo,
	ecntNoticeTypeMax,
};

enum ECamNoticeGroupCode
{
	ecngcEventInsufficient = 1,
	ecngcSetFeatureError,
	ecngcSetTransError,
	ecngcSoftTriggerError,
	ecngcSetImageFormatError,
	ecngcGetImageDataError,
	ecngcBusReset,
};

enum ECamNoticeInfoCode		//	ECamNoticeInfoCode is not used
{
	ecnicTemperature = 1,
	ecnicComment,
};

//-------------------------------------------------------
//	ecntTransError
//
struct CAM_NoticeTransError
{
	lx_uint32				uiTick;
	lx_uint32				uiRequestCode;
	lx_uint32				uiCameraErrorCode;	//	Camera error code
	lx_uint32				uiUsbErrorCode;		//	USB error code
	lx_uint32				uiDriverErrorCode;	//	Driver error code
};

//-------------------------------------------------------
//	ecntGroup
//
struct CAM_NoticeGroup
{
	lx_uint32				uiTick;
	ECamNoticeGroupCode		eCode;
	lx_int32				iDetail;
	lx_wchar				wszComment[CAM_TEXT_MAX];
};

//-------------------------------------------------------
//	ecntInfo
//
struct CAM_NoticeInfo	//	CAM_NoticeInfo is not used
{
	lx_uint32				uiTick;
	ECamNoticeInfoCode		eCode;
	lx_int32				iValue;
	double					dValue;
	bool					bValue;
	lx_wchar				wszText[CAM_TEXT_MAX];
};

//-------------------------------------------------------
//	CAM_Notice : notice parameter to application
//

struct CAM_Notice
{
	ECamNoticeType	eNoticeType;
	union
	{
		CAM_NoticeTransError		stTransError;			//	ecntTransError
		CAM_NoticeGroup				stGroup;				//	ecntGroup
		CAM_NoticeInfo				stInfo;					//	ecntInfo(not used)
	};
public:
	CAM_Notice() { memset(this, NULL, sizeof(CAM_Notice)); }
	CAM_Notice& operator = (CAM_Notice const& vec)
	{
		eNoticeType = vec.eNoticeType;
		switch(eNoticeType) {
			case	ecntTransError:
				stTransError = vec.stTransError;
				break;
			case	ecntGroup:
				stGroup = vec.stGroup;
				break;
			case	ecntInfo:
				stInfo = vec.stInfo;
				break;
		}
		return *this;
	}
};
