/*M///////////////////////////////////////////////////////////////////////////////////////
//
//  IMPORTANT: READ BEFORE DOWNLOADING, COPYING, INSTALLING OR USING.
//
//  By downloading, copying, installing or using the software you agree to this license.
//  If you do not agree to this license, do not download, install,
//  copy or use the software.
//
//
//                           License Agreement
//                For Open Source Computer Vision Library
//
// Copyright (C) 2000-2008, Intel Corporation, all rights reserved.
// Copyright (C) 2009, Willow Garage Inc., all rights reserved.
// Third party copyrights are property of their respective owners.
//
// Redistribution and use in source and binary forms, with or without modification,
// are permitted provided that the following conditions are met:
//
//   * Redistribution's of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//   * Redistribution's in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//   * The name of the copyright holders may not be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
// This software is provided by the copyright holders and contributors "as is" and
// any express or implied warranties, including, but not limited to, the implied
// warranties of merchantability and fitness for a particular purpose are disclaimed.
// In no event shall the Intel Corporation or contributors be liable for any direct,
// indirect, incidental, special, exemplary, or consequential damages
// (including, but not limited to, procurement of substitute goods or services;
// loss of use, data, or profits; or business interruption) however caused
// and on any theory of liability, whether in contract, strict liability,
// or tort (including negligence or otherwise) arising in any way out of
// the use of this software, even if advised of the possibility of such damage.
//
//M*/

#include "precomp.hpp"

#ifdef HAVE_VAAPI
//#  include <va/va_drm.h>
#else // HAVE_VAAPI
#  define NO_VAAPI_SUPPORT_ERROR CV_ErrorNoReturn(cv::Error::StsBadFunc, "OpenCV was build without VA-API support")
#endif // HAVE_VAAPI

using namespace cv;
//using namespace cv::cuda;

////////////////////////////////////////////////////////////////////////
// CL-VA Interoperability

#ifdef HAVE_OPENCL
#  include "opencv2/core/opencl/runtime/opencl_core.hpp"
#  include "opencv2/core.hpp"
#  include "opencv2/core/ocl.hpp"
#  include "opencl_kernels_core.hpp"
#else // HAVE_OPENCL
#  define NO_OPENCL_SUPPORT_ERROR CV_ErrorNoReturn(cv::Error::StsBadFunc, "OpenCV was build without OpenCL support")
#endif // HAVE_OPENCL

#if defined(HAVE_VAAPI) && defined(HAVE_OPENCL)
#  include "va_ext.h" //<CL/va_ext.h>
#endif // HAVE_VAAPI && HAVE_OPENCL

