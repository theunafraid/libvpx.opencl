/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include <stdlib.h>

#include <CL/cl.h>

#define BLOCK_HEIGHT_WIDTH 4
#define VP8_FILTER_WEIGHT 128
#define VP8_FILTER_SHIFT  7

#define REGISTER_FILTER 0
#define CLAMP(x,min,max) if (x < min) x = min; else if ( x > max ) x = max;
#define PRE_CALC_PIXEL_STEPS 0
#define PRE_CALC_SRC_INCREMENT 0

#if PRE_CALC_PIXEL_STEPS
#define PS2 two_pixel_steps
#define PS3 three_pixel_steps
#else
#define PS2 2*(int)pixel_step
#define PS3 3*(int)pixel_step
#endif

#if REGISTER_FILTER
#define FILTER0 filter0
#define FILTER1 filter1
#define FILTER2 filter2
#define FILTER3 filter3
#define FILTER4 filter4
#define FILTER5 filter5
#else
#define FILTER0 vp8_filter[0]
#define FILTER1 vp8_filter[1]
#define FILTER2 vp8_filter[2]
#define FILTER3 vp8_filter[3]
#define FILTER4 vp8_filter[4]
#define FILTER5 vp8_filter[5]
#endif


#if PRE_CALC_SRC_INCREMENT
#define SRC_INCREMENT src_increment
#else
#define SRC_INCREMENT (src_pixels_per_line - output_width)
#endif

static const int bilinear_filters[8][2] =
{
    { 128,   0 },
    { 112,  16 },
    {  96,  32 },
    {  80,  48 },
    {  64,  64 },
    {  48,  80 },
    {  32,  96 },
    {  16, 112 }
};


static const short sub_pel_filters[8][6] =
{

    { 0,  0,  128,    0,   0,  0 },         /* note that 1/8 pel positions are just as per alpha -0.5 bicubic */
    { 0, -6,  123,   12,  -1,  0 },
    { 2, -11, 108,   36,  -8,  1 },         /* New 1/4 pel 6 tap filter */
    { 0, -9,   93,   50,  -6,  0 },
    { 3, -16,  77,   77, -16,  3 },         /* New 1/2 pel 6 tap filter */
    { 0, -6,   50,   93,  -9,  0 },
    { 1, -8,   36,  108, -11,  2 },         /* New 1/4 pel 6 tap filter */
    { 0, -1,   12,  123,  -6,  0 },



};



void vp8_filter_block2d_first_pass_cl
(
    unsigned char *src_ptr,
    int *output_ptr,
    unsigned int src_pixels_per_line,
    unsigned int pixel_step,
    unsigned int output_height,
    unsigned int output_width,
    const short *vp8_filter
)
{

    unsigned int src_offset,out_offset,i;
    int Temp;

#if REGISTER_FILTER
    short filter0 = vp8_filter[0];
    short filter1 = vp8_filter[1];
    short filter2 = vp8_filter[2];
    short filter3 = vp8_filter[3];
    short filter4 = vp8_filter[4];
    short filter5 = vp8_filter[5];
#endif

#if PRE_CALC_PIXEL_STEPS
    int two_pixel_steps = 2*(int)pixel_step;
    int three_pixel_steps = 3*(int)pixel_step;//two_pixel_steps + (int)pixel_step;
#endif

#if PRE_CALC_SRC_INCREMENT
    unsigned int src_increment = src_pixels_per_line - output_width;
#endif
    for (i = 0; i < output_height*output_width; i++){
		out_offset = src_offset = i/output_width;
		out_offset = (i - out_offset*output_width) + (out_offset * output_width);
		src_offset = i + (src_offset * SRC_INCREMENT);
        Temp = ((int)src_ptr[src_offset - PS2]         * FILTER0) +
           ((int)src_ptr[src_offset - (int)pixel_step] * FILTER1) +
           ((int)src_ptr[src_offset]                * FILTER2) +
           ((int)src_ptr[src_offset + pixel_step]       * FILTER3) +
           ((int)src_ptr[src_offset + PS2]              * FILTER4) +
           ((int)src_ptr[src_offset + PS3]              * FILTER5) +
           (VP8_FILTER_WEIGHT >> 1);      /* Rounding */

        /* Normalize back to 0-255 */
        Temp = Temp >> VP8_FILTER_SHIFT;
        CLAMP(Temp, 0, 255);

        output_ptr[out_offset] = Temp;
    }
}

