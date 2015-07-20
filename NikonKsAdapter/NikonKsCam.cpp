///////////////////////////////////////////////////////////////////////////////
// FILE:          NikonKsCam.cpp
// PROJECT:       Micro-Manager
// SUBSYSTEM:     DeviceAdapters
//-----------------------------------------------------------------------------
// DESCRIPTION:   Device adapter for Nikon DS-Ri2 and DS-Qi2
//                Based on several other DeviceAdapters, 
//				  especially ThorLabsUSBCamera
//
// AUTHOR:        Andrew Gomella, andrewgomella@gmail.com, 2015
//
// LICENSE:       This file is distributed under the BSD license.
//                License text is included with the source distribution.
//
//                This file is distributed in the hope that it will be useful,
//                but WITHOUT ANY WARRANTY; without even the implied warranty
//                of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
//                IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//                CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
//                INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES.

#include "NikonKsCam.h"
#include "ModuleInterface.h"
#include <cstdio>
#include <string>
#include <sstream>
#include <boost/algorithm/string.hpp>
#include <boost/lexical_cast.hpp>

//Nikon SDK defines
#define countof(A) (sizeof(A) / sizeof((A)[0]))
/* I am not sure if this number matters in practice as I have never seen the buffer go
over 1. Perhaps it only matters when using the MultiExposure Feature */
#define DEF_DRIVER_BUFFER_NUM       5 
#define DEF_FRAME_SIZE_MAX          (4908 * (3264 + 1) * 3)

using namespace std;

// External names used used by the rest of the system
const char* g_CameraDeviceName = "NikonKsCam";

// Feature names that can't be dynamically set through SDK
const char* g_RoiPositionX = "ROI Position X";
const char* g_RoiPositionY = "ROI Position Y";
const char* g_TriggerFrameCt = "Trigger Frame Count";
const char* g_TriggerFrameDelay = "Trigger Frame Delay";
const char* g_MeteringAreaLeft = "Metering Area Left";
const char* g_MeteringAreaTop = "Metering Area Top";
const char* g_MeteringAreaWidth = "Metering Area Width";
const char* g_MeteringAreaHeight = "Metering Area Height";

///////////////////////////////////////////////////////////////////////////////
// Exported MMDevice API
///////////////////////////////////////////////////////////////////////////////

MODULE_API void InitializeModuleData()
{
   RegisterDevice(g_CameraDeviceName, MM::CameraDevice, "Nikon Ks Camera Adapter");
}

MODULE_API MM::Device* CreateDevice(const char* deviceName)
{
   if (deviceName == 0)
      return 0;

   // decide which device class to create based on the deviceName parameter
   if (strcmp(deviceName, g_CameraDeviceName) == 0)
   {
      // create camera
      return new NikonKsCam();
   }

   // ...supplied name not recognized
   return 0;
}

MODULE_API void DeleteDevice(MM::Device* pDevice)
{
   delete pDevice;
}

///////////////////////////////////////////////////////////////////////////////
// NikonKsCam implementation
// ~~~~~~~~~~~~~~~~~~~~~~~~~~
/* Pointer for KsCam callback function to use */
NikonKsCam* g_pDlg = NULL;

/* Camera event callback function */
FCAM_EventCallback fCAM_EventCallback(const lx_uint32 uiCameraHandle,
                                      CAM_Event* pstEvent, void* pTransData)
{
    g_pDlg->DoEvent(uiCameraHandle, pstEvent, pTransData);
    return nullptr;
}

/* Camera event callback function handler */
void NikonKsCam::DoEvent(const lx_uint32 uiCameraHandle, CAM_Event* pstEvent, void* pTransData)
{
	lx_uint32 lResult = LX_OK;
	std::ostringstream os;
	char strWork[CAM_FEA_COMMENT_MAX];

  if ( uiCameraHandle != this->m_uiCameraHandle )
  {
   LogMessage("DoEvent Error, Invalid Camera Handle \n");
   return;
  }
  switch(pstEvent->eEventType)
  {
    case    ecetImageReceived:
		os << "ImageRecieved Frameno=" << pstEvent->stImageReceived.uiFrameNo << " uiRemain= " << pstEvent->stImageReceived.uiRemained << endl;
		LogMessage(os.str().c_str());
		/* Signal m_frameDoneEvent so we know an image has been recieved */
        m_frameDoneEvent.Set();
		break;
    case    ecetFeatureChanged:
		strcpy(strWork, ConvFeatureIdToName(pstEvent->stFeatureChanged.uiFeatureId));
		os << "Feature Changed Callback: " << strWork << endl;
		LogMessage(os.str().c_str());
		/* Update m_vectFeatureValue to have the new stVariant */
		m_vectFeatureValue.pstFeatureValue[m_mapFeatureIndex[pstEvent->stFeatureChanged.uiFeatureId]].stVariant
			= pstEvent->stFeatureChanged.stVariant;
		/* Update FeatureDesc because it may have changed */
		lResult = CAM_GetFeatureDesc(m_uiCameraHandle, pstEvent->stFeatureChanged.uiFeatureId,
			m_pFeatureDesc[m_mapFeatureIndex[pstEvent->stFeatureChanged.uiFeatureId]]);
		if (lResult != LX_OK) { LogMessage("Error updating featuredesc after callback"); }
		switch (pstEvent->stFeatureChanged.uiFeatureId){
			case eExposureTime:
				UpdateProperty((char*)MM::g_Keyword_Exposure);
				break;
			default:	
				UpdateProperty(strWork);
		}
		break;
    case    ecetExposureEnd:
        break;
    case    ecetTriggerReady:
        break;
    case    ecetDeviceCapture:
        break;
    case    ecetAeStay:
        break;
    case    ecetAeRunning:
        break;
    case    ecetAeDisable:
        break;
    case    ecetTransError:
		os << "Transmit Error T" << pstEvent->stTransError.uiTick << " UsbErrorCode " << pstEvent->stTransError.uiUsbErrorCode << " DriverErrorCode "
			<< pstEvent->stTransError.uiDriverErrorCode << " RecievedSize " << pstEvent->stTransError.uiReceivedSize << " SettingSize " 
			<< pstEvent->stTransError.uiReceivedSize << endl;
		LogMessage(os.str().c_str());
        break;
    case    ecetBusReset:
		os << "Bus Reset Error Code:" << pstEvent->stBusReset.eBusResetCode << " ImageCleared: " << pstEvent->stBusReset.bImageCleared << endl;
		LogMessage(os.str().c_str());
        break;
    default:
        LogMessage("Error: Unknown Event");
        break;
  }
}

/**
* NikonKsCam constructor.
* Setup default all variables and create device properties required to exist
* before intialization. In this case, no such properties were required. All
* properties will be created in the Initialize() method.
*
* As a general guideline Micro-Manager devices do not access hardware in the
* the constructor. We should do as little as possible in the constructor and
* perform most of the initialization in the Initialize() method.
*/
NikonKsCam::NikonKsCam() :
   CCameraBase<NikonKsCam> (),
   m_isInitialized(false),
   m_isRi2(false),
   m_bitDepth(8),
   m_nComponents(1),
   sequenceStartTime_(0),
   roiX_(0),
   roiY_(0),
   roiWidth_(0),
   roiHeight_(0),
   binSize_(1),
   readoutUs_(0.0),
   framesPerSecond(0.0),
   cameraBuf(nullptr),
   cameraBufId(0)
{
   // call the base class method to set-up default error codes/messages
   InitializeDefaultErrorMessages();
   readoutStartTime_ = GetCurrentMMTime();
   thd_ = new MySequenceThread(this);

   /*Initialize image data buffer*/
   stImage.pDataBuffer = new BYTE[DEF_FRAME_SIZE_MAX];
   
   // Create a pre-initialization property and list all the available cameras
   // Demo cameras will be included in the list 
   auto pAct = new CPropertyAction(this, &NikonKsCam::OnCameraSelection);
   CreateProperty("Camera", "", MM::String, false, pAct, true);
   SearchDevices();
}

