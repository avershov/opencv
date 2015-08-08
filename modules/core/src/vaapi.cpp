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
# include <va/va_drm.h>
#else // HAVE_VAAPI
#  define NO_VAAPI_SUPPORT_ERROR CV_ErrorNoReturn(cv::Error::StsBadFunc, "OpenCV was build without VA-API support")
#endif // HAVE_VAAPI

using namespace cv;
using namespace cv::cuda;

////////////////////////////////////////////////////////////////////////
// CL-VA Interoperability

#ifdef HAVE_OPENCL
#else // HAVE_OPENCL
#  define NO_OPENCL_SUPPORT_ERROR CV_ErrorNoReturn(cv::Error::StsBadFunc, "OpenCV was build without OpenCL support")
#endif // HAVE_OPENCL

#if defined(HAVE_VAAPI) && defined(HAVE_OPENCL)
# include <CL/va_ext.h>
#endif // HAVE_VAAPI && HAVE_OPENCL

namespace cv { namespace vaapi {

#if defined(HAVE_VAAPI) && defined(HAVE_OPENCL)

#define VAAPI_PCI_DIR "/sys/bus/pci/devices"
#define VAAPI_DRI_DIR "/dev/dri/"
#define VAAPI_PCI_DISPLAY_CONTROLLER_CLASS 0x03

static int vaFDdrm = -1;
static VADisplay vaDisplay = NULL;

class Directory
{
    typedef int (*fsort)(const struct dirent**, const struct dirent**);
public:
    Directory(const char* path)
        {
            dirEntries = 0;
            numEntries = scandir(path, &dirEntries, filterFunc, (fsort)alphasort);
        }
    ~Directory()
        {
            if (numEntries && dirEntries)
            {
                for (int i = 0;  i < numEntries;  ++i)
                    free(dirEntries[i]);
                free(dirEntries);
            }
        }
    int count() const
        {
            return numEntries;
        }
    const struct dirent* operator[](int index) const
        {
            return ((dirEntries != 0) && (index >= 0) && (index < numEntries)) ? dirEntries[index] : 0;
        }
protected:
    static int filterFunc(const struct dirent* dir)
        {
            if (!dir) return 0;
            if (!strcmp(dir->d_name, ".")) return 0;
            if (!strcmp(dir->d_name, "..")) return 0;
            return 1;
        }
private:
    int numEntries;
    struct dirent** dirEntries;
};

static unsigned readId(const char* devName, const char* idName)
{
    long int id = 0;

    char fileName[256];
    snprintf(fileName, sizeof(fileName), "%s/%s/%s", VAAPI_PCI_DIR, devName, idName);

    FILE* file = fopen(fileName, "r");
    if (file)
    {
        char str[16] = "";
        if (fgets(str, sizeof(str), file))
            id = strtol(str, NULL, 16);
        fclose(file);
    }
    return (unsigned)id;
}

static int findAdapter(unsigned desiredVendorId)
{
    int adapterIndex = -1;
    int numAdapters = 0;

    Directory dir(VAAPI_PCI_DIR);

    for (int i = 0;  i < dir.count();  ++i)
    {
        const char* name = dir[i]->d_name;

        unsigned classId = readId(name, "class");
        if ((classId >> 16) == VAAPI_PCI_DISPLAY_CONTROLLER_CLASS)
        {
            unsigned vendorId = readId(name, "vendor");
            if (vendorId == desiredVendorId)
            {
                adapterIndex = numAdapters;
                break;
            }
            ++numAdapters;
        }
    }

    return adapterIndex;
}

class NodeInfo
{
    enum { NUM_NODES = 2 };
public:
    NodeInfo(int adapterIndex)
        {
            const char* names[NUM_NODES] = { "renderD", "card" };
            int numbers[NUM_NODES];
            numbers[0] = adapterIndex+128;
            numbers[1] = adapterIndex;
            for (int i = 0;  i < NUM_NODES;  ++i)
            {
                int sz = sizeof(VAAPI_DRI_DIR) + strlen(names[i]) + 3;
                paths[i] = new char [sz];
                snprintf(paths[i], sz, "%s%s%d", VAAPI_DRI_DIR, names[i], numbers[i]);
            }
        }
    ~NodeInfo()
        {
            for (int i = 0;  i < NUM_NODES;  ++i)
            {
                delete paths[i];
                paths[i] = 0;
            }
        }
    int count() const
        {
            return NUM_NODES;
        }
    const char* path(int index) const
        {
            return ((index >= 0) && (index < NUM_NODES)) ? paths[index] : 0;
        }
private:
    char* paths[NUM_NODES];
};

static void initVAdrm()
{
    const unsigned IntelVendorID = 0x8086;

    vaFDdrm = -1;
    vaDisplay = 0;

    int adapterIndex = findAdapter(IntelVendorID);
    if (adapterIndex >= 0)
    {
        NodeInfo nodes(adapterIndex);

        for (int i = 0;  i < nodes.count();  ++i)
        {
            vaFDdrm = open(nodes.path(i), O_RDWR);
            if (vaFDdrm >= 0)
            {
                vaDisplay = vaGetDisplayDRM(vaFDdrm);
                if (vaDisplay)
                {
                    int majorVersion = 0, minorVersion = 0;
                    if (vaInitialize(vaDisplay, &majorVersion, &minorVersion) == VA_STATUS_SUCCESS)
                        return;
                    vaDisplay = 0;
                }
                close(vaFDdrm);
                vaFDdrm = -1;
            }
        }
    }

    if (adapterIndex < 0)
        CV_Error(cv::Error::OpenCLInitError, "OpenCL: Can't find Intel display adapter for VA-API interop");
    if ((vaFDdrm < 0) || !vaDisplay)
        CV_Error(cv::Error::OpenCLInitError, "OpenCL: Can't load VA display for VA-API interop");
}

static void doneVA()
{
    if (vaDisplay)
        vaTerminate(vaDisplay);
    if (vaFDdrm >= 0)
        close(vaFDdrm);
    vaDisplay = 0;
    vaFDdrm = -1;
}

clGetDeviceIDsFromVA_APIMediaAdapterINTEL_fn clGetDeviceIDsFromVA_APIMediaAdapterINTEL = NULL;
clCreateFromVA_APIMediaSurfaceINTEL_fn       clCreateFromVA_APIMediaSurfaceINTEL       = NULL;
clEnqueueAcquireVA_APIMediaSurfacesINTEL_fn  clEnqueueAcquireVA_APIMediaSurfacesINTEL  = NULL;
clEnqueueReleaseVA_APIMediaSurfacesINTEL_fn  clEnqueueReleaseVA_APIMediaSurfacesINTEL  = NULL;

static void __OpenCLinitializeVA()
{
    using namespace cv::ocl;
    static cl_platform_id initializedPlatform = NULL;
    cl_platform_id platform = (cl_platform_id)Platform::getDefault().ptr();
    if (initializedPlatform != platform)
    {
        clGetDeviceIDsFromVA_APIMediaAdapterINTEL = (clGetDeviceIDsFromVA_APIMediaAdapterINTEL_fn)
            clGetExtensionFunctionAddressForPlatform(platform, "clGetDeviceIDsFromVA_APIMediaAdapterINTEL");
        clCreateFromVA_APIMediaSurfaceINTEL = (clCreateFromVA_APIMediaSurfaceINTEL_fn)
            clGetExtensionFunctionAddressForPlatform(platform, "clCreateFromVA_APIMediaSurfaceINTEL");
        clEnqueueAcquireVA_APIMediaSurfacesINTEL = (clEnqueueAcquireVA_APIMediaSurfacesINTEL_fn)
            clGetExtensionFunctionAddressForPlatform(platform, "clEnqueueAcquireVA_APIMediaSurfacesINTEL");
        clEnqueueReleaseVA_APIMediaSurfacesINTEL = (clEnqueueReleaseVA_APIMediaSurfacesINTEL_fn)
            clGetExtensionFunctionAddressForPlatform(platform, "clEnqueueReleaseVA_APIMediaSurfacesINTEL");
        initializedPlatform = platform;
    }
    if (!clGetDeviceIDsFromVA_APIMediaAdapterINTEL ||
        !clCreateFromVA_APIMediaSurfaceINTEL ||
        !clEnqueueAcquireVA_APIMediaSurfacesINTEL ||
        !clEnqueueReleaseVA_APIMediaSurfacesINTEL)
    {
        CV_Error(cv::Error::OpenCLInitError, "OpenCL: Can't find functions for CL-VA Interop");
    }
}
#endif // HAVE_VAAPI && HAVE_OPENCL

namespace ocl {

Context& initializeContextFromVA()
{
#if !defined(HAVE_VAAPI)
    NO_VAAPI_SUPPORT_ERROR;
#elif !defined(HAVE_OPENCL)
    NO_OPENCL_SUPPORT_ERROR;
#else
    __OpenCLinitializeVA();

    Context& ctx = Context::getDefault(false);
//    initializeContextFromHandle(ctx, platforms[found], context, device);
    return ctx;
#endif
}

} // namespace cv::vaapi::ocl

void convertToVASurface(InputArray src, VASurface* surface)
{
    (void)src; (void)surface;
#if !defined(HAVE_VAAPI)
    NO_VAAPI_SUPPORT_ERROR;
#elif !defined(HAVE_OPENCL)
    NO_OPENCL_SUPPORT_ERROR;
#else
    __OpenCLinitializeVA();
#endif
}

void convertFromVASurface(VASurface* surface, OutputArray dst)
{
    (void)surface; (void)dst;
#if !defined(HAVE_VAAPI)
    NO_VAAPI_SUPPORT_ERROR;
#elif !defined(HAVE_OPENCL)
    NO_OPENCL_SUPPORT_ERROR;
#else
    __OpenCLinitializeVA();
#endif
}

}} // namespace cv::vaapi