void vp8_filter_block2d_second_pass_cl
(
    int *src_ptr,
    unsigned char *output_ptr,
    int output_pitch,
    unsigned int src_pixels_per_line,
    unsigned int pixel_step,
    unsigned int output_height,
    unsigned int output_width,
    const short *vp8_filter
)
{
	unsigned int i;
	int out_offset,src_offset;

	int  Temp;

#if REGISTER_FILTER
    short filter0 = vp8_filter[0];
    short filter1 = vp8_filter[1];
    short filter2 = vp8_filter[2];
    short filter3 = vp8_filter[3];
    short filter4 = vp8_filter[4];
    short filter5 = vp8_filter[5];
#endif

#if PRE_CALC_PIXEL_STEPS
    int two_pixel_steps = ((int)pixel_step) << 1;
    int three_pixel_steps = two_pixel_steps + (int)pixel_step;
#endif

#if PRE_CALC_SRC_INCREMENT
    unsigned int src_increment = src_pixels_per_line - output_width;
#endif
	for (i = 0; i < output_height * output_width; i++){
		out_offset = src_offset = i/output_width;
		out_offset = (i - out_offset*output_width) + (out_offset * output_pitch);
		src_offset = i + (src_offset * SRC_INCREMENT);
        /* Apply filter */
        Temp = ((int)src_ptr[src_offset - PS2] * FILTER0) +
               ((int)src_ptr[src_offset - (int)pixel_step] * FILTER1) +
               ((int)src_ptr[src_offset]                  * FILTER2) +
               ((int)src_ptr[src_offset + pixel_step]         * FILTER3) +
               ((int)src_ptr[src_offset + PS2]       * FILTER4) +
               ((int)src_ptr[src_offset + PS3]       * FILTER5) +
               (VP8_FILTER_WEIGHT >> 1);   /* Rounding */

		/* Normalize back to 0-255 */
		Temp = Temp >> VP8_FILTER_SHIFT;
		CLAMP(Temp, 0, 255);

        output_ptr[out_offset] = (unsigned char)Temp;
    }
}


void vp8_filter_block2d_cl
(
    unsigned char  *src_ptr,
    unsigned char  *output_ptr,
    unsigned int src_pixels_per_line,
    int output_pitch,
    const short  *HFilter,
    const short  *VFilter
)
{
    int FData[9*4]; /* Temp data buffer used in filtering */

    /* First filter 1-D horizontally... */
    vp8_filter_block2d_first_pass_cl(src_ptr - (2 * src_pixels_per_line), FData, src_pixels_per_line, 1, 9, 4, HFilter);

    /* then filter vertically... */
    vp8_filter_block2d_second_pass_cl(FData + 8, output_ptr, output_pitch, 4, 4, 4, 4, VFilter);
}


void vp8_block_variation_cl
(
    unsigned char  *src_ptr,
    int   src_pixels_per_line,
    int *HVar,
    int *VVar
)
{
    int i, j;
    unsigned char *Ptr = src_ptr;

    for (i = 0; i < 4; i++)
    {
        for (j = 0; j < 4; j++)
        {
            *HVar += abs((int)Ptr[j] - (int)Ptr[j+1]);
            *VVar += abs((int)Ptr[j] - (int)Ptr[j+src_pixels_per_line]);
        }

        Ptr += src_pixels_per_line;
    }
}


void vp8_sixtap_predict_cl
(
    unsigned char  *src_ptr,
    int   src_pixels_per_line,
    int  xoffset,
    int  yoffset,
    unsigned char *dst_ptr,
    int dst_pitch
)
{
    const short  *HFilter;
    const short  *VFilter;

    HFilter = sub_pel_filters[xoffset];   /* 6 tap */
    VFilter = sub_pel_filters[yoffset];   /* 6 tap */

    vp8_filter_block2d_cl(src_ptr, dst_ptr, src_pixels_per_line, dst_pitch, HFilter, VFilter);
}

void vp8_sixtap_predict8x8_cl
(
    unsigned char  *src_ptr,
    int  src_pixels_per_line,
    int  xoffset,
    int  yoffset,
    unsigned char *dst_ptr,
    int  dst_pitch
)
{
    const short  *HFilter;
    const short  *VFilter;
    int FData[13*16];   /* Temp data buffer used in filtering */

    HFilter = sub_pel_filters[xoffset];   /* 6 tap */
    VFilter = sub_pel_filters[yoffset];   /* 6 tap */

    /* First filter 1-D horizontally... */
    vp8_filter_block2d_first_pass_cl(src_ptr - (2 * src_pixels_per_line), FData, src_pixels_per_line, 1, 13, 8, HFilter);


    /* then filter vertically... */
    vp8_filter_block2d_second_pass_cl(FData + 16, dst_ptr, dst_pitch, 8, 8, 8, 8, VFilter);

}