/* Search for cameras and populate the pre-init camera selection list */
void NikonKsCam::SearchDevices()
{
	auto camName = new char[CAM_NAME_MAX];
	auto lResult = LX_OK;
	CAM_Device* m_pstDevice;

	if (m_uiDeviceCount > 0)
	{
		lResult = CAM_CloseDevices();
		if (lResult != LX_OK)
		{
			LogMessage("Close device error!");
			return;
		}

		m_pstDevice = nullptr;
		m_uiDeviceCount = 0;
	}

	lResult = CAM_OpenDevices(m_uiDeviceCount, &m_pstDevice);
	if (lResult != LX_OK)
	{
		LogMessage("Error calling CAM_OpenDevices().");
		return;
	}

	if (m_uiDeviceCount < 0)
	{
		LogMessage("Device is not connected");
		return;
	}

	for (size_t i = 0; i < m_uiDeviceCount; i++)
	{
		wcstombs(camName, reinterpret_cast<wchar_t const*>(m_pstDevice[i].wszCameraName), CAM_NAME_MAX);
		AddAllowedValue("Camera", camName);
	}
}

/**
* NikonKsCam destructor.
* If this device used as intended within the Micro-Manager system,
* Shutdown() will be always called before the destructor. But in any case
* we need to make sure that all resources are properly released even if
* Shutdown() was not called.
*/
NikonKsCam::~NikonKsCam()
{
	//delete stImage.pDataBuffer;
   StopSequenceAcquisition();
   delete thd_;
}

/**
* Obtains device name.
* Required by the MM::Device API.
*/
void NikonKsCam::GetName(char* name) const
{
   // Return the name used to refer to this device adapter
   CDeviceUtils::CopyLimitedString(name, g_CameraDeviceName);
}

/**
* Initializes the hardware.
* Required by the MM::Device API.
* Typically we access and initialize hardware at this point.
* Device properties are typically created here as well, except
* the ones we need to use for defining initialization parameters.
* Such pre-initialization properties are created in the constructor.
* (This device does not have any pre-initialization properties)
*/
int NikonKsCam::Initialize()
{
	if (m_isInitialized)
	{
		return DEVICE_OK;
	}

	auto camName = new char[CAM_NAME_MAX];
	char strWork[CAM_FEA_COMMENT_MAX];
	auto lResult = LX_OK;
	CAM_Device* m_pstDevice;
	lx_uint32 i;
	lx_wchar szError[CAM_ERRMSG_MAX];
	string camID_string = camID;
	ostringstream os;

	/* Rescan device list */
	lResult = CAM_OpenDevices(m_uiDeviceCount, &m_pstDevice);
	if (lResult != LX_OK)
	{
		LogMessage("Error calling CAM_OpenDevices().");
	}

	os << "Opening Camera: " << camID << endl;
	LogMessage(os.str().c_str());

	/* Find camera in list */
	for (i = 0; i < m_uiDeviceCount; i++)
	{
		wcstombs(strWork, reinterpret_cast<wchar_t const*>(m_pstDevice[i].wszCameraName), CAM_NAME_MAX);

		if (camID_string.compare(strWork) == 0)
		{
			m_uiDeviceIndex = i;
			break;
		}
	}

	ZeroMemory(szError, sizeof(szError));

	/* Open the camera using the deviceIndex number */
	lResult = CAM_Open(m_uiDeviceIndex, m_uiCameraHandle, countof(szError), szError);
	if (lResult != LX_OK)
	{
		os << "Error calling CAM_Open():" << szError << " DeviceIndex: " << m_uiDeviceIndex << endl;
		LogMessage(os.str().c_str());
		throw DEVICE_ERR ;
	}

	/* Connection was succesful, set Device info so we can use it later */
	this->m_stDevice = m_pstDevice[this->m_uiDeviceIndex];
	this->m_isOpened = TRUE;

	/* Setup callback function for event notification and handling */
	lResult = CAM_SetEventCallback(m_uiCameraHandle, reinterpret_cast<FCAM_EventCallback>(fCAM_EventCallback),
	                               m_ptrEventTransData);
	if (lResult != LX_OK)
	{
		LogMessage("Error calling CAM_SetEventCallback().");
		throw DEVICE_ERR ;
	}

	/* Needed to setup callback access */
	g_pDlg = this;

	/* Get all feature values and descriptions */
	GetAllFeatures();
	GetAllFeaturesDesc();

	/* Ri2 has additional features, check if camera is Ri2 */
	switch (m_stDevice.eCamDeviceType)
	{
	case eRi2:
		m_isRi2 = TRUE;
		break;
	case eQi2:
		break;
	case eRi2_Simulator:
		m_isRi2 = TRUE;
		break;
	case eQi2_Simulator:
		break;
	default:
		break;
	}

	// set property list
	// -----------------
	// CameraName
	wcstombs(camName, reinterpret_cast<wchar_t const*>(m_stDevice.wszCameraName), CAM_NAME_MAX);
	auto nRet = CreateProperty(MM::g_Keyword_CameraName, camName, MM::String, true);
	assert(nRet == DEVICE_OK);

	os.str("");
	// Read Only Camera Info
	os << m_stDevice.uiSerialNo << endl;
	nRet = CreateProperty("Serial Number", os.str().c_str(), MM::String, true);
	wcstombs(strWork, reinterpret_cast<wchar_t const*>(m_stDevice.wszFwVersion), CAM_VERSION_MAX);
	nRet = CreateProperty("FW Version", strWork, MM::String, true);
	wcstombs(strWork, reinterpret_cast<wchar_t const*>(m_stDevice.wszFpgaVersion), CAM_VERSION_MAX);
	nRet = CreateProperty("FPGA Version", strWork, MM::String, true);
	wcstombs(strWork, reinterpret_cast<wchar_t const*>(m_stDevice.wszFx3Version), CAM_VERSION_MAX);
	nRet = CreateProperty("FX3 Version", strWork, MM::String, true);
	wcstombs(strWork, reinterpret_cast<wchar_t const*>(m_stDevice.wszUsbVersion), CAM_VERSION_MAX);
	nRet = CreateProperty("USB Version", strWork, MM::String, true);
	wcstombs(strWork, reinterpret_cast<wchar_t const*>(m_stDevice.wszDriverVersion), CAM_VERSION_MAX);
	nRet = CreateProperty("Driver Version", strWork, MM::String, true);
	assert(nRet == DEVICE_OK);


	//Binning is handled by the Image Format setting and this camera only allows hardware bin 3
	nRet = CreateProperty(MM::g_Keyword_Binning, "1", MM::Integer, false);
	nRet = AddAllowedValue(MM::g_Keyword_Binning, "1");
	assert(nRet == DEVICE_OK);

	//Exposure
	auto* pAct = new CPropertyAction(this, &NikonKsCam::OnExposureTime);
	nRet = CreateKsProperty(eExposureTime, pAct);
	assert(nRet == DEVICE_OK);

	//Camera gain
	pAct = new CPropertyAction(this, &NikonKsCam::OnHardwareGain);
	nRet = CreateKsProperty(eGain, pAct);
	assert(nRet == DEVICE_OK);

	//Exposure Limit (for auto exposure)
	pAct = new CPropertyAction(this, &NikonKsCam::OnExposureTimeLimit);
	nRet = CreateKsProperty(eExposureTimeLimit, pAct);
	assert(nRet == DEVICE_OK);

	//Gain Limit (for auto exposure)
	pAct = new CPropertyAction(this, &NikonKsCam::OnGainLimit);
	nRet = CreateKsProperty(eGainLimit, pAct);
	assert(nRet == DEVICE_OK);

	//Brightness
	pAct = new CPropertyAction(this, &NikonKsCam::OnBrightness);
	nRet = CreateKsProperty(eBrightness, pAct);
	assert(nRet == DEVICE_OK);

	//Create properties that are only found on Ri2
	//(All pertain to color post-processing)
	if (m_isRi2 == TRUE)
	{
		pAct = new CPropertyAction(this, &NikonKsCam::OnSharpness);
		nRet = CreateKsProperty(eSharpness, pAct);
		assert(nRet == DEVICE_OK);

		pAct = new CPropertyAction(this, &NikonKsCam::OnHue);
		nRet = CreateKsProperty(eHue, pAct);
		assert(nRet == DEVICE_OK);

		pAct = new CPropertyAction(this, &NikonKsCam::OnSaturation);
		nRet = CreateKsProperty(eSaturation, pAct);
		assert(nRet == DEVICE_OK);

		pAct = new CPropertyAction(this, &NikonKsCam::OnWhiteBalanceRed);
		nRet = CreateKsProperty(eWhiteBalanceRed, pAct);
		assert(nRet == DEVICE_OK);

		pAct = new CPropertyAction(this, &NikonKsCam::OnWhiteBalanceBlue);
		nRet = CreateKsProperty(eWhiteBalanceBlue, pAct);
		assert(nRet == DEVICE_OK);

		pAct = new CPropertyAction(this, &NikonKsCam::OnPresets);
		nRet = CreateKsProperty(ePresets, pAct);
		assert(nRet == DEVICE_OK);
	}

	//ROI Position (range is subject to change depending on format!)
	auto* pFeatureDesc = &m_pFeatureDesc[m_mapFeatureIndex[eRoiPosition]];
	pAct = new CPropertyAction(this, &NikonKsCam::OnRoiX);
	nRet = CreateProperty(g_RoiPositionX, "", MM::Integer, false, pAct);
	assert(nRet == DEVICE_OK);

	pAct = new CPropertyAction(this, &NikonKsCam::OnRoiY);
	nRet = CreateProperty(g_RoiPositionY, "", MM::Integer, false, pAct);
	assert(nRet == DEVICE_OK);
	SetROILimits();

	//Trigger Options
	pFeatureDesc = &m_pFeatureDesc[m_mapFeatureIndex[eTriggerOption]];
	pAct = new CPropertyAction(this, &NikonKsCam::OnTriggerFrame);
	nRet = CreateProperty(g_TriggerFrameCt, "", MM::Integer, false, pAct);
	nRet |= SetPropertyLimits(g_TriggerFrameCt, pFeatureDesc->stTriggerOption.stRangeFrameCount.stMin.ui32Value, pFeatureDesc->stTriggerOption.stRangeFrameCount.stMax.ui32Value);
	assert(nRet == DEVICE_OK);

	pAct = new CPropertyAction(this, &NikonKsCam::OnTriggerDelay);
	nRet = CreateProperty(g_TriggerFrameDelay, "", MM::Integer, false, pAct);
	nRet |= SetPropertyLimits(g_TriggerFrameDelay, pFeatureDesc->stTriggerOption.stRangeDelayTime.stMin.i32Value, pFeatureDesc->stTriggerOption.stRangeDelayTime.stMax.i32Value);
	assert(nRet == DEVICE_OK);

	//Metering Area (range is subject to change depending on format!)
	pAct = new CPropertyAction(this, &NikonKsCam::OnMeteringAreaLeft);
	nRet = CreateProperty(g_MeteringAreaLeft, "", MM::Integer, false, pAct);
	pAct = new CPropertyAction(this, &NikonKsCam::OnMeteringAreaTop);
	nRet |= CreateProperty(g_MeteringAreaTop, "", MM::Integer, false, pAct);
	pAct = new CPropertyAction(this, &NikonKsCam::OnMeteringAreaWidth);
	nRet |= CreateProperty(g_MeteringAreaWidth, "", MM::Integer, false, pAct);
	pAct = new CPropertyAction(this, &NikonKsCam::OnMeteringAreaHeight);
	nRet |= CreateProperty(g_MeteringAreaHeight, "", MM::Integer, false, pAct);
	assert(nRet == DEVICE_OK);
	SetMeteringAreaLimits();

	//List Settings
	//Image Format
	pAct = new CPropertyAction(this, &NikonKsCam::OnImageFormat);
	nRet = CreateKsProperty(eFormat, pAct);
	assert(nRet == DEVICE_OK);

	pAct = new CPropertyAction(this, &NikonKsCam::OnCaptureMode);
	nRet = CreateKsProperty(eCaptureMode, pAct);
	assert(nRet == DEVICE_OK);

	pAct = new CPropertyAction(this, &NikonKsCam::OnMeteringMode);
	nRet = CreateKsProperty(eMeteringMode, pAct);
	assert(nRet == DEVICE_OK);

	//Trigger Mode
	pAct = new CPropertyAction(this, &NikonKsCam::OnTriggerMode);
	nRet = CreateKsProperty(eTriggerMode, pAct);
	assert(nRet == DEVICE_OK);

	//Exposure Mode
	pAct = new CPropertyAction(this, &NikonKsCam::OnExposureMode);
	nRet = CreateKsProperty(eExposureMode, pAct);
	assert(nRet == DEVICE_OK);

	//Exposure Bias
	pAct = new CPropertyAction(this, &NikonKsCam::OnExposureBias);
	nRet = CreateKsProperty(eExposureBias, pAct);
	assert(nRet == DEVICE_OK);

	//SignalExposureEnd
	pAct = new CPropertyAction(this, &NikonKsCam::OnSignalExposureEnd);
	nRet = CreateKsProperty(eSignalExposureEnd, pAct);
	assert(nRet == DEVICE_OK);

	//SignalTriggerReady
	pAct = new CPropertyAction(this, &NikonKsCam::OnSignalTriggerReady);
	nRet = CreateKsProperty(eSignalTriggerReady, pAct);
	assert(nRet == DEVICE_OK);

	//SignalDeviceCapture
	pAct = new CPropertyAction(this, &NikonKsCam::OnSignalDeviceCapture);
	nRet = CreateKsProperty(eSignalDeviceCapture, pAct);
	assert(nRet == DEVICE_OK);

	//ExposureOutput
	pAct = new CPropertyAction(this, &NikonKsCam::OnExposureOutput);
	nRet = CreateKsProperty(eExposureOutput, pAct);
	assert(nRet == DEVICE_OK);

	// setup the buffer
	// ----------------
	updateImageSettings();

	// synchronize all properties
	// --------------------------
	nRet = DEVICE_OK;
	nRet = UpdateStatus();
	if (nRet != DEVICE_OK)
		return nRet;

	m_isInitialized = true;

	return DEVICE_OK;
}

