///////////////////////////////////////////////////////////////////////////////
// FILE:          NikonKsCam.h
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

#ifndef _NIKONKS_H_
#define _NIKONKS_H_

#include "../../../MMDevice/DeviceBase.h"
#include "../../../MMDevice/MMDevice.h"
#include "../../../MMDevice/ImgBuffer.h"
#include "../../../MMDevice/DeviceUtils.h"
#include "../../../MMDevice/DeviceThreads.h"
#include "DeviceEvents.h"

#include <KsCam.h>
#include <KsCamCommand.h>
#include <KsCamEvent.h>
#include <KsCamFeature.h>
#include <KsCamImage.h>

#include <map>



//////////////////////////////////////////////////////////////////////////////
// NikonKsCam class
//////////////////////////////////////////////////////////////////////////////

class MySequenceThread;

class NikonKsCam : public CCameraBase<NikonKsCam>
{
public:
	NikonKsCam();
	~NikonKsCam();

	// MMDevice API
	// ------------
	int Initialize();
	int Shutdown();
	void GetName(char* name) const;

	// MMCamera API
	// ------------
	int SnapImage();
	const unsigned char* GetImageBuffer();

	unsigned GetImageWidth() const{return img_.Width();};
	unsigned GetImageHeight() const{return img_.Height();};
	unsigned GetImageBytesPerPixel() const{return img_.Depth();};
	unsigned GetBitDepth() const{return m_bitDepth;};
	long GetImageBufferSize() const{return img_.Width() * img_.Height() * GetImageBytesPerPixel();}
	int GetComponentName(unsigned comp, char* name);
	unsigned GetNumberOfComponents() const{return m_nComponents;};
	double GetExposure() const;
	void SetExposure(double exp);
	int SetROI(unsigned x, unsigned y, unsigned xSize, unsigned ySize);
	int GetROI(unsigned& x, unsigned& y, unsigned& xSize, unsigned& ySize);
	int ClearROI();
	int PrepareSequenceAcqusition(){return DEVICE_OK;}
	int StartSequenceAcquisition(double interval);
	int StartSequenceAcquisition(long numImages, double interval_ms, bool stopOnOverflow);
	int StopSequenceAcquisition();
	int InsertImage();
	int ThreadRun();
	bool IsCapturing();
	void OnThreadExiting() throw();
	int GetBinning() const;
	int SetBinning(int bS);

	int IsExposureSequenceable(bool& isSequenceable) const
	{
		isSequenceable = false;
		return DEVICE_OK;
	}