void vp8_sixtap_predict8x4_cl
(
    unsigned char  *src_ptr,
    int  src_pixels_per_line,
    int  xoffset,
    int  yoffset,
    unsigned char *dst_ptr,
    int  dst_pitch
)
{
    const short  *HFilter;
    const short  *VFilter;
    int FData[13*16];   /* Temp data buffer used in filtering */

    HFilter = sub_pel_filters[xoffset];   /* 6 tap */
    VFilter = sub_pel_filters[yoffset];   /* 6 tap */

    /* First filter 1-D horizontally... */
    vp8_filter_block2d_first_pass_cl(src_ptr - (2 * src_pixels_per_line), FData, src_pixels_per_line, 1, 9, 8, HFilter);


    /* then filter vertically... */
    vp8_filter_block2d_second_pass_cl(FData + 16, dst_ptr, dst_pitch, 8, 8, 4, 8, VFilter);

}

void vp8_sixtap_predict16x16_cl
(
    unsigned char  *src_ptr,
    int  src_pixels_per_line,
    int  xoffset,
    int  yoffset,
    unsigned char *dst_ptr,
    int  dst_pitch
)
{
    const short  *HFilter;
    const short  *VFilter;
    int FData[21*24];   /* Temp data buffer used in filtering */


    HFilter = sub_pel_filters[xoffset];   /* 6 tap */
    VFilter = sub_pel_filters[yoffset];   /* 6 tap */

    /* First filter 1-D horizontally... */
    vp8_filter_block2d_first_pass_cl(src_ptr - (2 * src_pixels_per_line), FData, src_pixels_per_line, 1, 21, 16, HFilter);

    /* then filter vertically... */
    vp8_filter_block2d_second_pass_cl(FData + 32, dst_ptr, dst_pitch, 16, 16, 16, 16, VFilter);

}


/****************************************************************************
 *
 *  ROUTINE       : filter_block2d_bil_first_pass
 *
 *  INPUTS        : UINT8  *src_ptr          : Pointer to source block.
 *                  UINT32 src_pixels_per_line : Stride of input block.
 *                  UINT32 pixel_step        : Offset between filter input samples (see notes).
 *                  UINT32 output_height     : Input block height.
 *                  UINT32 output_width      : Input block width.
 *                  INT32  *vp8_filter          : Array of 2 bi-linear filter taps.
 *
 *  OUTPUTS       : INT32 *output_ptr        : Pointer to filtered block.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Applies a 1-D 2-tap bi-linear filter to the source block in
 *                  either horizontal or vertical direction to produce the
 *                  filtered output block. Used to implement first-pass
 *                  of 2-D separable filter.
 *
 *  SPECIAL NOTES : Produces INT32 output to retain precision for next pass.
 *                  Two filter taps should sum to VP8_FILTER_WEIGHT.
 *                  pixel_step defines whether the filter is applied
 *                  horizontally (pixel_step=1) or vertically (pixel_step=stride).
 *                  It defines the offset required to move from one input
 *                  to the next.
 *
 ****************************************************************************/
void vp8_filter_block2d_bil_first_pass_cl
(
    unsigned char *src_ptr,
    unsigned short *output_ptr,
    unsigned int src_pixels_per_line,
    int pixel_step,
    unsigned int output_height,
    unsigned int output_width,
    const int *vp8_filter
)
{
    unsigned int i, j;

    for (i = 0; i < output_height; i++)
    {
        for (j = 0; j < output_width; j++)
        {
            /* Apply bilinear filter */
            output_ptr[j] = (((int)src_ptr[0]          * vp8_filter[0]) +
                             ((int)src_ptr[pixel_step] * vp8_filter[1]) +
                             (VP8_FILTER_WEIGHT / 2)) >> VP8_FILTER_SHIFT;
            src_ptr++;
        }

        /* Next row... */
        src_ptr    += src_pixels_per_line - output_width;
        output_ptr += output_width;
    }
}

/****************************************************************************
 *
 *  ROUTINE       : filter_block2d_bil_second_pass
 *
 *  INPUTS        : INT32  *src_ptr          : Pointer to source block.
 *                  UINT32 src_pixels_per_line : Stride of input block.
 *                  UINT32 pixel_step        : Offset between filter input samples (see notes).
 *                  UINT32 output_height     : Input block height.
 *                  UINT32 output_width      : Input block width.
 *                  INT32  *vp8_filter          : Array of 2 bi-linear filter taps.
 *
 *  OUTPUTS       : UINT16 *output_ptr       : Pointer to filtered block.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : Applies a 1-D 2-tap bi-linear filter to the source block in
 *                  either horizontal or vertical direction to produce the
 *                  filtered output block. Used to implement second-pass
 *                  of 2-D separable filter.
 *
 *  SPECIAL NOTES : Requires 32-bit input as produced by filter_block2d_bil_first_pass.
 *                  Two filter taps should sum to VP8_FILTER_WEIGHT.
 *                  pixel_step defines whether the filter is applied
 *                  horizontally (pixel_step=1) or vertically (pixel_step=stride).
 *                  It defines the offset required to move from one input
 *                  to the next.
 *
 ****************************************************************************/