/* Create MM Property for a given FeatureId */
int NikonKsCam::CreateKsProperty(lx_uint32 FeatureId, CPropertyAction *pAct)
{
	auto    uiFeatureIndex = m_mapFeatureIndex[FeatureId];
	auto*	pFeatureValue = &m_vectFeatureValue.pstFeatureValue[uiFeatureIndex];
	auto*   pFeatureDesc = &m_pFeatureDesc[uiFeatureIndex];
	char	strWork[50];
	const char*	strTitle;
	auto nRet = DEVICE_OK;


	/* StrTitle is readable name of feature */
	strTitle = ConvFeatureIdToName(pFeatureValue->uiFeatureId);
	printf(strTitle);
	switch (m_pFeatureDesc[uiFeatureIndex].eFeatureDescType) {
	case edesc_Range:
		switch (pFeatureValue->stVariant.eVarType) {
		case	evrt_int32:
			nRet |= CreateProperty(strTitle, "", MM::Integer, false, pAct);
			nRet |= SetPropertyLimits(strTitle, pFeatureDesc->stRange.stMin.i32Value, pFeatureDesc->stRange.stMax.i32Value);
			break;
		case	evrt_uint32:
			if(pFeatureValue->uiFeatureId == eExposureTime)
			{ 
				strTitle = const_cast<char*>(MM::g_Keyword_Exposure);
				CreateProperty(strTitle, "", MM::Float, false, pAct);
				SetPropertyLimits(strTitle, pFeatureDesc->stRange.stMin.ui32Value/1000, pFeatureDesc->stRange.stMax.ui32Value/1000);
			}
			else if (pFeatureValue->uiFeatureId == eExposureTimeLimit)
			{
				CreateProperty(strTitle, "", MM::Float, false, pAct);
				SetPropertyLimits(strTitle, pFeatureDesc->stRange.stMin.ui32Value/1000, pFeatureDesc->stRange.stMax.ui32Value/1000);
			}
			else
			{
				CreateProperty(strTitle, "", MM::Integer, false, pAct);
				SetPropertyLimits(strTitle, pFeatureDesc->stRange.stMin.ui32Value, pFeatureDesc->stRange.stMax.ui32Value);
			}
			break;
		default: break;
		}
		break;
	case edesc_Area:
		//Metering Area handled in initialize
		break;
	case edesc_Position:
		//ROI position handled in initialize
		break;
	case edesc_TriggerOption:
		//TriggerOption handled in initialize
		break;
	case edesc_ElementList:
		nRet |= CreateProperty(strTitle, "", MM::String, false, pAct);
		for (auto Index = 0; Index < pFeatureDesc->uiListCount; Index++)
		{
			wcstombs(strWork, reinterpret_cast<wchar_t const*>(pFeatureDesc->stElementList[Index].wszComment), CAM_FEA_COMMENT_MAX);
			/* OnePushAE is not allowed to be set by User */
			if (strcmp(strWork, "OnePushAE") != 0)
				nRet |= AddAllowedValue(strTitle, strWork);
		}
		break;
	case edesc_FormatList:
		nRet |= CreateProperty(strTitle, "", MM::String, false, pAct);
		for (auto formatIndex = 0; formatIndex < pFeatureDesc->uiListCount; formatIndex++)
		{
			wcstombs(strWork, reinterpret_cast<wchar_t const*>(pFeatureDesc->stFormatList[formatIndex].wszComment), CAM_FEA_COMMENT_MAX);
			nRet |= AddAllowedValue(strTitle, strWork);
		}
		break;
	case edesc_unknown: break;
	case edesc_int32List: break;
	case edesc_doubleList: break;
	default: break;
	}

	return nRet;
}

