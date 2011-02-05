/*
 *  Copyright (c) 2011 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

//for the decoder, all subpixel prediction is done in this file.
//
//Need to determine some sort of mechanism for easily determining SIXTAP/BILINEAR
//and what arguments to feed into the kernels. These kernels SHOULD be 2-pass,
//and ideally there'd be a data structure that determined what static arguments
//to pass in.
//
//Also, the only external functions being called here are the subpixel prediction
//functions. Hopefully this means no worrying about when to copy data back/forth.

#include "vpx_ports/config.h"
//#include "../recon.h"
#include "../subpixel.h"
//#include "../blockd.h"
//#include "../reconinter.h"
#if CONFIG_RUNTIME_CPU_DETECT
//#include "../onyxc_int.h"
#endif

#include "vp8_opencl.h"
#include "filter_cl.h"
#include "reconinter_cl.h"
#include "blockd_cl.h"

/* use this define on systems where unaligned int reads and writes are
 * not allowed, i.e. ARM architectures
 */
/*#define MUST_BE_ALIGNED*/


static const int bbb[4] = {0, 2, 8, 10};


//Copy 16 x 16-bytes from src to dst.
void vp8_copy_mem16x16_cl(
    unsigned char *src,
    int src_stride,
    unsigned char *dst,
    int dst_stride)
{

    int r;

	//Set this up as a 2D kernel. Each loop iteration is X, each byte/int within
	//is the Y address.

    for (r = 0; r < 16; r++)
    {
#ifdef MUST_BE_ALIGNED
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = src[3];
        dst[4] = src[4];
        dst[5] = src[5];
        dst[6] = src[6];
        dst[7] = src[7];
        dst[8] = src[8];
        dst[9] = src[9];
        dst[10] = src[10];
        dst[11] = src[11];
        dst[12] = src[12];
        dst[13] = src[13];
        dst[14] = src[14];
        dst[15] = src[15];

#else
        ((int *)dst)[0] = ((int *)src)[0] ;
        ((int *)dst)[1] = ((int *)src)[1] ;
        ((int *)dst)[2] = ((int *)src)[2] ;
        ((int *)dst)[3] = ((int *)src)[3] ;

#endif
        src += src_stride;
        dst += dst_stride;

    }

}

//Copy 8 x 8-bytes
void vp8_copy_mem8x8_cl(
    unsigned char *src,
    int src_stride,
    unsigned char *dst,
    int dst_stride)
{
    int r;

    for (r = 0; r < 8; r++)
    {
#ifdef MUST_BE_ALIGNED
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = src[3];
        dst[4] = src[4];
        dst[5] = src[5];
        dst[6] = src[6];
        dst[7] = src[7];
#else
        ((int *)dst)[0] = ((int *)src)[0] ;
        ((int *)dst)[1] = ((int *)src)[1] ;
#endif
        src += src_stride;
        dst += dst_stride;

    }

}

void vp8_copy_mem8x4_cl(
    unsigned char *src,
    int src_stride,
    unsigned char *dst,
    int dst_stride)
{
    int r;

    for (r = 0; r < 4; r++)
    {
#ifdef MUST_BE_ALIGNED
        dst[0] = src[0];
        dst[1] = src[1];
        dst[2] = src[2];
        dst[3] = src[3];
        dst[4] = src[4];
        dst[5] = src[5];
        dst[6] = src[6];
        dst[7] = src[7];
#else
        ((int *)dst)[0] = ((int *)src)[0] ;
        ((int *)dst)[1] = ((int *)src)[1] ;
#endif
        src += src_stride;
        dst += dst_stride;

    }

}



