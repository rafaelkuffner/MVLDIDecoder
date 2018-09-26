//TIRAR NREPEAT PARA ACABAR COM A CONFUSÃO
//TERMINAR O DISPLAY


/*
 * Copyright 1993-2015 NVIDIA Corporation.  All rights reserved.
 *
 * Please refer to the NVIDIA end user license agreement (EULA) associated
 * with this source code for terms and conditions that govern your use of
 * this software. Any use, reproduction, disclosure, or distribution of
 * this software and related documentation outside the terms of the EULA
 * is strictly prohibited.
 *
 */

 /* This example demonstrates how to use the Video Decode Library with CUDA
  * bindings to interop between NVDECODE(using CUDA surfaces) and OpenGL (PBOs).
  * Post-Processing video (de-interlacing) is supported with this sample.
  */

#include "NvDecodeGL.h"

#if !defined (WIN32) && !defined (_WIN32) && !defined(WIN64) && !defined(_WIN64)
typedef unsigned char BYTE;
#define S_OK true;
#endif


#if defined(WIN32) || defined(_WIN32) || defined(WIN64) || defined(_WIN64)
typedef bool (APIENTRY *PFNWGLSWAPINTERVALFARPROC)(int);
PFNWGLSWAPINTERVALFARPROC wglSwapIntervalEXT = 0;

// This allows us to turn off vsync for OpenGL
void setVSync(int interval)
{
	const GLubyte *extensions = glGetString(GL_EXTENSIONS);

	if (strstr((const char *)extensions, "WGL_EXT_swap_control") == 0)
	{
		return;    // Error: WGL_EXT_swap_control extension not supported on your computer.\n");
	}
	else
	{
		wglSwapIntervalEXT = (PFNWGLSWAPINTERVALFARPROC)wglGetProcAddress("wglSwapIntervalEXT");

		if (wglSwapIntervalEXT)
		{
			wglSwapIntervalEXT(interval);
		}
	}
}

#ifndef STRCASECMP
#define STRCASECMP  _stricmp
#endif
#ifndef STRNCASECMP
#define STRNCASECMP _strnicmp
#endif

#else
void setVSync(int interval)
{
}

#ifndef STRCASECMP
#define STRCASECMP  strcasecmp
#endif
#ifndef STRNCASECMP
#define STRNCASECMP strncasecmp
#endif

#endif

bool cNvDecoder::initCudaResources(int device)
{
	int bTCC = 0;
	char name[100];
	CUdevice cuda_device;
	cuda_device = gpuDeviceInitDRV(device);
	checkCudaErrors(cuDeviceGetAttribute(&bTCC, CU_DEVICE_ATTRIBUTE_TCC_DRIVER, cuda_device));
	checkCudaErrors(cuDeviceGetName(name, 100, cuda_device));
	printf("  -> GPU %d: < %s > driver mode is: %s\n", cuda_device, name, bTCC ? "TCC" : "WDDM");

	if (cuda_device < 0)
	{
		printf("No CUDA Capable devices found, exiting...\n");
		exit(EXIT_SUCCESS);
	}

	checkCudaErrors(cuDeviceGet(&g_oDevice, cuda_device));

	// get compute capabilities and the devicename
	int major, minor;
	size_t totalGlobalMem;
	char deviceName[256];
	checkCudaErrors(cuDeviceComputeCapability(&major, &minor, g_oDevice));
	checkCudaErrors(cuDeviceGetName(deviceName, 256, g_oDevice));
	printf("> Using GPU Device %d: %s has SM %d.%d compute capability\n", cuda_device, deviceName, major, minor);

	checkCudaErrors(cuDeviceTotalMem(&totalGlobalMem, g_oDevice));
	printf("  Total amount of global memory:     %4.4f MB\n", (float)totalGlobalMem / (1024 * 1024));
	checkCudaErrors(cuGLCtxCreate(&g_oContext, CU_CTX_BLOCKING_SYNC, g_oDevice));

	try
	{
		// Initialize CUDA releated Driver API (32-bit or 64-bit), depending the platform running
		if (sizeof(void *) == 4)
		{
			g_pCudaModule = new CUmoduleManager("NV12ToARGB_drvapi_Win32.ptx", exec_path, 2, 2, 2);
		}
		else
		{
			g_pCudaModule = new CUmoduleManager("NV12ToARGB_drvapi_x64.ptx", exec_path, 2, 2, 2);
		}
	}
	catch (char const *p_file)
	{
		// If the CUmoduleManager constructor fails to load the PTX file, it will throw an exception
		printf("\n>> CUmoduleManager::Exception!  %s not found!\n", p_file);
		printf(">> Please rebuild NV12ToARGB_drvapi.cu or re-install this sample.\n");
		return false;
	}

	g_pCudaModule->GetCudaFunction("NV12ToARGB_drvapi", &g_kernelNV12toARGB);
	g_pCudaModule->GetCudaFunction("Passthru_drvapi", &g_kernelPassThru);

	/////////////////Change///////////////////////////
	// Now we create the CUDA resources and the CUDA decoder context
	initCudaVideo();

	initGLTexture(g_pVideoDecoder->targetWidth(), g_pVideoDecoder->targetHeight());

	CUcontext cuCurrent = NULL;
	CUresult result = cuCtxPopCurrent(&cuCurrent);

	if (result != CUDA_SUCCESS)
	{
		printf("cuCtxPopCurrent: %d\n", result);
		assert(0);
	}

	/////////////////////////////////////////
	return ((g_pCudaModule && g_pVideoDecoder) ? true : false);
}

