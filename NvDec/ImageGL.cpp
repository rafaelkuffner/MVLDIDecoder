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

#include "ImageGL.h"

#include "dynlink_cuda.h"   // <cuda.h>
#include "dynlink_cudaGL.h" // <cudaGL.h>

#include <cassert>

#include "helper_cuda_drvapi.h"


ImageGL::ImageGL(unsigned int nDispWidth,
                 unsigned int nDispHeight,
                 unsigned int nTexWidth,
                 unsigned int nTexHeight,
                 PixelFormat ePixelFormat)
    : nWidth_(nDispWidth)
    , nHeight_(nDispHeight)
    , nTexWidth_(nTexWidth)
    , nTexHeight_(nTexHeight)
    , e_PixFmt_(ePixelFormat)
    , bIsCudaResource_(false)
{

    glGenBuffers(1, &gl_pbo_);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_pbo_);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, nTexWidth*nTexHeight*4, NULL, GL_STREAM_DRAW);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    registerAsCudaResource();
  
    // create texture for display
    glGenTextures(1, &gl_texid_);
    // setup the Texture filtering mode
    setTextureFilterMode(GL_NEAREST, GL_NEAREST);

}

ImageGL::~ImageGL()
{
    unregisterAsCudaResource();
    glDeleteBuffersARB(1,&gl_pbo_);
    glDeleteTextures(1, &gl_texid_);
}

void
ImageGL::registerAsCudaResource()
{
    // register the OpenGL resources that we'll use within CD
    checkCudaErrors(cuGLRegisterBufferObject(gl_pbo_));
    getLastCudaDrvErrorMsg("cuGLRegisterBufferObject (gl_pbo_) failed");
    bIsCudaResource_ = true;
}

void
ImageGL::unregisterAsCudaResource()
{
    cuCtxPushCurrent(oContext_);
    checkCudaErrors(cuGLUnregisterBufferObject(gl_pbo_));
    bIsCudaResource_ = false;
    cuCtxPopCurrent(NULL);
}

void
ImageGL::setTextureFilterMode(GLuint nMINfilter, GLuint nMAGfilter)
{
    int nFrames = bVsync_ ? 3 : 1;       

    printf("setTextureFilterMode(%s,%s)\n",
           (nMINfilter == GL_NEAREST) ? "GL_NEAREST" : "GL_LINEAR",
           (nMAGfilter == GL_NEAREST) ? "GL_NEAREST" : "GL_LINEAR");

   glBindTexture(GL_TEXTURE_TYPE, gl_texid_);
   glTexImage2D(GL_TEXTURE_TYPE, 0, GL_RGBA8, nTexWidth_, nTexHeight_, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
   glTexParameteri(GL_TEXTURE_TYPE, GL_TEXTURE_MIN_FILTER, nMINfilter);
   glTexParameteri(GL_TEXTURE_TYPE, GL_TEXTURE_MAG_FILTER, nMAGfilter);
   glBindTexture(GL_TEXTURE_TYPE, 0);
}

void
ImageGL::setCUDAcontext(CUcontext oContext)
{
    oContext_ = oContext;
    printf("ImageGL::CUcontext = %08lx\n", (unsigned long)oContext);
}

void
ImageGL::setCUDAdevice(CUdevice oDevice)
{
    oDevice_ = oDevice;
    printf("ImageGL::CUdevice  = %08lx\n", (unsigned long)oDevice);
}

bool
ImageGL::isCudaResource()
const
{
    return bIsCudaResource_;
}

void
ImageGL::map(CUdeviceptr *pImageData, size_t *pImagePitch)
{
    checkCudaErrors(cuGLMapBufferObject(pImageData, pImagePitch, gl_pbo_));
    assert(0 != *pImagePitch);
}

void
ImageGL::unmap()
{
    checkCudaErrors(cuGLUnmapBufferObject(gl_pbo_));
}

void
ImageGL::clear(unsigned char nClearColor)
{
    // Can only be cleared if surface is a CUDA resource
    assert(bIsCudaResource_);

    int nFrames = bVsync_ ? 3 : 1;        
    size_t       imagePitch;
    CUdeviceptr  pImageData;

    for (int field_num=0; field_num < nFrames; field_num++)
    {
        map(&pImageData, &imagePitch);
        // clear the surface to solid white
        checkCudaErrors(cuMemsetD8(pImageData, nClearColor, nTexWidth_*nTexHeight_* Bpp()));
        unmap();
    }
}

void
ImageGL::prerender()
const
{
    // load texture from pbo
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, gl_pbo_);
    glBindTexture(GL_TEXTURE_TYPE, gl_texid_);
    glTexSubImage2D(GL_TEXTURE_TYPE, 0, 0, 0, nTexWidth_, nTexHeight_, GL_BGRA, GL_UNSIGNED_BYTE, 0);
}

void
ImageGL::postrender()
const
{
	glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	glBindTexture(GL_TEXTURE_TYPE, 0);
}