void vp8_build_inter_predictors_b_cl(BLOCKD *d, int pitch, vp8_subpix_fn_t sppf)
{
    int r;
    unsigned char *ptr_base;
    unsigned char *ptr;
    unsigned char *pred_ptr = d->predictor_base + d->predictor_offset;

    int ptr_offset = d->pre + (d->bmi.mv.as_mv.row >> 3) * d->pre_stride + (d->bmi.mv.as_mv.col >> 3);

    //d->base_pre is the start of the Macroblock's y_buffer, u_buffer, or v_buffer
    ptr_base = *(d->base_pre);
    ptr = ptr_base + ptr_offset;

    if (d->bmi.mv.as_mv.row & 7 || d->bmi.mv.as_mv.col & 7)
    {
        sppf(ptr, d->pre_stride, d->bmi.mv.as_mv.col & 7, d->bmi.mv.as_mv.row & 7, pred_ptr, pitch);
    }
    else
    {
#if CONFIG_OPENCL
        CL_FINISH(d->cl_commands)
#endif

        for (r = 0; r < 4; r++)
        {
#ifdef MUST_BE_ALIGNED
            pred_ptr[0]  = ptr[0];
            pred_ptr[1]  = ptr[1];
            pred_ptr[2]  = ptr[2];
            pred_ptr[3]  = ptr[3];
#else
            *(int *)pred_ptr = *(int *)ptr ;
#endif
            pred_ptr     += pitch;
            ptr         += d->pre_stride;
        }
    }

	//copy back cl_predictor_mem to b->predictor_base
}

void vp8_build_inter_predictors4b_cl(MACROBLOCKD *x, BLOCKD *d, int pitch)
{
    unsigned char *ptr_base;
    unsigned char *ptr;
    unsigned char *pred_ptr = d->predictor_base + d->predictor_offset;

    ptr_base = *(d->base_pre);
    ptr = ptr_base + d->pre + (d->bmi.mv.as_mv.row >> 3) * d->pre_stride + (d->bmi.mv.as_mv.col >> 3);

    if (d->bmi.mv.as_mv.row & 7 || d->bmi.mv.as_mv.col & 7)
    {
#if CONFIG_OPENCL
            if (cl_initialized == CL_SUCCESS && x->sixtap_filter == CL_TRUE)
                vp8_sixtap_predict8x8_cl(x, ptr, d->pre_stride, d->bmi.mv.as_mv.col & 7, d->bmi.mv.as_mv.row & 7, pred_ptr, pitch);
            else
#endif
        x->subpixel_predict8x8(ptr, d->pre_stride, d->bmi.mv.as_mv.col & 7, d->bmi.mv.as_mv.row & 7, pred_ptr, pitch);
    }
    else
    {
#if CONFIG_OPENCL
        CL_FINISH(d->cl_commands)
#endif
        vp8_copy_mem8x8_cl(ptr, d->pre_stride, pred_ptr, pitch);
    }
}

void vp8_build_inter_predictors2b_cl(MACROBLOCKD *x, BLOCKD *d, int pitch)
{
    unsigned char *ptr_base;
    unsigned char *ptr;
    unsigned char *pred_ptr = d->predictor_base + d->predictor_offset;

    ptr_base = *(d->base_pre);
    ptr = ptr_base + d->pre + (d->bmi.mv.as_mv.row >> 3) * d->pre_stride + (d->bmi.mv.as_mv.col >> 3);

    if (d->bmi.mv.as_mv.row & 7 || d->bmi.mv.as_mv.col & 7)
    {
        x->subpixel_predict8x4(ptr, d->pre_stride, d->bmi.mv.as_mv.col & 7, d->bmi.mv.as_mv.row & 7, pred_ptr, pitch);
    }
    else
    {
#if CONFIG_OPENCL
        CL_FINISH(d->cl_commands)
#endif
        vp8_copy_mem8x4_cl(ptr, d->pre_stride, pred_ptr, pitch);
    }
}