/* This function populates m_vectFeatureValue with all features */
void NikonKsCam::GetAllFeatures()
{
	auto lResult = LX_OK;

    Free_Vector_CAM_FeatureValue(m_vectFeatureValue);

    m_vectFeatureValue.uiCapacity = CAM_FEA_CAPACITY;
    m_vectFeatureValue.pstFeatureValue = new
    CAM_FeatureValue[m_vectFeatureValue.uiCapacity];

    if ( !m_vectFeatureValue.pstFeatureValue )
    {
		LogMessage("GetAllFeatures() Memory allocation error. \n");
        return;
    }

    lResult = CAM_GetAllFeatures(m_uiCameraHandle, m_vectFeatureValue);
    if ( lResult != LX_OK )
    {
		LogMessage("GetAllFeatures() error. \n");
        return;
    }

    if ( m_vectFeatureValue.uiCountUsed == 0 )
    {
		LogMessage("Error: GetAllFeatures() returned no features.\n");
        return;
    }
    return; 
}

/* This function creates the feature map and calls featureChanged for every feature. populates all m_pFeatureDesc */
void NikonKsCam::GetAllFeaturesDesc()
{
    lx_uint32   uiFeatureId, i;

    Free_CAM_FeatureDesc();
    m_mapFeatureIndex.clear();
    m_pFeatureDesc = new CAM_FeatureDesc[m_vectFeatureValue.uiCountUsed];
    if ( !m_pFeatureDesc )
    {
		LogMessage("GetAllFeaturesDesc memory allocate Error.[] \n");
        return;
    }

    /* This loops through the total number of features on the device */
    for( i=0; i<m_vectFeatureValue.uiCountUsed; i++ )
    {
        uiFeatureId = m_vectFeatureValue.pstFeatureValue[i].uiFeatureId;
        /* map the FeatureId to i */
        m_mapFeatureIndex.insert(std::make_pair(uiFeatureId, i));
	    auto lResult = CAM_GetFeatureDesc(m_uiCameraHandle, uiFeatureId, m_pFeatureDesc[i]);
		if (lResult != LX_OK)
		{
			LogMessage("CAM_GetFeatureDesc Error");
			return;
		}
    }
}

/* This function calls SetFeature for a given uiFeatureId */
void NikonKsCam::SetFeature(lx_uint32 uiFeatureId)
{
	auto lResult = LX_OK;
	lx_uint32                   uiIndex;
	Vector_CAM_FeatureValue     vectFeatureValue;

	/* Prepare the vectFeatureValue structure to use in the CAM_setFeatures command */
	vectFeatureValue.uiCountUsed = 1;
	vectFeatureValue.uiCapacity = 1;
	vectFeatureValue.uiPauseTransfer = 0;
	vectFeatureValue.pstFeatureValue = new CAM_FeatureValue[1];
	if (vectFeatureValue.pstFeatureValue == nullptr)
	{
		LogMessage("Error allocating memory vecFeatureValue.");
		return;
	}

	uiIndex = m_mapFeatureIndex[uiFeatureId];
	vectFeatureValue.pstFeatureValue[0] = m_vectFeatureValue.pstFeatureValue[uiIndex];

	lResult = CAM_SetFeatures(m_uiCameraHandle, vectFeatureValue);
	Free_Vector_CAM_FeatureValue(vectFeatureValue);
	if (lResult != LX_OK)
	{
		LogMessage("CAM_SetFeatures Error");
		GetAllFeatures(); 
		return;
	}

	LogMessage("SetFeature() Success");
	return;
}

/* This function calls CAM_Command */
void NikonKsCam::Command(const lx_wchar* wszCommand)
{
	auto lResult = LX_OK;

	if (!_wcsicmp(reinterpret_cast<wchar_t const *>(wszCommand), CAM_CMD_START_FRAMETRANSFER))
	{
		CAM_CMD_StartFrameTransfer      stCmd;

		/* Start frame transfer */
		stCmd.uiImageBufferNum = DEF_DRIVER_BUFFER_NUM;
		lResult = CAM_Command(m_uiCameraHandle, CAM_CMD_START_FRAMETRANSFER, &stCmd);
		if (lResult != LX_OK)
		{
			LogMessage("CAM_Command start frame transfer error");
			return;
		}
	}
	else
	{
		lResult = CAM_Command(m_uiCameraHandle, wszCommand, nullptr);
		if (lResult != LX_OK)
		{
			LogMessage("CAM_Command error");
			return;
		}
	}
	return;
}

/* This should be called at initialization after features have been received, as well as whenever imgFormat is changed
/* it should update the image buffer to have the proper width/height/depth as well as update relevant Properties */ 
void NikonKsCam::updateImageSettings()
{
  auto lResult = LX_OK;

  switch(m_vectFeatureValue.pstFeatureValue[m_mapFeatureIndex[eFormat]].stVariant.stFormat.eColor)
  {
	case ecfcUnknown:
	  LogMessage("Error: unknown image type.");
      break;
	case ecfcRgb24:
	  m_nComponents = 4;
	  m_byteDepth = 4;
      m_bitDepth = 8;
	  m_Color = true;
      break;
	case ecfcYuv444:
	  m_nComponents = 4;
	  m_byteDepth = 4;
	  m_bitDepth = 8;
	  m_Color = true;
      break;
	case ecfcMono16:
	  m_nComponents = 1;
	  m_byteDepth = 2;
      m_bitDepth = 16;
	  m_Color = false;
      break;
  }

  switch(m_vectFeatureValue.pstFeatureValue[m_mapFeatureIndex[eFormat]].stVariant.stFormat.eMode)
  {
	case ecfmUnknown:
	  LogMessage("Error: unknown image resolution.");
      break;
	case ecfm4908x3264:
      m_image_width=4908;
      m_image_height=3264;
      break;
	case ecfm2454x1632:
      m_image_width=2454;
      m_image_height=1632;    
      break;
	case ecfm1636x1088:
      m_image_width=1636;
      m_image_height=1088;   
      break;
	case ecfm818x544:
      m_image_width=818;
      m_image_height=544;   
      break;
	case ecfm1608x1608:
      m_image_width=1608;
      m_image_height=1608;   
      break;
	case ecfm804x804:
      m_image_width=804;
      m_image_height=804; 
      break;
	case ecfm536x536:
      m_image_width=536;
      m_image_height=536; 
      break;
  }

  /* Update the buffer to have the proper width height and depth */
  img_.Resize(m_image_width, m_image_height, m_byteDepth);

  /* Update m_stFrameSize so we know how to size stImage in the GetImage() calls to driver*/
  lResult = CAM_Command(m_uiCameraHandle, CAM_CMD_GET_FRAMESIZE, &m_stFrameSize);
  if ( lResult != LX_OK )
  { 
	  LogMessage("GetFrameSize Error.");
  }

}

