/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "vp8_opencl.h"

#define CLAMP(x,min,max) if (x < min) x = min; else if ( x > max ) x = max;

//External functions that are fallbacks if CL is unavailable
void vp8_short_idct4x4llm_c(short *input, short *output, int pitch);
void vp8_short_idct4x4llm_1_c(short *input, short *output, int pitch);
void vp8_dc_only_idct_add_c(short input_dc, unsigned char *pred_ptr, unsigned char *dst_ptr, int pitch, int stride);
void vp8_short_inv_walsh4x4_c(short *input, short *output);
void vp8_short_inv_walsh4x4_1_c(short *input, short *output);

const char *idctCompileOptions = "-Ivp8/common/opencl";
const char *idctllm_cl_file_name = "vp8/common/opencl/idctllm_cl.cl";
