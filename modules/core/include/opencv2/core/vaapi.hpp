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

#ifndef __OPENCV_CORE_VAAPI_HPP__
#define __OPENCV_CORE_VAAPI_HPP__

#ifndef __cplusplus
#  error vaapi.hpp header must be compiled as C++
#endif

#include "opencv2/core.hpp"
#include "ocl.hpp"

#if defined(HAVE_VAAPI)
# include "va/va.h"
#endif // HAVE_VAAPI

namespace cv { namespace vaapi {

#if !defined(_VA_H_)
typedef void* VADisplay;
typedef unsigned int VASurfaceID;
#endif // !_VA_H_

/** @addtogroup core_vaapi
This section describes CL-VA (VA-API) interoperability.

To enable CL-VA interoperability support, configure OpenCV using CMake with WITH_VAAPI=ON . Currently VA-API is
supported on Linux only. You should also install Intel Media Server Studio (MSS) to use this feature. You may
have to specify the path(s) to MSS components for cmake in environment variables: VAAPI_MSDK_ROOT for Media SDK
(default is "/opt/intel/mediasdk"), and VAAPI_IOCL_ROOT for Intel OpenCL (default is "/opt/intel/opencl").

To use VA-API interoperability you should first create VADisplay (libva), and then call initializeContextFromVA()
function to create OpenCL context and set up interoperability.
*/
//! @{

/////////////////// CL-VA Interoperability Functions ///////////////////

namespace ocl {
using namespace cv::ocl;

// TODO static functions in the Context class
/** @brief Creates OpenCL context from VA.
@param display - VADisplay for which CL interop should be established.
@return Returns reference to OpenCL Context
 */
CV_EXPORTS Context& initializeContextFromVA(VADisplay display);

} // namespace cv::vaapi::ocl

/** @brief Converts InputArray to VASurfaceID object.
@param src     - source InputArray.
@param surface - destination VASurfaceID object.
@param size    - size of image represented by VASurfaceID object.
 */
CV_EXPORTS void convertToVASurface(InputArray src, VASurfaceID surface, Size size);

/** @brief Converts VASurfaceID object to OutputArray.
@param surface - source VASurfaceID object.
@param size    - size of image represented by VASurfaceID object.
@param dst     - destination OutputArray.
 */
CV_EXPORTS void convertFromVASurface(VASurfaceID surface, Size size, OutputArray dst);

//! @}

}} // namespace cv::vaapi

#endif /* __OPENCV_CORE_VAAPI_HPP__ */