bool cNvDecoder::reinitCudaResources()
{
	// Free resources
	cleanup(false, false);

	// Reinit VideoSource and Frame Queue
	g_bIsProgressive = loadVideoSource(sFileName.c_str(),
		g_nVideoWidth, g_nVideoHeight);

	/////////////////Change///////////////////////////
	initCudaVideo();
	//initGLTexture(g_pVideoDecoder->targetWidth(),
	//              g_pVideoDecoder->targetHeight());
	/////////////////////////////////////////

	return S_OK;
}


// Initializes OpenGL Textures (allocation and initialization)
bool
cNvDecoder::initGLTexture(unsigned int nWidth, unsigned int nHeight)
{
	g_pImageGL = new ImageGL(nWidth, nHeight,
		nWidth, nHeight,
		ImageGL::BGRA_PIXEL_FORMAT);
	g_pImageGL->clear(0x80);

	g_pImageGL->setCUDAcontext(g_oContext);
	g_pImageGL->setCUDAdevice(g_oDevice);
	return true;
}


bool
cNvDecoder::loadVideoSource(const char *video_file,
	unsigned int &width, unsigned int &height)
{
	std::auto_ptr<FrameQueue> apFrameQueue(new FrameQueue);
	std::auto_ptr<VideoSource> apVideoSource(new VideoSource(video_file, apFrameQueue.get()));

	// retrieve the video source (width,height)
	apVideoSource->getSourceDimensions(width, height);

	memset(&g_stFormat, 0, sizeof(CUVIDEOFORMAT));
	std::cout << (g_stFormat = apVideoSource->format()) << std::endl;

	g_pFrameQueue = apFrameQueue.release();
	g_pVideoSource = apVideoSource.release();

	if (g_pVideoSource->format().codec == cudaVideoCodec_JPEG)
	{
		g_eVideoCreateFlags = cudaVideoCreate_PreferCUDA;
	}

	bool IsProgressive = 0;
	g_pVideoSource->getProgressive(IsProgressive);
	return IsProgressive;
}

void
cNvDecoder::initCudaVideo()
{
	// bind the context lock to the CUDA context
	CUresult result = cuvidCtxLockCreate(&g_CtxLock, g_oContext);
	CUVIDEOFORMATEX oFormatEx;
	memset(&oFormatEx, 0, sizeof(CUVIDEOFORMATEX));
	oFormatEx.format = g_stFormat;

	if (result != CUDA_SUCCESS)
	{
		printf("cuvidCtxLockCreate failed: %d\n", result);
		assert(0);
	}

	std::auto_ptr<VideoDecoder> apVideoDecoder(new VideoDecoder(g_pVideoSource->format(), g_oContext, g_eVideoCreateFlags, g_CtxLock));
	std::auto_ptr<VideoParser> apVideoParser(new VideoParser(apVideoDecoder.get(), g_pFrameQueue, &oFormatEx, &g_oContext));
	g_pVideoSource->setParser(*apVideoParser.get());

	g_pVideoParser = apVideoParser.release();
	g_pVideoDecoder = apVideoDecoder.release();

	// Create a Stream ID for handling Readback
	if (g_bReadback)
	{
		checkCudaErrors(cuStreamCreate(&g_ReadbackSID, 0));
		checkCudaErrors(cuStreamCreate(&g_KernelSID, 0));
		printf("> initCudaVideo()\n");
		printf("  CUDA Streams (%s) <g_ReadbackSID = %p>\n", ((g_ReadbackSID == 0) ? "Disabled" : "Enabled"), g_ReadbackSID);
		printf("  CUDA Streams (%s) <g_KernelSID   = %p>\n", ((g_KernelSID == 0) ? "Disabled" : "Enabled"), g_KernelSID);
	}
}