void vp8_build_inter_predictors_mbuv_cl(MACROBLOCKD *x)
{
    int i;

    if (x->mode_info_context->mbmi.ref_frame != INTRA_FRAME &&
        x->mode_info_context->mbmi.mode != SPLITMV)
    {
        unsigned char *uptr, *vptr;
        unsigned char *upred_ptr = &x->predictor[256];
        unsigned char *vpred_ptr = &x->predictor[320];

        int mv_row = x->block[16].bmi.mv.as_mv.row;
        int mv_col = x->block[16].bmi.mv.as_mv.col;
        int offset;
        int pre_stride = x->block[16].pre_stride;

        offset = (mv_row >> 3) * pre_stride + (mv_col >> 3);
        uptr = x->pre.u_buffer + offset;
        vptr = x->pre.v_buffer + offset;

        if ((mv_row | mv_col) & 7)
        {
#if CONFIG_OPENCL
            if (cl_initialized == CL_SUCCESS && x->sixtap_filter == CL_TRUE){
                vp8_sixtap_predict8x8_cl(x,uptr, pre_stride, mv_col & 7, mv_row & 7, upred_ptr, 8);
                vp8_sixtap_predict8x8_cl(x,vptr, pre_stride, mv_col & 7, mv_row & 7, vpred_ptr, 8);
            }
            else{
#endif
                x->subpixel_predict8x8(uptr, pre_stride, mv_col & 7, mv_row & 7, upred_ptr, 8);
                x->subpixel_predict8x8(vptr, pre_stride, mv_col & 7, mv_row & 7, vpred_ptr, 8);
#if CONFIG_OPENCL
            }
#endif

        }
        else
        {
#if CONFIG_OPENCL
        CL_FINISH(x->cl_commands)
#endif

            vp8_copy_mem8x8_cl(uptr, pre_stride, upred_ptr, 8);
            vp8_copy_mem8x8_cl(vptr, pre_stride, vpred_ptr, 8);
        }
    }
    else
    {
        for (i = 16; i < 24; i += 2)
        {
            BLOCKD *d0 = &x->block[i];
            BLOCKD *d1 = &x->block[i+1];

            if (d0->bmi.mv.as_int == d1->bmi.mv.as_int)
                vp8_build_inter_predictors2b_cl(x, d0, 8);
            else
            {
                vp8_build_inter_predictors_b_cl(d0, 8, x->subpixel_predict);
                vp8_build_inter_predictors_b_cl(d1, 8, x->subpixel_predict);
            }
        }
    }
#if CONFIG_OPENCL
    CL_FINISH(x->cl_commands)
#endif
}

void vp8_build_inter_predictors_mb_cl(MACROBLOCKD *x)
{

    if (x->mode_info_context->mbmi.ref_frame != INTRA_FRAME &&
        x->mode_info_context->mbmi.mode != SPLITMV)
    {
        int offset;
        unsigned char *ptr_base;
        unsigned char *ptr;
        unsigned char *uptr, *vptr;
        unsigned char *pred_ptr = x->predictor;
        unsigned char *upred_ptr = &x->predictor[256];
        unsigned char *vpred_ptr = &x->predictor[320];

        int mv_row = x->mode_info_context->mbmi.mv.as_mv.row;
        int mv_col = x->mode_info_context->mbmi.mv.as_mv.col;
        int pre_stride = x->block[0].pre_stride;

        ptr_base = x->pre.y_buffer;
        ptr = ptr_base + (mv_row >> 3) * pre_stride + (mv_col >> 3);

        if ((mv_row | mv_col) & 7)
        {
//#if CONFIG_OPENCL
//            if (cl_initialized == CL_SUCCESS && x->sixtap_filter == CL_TRUE)
//                vp8_sixtap_predict16x16_cl(x, ptr, pre_stride, mv_col & 7, mv_row & 7, pred_ptr, 16);
//            else
//#endif
            x->subpixel_predict16x16(ptr, pre_stride, mv_col & 7, mv_row & 7, pred_ptr, 16);
        }
        else
        {
#if CONFIG_OPENCL
            CL_FINISH(x->cl_commands);
#endif
            vp8_copy_mem16x16_cl(ptr, pre_stride, pred_ptr, 16);
        }

        mv_row = x->block[16].bmi.mv.as_mv.row;
        mv_col = x->block[16].bmi.mv.as_mv.col;
        pre_stride >>= 1;
        offset = (mv_row >> 3) * pre_stride + (mv_col >> 3);
        uptr = x->pre.u_buffer + offset;
        vptr = x->pre.v_buffer + offset;

        if ((mv_row | mv_col) & 7)
        {
#if CONFIG_OPENCL
            if (cl_initialized == CL_SUCCESS && x->sixtap_filter == CL_TRUE){
                vp8_sixtap_predict8x8_cl(x, uptr, pre_stride, mv_col & 7, mv_row & 7, upred_ptr, 8);
                vp8_sixtap_predict8x8_cl(x, vptr, pre_stride, mv_col & 7, mv_row & 7, vpred_ptr, 8);

            }
            else {
#endif
                x->subpixel_predict8x8(uptr, pre_stride, mv_col & 7, mv_row & 7, upred_ptr, 8);
                x->subpixel_predict8x8(vptr, pre_stride, mv_col & 7, mv_row & 7, vpred_ptr, 8);
#if CONFIG_OPENCL
            }
#endif
        }
        else
        {
#if CONFIG_OPENCL
            CL_FINISH(x->cl_commands);
#endif
            vp8_copy_mem8x8_cl(uptr, pre_stride, upred_ptr, 8);
            vp8_copy_mem8x8_cl(vptr, pre_stride, vpred_ptr, 8);
        }
    }
    else
    {
        int i;

        if (x->mode_info_context->mbmi.partitioning < 3)
        {
            for (i = 0; i < 4; i++)
            {
                BLOCKD *d = &x->block[bbb[i]];
                vp8_build_inter_predictors4b_cl(x, d, 16);
            }
        }
        else
        {
            for (i = 0; i < 16; i += 2)
            {
                BLOCKD *d0 = &x->block[i];
                BLOCKD *d1 = &x->block[i+1];

                if (d0->bmi.mv.as_int == d1->bmi.mv.as_int)
                    vp8_build_inter_predictors2b_cl(x, d0, 16);
                else
                {
                    vp8_build_inter_predictors_b_cl(d0, 16, x->subpixel_predict);
                    vp8_build_inter_predictors_b_cl(d1, 16, x->subpixel_predict);
                }

            }

        }

        for (i = 16; i < 24; i += 2)
        {
            BLOCKD *d0 = &x->block[i];
            BLOCKD *d1 = &x->block[i+1];

            if (d0->bmi.mv.as_int == d1->bmi.mv.as_int)
                vp8_build_inter_predictors2b_cl(x, d0, 8);
            else
            {
                vp8_build_inter_predictors_b_cl(d0, 8, x->subpixel_predict);
                vp8_build_inter_predictors_b_cl(d1, 8, x->subpixel_predict);
            }

        }

    }

#if CONFIG_OPENCL
    CL_FINISH(x->cl_commands)
#endif

}