void vp8_filter_block2d_bil_second_pass_cl
(
    unsigned short *src_ptr,
    unsigned char  *output_ptr,
    int output_pitch,
    unsigned int  src_pixels_per_line,
    unsigned int  pixel_step,
    unsigned int  output_height,
    unsigned int  output_width,
    const int *vp8_filter
)
{
    unsigned int  i, j;
    int  Temp;

    for (i = 0; i < output_height; i++)
    {
        for (j = 0; j < output_width; j++)
        {
            /* Apply filter */
            Temp = ((int)src_ptr[0]         * vp8_filter[0]) +
                   ((int)src_ptr[pixel_step] * vp8_filter[1]) +
                   (VP8_FILTER_WEIGHT / 2);
            output_ptr[j] = (unsigned int)(Temp >> VP8_FILTER_SHIFT);
            src_ptr++;
        }

        /* Next row... */
        src_ptr    += src_pixels_per_line - output_width;
        output_ptr += output_pitch;
    }
}


/****************************************************************************
 *
 *  ROUTINE       : filter_block2d_bil
 *
 *  INPUTS        : UINT8  *src_ptr          : Pointer to source block.
 *                  UINT32 src_pixels_per_line : Stride of input block.
 *                  INT32  *HFilter         : Array of 2 horizontal filter taps.
 *                  INT32  *VFilter         : Array of 2 vertical filter taps.
 *
 *  OUTPUTS       : UINT16 *output_ptr       : Pointer to filtered block.
 *
 *  RETURNS       : void
 *
 *  FUNCTION      : 2-D filters an input block by applying a 2-tap
 *                  bi-linear filter horizontally followed by a 2-tap
 *                  bi-linear filter vertically on the result.
 *
 *  SPECIAL NOTES : The largest block size can be handled here is 16x16
 *
 ****************************************************************************/
void vp8_filter_block2d_bil_cl
(
    unsigned char *src_ptr,
    unsigned char *output_ptr,
    unsigned int   src_pixels_per_line,
    unsigned int   dst_pitch,
    const int      *HFilter,
    const int      *VFilter,
    int            Width,
    int            Height
)
{

    unsigned short FData[17*16];    /* Temp data buffer used in filtering */

    /* First filter 1-D horizontally... */
    vp8_filter_block2d_bil_first_pass_cl(src_ptr, FData, src_pixels_per_line, 1, Height + 1, Width, HFilter);

    /* then 1-D vertically... */
    vp8_filter_block2d_bil_second_pass_cl(FData, output_ptr, dst_pitch, Width, Width, Height, Width, VFilter);
}


void vp8_bilinear_predict4x4_cl
(
    unsigned char  *src_ptr,
    int   src_pixels_per_line,
    int  xoffset,
    int  yoffset,
    unsigned char *dst_ptr,
    int dst_pitch
)
{
    const int  *HFilter;
    const int  *VFilter;

    HFilter = bilinear_filters[xoffset];
    VFilter = bilinear_filters[yoffset];

    vp8_filter_block2d_bil_cl(src_ptr, dst_ptr, src_pixels_per_line, dst_pitch, HFilter, VFilter, 4, 4);

}

void vp8_bilinear_predict8x8_cl
(
    unsigned char  *src_ptr,
    int  src_pixels_per_line,
    int  xoffset,
    int  yoffset,
    unsigned char *dst_ptr,
    int  dst_pitch
)
{
    const int  *HFilter;
    const int  *VFilter;

    HFilter = bilinear_filters[xoffset];
    VFilter = bilinear_filters[yoffset];

    vp8_filter_block2d_bil_cl(src_ptr, dst_ptr, src_pixels_per_line, dst_pitch, HFilter, VFilter, 8, 8);

}

void vp8_bilinear_predict8x4_cl
(
    unsigned char  *src_ptr,
    int  src_pixels_per_line,
    int  xoffset,
    int  yoffset,
    unsigned char *dst_ptr,
    int  dst_pitch
)
{
    const int  *HFilter;
    const int  *VFilter;

    HFilter = bilinear_filters[xoffset];
    VFilter = bilinear_filters[yoffset];

    vp8_filter_block2d_bil_cl(src_ptr, dst_ptr, src_pixels_per_line, dst_pitch, HFilter, VFilter, 8, 4);

}

void vp8_bilinear_predict16x16_cl
(
    unsigned char  *src_ptr,
    int  src_pixels_per_line,
    int  xoffset,
    int  yoffset,
    unsigned char *dst_ptr,
    int  dst_pitch
)
{
    const int  *HFilter;
    const int  *VFilter;

    HFilter = bilinear_filters[xoffset];
    VFilter = bilinear_filters[yoffset];

    vp8_filter_block2d_bil_cl(src_ptr, dst_ptr, src_pixels_per_line, dst_pitch, HFilter, VFilter, 16, 16);
}