void
cNvDecoder::freeCudaResources(bool bDestroyContext)
{
	if (g_pVideoParser)
	{
		delete g_pVideoParser;
	}

	if (g_pVideoDecoder)
	{
		delete g_pVideoDecoder;
	}

	if (g_pVideoSource)
	{
		delete g_pVideoSource;
	}

	if (g_pFrameQueue)
	{
		delete g_pFrameQueue;
	}

	if (g_ReadbackSID)
	{
		checkCudaErrors(cuStreamDestroy(g_ReadbackSID));
	}

	if (g_KernelSID)
	{
		checkCudaErrors(cuStreamDestroy(g_KernelSID));
	}

	if (g_CtxLock)
	{
		checkCudaErrors(cuvidCtxLockDestroy(g_CtxLock));
	}

	if (g_oContext && bDestroyContext)
	{
		checkCudaErrors(cuCtxDestroy(g_oContext));
		g_oContext = NULL;
	}
}

// Run the Cuda part of the computation (if g_pFrameQueue is empty, then return false)
bool cNvDecoder::copyDecodedFrameToTexture(int *pbIsProgressive)
{
	CUVIDPARSERDISPINFO oDisplayInfo;

	if (g_pFrameQueue->dequeue(&oDisplayInfo))
	{
		CCtxAutoLock lck(g_CtxLock);
		// Push the current CUDA context (only if we are using CUDA decoding path)
		cuCtxPushCurrent(g_oContext);

		CUdeviceptr  pDecodedFrame = 0;
		CUdeviceptr  pInteropFrame = 0;

		*pbIsProgressive = oDisplayInfo.progressive_frame;
		g_bIsProgressive = oDisplayInfo.progressive_frame ? true : false;


		CUVIDPROCPARAMS oVideoProcessingParameters;
		memset(&oVideoProcessingParameters, 0, sizeof(CUVIDPROCPARAMS));

		oVideoProcessingParameters.progressive_frame = oDisplayInfo.progressive_frame;
		oVideoProcessingParameters.top_field_first = oDisplayInfo.top_field_first;
		oVideoProcessingParameters.unpaired_field = (oDisplayInfo.repeat_first_field < 0);

		unsigned int nDecodedPitch = 0;
		unsigned int nWidth = 0;
		unsigned int nHeight = 0;

		oVideoProcessingParameters.second_field = 0;

		if (g_pVideoDecoder->mapFrame(oDisplayInfo.picture_index, &pDecodedFrame, &nDecodedPitch, &oVideoProcessingParameters) != CUDA_SUCCESS)
		{
			// release the frame, so it can be re-used in decoder
			g_pFrameQueue->releaseFrame(&oDisplayInfo);

			// Detach from the Current thread
			checkCudaErrors(cuCtxPopCurrent(NULL));

			return false;
		}
		nWidth = g_pVideoDecoder->targetWidth(); // PAD_ALIGN(g_pVideoDecoder->targetWidth(), 0x3F);
		nHeight = g_pVideoDecoder->targetHeight(); // PAD_ALIGN(g_pVideoDecoder->targetHeight(), 0x0F);
		// map OpenGL PBO or CUDA memory
		size_t nTexturePitch = 0;

		// If we are Encoding and this is the 1st Frame, we make sure we allocate system memory for readbacks
		if (g_bReadback && g_bFirstFrame && g_ReadbackSID)
		{
			CUresult result;
			checkCudaErrors(result = cuMemAllocHost((void **)&g_pFrameYUV, (nDecodedPitch * nHeight + nDecodedPitch*nHeight / 2)));

			g_bFirstFrame = false;

			if (result != CUDA_SUCCESS)
			{
				printf("cuMemAllocHost returned %d\n", (int)result);
				checkCudaErrors(result);
			}
		}

		// If streams are enabled, we can perform the readback to the host while the kernel is executing
		if (g_bReadback && g_ReadbackSID)
		{
			CUresult result = cuMemcpyDtoHAsync(g_pFrameYUV, pDecodedFrame, (nDecodedPitch * nHeight * 3 / 2), g_ReadbackSID);

			if (result != CUDA_SUCCESS)
			{
				printf("cuMemAllocHost returned %d\n", (int)result);
				checkCudaErrors(result);
			}
		}


		g_pImageGL->map(&pInteropFrame, &nTexturePitch);
		nTexturePitch /= g_pVideoDecoder->targetHeight();

		// perform post processing on the CUDA surface (performs colors space conversion and post processing)
		// comment this out if we inclue the line of code seen above

		cudaPostProcessFrame(&pDecodedFrame, nDecodedPitch, &pInteropFrame,
			nTexturePitch, g_pCudaModule->getModule(), g_kernelNV12toARGB, g_KernelSID);


		g_pImageGL->unmap();


		// unmap video frame
		// unmapFrame() synchronizes with the VideoDecode API (ensures the frame has finished decoding)
		g_pVideoDecoder->unmapFrame(pDecodedFrame);
		g_DecodeFrameCount++;

		// Detach from the Current thread
		checkCudaErrors(cuCtxPopCurrent(NULL));
		// release the frame, so it can be re-used in decoder
		g_pFrameQueue->releaseFrame(&oDisplayInfo);
	}
	else
	{
		// Frame Queue has no frames, we don't compute FPS until we start
		return false;
	}

	// check if decoding has come to an end.
	// if yes, signal the app to shut down.
	if (!g_pVideoSource->isStarted() && g_pFrameQueue->isEndOfDecode() && g_pFrameQueue->isEmpty())
	{
		// Let's free the Frame Data
		if (g_ReadbackSID)
		{
			cuMemFreeHost((void *)g_pFrameYUV);
			g_pFrameYUV = NULL;

		}

		// Let's just stop, and allow the user to quit, so they can at least see the results
		g_pVideoSource->stop();

		// If we want to loop reload the video file and restart
		if (g_bLoop && !g_bAutoQuit)
		{
			reinitCudaResources();
			g_FrameCount = 0;
			g_DecodeFrameCount = 0;
			g_pVideoSource->start();
		}

		if (g_bAutoQuit)
		{
			g_bDone = true;
		}
	}

	return true;
}