	// action interface
	// ----------------
	int OnCameraSelection(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnBinning(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnImageFormat(MM::PropertyBase*, MM::ActionType);
	int OnExposureTime(MM::PropertyBase*, MM::ActionType);
	int OnHardwareGain(MM::PropertyBase*, MM::ActionType);
	int OnBrightness(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnSharpness(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnHue(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnSaturation(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnWhiteBalanceRed(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnWhiteBalanceBlue(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnPresets(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnExposureTimeLimit(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnGainLimit(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnMeteringAreaLeft(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnMeteringAreaTop(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnMeteringAreaWidth(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnMeteringAreaHeight(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnTriggerMode(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnExposureMode(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnExposureBias(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnMeteringMode(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnCaptureMode(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnSignalExposureEnd(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnSignalTriggerReady(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnSignalDeviceCapture(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnExposureOutput(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnRoiX(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnRoiY(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnTriggerFrame(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnTriggerDelay(MM::PropertyBase* pProp, MM::ActionType eAct);
	int OnList(MM::PropertyBase* pProp, MM::ActionType eAct, lx_uint32 uiFeatureId);
	int OnRange(MM::PropertyBase* pProp, MM::ActionType eAct, lx_uint32 uiFeatureId);
	int OnExposureChange(MM::PropertyBase* pProp, MM::ActionType eAct, lx_uint32 uiFeatureId);
	
	/* KsCam Event Handler must be public */
	void DoEvent(const lx_uint32 uiCameraHandle, CAM_Event* pstEvent, void* pTransData);

private:
	int NikonKsCam::CreateKsProperty(lx_uint32 FeatureId, CPropertyAction* pAct);
	void NikonKsCam::SearchDevices();
	void NikonKsCam::bgr8ToBGRA8(unsigned char* dest, unsigned char* src, lx_uint32 width, lx_uint32 height);
	void NikonKsCam::GrabFrame();
	void NikonKsCam::SetFeature(lx_uint32 uiFeatureId);
	void NikonKsCam::GetAllFeaturesDesc();
	void NikonKsCam::GetAllFeatures();
	void NikonKsCam::updateImageSettings();
	void NikonKsCam::SetROILimits();
	void SetMeteringAreaLimits();
	void NikonKsCam::Command(const lx_wchar* wszCommand);
	const char* NikonKsCam::ConvFeatureIdToName(const lx_uint32 uiFeatureId);
	lx_uint32 NikonKsCam::AdjustExposureTime(lx_uint32 uiValue);

	//  Device Info ----------------------------------------
	BOOL m_isOpened;
	BOOL m_isInitialized;
	BOOL m_isRi2;
	lx_uint32 m_uiDeviceIndex;
	lx_uint32 m_uiDeviceCount;
	CAM_Device m_stDevice;
	CAM_Image stImage;

	//  Camera ---------------------------------------------
	lx_uint32 m_uiCameraHandle;
	CAM_CMD_GetFrameSize m_stFrameSize;
	char camID[CAM_NAME_MAX + CAM_VERSION_MAX];

	//  Callback -------------------------------------------
	void* m_ptrEventTransData;

	//  FrameRate ---------------------------------------------
	DWORD m_dwStartTick;
	DWORD m_dwStartTickAve;
	DWORD m_dwCount;
	DWORD m_dwCountAve;

	// Feature ---------------------------------------------
	Vector_CAM_FeatureValue m_vectFeatureValue;
	CAM_FeatureDesc* m_pFeatureDesc;
	CAM_FeatureDescFormat m_stDescFormat;
	std::map<lx_uint32, lx_uint32> m_mapFeatureIndex;

	inline void Free_Vector_CAM_FeatureValue(Vector_CAM_FeatureValue& vectFeatureValue)
	{
		if (vectFeatureValue.pstFeatureValue != NULL)
		{
			delete [] vectFeatureValue.pstFeatureValue;
			ZeroMemory(&vectFeatureValue, sizeof(Vector_CAM_FeatureValue));
		}
	}

	inline void Free_CAM_FeatureDesc()
	{
		if (m_pFeatureDesc != NULL)
		{
			delete [] m_pFeatureDesc;
			m_pFeatureDesc = NULL;
		}
	}
	
	//Image info
	ImgBuffer img_;
	long m_image_width;
	long m_image_height;
	int m_bitDepth;
	int m_byteDepth;
	int m_nComponents;
	bool m_Color;

	MMEvent m_frameDoneEvent; // Signals the sequence thread when a frame was captured
	bool busy_;
	bool stopOnOverFlow_;
	MM::MMTime readoutStartTime_;
	MM::MMTime sequenceStartTime_;
	unsigned roiX_;
	unsigned roiY_;
	unsigned roiWidth_;
	unsigned roiHeight_;
	long imageCounter_;
	long binSize_;
	double readoutUs_;
	volatile double framesPerSecond;

	MMThreadLock imgPixelsLock_;
	friend class MySequenceThread;
	MySequenceThread* thd_;
	char* cameraBuf; // camera buffer for image transfer
	int cameraBufId; // buffer id, required by the SDK
};

class MySequenceThread : public MMDeviceThreadBase
{
	friend class NikonKsCam;

	enum
	{
		default_numImages=1,
		default_intervalMS = 100
	};

public:
	MySequenceThread(NikonKsCam* pCam);
	~MySequenceThread();
	void Stop();
	void Start(long numImages, double intervalMs);
	bool IsStopped();
	void Suspend();
	bool IsSuspended();
	void Resume();

	double GetIntervalMs()
	{
		return intervalMs_;
	}

	void SetLength(long images)
	{
		numImages_ = images;
	}

	long GetLength() const
	{
		return numImages_;
	}

	long GetImageCounter()
	{
		return imageCounter_;
	}

	MM::MMTime GetStartTime()
	{
		return startTime_;
	}

	MM::MMTime GetActualDuration()
	{
		return actualDuration_;
	}

private:
	int svc(void) throw();
	bool stop_;
	bool suspend_;
	long numImages_;
	long imageCounter_;
	double intervalMs_;
	MM::MMTime startTime_;
	MM::MMTime actualDuration_;
	MM::MMTime lastFrameTime_;
	MMThreadLock stopLock_;
	MMThreadLock suspendLock_;
	NikonKsCam* camera_;
};


#endif //_NIKONKS_H_