/* The following functions are written for skip_recon_mb() to call. Since there is no recon in this
 * situation, we can write the result directly to dst buffer instead of writing it to predictor
 * buffer and then copying it to dst buffer.
 */

static void vp8_build_inter_predictors_b_s_cl(BLOCKD *d, unsigned char *dst_ptr, vp8_subpix_fn_t sppf)
{
    int r;
    unsigned char *ptr_base;
    unsigned char *ptr;
    /*unsigned char *pred_ptr = d->predictor_base + d->predictor_offset;*/
    int dst_stride = d->dst_stride;
    int pre_stride = d->pre_stride;
    int ptr_offset = d->pre + (d->bmi.mv.as_mv.row >> 3) * d->pre_stride + (d->bmi.mv.as_mv.col >> 3);

    ptr_base = *(d->base_pre);
    ptr = ptr_base + ptr_offset;

#if CONFIG_OPENCL
    CL_FINISH(d->cl_commands)
#endif

    if (d->bmi.mv.as_mv.row & 7 || d->bmi.mv.as_mv.col & 7)
    {
        sppf(ptr, pre_stride, d->bmi.mv.as_mv.col & 7, d->bmi.mv.as_mv.row & 7, dst_ptr, dst_stride);
    }
    else
    {
        for (r = 0; r < 4; r++)
        {
#ifdef MUST_BE_ALIGNED
            dst_ptr[0]   = ptr[0];
            dst_ptr[1]   = ptr[1];
            dst_ptr[2]   = ptr[2];
            dst_ptr[3]   = ptr[3];
#else
            *(int *)dst_ptr = *(int *)ptr ;
#endif
            dst_ptr      += dst_stride;
            ptr         += pre_stride;
        }
    }
}