namespace cv { namespace vaapi {

#if defined(HAVE_VAAPI) && defined(HAVE_OPENCL)

#if 0
static int vaInitialize(VADisplay display, int* majorVersion, int* minorVersion) { (void)display; (void)majorVersion; (void)minorVersion; return 0; }
static int vaTerminate(VADisplay display) { (void)display; return 0; }
//static VADisplay vaGetDisplayDRM(int fd) { (void)fd; return 0; }
#endif

static clGetDeviceIDsFromVA_APIMediaAdapterINTEL_fn clGetDeviceIDsFromVA_APIMediaAdapterINTEL = NULL;
static clCreateFromVA_APIMediaSurfaceINTEL_fn       clCreateFromVA_APIMediaSurfaceINTEL       = NULL;
static clEnqueueAcquireVA_APIMediaSurfacesINTEL_fn  clEnqueueAcquireVA_APIMediaSurfacesINTEL  = NULL;
static clEnqueueReleaseVA_APIMediaSurfacesINTEL_fn  clEnqueueReleaseVA_APIMediaSurfacesINTEL  = NULL;

#endif // HAVE_VAAPI && HAVE_OPENCL

namespace ocl {

Context& initializeContextFromVA(VADisplay display)
{
#if !defined(HAVE_VAAPI)
    NO_VAAPI_SUPPORT_ERROR;
#elif !defined(HAVE_OPENCL)
    NO_OPENCL_SUPPORT_ERROR;
#else
    cl_uint numPlatforms;
    cl_int status = clGetPlatformIDs(0, NULL, &numPlatforms);
    if (status != CL_SUCCESS)
        CV_Error(cv::Error::OpenCLInitError, "OpenCL: Can't get number of platforms");
    if (numPlatforms == 0)
        CV_Error(cv::Error::OpenCLInitError, "OpenCL: No available platforms");

    std::vector<cl_platform_id> platforms(numPlatforms);
    status = clGetPlatformIDs(numPlatforms, &platforms[0], NULL);
    if (status != CL_SUCCESS)
        CV_Error(cv::Error::OpenCLInitError, "OpenCL: Can't get platform Id list");

    // For CL-VA interop, we must find platform/device with "cl_intel_va_api_media_sharing" extension.
    // With standard initialization procedure, we should examine platform extension string for that.
    // But in practice, the platform ext string doesn't contain it, while device ext string does.
    // Follow Intel procedure (see tutorial), we should obtain device IDs by extension call.
    // Note that we must obtain function pointers using specific platform ID, and can't provide pointers in advance.
    // So, we iterate and select the first platform, for which we got non-NULL pointers, device, and CL context.

    int found = -1;
    cl_context context = 0;
    cl_device_id device = 0;

    for (int i = 0; i < (int)numPlatforms; ++i)
    {
        // Get extension function pointers

        clGetDeviceIDsFromVA_APIMediaAdapterINTEL = (clGetDeviceIDsFromVA_APIMediaAdapterINTEL_fn)
            clGetExtensionFunctionAddressForPlatform(platforms[i], "clGetDeviceIDsFromVA_APIMediaAdapterINTEL");
        clCreateFromVA_APIMediaSurfaceINTEL       = (clCreateFromVA_APIMediaSurfaceINTEL_fn)
            clGetExtensionFunctionAddressForPlatform(platforms[i], "clCreateFromVA_APIMediaSurfaceINTEL");
        clEnqueueAcquireVA_APIMediaSurfacesINTEL  = (clEnqueueAcquireVA_APIMediaSurfacesINTEL_fn)
            clGetExtensionFunctionAddressForPlatform(platforms[i], "clEnqueueAcquireVA_APIMediaSurfacesINTEL");
        clEnqueueReleaseVA_APIMediaSurfacesINTEL  = (clEnqueueReleaseVA_APIMediaSurfacesINTEL_fn)
            clGetExtensionFunctionAddressForPlatform(platforms[i], "clEnqueueReleaseVA_APIMediaSurfacesINTEL");

        if (((void*)clGetDeviceIDsFromVA_APIMediaAdapterINTEL == NULL) ||
            ((void*)clCreateFromVA_APIMediaSurfaceINTEL == NULL) ||
            ((void*)clEnqueueAcquireVA_APIMediaSurfacesINTEL == NULL) ||
            ((void*)clEnqueueReleaseVA_APIMediaSurfacesINTEL == NULL))
        {
            continue;
        }

        // Query device list

        cl_uint numDevices = 0;

        status = clGetDeviceIDsFromVA_APIMediaAdapterINTEL(platforms[i], CL_VA_API_DISPLAY_INTEL, display,
                                                           CL_PREFERRED_DEVICES_FOR_VA_API_INTEL, 0, NULL, &numDevices);
        if ((status != CL_SUCCESS) || !(numDevices > 0))
            continue;
        numDevices = 1; // initializeContextFromHandle() expects only 1 device
        status = clGetDeviceIDsFromVA_APIMediaAdapterINTEL(platforms[i], CL_VA_API_DISPLAY_INTEL, display,
                                                           CL_PREFERRED_DEVICES_FOR_VA_API_INTEL, numDevices, &device, NULL);
        if (status != CL_SUCCESS)
            continue;

        // Creating CL-VA media sharing OpenCL context

        cl_context_properties props[] = {
            CL_CONTEXT_VA_API_DISPLAY_INTEL, (cl_context_properties) display,
            CL_CONTEXT_INTEROP_USER_SYNC, CL_FALSE, // no explicit sync required
            0
        };

        context = clCreateContext(props, numDevices, &device, NULL, NULL, &status);
        if (status != CL_SUCCESS)
        {
            clReleaseDevice(device);
        }
        else
        {
            found = i;
            break;
        }
    }

    if (found < 0)
        CV_Error(cv::Error::OpenCLInitError, "OpenCL: Can't create context for VA-API interop");

    Context& ctx = Context::getDefault(false);
    initializeContextFromHandle(ctx, platforms[found], context, device);
    return ctx;
#endif
}

#if defined(HAVE_VAAPI) && defined(HAVE_OPENCL)
static bool ocl_convert_nv12_to_bgr(cl_mem clImageY, cl_mem clImageUV, cl_mem clBuffer, int step, int cols, int rows)
{
    ocl::Kernel k;
    k.create("YUV2BGR_NV12_8u", cv::ocl::core::cvtclr_dx_oclsrc, "");
    if (k.empty())
        return false;

    k.args(clImageY, clImageUV, clBuffer, step, cols, rows);

    size_t globalsize[] = { cols, rows };
    return k.run(2, globalsize, 0, false);
}

static bool ocl_convert_bgr_to_nv12(cl_mem clBuffer, int step, int cols, int rows, cl_mem clImageY, cl_mem clImageUV)
{
    ocl::Kernel k;
    k.create("BGR2YUV_NV12_8u", cv::ocl::core::cvtclr_dx_oclsrc, "");
    if (k.empty())
        return false;

    k.args(clBuffer, step, cols, rows, clImageY, clImageUV);

    size_t globalsize[] = { cols, rows };
    return k.run(2, globalsize, 0, false);
}
#endif // HAVE_VAAPI && HAVE_OPENCL

} // namespace cv::vaapi::ocl

void convertToVASurface(InputArray src, VASurfaceID* surface, Size size)
{
    (void)src; (void)surface;
#if !defined(HAVE_VAAPI)
    NO_VAAPI_SUPPORT_ERROR;
#elif !defined(HAVE_OPENCL)
    NO_OPENCL_SUPPORT_ERROR;
#else
    const int stype = CV_8UC4;

    int srcType = src.type();
    CV_Assert(srcType == stype);

    Size srcSize = src.size();
    CV_Assert(srcSize.width == size.width && srcSize.height == size.height);

    UMat u = src.getUMat();

    // TODO Add support for roi
    CV_Assert(u.offset == 0);
    CV_Assert(u.isContinuous());

    cl_mem clBuffer = (cl_mem)u.handle(ACCESS_WRITE);

    using namespace cv::ocl;
    Context& ctx = Context::getDefault();
    cl_context context = (cl_context)ctx.ptr();

    cl_int status = 0;

    cl_mem clImageY = clCreateFromVA_APIMediaSurfaceINTEL(context, CL_MEM_WRITE_ONLY, surface, 0, &status);
    if (status != CL_SUCCESS)
        CV_Error(cv::Error::OpenCLApiCallError, "OpenCL: clCreateFromVA_APIMediaSurfaceINTEL failed (Y plane)");
    cl_mem clImageUV = clCreateFromVA_APIMediaSurfaceINTEL(context, CL_MEM_WRITE_ONLY, surface, 1, &status);
    if (status != CL_SUCCESS)
        CV_Error(cv::Error::OpenCLApiCallError, "OpenCL: clCreateFromVA_APIMediaSurfaceINTEL failed (UV plane)");

    cl_command_queue q = (cl_command_queue)Queue::getDefault().ptr();

    cl_mem images[2] = { clImageY, clImageUV };
    status = clEnqueueAcquireVA_APIMediaSurfacesINTEL(q, 2, images, 0, NULL, NULL);
    if (status != CL_SUCCESS)
        CV_Error(cv::Error::OpenCLApiCallError, "OpenCL: clEnqueueAcquireVA_APIMediaSurfacesINTEL failed");
    if (!ocl::ocl_convert_bgr_to_nv12(clBuffer, (int)u.step[0], u.cols, u.rows, clImageY, clImageUV))
        CV_Error(cv::Error::OpenCLApiCallError, "OpenCL: ocl_convert_bgr_to_nv12 failed");
    clEnqueueReleaseVA_APIMediaSurfacesINTEL(q, 2, images, 0, NULL, NULL);
    if (status != CL_SUCCESS)
        CV_Error(cv::Error::OpenCLApiCallError, "OpenCL: clEnqueueReleaseVA_APIMediaSurfacesINTEL failed");

    status = clFinish(q); // TODO Use events
    if (status != CL_SUCCESS)
        CV_Error(cv::Error::OpenCLApiCallError, "OpenCL: clFinish failed");

    status = clReleaseMemObject(clImageY); // TODO RAII
    if (status != CL_SUCCESS)
        CV_Error(cv::Error::OpenCLApiCallError, "OpenCL: clReleaseMem failed (Y plane)");
    status = clReleaseMemObject(clImageUV);
    if (status != CL_SUCCESS)
        CV_Error(cv::Error::OpenCLApiCallError, "OpenCL: clReleaseMem failed (UV plane)");
#endif
}

void convertFromVASurface(VASurfaceID* surface, Size size, OutputArray dst)
{
    (void)surface; (void)dst;
#if !defined(HAVE_VAAPI)
    NO_VAAPI_SUPPORT_ERROR;
#elif !defined(HAVE_OPENCL)
    NO_OPENCL_SUPPORT_ERROR;
#else
    const int dtype = CV_8UC4;

    // TODO Need to specify ACCESS_WRITE here somehow to prevent useless data copying!
    dst.create(size, dtype);
    UMat u = dst.getUMat();

    // TODO Add support for roi
    CV_Assert(u.offset == 0);
    CV_Assert(u.isContinuous());

    cl_mem clBuffer = (cl_mem)u.handle(ACCESS_READ);

    using namespace cv::ocl;
    Context& ctx = Context::getDefault();
    cl_context context = (cl_context)ctx.ptr();

    cl_int status = 0;

    cl_mem clImageY = clCreateFromVA_APIMediaSurfaceINTEL(context, CL_MEM_READ_ONLY, surface, 0, &status);
    if (status != CL_SUCCESS)
        CV_Error(cv::Error::OpenCLApiCallError, "OpenCL: clCreateFromVA_APIMediaSurfaceINTEL failed (Y plane)");
    cl_mem clImageUV = clCreateFromVA_APIMediaSurfaceINTEL(context, CL_MEM_READ_ONLY, surface, 1, &status);
    if (status != CL_SUCCESS)
        CV_Error(cv::Error::OpenCLApiCallError, "OpenCL: clCreateFromVA_APIMediaSurfaceINTEL failed (UV plane)");

    cl_command_queue q = (cl_command_queue)Queue::getDefault().ptr();

    cl_mem images[2] = { clImageY, clImageUV };
    status = clEnqueueAcquireVA_APIMediaSurfacesINTEL(q, 2, images, 0, NULL, NULL);
    if (status != CL_SUCCESS)
        CV_Error(cv::Error::OpenCLApiCallError, "OpenCL: clEnqueueAcquireVA_APIMediaSurfacesINTEL failed");
    if (!ocl::ocl_convert_nv12_to_bgr(clImageY, clImageUV, clBuffer, (int)u.step[0], u.cols, u.rows))
        CV_Error(cv::Error::OpenCLApiCallError, "OpenCL: ocl_convert_nv12_to_bgr failed");
    status = clEnqueueReleaseVA_APIMediaSurfacesINTEL(q, 2, images, 0, NULL, NULL);
    if (status != CL_SUCCESS)
        CV_Error(cv::Error::OpenCLApiCallError, "OpenCL: clEnqueueReleaseVA_APIMediaSurfacesINTEL failed");

    status = clFinish(q); // TODO Use events
    if (status != CL_SUCCESS)
        CV_Error(cv::Error::OpenCLApiCallError, "OpenCL: clFinish failed");

    status = clReleaseMemObject(clImageY); // TODO RAII
    if (status != CL_SUCCESS)
        CV_Error(cv::Error::OpenCLApiCallError, "OpenCL: clReleaseMem failed (Y plane)");
    status = clReleaseMemObject(clImageUV);
    if (status != CL_SUCCESS)
        CV_Error(cv::Error::OpenCLApiCallError, "OpenCL: clReleaseMem failed (UV plane)");
#endif
}

}} // namespace cv::vaapi