// This is the CUDA stage for Video Post Processing.  Last stage takes care of the NV12 to ARGB
void
cNvDecoder::cudaPostProcessFrame(CUdeviceptr *ppDecodedFrame, size_t nDecodedPitch,
	CUdeviceptr *ppTextureData, size_t nTexturePitch,
	CUmodule cuModNV12toARGB,
	CUfunction fpCudaKernel, CUstream streamID)
{
	uint32 nWidth = g_pVideoDecoder->targetWidth();
	uint32 nHeight = g_pVideoDecoder->targetHeight();

	// Upload the Color Space Conversion Matrices
	if (g_bUpdateCSC)
	{
		// CCIR 601/709
		float hueColorSpaceMat[9];
		setColorSpaceMatrix(g_eColorSpace, hueColorSpaceMat, g_nHue);
		updateConstantMemory_drvapi(cuModNV12toARGB, hueColorSpaceMat);

		if (!g_bUpdateAll)
		{
			g_bUpdateCSC = false;
		}
	}

	// TODO: Stage for handling video post processing

	// Final Stage: NV12toARGB color space conversion
	cudaLaunchNV12toARGBDrv(*ppDecodedFrame, nDecodedPitch,
		*ppTextureData, nTexturePitch,
		nWidth, nHeight, fpCudaKernel, streamID);
}

// Release all previously initd objects
bool cNvDecoder::cleanup(bool bDestroyContext, bool deleteImage)
{
	if (deleteImage) {
		delete g_pImageGL;
		g_pImageGL = NULL;
	}

	freeCudaResources(bDestroyContext);

	return true;
}