/* Update ROI Property x and y limits */
void NikonKsCam::SetROILimits()
{
	auto lResult = CAM_GetFeatureDesc(m_uiCameraHandle, eRoiPosition,
		m_pFeatureDesc[m_mapFeatureIndex[eRoiPosition]]);
	if (lResult != LX_OK)
	{
		LogMessage("CAM_GetFeatureDesc Error");
		return;
	}

	auto pROIFeatureDesc = &m_pFeatureDesc[m_mapFeatureIndex[eRoiPosition]];
	
	/* If not in an ROI format setting (e.g. full frame), SDK will return min=max=1 */
	/* which will cause an error in micromanager SetPropertyLimits() function */
	/* for now just set min to 0 and max to 1 */
	if (pROIFeatureDesc->stPosition.stMin.uiX == pROIFeatureDesc->stPosition.stMax.uiX)
	{
		SetPropertyLimits(g_RoiPositionX, 0, 1);
		SetPropertyLimits(g_RoiPositionY, 0, 1);
	}
	else{
		SetPropertyLimits(g_RoiPositionX, pROIFeatureDesc->stPosition.stMin.uiX, pROIFeatureDesc->stPosition.stMax.uiX);
		SetPropertyLimits(g_RoiPositionY, pROIFeatureDesc->stPosition.stMin.uiY, pROIFeatureDesc->stPosition.stMax.uiY);
	}
	UpdateProperty(g_RoiPositionX);
	UpdateProperty(g_RoiPositionY);
}

/* Update Metering Area limits */
void NikonKsCam::SetMeteringAreaLimits()
{
	auto lResult = CAM_GetFeatureDesc(m_uiCameraHandle, eMeteringArea,	m_pFeatureDesc[m_mapFeatureIndex[eMeteringArea]]);
	if (lResult != LX_OK)
	{
		LogMessage("CAM_GetFeatureDesc Error");
		return;
	}
	auto pFeatureDesc = &m_pFeatureDesc[m_mapFeatureIndex[eMeteringArea]];

	SetPropertyLimits(g_MeteringAreaLeft, pFeatureDesc->stArea.stMin.uiLeft, pFeatureDesc->stArea.stMax.uiLeft);
	SetPropertyLimits(g_MeteringAreaTop, pFeatureDesc->stArea.stMin.uiTop, pFeatureDesc->stArea.stMax.uiTop);
	SetPropertyLimits(g_MeteringAreaWidth, pFeatureDesc->stArea.stMin.uiWidth, pFeatureDesc->stArea.stMax.uiWidth);
	SetPropertyLimits(g_MeteringAreaHeight, pFeatureDesc->stArea.stMin.uiHeight, pFeatureDesc->stArea.stMax.uiHeight);

	UpdateProperty(g_MeteringAreaLeft);
	UpdateProperty(g_MeteringAreaTop);
	UpdateProperty(g_MeteringAreaWidth);
	UpdateProperty(g_MeteringAreaHeight);
}

/**
* Shuts down (unloads) the device.
* Required by the MM::Device API.
* Ideally this method will completely unload the device and release all resources.
* Shutdown() may be called multiple times in a row.
* After Shutdown() we should be allowed to call Initialize() again to load the device
* without causing problems.
*/
int NikonKsCam::Shutdown()
{
  auto lResult = LX_OK;
  
  if ( this->m_isOpened )
  {
      lResult = CAM_Close(m_uiCameraHandle);
      if ( lResult != LX_OK )
      {
        LogMessage("Error Closing Camera.");
      }
      Free_Vector_CAM_FeatureValue(m_vectFeatureValue);
      Free_CAM_FeatureDesc();
      this->m_uiDeviceIndex = 0;
      this->m_uiCameraHandle = 0;
      this->m_isOpened = FALSE;
	  this->m_isInitialized = FALSE;
	  this->m_isRi2 = FALSE;
      g_pDlg = nullptr;
  }

  return DEVICE_OK;
}

/**
* Performs exposure and grabs a single image.
* This function should block during the actual exposure and return immediately afterwards 
* (i.e., before readout).  This behavior is needed for proper synchronization with the shutter.
* Required by the MM::Camera API.
*/
int NikonKsCam::SnapImage()
{
  //Determine exposureLength so we know a reasonable time to wait for frame arrival
  auto exposureLength = m_vectFeatureValue.pstFeatureValue[m_mapFeatureIndex[eExposureTime]].stVariant.ui32Value / 1000;
  char buf[MM::MaxStrLength];
  //Determine current trigger mode
  GetProperty(ConvFeatureIdToName(eTriggerMode), buf);

  Command(CAM_CMD_START_FRAMETRANSFER);
  //If in soft trigger mode we need to send the signal to capture. 
  if (!strcmp(buf, "Soft"))
	  Command(CAM_CMD_ONEPUSH_SOFTTRIGGER);
  //Wait for frameDoneEvent from callback method
  // (time out after exposure length + 100 ms)
  m_frameDoneEvent.Wait(exposureLength + 100);
  Command(CAM_CMD_STOP_FRAMETRANSFER);
  GrabFrame();

  return DEVICE_OK;
}

//Call after a frame is recieved to get the image from camera and copy to img_ buffer
void NikonKsCam::GrabFrame()
{
	lx_result           lResult = LX_OK;
	lx_uint32           uiRemained;

	/* Set stImage buffer to appropriate size */
	stImage.uiDataBufferSize = this->m_stFrameSize.uiFrameSize;

	/* Grab the Image */
	lResult = CAM_GetImage(m_uiCameraHandle, true, stImage, uiRemained);
	if (lResult != LX_OK)
	{
		LogMessage("CAM_GetImage error.");
	}

	if (m_Color)
		bgr8ToBGRA8(img_.GetPixelsRW(), (uint8_t*)stImage.pDataBuffer, img_.Width(), img_.Height());
	else
	    memcpy(img_.GetPixelsRW(), stImage.pDataBuffer, img_.Width()*img_.Height()*img_.Depth());
	
}

//copied from MM dc1394.cpp driver file 
//EF: converts bgr image to Micromanager BGRA
// It is the callers responsibility that both src and destination exist
void NikonKsCam::bgr8ToBGRA8(unsigned char* dest, unsigned char* src, unsigned int width, unsigned int height)
{
	for (register uint64_t i = 0, j = 0; i < (width * height * 3); i += 3, j += 4)
	{ 
		dest[j] = src[i];
		dest[j + 1] = src[i + 1];
		dest[j + 2] = src[i + 2];
		dest[j + 3] = 0;
	}
}

/**
* Returns pixel data.
* Required by the MM::Camera API.
* The calling program will assume the size of the buffer based on the values
* obtained from GetImageBufferSize(), which in turn should be consistent with
* values returned by GetImageWidth(), GetImageHight() and GetImageBytesPerPixel().
* The calling program allso assumes that camera never changes the size of
* the pixel buffer on its own. In other words, the buffer can change only if
* appropriate properties are set (such as binning, pixel type, etc.)
*/
const unsigned char* NikonKsCam::GetImageBuffer()
{
  auto pB = const_cast<unsigned char*>(img_.GetPixels());
  return pB;
}

/**
* Sets the camera Region Of Interest.
* Required by the MM::Camera API.
* This command will change the dimensions of the image.
* Depending on the hardware capabilities the camera may not be able to configure the
* exact dimensions requested - but should try do as close as possible.
* If the hardware does not have this capability the software should simulate the ROI by
* appropriately cropping each frame.
* This demo implementation ignores the position coordinates and just crops the buffer.
* @param x - top-left corner coordinate
* @param y - top-left corner coordinate
* @param xSize - width
* @param ySize - height
*/
/* KsCam only has fixed ROI settings which are handled by the format setting */
int NikonKsCam::SetROI(unsigned /*x*/, unsigned /*y*/, unsigned /*xSize*/, unsigned /*ySize*/)
{
   return DEVICE_UNSUPPORTED_COMMAND;
}

/**
* Returns the actual dimensions of the current ROI.
* Required by the MM::Camera API.
*/
/* KsCam only has fixed ROI settings which are handled by the format setting */
int NikonKsCam::GetROI(unsigned& x, unsigned& y, unsigned& xSize, unsigned& ySize)
{
  x = roiX_;
  y = roiY_;

  xSize = img_.Width();
  ySize = img_.Height();

  return DEVICE_OK;
}

/**
* Resets the Region of Interest to full frame.
* Required by the MM::Camera API.
*/
int NikonKsCam::ClearROI()
{
   roiX_ = 0;
   roiY_ = 0;
   roiWidth_ = 4908; //true for both ri2 and qi2
   roiHeight_ = 3264;

   return DEVICE_OK;
}

