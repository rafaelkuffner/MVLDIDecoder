
// Includes
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <memory>
#include <iostream>
#include <cassert>

// CUDA Header includes
#include "dynlink_nvcuvid.h" // <nvcuvid.h>
#include "dynlink_cuda.h"    // <cuda.h>
#include "dynlink_cudaGL.h"  // <cudaGL.h>
#include "dynlink_builtin_types.h"

// CUDA utilities and system includes
#include "helper_functions.h"
#include "helper_cuda_drvapi.h"



#include "cudaProcessFrame.h"
#include "cudaModuleMgr.h"
// cudaDecodeGL related helper functions
#include "FrameQueue.h"
#include "VideoSource.h"
#include "VideoParser.h"
#include "VideoDecoder.h"
#include "ImageGL.h"

class cNvDecoder{

	int                 g_DeviceID;
	bool                g_bWindowed;
	bool                g_bDeviceLost;
	bool                g_bDone;
	bool                g_bRunning;
	bool                g_bAutoQuit;
	bool                g_bUseVsync;
	bool                g_bFrameStep;
	bool                g_bQAReadback;
	bool                g_bGLVerify;
	bool                g_bFirstFrame;
	bool                g_bLoop;
	bool                g_bUpdateCSC;
	bool                g_bUpdateAll;
	bool                g_bLinearFiltering;
	bool                g_bUseInterop;
	bool                g_bReadback; // this flag enables/disables reading back of a video from a window
	bool                g_bWriteFile; // this flag enables/disables writing of a file
	bool                g_bPSNR; // if this flag is set true, then we want to compute the PSNR
	bool                g_bIsProgressive; // assume it is progressive, unless otherwise noted
	bool                g_bException;
	bool                g_bWaived;

	long                g_nFrameStart;
	long                g_nFrameEnd;

	int   *pArgc;
	char **pArgv;

	FILE *fpWriteYUV;
	FILE *fpRefYUV;

	cudaVideoCreateFlags g_eVideoCreateFlags;
	CUvideoctxlock       g_CtxLock;

	float present_fps, decoded_fps, total_time;

	// These are CUDA function pointers to the CUDA kernels
	CUmoduleManager   *g_pCudaModule;

	CUmodule           cuModNV12toARGB;
	CUfunction         g_kernelNV12toARGB;
	CUfunction         g_kernelPassThru;

	CUcontext          g_oContext;
	CUdevice           g_oDevice;

	CUstream           g_ReadbackSID, g_KernelSID;

	eColorSpace        g_eColorSpace;
	float              g_nHue;

	// System Memory surface we want to readback to
	BYTE          *g_pFrameYUV;
	FrameQueue    *g_pFrameQueue;
	VideoSource   *g_pVideoSource;
	VideoParser   *g_pVideoParser;
	VideoDecoder  *g_pVideoDecoder;

	ImageGL       *g_pImageGL; // if we're using OpenGL

	CUVIDEOFORMAT g_stFormat;

	std::string sFileName;

	char exec_path[256];

	unsigned int g_nClientAreaWidth;
	unsigned int g_nClientAreaHeight;

	unsigned int g_nVideoWidth;
	unsigned int g_nVideoHeight;

	unsigned int g_FrameCount;
	unsigned int g_DecodeFrameCount;
	unsigned int g_fpsCount;      // FPS count for averaging
	unsigned int g_fpsLimit;     // FPS limit for sampling timer;

	// Forward declarations
public:
	cNvDecoder(std::string fileName, int device, std::string execPath);
	bool initGLTexture(unsigned int nWidth, unsigned int nHeight);
	bool loadVideoSource(const char *video_file,
		unsigned int &width, unsigned int &height);
	void initCudaVideo();

	void freeCudaResources(bool bDestroyContext);

	bool copyDecodedFrameToTexture( int *pbIsProgressive);
	void cudaPostProcessFrame(CUdeviceptr *ppDecodedFrame, size_t nDecodedPitch,
		CUdeviceptr *ppInteropFrame, size_t pFramePitch,
		CUmodule cuModNV12toARGB,
		CUfunction fpCudaKernel, CUstream streamID);
	bool cleanup(bool bDestroyContext, bool deleteImage);
	bool initCudaResources(int device);
	bool reinitCudaResources();
	bool renderVideoFrame(bool requestFrame);
	void finalizeRender();

	bool init();
	void finalize();
	static void InitCuda();

};