void vp8_build_inter_predictors_mb_s_cl(MACROBLOCKD *x)
{
    /*unsigned char *pred_ptr = x->block[0].predictor_base + x->block[0].predictor_offset;
    unsigned char *dst_ptr = *(x->block[0].base_dst) + x->block[0].dst;*/
    unsigned char *pred_ptr = x->predictor;
    unsigned char *dst_ptr = x->dst.y_buffer;

    if (x->mode_info_context->mbmi.mode != SPLITMV)
    {
        int offset;
        unsigned char *ptr_base;
        unsigned char *ptr;
        unsigned char *uptr, *vptr;
        /*unsigned char *pred_ptr = x->predictor;
        unsigned char *upred_ptr = &x->predictor[256];
        unsigned char *vpred_ptr = &x->predictor[320];*/
        unsigned char *udst_ptr = x->dst.u_buffer;
        unsigned char *vdst_ptr = x->dst.v_buffer;

        int mv_row = x->mode_info_context->mbmi.mv.as_mv.row;
        int mv_col = x->mode_info_context->mbmi.mv.as_mv.col;
        int pre_stride = x->dst.y_stride; /*x->block[0].pre_stride;*/

        ptr_base = x->pre.y_buffer;
        ptr = ptr_base + (mv_row >> 3) * pre_stride + (mv_col >> 3);

        if ((mv_row | mv_col) & 7)
        {
#if CONFIG_OPENCL
            if (cl_initialized == CL_SUCCESS && x->sixtap_filter == CL_TRUE)
                vp8_sixtap_predict16x16_cl(x, ptr, pre_stride, mv_col & 7, mv_row & 7, dst_ptr, x->dst.y_stride);
            else
#endif
                x->subpixel_predict16x16(ptr, pre_stride, mv_col & 7, mv_row & 7, dst_ptr, x->dst.y_stride); /*x->block[0].dst_stride);*/
        }
        else
        {
#if CONFIG_OPENCL
        CL_FINISH(x->cl_commands)
#endif
            vp8_copy_mem16x16_cl(ptr, pre_stride, dst_ptr, x->dst.y_stride); /*x->block[0].dst_stride);*/
        }

        mv_row = x->block[16].bmi.mv.as_mv.row;
        mv_col = x->block[16].bmi.mv.as_mv.col;
        pre_stride >>= 1;
        offset = (mv_row >> 3) * pre_stride + (mv_col >> 3);
        uptr = x->pre.u_buffer + offset;
        vptr = x->pre.v_buffer + offset;

        if ((mv_row | mv_col) & 7)
        {
#if CONFIG_OPENCL
            if (cl_initialized == CL_SUCCESS && x->sixtap_filter == CL_TRUE){
                vp8_sixtap_predict8x8_cl(x, uptr, pre_stride, mv_col & 7, mv_row & 7, udst_ptr, x->dst.uv_stride);
                vp8_sixtap_predict8x8_cl(x, vptr, pre_stride, mv_col & 7, mv_row & 7, vdst_ptr, x->dst.uv_stride);
            } else {
#endif
                x->subpixel_predict8x8(uptr, pre_stride, mv_col & 7, mv_row & 7, udst_ptr, x->dst.uv_stride);
                x->subpixel_predict8x8(vptr, pre_stride, mv_col & 7, mv_row & 7, vdst_ptr, x->dst.uv_stride);
#if CONFIG_OPENCL
            }
#endif
        }
        else
        {
#if CONFIG_OPENCL
        CL_FINISH(x->cl_commands)
#endif
            vp8_copy_mem8x8_cl(uptr, pre_stride, udst_ptr, x->dst.uv_stride);
            vp8_copy_mem8x8_cl(vptr, pre_stride, vdst_ptr, x->dst.uv_stride);
        }
    }
    else
    {
        /* note: this whole ELSE part is not executed at all. So, no way to test the correctness of my modification. Later,
         * if sth is wrong, go back to what it is in build_inter_predictors_mb.
         *
         * ACW: note: Not sure who the above comment belongs to.
         */
        int i;

        if (x->mode_info_context->mbmi.partitioning < 3)
        {
            for (i = 0; i < 4; i++)
            {
                BLOCKD *d = &x->block[bbb[i]];
                /*vp8_build_inter_predictors4b(x, d, 16);*/

                {
                    unsigned char *ptr_base;
                    unsigned char *ptr;
                    unsigned char *pred_ptr = d->predictor_base + d->predictor_offset;

                    ptr_base = *(d->base_pre);
                    ptr = ptr_base + d->pre + (d->bmi.mv.as_mv.row >> 3) * d->pre_stride + (d->bmi.mv.as_mv.col >> 3);

                    if (d->bmi.mv.as_mv.row & 7 || d->bmi.mv.as_mv.col & 7)
                    {
                        x->subpixel_predict8x8(ptr, d->pre_stride, d->bmi.mv.as_mv.col & 7, d->bmi.mv.as_mv.row & 7, dst_ptr, x->dst.y_stride); /*x->block[0].dst_stride);*/
                    }
                    else
                    {
#if CONFIG_OPENCL
                        CL_FINISH(x->cl_commands)
#endif
                        vp8_copy_mem8x8_cl(ptr, d->pre_stride, dst_ptr, x->dst.y_stride); /*x->block[0].dst_stride);*/
                    }
                }
            }
        }
		else
        {
            for (i = 0; i < 16; i += 2)
            {
                BLOCKD *d0 = &x->block[i];
                BLOCKD *d1 = &x->block[i+1];

                if (d0->bmi.mv.as_int == d1->bmi.mv.as_int)
                {
                    /*vp8_build_inter_predictors2b(x, d0, 16);*/
                    unsigned char *ptr_base;
                    unsigned char *ptr;
                    unsigned char *pred_ptr = d0->predictor_base + d0->predictor_offset;

                    ptr_base = *(d0->base_pre);
                    ptr = ptr_base + d0->pre + (d0->bmi.mv.as_mv.row >> 3) * d0->pre_stride + (d0->bmi.mv.as_mv.col >> 3);

                    if (d0->bmi.mv.as_mv.row & 7 || d0->bmi.mv.as_mv.col & 7)
                    {
                        x->subpixel_predict8x4(ptr, d0->pre_stride, d0->bmi.mv.as_mv.col & 7, d0->bmi.mv.as_mv.row & 7, dst_ptr, x->dst.y_stride);
                    }
                    else
                    {
#if CONFIG_OPENCL
                        CL_FINISH(x->cl_commands)
#endif
                        vp8_copy_mem8x4_cl(ptr, d0->pre_stride, dst_ptr, x->dst.y_stride);
                    }
                }
                else
                {
                    vp8_build_inter_predictors_b_s_cl(d0, dst_ptr, x->subpixel_predict);
                    vp8_build_inter_predictors_b_s_cl(d1, dst_ptr, x->subpixel_predict);
                }
            }
        }

        for (i = 16; i < 24; i += 2)
        {
            BLOCKD *d0 = &x->block[i];
            BLOCKD *d1 = &x->block[i+1];

            if (d0->bmi.mv.as_int == d1->bmi.mv.as_int)
            {
                /*vp8_build_inter_predictors2b(x, d0, 8);*/
                unsigned char *ptr_base;
                unsigned char *ptr;
                unsigned char *pred_ptr = d0->predictor_base + d0->predictor_offset;

                ptr_base = *(d0->base_pre);
                ptr = ptr_base + d0->pre + (d0->bmi.mv.as_mv.row >> 3) * d0->pre_stride + (d0->bmi.mv.as_mv.col >> 3);

                if (d0->bmi.mv.as_mv.row & 7 || d0->bmi.mv.as_mv.col & 7)
                {
                    x->subpixel_predict8x4(ptr, d0->pre_stride,
                        d0->bmi.mv.as_mv.col & 7,
                        d0->bmi.mv.as_mv.row & 7,
                        dst_ptr, x->dst.uv_stride);
                }
                else
                {
#if CONFIG_OPENCL
                    CL_FINISH(x->cl_commands)
#endif
                    vp8_copy_mem8x4_cl(ptr,
                        d0->pre_stride, dst_ptr, x->dst.uv_stride);
                }
            }
            else
            {
                vp8_build_inter_predictors_b_s_cl(d0, dst_ptr, x->subpixel_predict);
                vp8_build_inter_predictors_b_s_cl(d1, dst_ptr, x->subpixel_predict);
            }
        }
    }
#if CONFIG_OPENCL
    CL_FINISH(x->cl_commands)
#endif
}