/**
* Returns the current exposure setting in milliseconds.
* Required by the MM::Camera API.
*/
double NikonKsCam::GetExposure() const
{
   char buf[MM::MaxStrLength];
   int ret = GetProperty(MM::g_Keyword_Exposure, buf);
   if (ret != DEVICE_OK)
      return 0.0;
   return atof(buf);
}

/**
* Sets exposure in milliseconds.
* Required by the MM::Camera API.
*/
void NikonKsCam::SetExposure(double exp)
{
   SetProperty(MM::g_Keyword_Exposure, CDeviceUtils::ConvertToString(exp));
}

/**
* Returns the current binning factor.
* Required by the MM::Camera API.
*/
int NikonKsCam::GetBinning() const
{
	return DEVICE_OK;
}

/**
* Sets binning factor.
* Required by the MM::Camera API.
*/
int NikonKsCam::SetBinning(int binF)
{
   return DEVICE_OK;
}

int NikonKsCam::GetComponentName(unsigned comp, char* name)
{
	if (comp > 4)
	{
		name = "invalid comp";
		return DEVICE_ERR;
	}

	std::string rgba("RGBA");
	CDeviceUtils::CopyLimitedString(name, &rgba.at(comp));

	return DEVICE_OK;
}


//Sequence functions, mostly copied from other drivers with slight modification
/**
 * Required by the MM::Camera API
 * Please implement this yourself and do not rely on the base class implementation
 * The Base class implementation is deprecated and will be removed shortly
 */
int NikonKsCam::StartSequenceAcquisition(double interval) {
   return StartSequenceAcquisition(LONG_MAX, interval, false);            
}

/**                                                                       
* Stop and wait for the Sequence thread finished                                   
*/                                                                        
int NikonKsCam::StopSequenceAcquisition()                                     
{
	Command(CAM_CMD_STOP_FRAMETRANSFER);
	if (!thd_->IsStopped()) {
      thd_->Stop();                                                       
      thd_->wait();                                                       
   }                                                                  
                                                                    
   return DEVICE_OK;                                                      
} 

/**
* Simple implementation of Sequence Acquisition
* A sequence acquisition should run on its own thread and transport new images
* coming of the camera into the MMCore circular buffer.
*/
int NikonKsCam::StartSequenceAcquisition(long numImages, double interval_ms, bool stopOnOverflow)
{
   char triggerOffChar[10];
   if (IsCapturing())
      return DEVICE_CAMERA_BUSY_ACQUIRING;

   auto ret = GetCoreCallback()->PrepareForAcq(this);
   if (ret != DEVICE_OK)
      return ret;
   sequenceStartTime_ = GetCurrentMMTime();
   imageCounter_ = 0;
   
   auto*   pFeatureDesc = &m_pFeatureDesc[m_mapFeatureIndex[eTriggerMode]];
   wcstombs(triggerOffChar, reinterpret_cast<wchar_t const*>(pFeatureDesc->stElementList[ectmOff].wszComment), CAM_FEA_COMMENT_MAX);
   
   char triggerMode[MM::MaxStrLength];
   /* if not in trigger mode:off set to trigger mode: off (for "live view") */
   GetProperty(ConvFeatureIdToName(eTriggerMode), triggerMode);
   if (strcmp(triggerMode, triggerOffChar) != 0)
   {
	   SetProperty(ConvFeatureIdToName(eTriggerMode), "OFF");
   }
   
   Command(CAM_CMD_START_FRAMETRANSFER);
   
   thd_->Start(numImages,interval_ms);
   stopOnOverFlow_ = stopOnOverflow;
   
   return DEVICE_OK;
}

/*
 * Inserts Image and MetaData into MMCore circular Buffer
 */
int NikonKsCam::InsertImage()
{
  
   // Image metadata
   Metadata md;
   char label[MM::MaxStrLength];
   this->GetLabel(label);
   md.put("Camera", label);
   md.put(MM::g_Keyword_Metadata_StartTime, CDeviceUtils::ConvertToString(sequenceStartTime_.getMsec()));
   md.put(MM::g_Keyword_Elapsed_Time_ms, CDeviceUtils::ConvertToString((GetCurrentMMTime() - sequenceStartTime_).getMsec()));
   md.put(MM::g_Keyword_Metadata_ImageNumber, CDeviceUtils::ConvertToString(imageCounter_)); 
   
   imageCounter_++;

   MMThreadGuard g(imgPixelsLock_);

   int ret = GetCoreCallback()->InsertImage(this, img_.GetPixels(),
                                                  img_.Width(),
                                                  img_.Height(), 
                                                  img_.Depth(), 
                                                  md.Serialize().c_str());

   if (!stopOnOverFlow_ && ret == DEVICE_BUFFER_OVERFLOW)
   {
      // do not stop on overflow, reset the buffer and insert the same image again
      GetCoreCallback()->ClearImageBuffer(this);
      return GetCoreCallback()->InsertImage(this, img_.GetPixels(),
                                                  img_.Width(),
                                                  img_.Height(), 
                                                  img_.Depth(), 
                                                  md.Serialize().c_str());
   } else
      return ret;
   
    return DEVICE_OK;
}

/*
 * Do actual capturing
 * Called from inside the thread  
 */
int NikonKsCam::ThreadRun (void)
{
   lx_uint32 lResult = LX_OK;
   lx_uint32 uiRemained;
   MM::MMTime startFrame = GetCurrentMMTime();

   auto exposureLength = m_vectFeatureValue.pstFeatureValue[m_mapFeatureIndex[eExposureTime]].stVariant.ui32Value / 1000;
   DWORD dwRet = m_frameDoneEvent.Wait(exposureLength + 300);//wait up to exposure length + 250 ms

   if (dwRet == MM_WAIT_TIMEOUT)
   {
	  LogMessage("Timeout");
      return 0;
   }
   else if (dwRet == MM_WAIT_OK)
   {
	   GrabFrame();

	  auto ret = InsertImage();

      MM::MMTime frameInterval = GetCurrentMMTime() - startFrame;
      if (frameInterval.getMsec() > 0.0)
         framesPerSecond = 1000.0 / frameInterval.getMsec();

      return ret;
   }
   else
   {
      ostringstream os;
      os << "Unknown event status " << dwRet;
      LogMessage(os.str());
      return 0;
   }
   
	return 1;
};

bool NikonKsCam::IsCapturing() {
   return !thd_->IsStopped();
}

/*
 * called from the thread function before exit 
 */
void NikonKsCam::OnThreadExiting() throw()
{
   try
   {
      LogMessage(g_Msg_SEQUENCE_ACQUISITION_THREAD_EXITING);
      GetCoreCallback()?GetCoreCallback()->AcqFinished(this,0):DEVICE_OK;
   }
   catch(...)
   {
      LogMessage(g_Msg_EXCEPTION_IN_ON_THREAD_EXITING, false);
   }
}


MySequenceThread::MySequenceThread(NikonKsCam* pCam)
   :stop_(true)
   ,suspend_(false)
   ,numImages_(default_numImages)
   ,imageCounter_(0)
   ,intervalMs_(default_intervalMS)
   ,startTime_(0)
   ,actualDuration_(0)
   ,lastFrameTime_(0)
   ,camera_(pCam)
{};

MySequenceThread::~MySequenceThread() {};

void MySequenceThread::Stop() {
   MMThreadGuard(this->stopLock_);
   stop_=true;
}

void MySequenceThread::Start(long numImages, double intervalMs)
{
   MMThreadGuard(this->stopLock_);
   MMThreadGuard(this->suspendLock_);
   numImages_=numImages;
   intervalMs_=intervalMs;
   imageCounter_=0;
   stop_ = false;
   suspend_=false;
   activate();
   actualDuration_ = 0;
   //startTime_= camera_->GetCurrentMMTime();
   lastFrameTime_ = 0;
}

bool MySequenceThread::IsStopped(){
   MMThreadGuard(this->stopLock_);
   return stop_;
}

void MySequenceThread::Suspend() {
   MMThreadGuard(this->suspendLock_);
   suspend_ = true;
}

bool MySequenceThread::IsSuspended() {
   MMThreadGuard(this->suspendLock_);
   return suspend_;
}

void MySequenceThread::Resume() {
   MMThreadGuard(this->suspendLock_);
   suspend_ = false;
}