cNvDecoder::cNvDecoder(std::string fileName, int device,std::string execPath) {
	sFileName = fileName;
	g_DeviceID = device;
	cuModNV12toARGB = 0;
	g_kernelNV12toARGB = 0;
	g_kernelPassThru = 0;
	g_oContext = 0;
	g_oDevice = 0;
	g_ReadbackSID = 0;
	g_KernelSID = 0;
	g_eColorSpace = ITU709;
	g_nHue = 0.0f;
	// System Memory surface we want to readback to
	g_pFrameYUV = 0;
	g_pFrameQueue = 0;
	g_pVideoSource = 0;
	g_pVideoParser = 0;
	g_pVideoDecoder = 0;
	g_pImageGL = 0;

	g_DeviceID = 0;
	g_bWindowed = true;
	g_bDeviceLost = false;
	g_bDone = false;
	g_bRunning = false;
	g_bAutoQuit = false;
	g_bUseVsync = false;
	g_bFrameStep = false;
	g_bQAReadback = false;
	g_bGLVerify = false;
	g_bFirstFrame = true;
	g_bLoop = true;
	g_bUpdateCSC = true;
	g_bUpdateAll = false;
	g_bLinearFiltering = false;
	g_bUseInterop = true;
	g_bReadback = false; // this flag enables/disables reading back of a video from a window
	g_bWriteFile = false; // this flag enables/disables writing of a file
	g_bPSNR = false; // if this flag is set true, then we want to compute the PSNR
	g_bIsProgressive = true; // assume it is progressive, unless otherwise noted
	g_bException = false;
	g_bWaived = false;

	g_nFrameStart = -1;
	g_nFrameEnd = -1;

	pArgc = NULL;
	pArgv = NULL;

	fpWriteYUV = NULL;
	fpRefYUV = NULL;

//	g_eVideoCreateFlags = cudaVideoCreate_PreferCUVID;
	g_eVideoCreateFlags = cudaVideoCreate_PreferCUDA;

	g_CtxLock = NULL;

	present_fps = decoded_fps = total_time = 0.0f;

	g_nClientAreaWidth = 0;
	g_nClientAreaHeight = 0;

	g_nVideoWidth = 0;
	g_nVideoHeight = 0;

	g_FrameCount = 0;
	g_DecodeFrameCount = 0;
	g_fpsCount = 0;      // FPS count for averaging
	g_fpsLimit = 16;     // FPS limit for sampling timer;

	strcpy(exec_path, execPath.c_str());
}


void cNvDecoder::InitCuda()
{
	CUDADRIVER hHandleDriver = 0;
	cuInit(0, __CUDA_API_VERSION, hHandleDriver);
	cuvidInit(0);
}

bool cNvDecoder::init()
{

	// Find out the video size (uses NVDECODE calls)
	g_bIsProgressive = loadVideoSource(sFileName.c_str(),
		g_nVideoWidth, g_nVideoHeight);

	// Determine the proper window size needed to create the correct *client* area
	// that is of the size requested by m_dimensions.
	RECT adjustedWindowSize;
	DWORD dwWindowStyle = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
	SetRect(&adjustedWindowSize, 0, 0, g_nVideoWidth, g_nVideoHeight);
	AdjustWindowRect(&adjustedWindowSize, dwWindowStyle, false);

	g_nVideoWidth = PAD_ALIGN(g_nVideoWidth, 0x3F);
	g_nVideoHeight = PAD_ALIGN(g_nVideoHeight, 0x0F);

	// Initialize CUDA and try to connect with an OpenGL context
	// Other video memory resources will be available
	int bTCC = 0;

	if (initCudaResources(g_DeviceID) == false)
	{
		g_bAutoQuit = true;
		g_bException = true;
		g_bWaived = true;
		return false;
	}

	g_pVideoSource->start();
	g_bRunning = true;

}

void cNvDecoder::finalize() {
	g_pFrameQueue->endDecode();
	g_pVideoSource->stop();
	cleanup(g_bWaived ? false : true, true);

}
// display results using OpenGL



// Launches the CUDA kernels to fill in the texture data
bool cNvDecoder::renderVideoFrame(bool requestFrame)
{
	int bIsProgressive = 1;
	bool bFramesDecoded = false;

	if (requestFrame && 0 != g_pFrameQueue)
	{
		// if not running, we simply don't copy new frames from the decoder
		if (!g_bDeviceLost && g_bRunning)
		{
			bFramesDecoded = copyDecodedFrameToTexture(&bIsProgressive);
		}
	}

	if (!requestFrame || bFramesDecoded)
	{
		g_pImageGL->prerender();
		bFramesDecoded = true;
	}
	return bFramesDecoded;
}

void cNvDecoder::finalizeRender()
{
	g_pImageGL->postrender();
}