int MySequenceThread::svc(void) throw()
{
   int ret = DEVICE_ERR;

   try 
   {
      do
      {  
         ret = camera_->ThreadRun();
      } while (DEVICE_OK == ret && !IsStopped() && imageCounter_++ < numImages_-1);

      if (IsStopped())
         camera_->LogMessage("SeqAcquisition interrupted by the user\n");

   } catch(...){
      camera_->LogMessage(g_Msg_EXCEPTION_IN_THREAD, false);
   }
   stop_=true;
   actualDuration_ = camera_->GetCurrentMMTime() - startTime_;
   camera_->OnThreadExiting();

   return ret;
}


///////////////////////////////////////////////////////////////////////////////
// NikonKsCam Action handlers
///////////////////////////////////////////////////////////////////////////////

/*Pre-initialization property which shows cameras (and simulators) detected */
int NikonKsCam::OnCameraSelection(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	if (eAct == MM::BeforeGet)
	{
		pProp->Set(camID);
	}
	else if (eAct == MM::AfterSet)
	{
		string value;
		pProp->Get(value);
		strcpy(camID, value.c_str());
	}
	return DEVICE_OK;
}

/*Nikon Ks cam only has hardware bin 3 which is handled by Image Format setting*/
int NikonKsCam::OnBinning(MM::PropertyBase* pProp, MM::ActionType eAct)
{
    return DEVICE_OK;
}

int NikonKsCam::OnExposureTime(MM::PropertyBase* pProp , MM::ActionType eAct)
{
   return OnExposureChange(pProp, eAct, eExposureTime);
}

int NikonKsCam::OnTriggerMode(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return OnList(pProp, eAct, eTriggerMode);
}

int NikonKsCam::OnExposureMode(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return OnList(pProp, eAct, eExposureMode);
}

int NikonKsCam::OnExposureBias(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return OnList(pProp, eAct, eExposureBias);
}

int NikonKsCam::OnCaptureMode(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return OnList(pProp, eAct, eCaptureMode);
}

int NikonKsCam::OnMeteringMode(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return OnList(pProp, eAct, eMeteringMode);
}

int NikonKsCam::OnSignalExposureEnd(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return OnList(pProp, eAct, eSignalExposureEnd);
}

int NikonKsCam::OnSignalTriggerReady(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return OnList(pProp, eAct, eSignalTriggerReady);
}

int NikonKsCam::OnSignalDeviceCapture(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return OnList(pProp, eAct, eSignalDeviceCapture);
}

int NikonKsCam::OnExposureOutput(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return OnList(pProp, eAct, eExposureOutput);
}

int NikonKsCam::OnHardwareGain(MM::PropertyBase* pProp , MM::ActionType eAct)
{
	return OnRange(pProp, eAct, eGain);
}

int NikonKsCam::OnBrightness(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return OnRange(pProp, eAct, eBrightness);
}

int NikonKsCam::OnSharpness(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return OnRange(pProp, eAct, eSharpness);
}

int NikonKsCam::OnHue(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return OnRange(pProp, eAct, eHue);
}

int NikonKsCam::OnSaturation(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return OnRange(pProp, eAct, eSaturation);
}

int NikonKsCam::OnWhiteBalanceRed(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return OnRange(pProp, eAct, eWhiteBalanceRed);
}

int NikonKsCam::OnWhiteBalanceBlue(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return OnRange(pProp, eAct, eWhiteBalanceBlue);
}

int NikonKsCam::OnPresets(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return OnList(pProp, eAct, ePresets);
}

int NikonKsCam::OnExposureTimeLimit(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return OnExposureChange(pProp, eAct, eExposureTimeLimit);
}

int NikonKsCam::OnGainLimit(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	return OnRange(pProp, eAct, eGainLimit);
}

int NikonKsCam::OnMeteringAreaLeft(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	long value;
	lx_uint32 uiIndex = m_mapFeatureIndex[eMeteringArea];
	CAM_FeatureValue*   pFeatureValue = &m_vectFeatureValue.pstFeatureValue[uiIndex];

	if (eAct == MM::AfterSet)
	{
		pProp->Get(value);
		pFeatureValue->stVariant.stArea.uiLeft = value;
		SetFeature(pFeatureValue->uiFeatureId);
	}

	if (eAct == MM::BeforeGet || eAct == MM::AfterSet)
	{
		pProp->Set((long)m_vectFeatureValue.pstFeatureValue[uiIndex].stVariant.stArea.uiLeft);
	}
	return DEVICE_OK;
}

int NikonKsCam::OnMeteringAreaTop(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	long value;
	lx_uint32 uiIndex = m_mapFeatureIndex[eMeteringArea];
	CAM_FeatureValue*   pFeatureValue = &m_vectFeatureValue.pstFeatureValue[uiIndex];

	if (eAct == MM::AfterSet)
	{
		pProp->Get(value);
		pFeatureValue->stVariant.stArea.uiTop = value;
		SetFeature(pFeatureValue->uiFeatureId);
	}

	if (eAct == MM::BeforeGet || eAct == MM::AfterSet)
	{
		pProp->Set((long)m_vectFeatureValue.pstFeatureValue[uiIndex].stVariant.stArea.uiTop);
	}
	return DEVICE_OK;
}

int NikonKsCam::OnMeteringAreaWidth(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	long value;
	lx_uint32 uiIndex = m_mapFeatureIndex[eMeteringArea];
	CAM_FeatureValue*   pFeatureValue = &m_vectFeatureValue.pstFeatureValue[uiIndex];

	if (eAct == MM::AfterSet)
	{
		pProp->Get(value);
		pFeatureValue->stVariant.stArea.uiWidth = value;
		SetFeature(pFeatureValue->uiFeatureId);
	}

	if (eAct == MM::BeforeGet || eAct == MM::AfterSet)
	{
		pProp->Set((long)m_vectFeatureValue.pstFeatureValue[uiIndex].stVariant.stArea.uiWidth);
	}
	return DEVICE_OK;
}

int NikonKsCam::OnMeteringAreaHeight(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	long value;
	lx_uint32 uiIndex = m_mapFeatureIndex[eMeteringArea];
	CAM_FeatureValue*   pFeatureValue = &m_vectFeatureValue.pstFeatureValue[uiIndex];

	if (eAct == MM::AfterSet)
	{
		pProp->Get(value);
		pFeatureValue->stVariant.stArea.uiHeight = value;
		SetFeature(pFeatureValue->uiFeatureId);
	}

	if (eAct == MM::BeforeGet || eAct == MM::AfterSet)
	{
		pProp->Set((long)m_vectFeatureValue.pstFeatureValue[uiIndex].stVariant.stArea.uiHeight);
	}
	return DEVICE_OK;
}

int NikonKsCam::OnRoiX(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	long value;
	lx_uint32 uiIndex = m_mapFeatureIndex[eRoiPosition];
	CAM_FeatureValue*   pFeatureValue = &m_vectFeatureValue.pstFeatureValue[uiIndex];

	if (eAct == MM::AfterSet)
	{
		pProp->Get(value);
		pFeatureValue->stVariant.stPosition.uiX = value;
		SetFeature(pFeatureValue->uiFeatureId);
	}

	if (eAct == MM::BeforeGet || eAct == MM::AfterSet)
	{
		pProp->Set((long)m_vectFeatureValue.pstFeatureValue[uiIndex].stVariant.stPosition.uiX);
	}
	return DEVICE_OK;
}

int NikonKsCam::OnRoiY(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	long value;
	lx_uint32 uiIndex = m_mapFeatureIndex[eRoiPosition];
	CAM_FeatureValue*   pFeatureValue = &m_vectFeatureValue.pstFeatureValue[uiIndex];

    if (eAct == MM::AfterSet)
	{
		pProp->Get(value);
		pFeatureValue->stVariant.stPosition.uiY = value;
		SetFeature(pFeatureValue->uiFeatureId);
	}

	if (eAct == MM::BeforeGet || eAct == MM::AfterSet)
	{
		pProp->Set((long)m_vectFeatureValue.pstFeatureValue[uiIndex].stVariant.stPosition.uiY);
	}

	return DEVICE_OK;
}

int NikonKsCam::OnTriggerFrame(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	long value;
	lx_uint32 uiIndex = m_mapFeatureIndex[eTriggerOption];
	CAM_FeatureValue*   pFeatureValue = &m_vectFeatureValue.pstFeatureValue[uiIndex];

	if (eAct == MM::BeforeGet)
	{
		pProp->Set((long)m_vectFeatureValue.pstFeatureValue[uiIndex].stVariant.stTriggerOption.iDelayTime);
	}
	else if (eAct == MM::AfterSet)
	{
		pProp->Get(value);
		pFeatureValue->stVariant.stTriggerOption.uiFrameCount = value;
		SetFeature(pFeatureValue->uiFeatureId);
	}
	return DEVICE_OK;
}

int NikonKsCam::OnTriggerDelay(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	lx_uint32 uiIndex = m_mapFeatureIndex[eTriggerOption];
	long value;
	CAM_FeatureValue*   pFeatureValue = &m_vectFeatureValue.pstFeatureValue[uiIndex];

	if (eAct == MM::BeforeGet)
	{
		pProp->Set((long)m_vectFeatureValue.pstFeatureValue[uiIndex].stVariant.stTriggerOption.iDelayTime);
	}
	else if (eAct == MM::AfterSet)
	{
		pProp->Get(value);
		pFeatureValue->stVariant.stTriggerOption.iDelayTime = value;
		SetFeature(pFeatureValue->uiFeatureId);
	}
	return DEVICE_OK;
}

int NikonKsCam::OnImageFormat(MM::PropertyBase* pProp, MM::ActionType eAct)
{
	char strWork[30];
	lx_uint32 uiFeatureId = eFormat;
	lx_uint32 uiIndex = m_mapFeatureIndex[uiFeatureId];
	CAM_FeatureDesc*    pFeatureDesc;
	CAM_FeatureValue*   pFeatureValue;
	pFeatureValue = &m_vectFeatureValue.pstFeatureValue[uiIndex];
	pFeatureDesc = &m_pFeatureDesc[uiIndex];

	if (eAct == MM::AfterSet)
	{
		string value;
		lx_uint32 i, index;
		pProp->Get(value);
		for (i = 0; i < pFeatureDesc->uiListCount; i++)
		{
			wcstombs(strWork, pFeatureDesc->stFormatList[i].wszComment, CAM_FEA_COMMENT_MAX);
			if (value.compare(strWork) == 0)
			{
				LogMessage(strWork);
				pFeatureValue->stVariant.stFormat = pFeatureDesc->stFormatList[i].stFormat;
				SetFeature(pFeatureValue->uiFeatureId);
				updateImageSettings();
				//Update ROI, MeteringArea limits, they change with format setting
				SetROILimits();
				SetMeteringAreaLimits();
				break;
			}
		}
	}
	if (eAct == MM::BeforeGet || eAct == MM::AfterSet )
	{
		for (lx_uint32 i = 0; i < pFeatureDesc->uiListCount; i++)
		{
			if (pFeatureDesc->stFormatList[i].stFormat == pFeatureValue->stVariant.stFormat)
			{
				wcstombs(strWork, pFeatureDesc->stFormatList[i].wszComment, CAM_FEA_COMMENT_MAX);
				pProp->Set(strWork);
				break;
			}
		}
	}
	return DEVICE_OK;
}

//Generic - Handle "Range" feature
int NikonKsCam::OnRange(MM::PropertyBase* pProp, MM::ActionType eAct, lx_uint32 uiFeatureId)
{
	lx_uint32 uiIndex = m_mapFeatureIndex[uiFeatureId];
	long value;
	CAM_FeatureValue*   pFeatureValue = &m_vectFeatureValue.pstFeatureValue[uiIndex];
	switch (pFeatureValue->stVariant.eVarType){
	case evrt_uint32:
		if (eAct == MM::BeforeGet)
		{
			pProp->Set((long)m_vectFeatureValue.pstFeatureValue[uiIndex].stVariant.ui32Value);
		}
		else if (eAct == MM::AfterSet)
		{
			pProp->Get(value);
			pFeatureValue->stVariant.ui32Value = value;
			SetFeature(pFeatureValue->uiFeatureId);
		}
	case evrt_int32:
		if (eAct == MM::BeforeGet)
		{
			pProp->Set((long)m_vectFeatureValue.pstFeatureValue[uiIndex].stVariant.i32Value);
		}
		else if (eAct == MM::AfterSet)
		{
			pProp->Get(value);
			pFeatureValue->stVariant.i32Value = value;
			SetFeature(pFeatureValue->uiFeatureId);
		}
	}
	return DEVICE_OK;
}

//Generic - Handle "List" feature
int NikonKsCam::OnList(MM::PropertyBase* pProp, MM::ActionType eAct, lx_uint32 uiFeatureId)
{
	char strWork[30];
	lx_uint32 uiIndex = m_mapFeatureIndex[uiFeatureId];
	CAM_FeatureDesc*    pFeatureDesc = &m_pFeatureDesc[uiIndex];
	CAM_FeatureValue*   pFeatureValue = &m_vectFeatureValue.pstFeatureValue[uiIndex];

	if (eAct == MM::BeforeGet)
	{
		for (lx_uint32 i = 0; i < pFeatureDesc->uiListCount; i++)
		{
			if (pFeatureDesc->stElementList[i].varValue.ui32Value == pFeatureValue->stVariant.ui32Value)
			{
				wcstombs(strWork, pFeatureDesc->stElementList[i].wszComment, CAM_FEA_COMMENT_MAX);
				pProp->Set(strWork);
				break;
			}
		}
	}
	else if (eAct == MM::AfterSet)
	{
		string value;
		lx_uint32 i;
		pProp->Get(value);
		for (i = 0; i < pFeatureDesc->uiListCount; i++)
		{
			wcstombs(strWork, pFeatureDesc->stElementList[i].wszComment, CAM_FEA_COMMENT_MAX);
			if (value.compare(strWork) == 0)
			{
				pFeatureValue->stVariant.ui32Value = pFeatureDesc->stElementList[i].varValue.ui32Value;
				SetFeature(pFeatureValue->uiFeatureId);
				updateImageSettings();
				break;
			}
		}


	}
	return DEVICE_OK;
}

//Generic- Set either exposure time or exposure time limit
int NikonKsCam::OnExposureChange(MM::PropertyBase* pProp, MM::ActionType eAct, lx_uint32 uiFeatureId)
{
	lx_uint32 uiIndex = m_mapFeatureIndex[uiFeatureId];

	if (eAct == MM::BeforeGet)
	{
		pProp->Set(m_vectFeatureValue.pstFeatureValue[uiIndex].stVariant.ui32Value / 1000.);
	}
	else if (eAct == MM::AfterSet)
	{
		double value;
		CAM_FeatureValue*   pFeatureValue;
		pProp->Get(value);
		value = (value * 1000)+0.5;
		pFeatureValue = &m_vectFeatureValue.pstFeatureValue[uiIndex];
		value = AdjustExposureTime((unsigned int)value);

		pFeatureValue->stVariant.ui32Value = value;
		SetFeature(pFeatureValue->uiFeatureId);
	}
	return DEVICE_OK;
}

///////////////////////////////////////////////////////////////////////////////
// Utility functions copied from Nikon SDK example code
///////////////////////////////////////////////////////////////////////////////
/* format exposure time */
lx_uint32 NikonKsCam::AdjustExposureTime(lx_uint32 uiValue)
{
	lx_uint32   len, i, dev, val;

	char buffer[64];
	len = sprintf(buffer, "%u", uiValue);

	if (len >= 6)
	{
		dev = 1;
		for (i = 0; i<len - 3; i++)
		{
			dev *= 10;
		}
		val = (uiValue / dev) * dev;
	}
	else if (len >= 3)
	{
		val = (uiValue / 100) * 100;
	}
	else
	{
		val = 100;
	}
	return val;
}

/* convert uiFeatureId to readable name */
const char* NikonKsCam::ConvFeatureIdToName(const lx_uint32 uiFeatureId)
{
	lx_uint32       i;
	auto* p_featureName= new char[30];

	for (i = 0;; i++)
	{
		if (stFeatureNameRef[i].uiFeatureId == eUnknown)
		{
			break;
		}
		if (stFeatureNameRef[i].uiFeatureId == uiFeatureId)
		{
			break;
		}
	}
	wcstombs(p_featureName, (wchar_t const *)stFeatureNameRef[i].wszName, 30);
	return p_featureName;
}
