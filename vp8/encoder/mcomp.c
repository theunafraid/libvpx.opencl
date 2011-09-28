/*
 *  Copyright (c) 2010 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */


#include "mcomp.h"
#include "vpx_mem/vpx_mem.h"

#include <stdio.h>
#include <limits.h>
#include <math.h>

#ifdef ENTROPY_STATS
static int mv_ref_ct [31] [4] [2];
static int mv_mode_cts [4] [2];
#endif

int vp8_mv_bit_cost(int_mv *mv, int_mv *ref, int *mvcost[2], int Weight)
{
    // MV costing is based on the distribution of vectors in the previous frame and as such will tend to
    // over state the cost of vectors. In addition coding a new vector can have a knock on effect on the
    // cost of subsequent vectors and the quality of prediction from NEAR and NEAREST for subsequent blocks.
    // The "Weight" parameter allows, to a limited extent, for some account to be taken of these factors.
    return ((mvcost[0][(mv->as_mv.row - ref->as_mv.row) >> 1] + mvcost[1][(mv->as_mv.col - ref->as_mv.col) >> 1]) * Weight) >> 7;
}

static int mv_err_cost(int_mv *mv, int_mv *ref, int *mvcost[2], int error_per_bit)
{
    return ((mvcost[0][(mv->as_mv.row - ref->as_mv.row) >> 1] +
        mvcost[1][(mv->as_mv.col - ref->as_mv.col) >> 1])
        * error_per_bit + 128) >> 8;
}

static int mvsad_err_cost(int_mv *mv, int_mv *ref, int *mvsadcost[2], int error_per_bit)
{
    /* Calculate sad error cost on full pixel basis. */
    return ((mvsadcost[0][(mv->as_mv.row - ref->as_mv.row)] +
        mvsadcost[1][(mv->as_mv.col - ref->as_mv.col)])
        * error_per_bit + 128) >> 8;
}

void vp8_init_dsmotion_compensation(MACROBLOCK *x, int stride)
{
    int Len;
    int search_site_count = 0;


    // Generate offsets for 4 search sites per step.
    Len = MAX_FIRST_STEP;
    x->ss[search_site_count].mv.col = 0;
    x->ss[search_site_count].mv.row = 0;
    x->ss[search_site_count].offset = 0;
    search_site_count++;

    while (Len > 0)
    {

        // Compute offsets for search sites.
        x->ss[search_site_count].mv.col = 0;
        x->ss[search_site_count].mv.row = -Len;
        x->ss[search_site_count].offset = -Len * stride;
        search_site_count++;

        // Compute offsets for search sites.
        x->ss[search_site_count].mv.col = 0;
        x->ss[search_site_count].mv.row = Len;
        x->ss[search_site_count].offset = Len * stride;
        search_site_count++;

        // Compute offsets for search sites.
        x->ss[search_site_count].mv.col = -Len;
        x->ss[search_site_count].mv.row = 0;
        x->ss[search_site_count].offset = -Len;
        search_site_count++;

        // Compute offsets for search sites.
        x->ss[search_site_count].mv.col = Len;
        x->ss[search_site_count].mv.row = 0;
        x->ss[search_site_count].offset = Len;
        search_site_count++;

        // Contract.
        Len /= 2;
    }

    x->ss_count = search_site_count;
    x->searches_per_step = 4;
}

void vp8_init3smotion_compensation(MACROBLOCK *x, int stride)
{
    int Len;
    int search_site_count = 0;

    // Generate offsets for 8 search sites per step.
    Len = MAX_FIRST_STEP;
    x->ss[search_site_count].mv.col = 0;
    x->ss[search_site_count].mv.row = 0;
    x->ss[search_site_count].offset = 0;
    search_site_count++;

    while (Len > 0)
    {

        // Compute offsets for search sites.
        x->ss[search_site_count].mv.col = 0;
        x->ss[search_site_count].mv.row = -Len;
        x->ss[search_site_count].offset = -Len * stride;
        search_site_count++;

        // Compute offsets for search sites.
        x->ss[search_site_count].mv.col = 0;
        x->ss[search_site_count].mv.row = Len;
        x->ss[search_site_count].offset = Len * stride;
        search_site_count++;

        // Compute offsets for search sites.
        x->ss[search_site_count].mv.col = -Len;
        x->ss[search_site_count].mv.row = 0;
        x->ss[search_site_count].offset = -Len;
        search_site_count++;

        // Compute offsets for search sites.
        x->ss[search_site_count].mv.col = Len;
        x->ss[search_site_count].mv.row = 0;
        x->ss[search_site_count].offset = Len;
        search_site_count++;

        // Compute offsets for search sites.
        x->ss[search_site_count].mv.col = -Len;
        x->ss[search_site_count].mv.row = -Len;
        x->ss[search_site_count].offset = -Len * stride - Len;
        search_site_count++;

        // Compute offsets for search sites.
        x->ss[search_site_count].mv.col = Len;
        x->ss[search_site_count].mv.row = -Len;
        x->ss[search_site_count].offset = -Len * stride + Len;
        search_site_count++;

        // Compute offsets for search sites.
        x->ss[search_site_count].mv.col = -Len;
        x->ss[search_site_count].mv.row = Len;
        x->ss[search_site_count].offset = Len * stride - Len;
        search_site_count++;

        // Compute offsets for search sites.
        x->ss[search_site_count].mv.col = Len;
        x->ss[search_site_count].mv.row = Len;
        x->ss[search_site_count].offset = Len * stride + Len;
        search_site_count++;


        // Contract.
        Len /= 2;
    }

    x->ss_count = search_site_count;
    x->searches_per_step = 8;
}


#define MVC(r,c) (((mvcost[0][(r)-rr] + mvcost[1][(c) - rc]) * error_per_bit + 128 )>>8 ) // estimated cost of a motion vector (r,c)
#define PRE(r,c) (*(d->base_pre) + d->pre + ((r)>>2) * d->pre_stride + ((c)>>2)) // pointer to predictor base of a motionvector
#define SP(x) (((x)&3)<<1) // convert motion vector component to offset for svf calc
#define DIST(r,c) vfp->svf( PRE(r,c), d->pre_stride, SP(c),SP(r), z,b->src_stride,&sse) // returns subpixel variance error function.
#define IFMVCV(r,c,s,e) if ( c >= minc && c <= maxc && r >= minr && r <= maxr) s else e;
#define ERR(r,c) (MVC(r,c)+DIST(r,c)) // returns distortion + motion vector cost
#define CHECK_BETTER(v,r,c) IFMVCV(r,c,{thismse = DIST(r,c); if((v = (MVC(r,c)+thismse)) < besterr) { besterr = v; br=r; bc=c; *distortion = thismse; *sse1 = sse; }}, v=INT_MAX;)// checks if (r,c) has better score than previous best
#define MIN(x,y) (((x)<(y))?(x):(y))
#define MAX(x,y) (((x)>(y))?(x):(y))

//#define CHECK_BETTER(v,r,c) if((v = ERR(r,c)) < besterr) { besterr = v; br=r; bc=c; }

int vp8_find_best_sub_pixel_step_iteratively(MACROBLOCK *x, BLOCK *b, BLOCKD *d,
                                             int_mv *bestmv, int_mv *ref_mv,
                                             int error_per_bit,
                                             const vp8_variance_fn_ptr_t *vfp,
                                             int *mvcost[2], int *distortion,
                                             unsigned int *sse1)
{
    unsigned char *y = *(d->base_pre) + d->pre + (bestmv->as_mv.row) * d->pre_stride + bestmv->as_mv.col;
    unsigned char *z = (*(b->base_src) + b->src);

    int rr = ref_mv->as_mv.row >> 1, rc = ref_mv->as_mv.col >> 1;
    int br = bestmv->as_mv.row << 2, bc = bestmv->as_mv.col << 2;
    int tr = br, tc = bc;
    unsigned int besterr = INT_MAX;
    unsigned int left, right, up, down, diag;
    unsigned int sse;
    unsigned int whichdir;
    unsigned int halfiters = 4;
    unsigned int quarteriters = 4;
    int thismse;

    int minc = MAX(x->mv_col_min << 2, (ref_mv->as_mv.col >> 1) - ((1 << mvlong_width) - 1));
    int maxc = MIN(x->mv_col_max << 2, (ref_mv->as_mv.col >> 1) + ((1 << mvlong_width) - 1));
    int minr = MAX(x->mv_row_min << 2, (ref_mv->as_mv.row >> 1) - ((1 << mvlong_width) - 1));
    int maxr = MIN(x->mv_row_max << 2, (ref_mv->as_mv.row >> 1) + ((1 << mvlong_width) - 1));

    // central mv
    bestmv->as_mv.row <<= 3;
    bestmv->as_mv.col <<= 3;

    // calculate central point error
    besterr = vfp->vf(y, d->pre_stride, z, b->src_stride, sse1);
    *distortion = besterr;
    besterr += mv_err_cost(bestmv, ref_mv, mvcost, error_per_bit);

    // TODO: Each subsequent iteration checks at least one point in common with the last iteration could be 2 ( if diag selected)
    while (--halfiters)
    {
        // 1/2 pel
        CHECK_BETTER(left, tr, tc - 2);
        CHECK_BETTER(right, tr, tc + 2);
        CHECK_BETTER(up, tr - 2, tc);
        CHECK_BETTER(down, tr + 2, tc);

        whichdir = (left < right ? 0 : 1) + (up < down ? 0 : 2);

        switch (whichdir)
        {
        case 0:
            CHECK_BETTER(diag, tr - 2, tc - 2);
            break;
        case 1:
            CHECK_BETTER(diag, tr - 2, tc + 2);
            break;
        case 2:
            CHECK_BETTER(diag, tr + 2, tc - 2);
            break;
        case 3:
            CHECK_BETTER(diag, tr + 2, tc + 2);
            break;
        }

        // no reason to check the same one again.
        if (tr == br && tc == bc)
            break;

        tr = br;
        tc = bc;
    }

    // TODO: Each subsequent iteration checks at least one point in common with the last iteration could be 2 ( if diag selected)
    // 1/4 pel
    while (--quarteriters)
    {
        CHECK_BETTER(left, tr, tc - 1);
        CHECK_BETTER(right, tr, tc + 1);
        CHECK_BETTER(up, tr - 1, tc);
        CHECK_BETTER(down, tr + 1, tc);

        whichdir = (left < right ? 0 : 1) + (up < down ? 0 : 2);

        switch (whichdir)
        {
        case 0:
            CHECK_BETTER(diag, tr - 1, tc - 1);
            break;
        case 1:
            CHECK_BETTER(diag, tr - 1, tc + 1);
            break;
        case 2:
            CHECK_BETTER(diag, tr + 1, tc - 1);
            break;
        case 3:
            CHECK_BETTER(diag, tr + 1, tc + 1);
            break;
        }

        // no reason to check the same one again.
        if (tr == br && tc == bc)
            break;

        tr = br;
        tc = bc;
    }

    bestmv->as_mv.row = br << 1;
    bestmv->as_mv.col = bc << 1;

    if ((abs(bestmv->as_mv.col - ref_mv->as_mv.col) > MAX_FULL_PEL_VAL) ||
        (abs(bestmv->as_mv.row - ref_mv->as_mv.row) > MAX_FULL_PEL_VAL))
        return INT_MAX;

    return besterr;
}
#undef MVC
#undef PRE
#undef SP
#undef DIST
#undef ERR
#undef CHECK_BETTER
#undef MIN
#undef MAX
int vp8_find_best_sub_pixel_step(MACROBLOCK *x, BLOCK *b, BLOCKD *d,
                                 int_mv *bestmv, int_mv *ref_mv,
                                 int error_per_bit,
                                 const vp8_variance_fn_ptr_t *vfp,
                                 int *mvcost[2], int *distortion,
                                 unsigned int *sse1)
{
    int bestmse = INT_MAX;
    int_mv startmv;
    int_mv this_mv;
    unsigned char *y = *(d->base_pre) + d->pre + (bestmv->as_mv.row) * d->pre_stride + bestmv->as_mv.col;
    unsigned char *z = (*(b->base_src) + b->src);
    int left, right, up, down, diag;
    unsigned int sse;
    int whichdir ;
    int thismse;


    // Trap uncodable vectors
    if ((abs((bestmv->as_mv.col << 3) - ref_mv->as_mv.col) > MAX_FULL_PEL_VAL)
        || (abs((bestmv->as_mv.row << 3) - ref_mv->as_mv.row) > MAX_FULL_PEL_VAL))
    {
        bestmv->as_mv.row <<= 3;
        bestmv->as_mv.col <<= 3;
        *distortion = INT_MAX;
        return INT_MAX;
    }

    // central mv
    bestmv->as_mv.row <<= 3;
    bestmv->as_mv.col <<= 3;
    startmv = *bestmv;

    // calculate central point error
    bestmse = vfp->vf(y, d->pre_stride, z, b->src_stride, sse1);
    *distortion = bestmse;
    bestmse += mv_err_cost(bestmv, ref_mv, mvcost, error_per_bit);

    // go left then right and check error
    this_mv.as_mv.row = startmv.as_mv.row;
    this_mv.as_mv.col = ((startmv.as_mv.col - 8) | 4);
    thismse = vfp->svf_halfpix_h(y - 1, d->pre_stride, z, b->src_stride, &sse);
    left = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);

    if (left < bestmse)
    {
        *bestmv = this_mv;
        bestmse = left;
        *distortion = thismse;
        *sse1 = sse;
    }

    this_mv.as_mv.col += 8;
    thismse = vfp->svf_halfpix_h(y, d->pre_stride, z, b->src_stride, &sse);
    right = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);

    if (right < bestmse)
    {
        *bestmv = this_mv;
        bestmse = right;
        *distortion = thismse;
        *sse1 = sse;
    }

    // go up then down and check error
    this_mv.as_mv.col = startmv.as_mv.col;
    this_mv.as_mv.row = ((startmv.as_mv.row - 8) | 4);
    thismse =  vfp->svf_halfpix_v(y - d->pre_stride, d->pre_stride, z, b->src_stride, &sse);
    up = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);

    if (up < bestmse)
    {
        *bestmv = this_mv;
        bestmse = up;
        *distortion = thismse;
        *sse1 = sse;
    }

    this_mv.as_mv.row += 8;
    thismse = vfp->svf_halfpix_v(y, d->pre_stride, z, b->src_stride, &sse);
    down = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);

    if (down < bestmse)
    {
        *bestmv = this_mv;
        bestmse = down;
        *distortion = thismse;
        *sse1 = sse;
    }


    // now check 1 more diagonal
    whichdir = (left < right ? 0 : 1) + (up < down ? 0 : 2);
    //for(whichdir =0;whichdir<4;whichdir++)
    //{
    this_mv = startmv;

    switch (whichdir)
    {
    case 0:
        this_mv.as_mv.col = (this_mv.as_mv.col - 8) | 4;
        this_mv.as_mv.row = (this_mv.as_mv.row - 8) | 4;
        thismse = vfp->svf_halfpix_hv(y - 1 - d->pre_stride, d->pre_stride, z, b->src_stride, &sse);
        break;
    case 1:
        this_mv.as_mv.col += 4;
        this_mv.as_mv.row = (this_mv.as_mv.row - 8) | 4;
        thismse = vfp->svf_halfpix_hv(y - d->pre_stride, d->pre_stride, z, b->src_stride, &sse);
        break;
    case 2:
        this_mv.as_mv.col = (this_mv.as_mv.col - 8) | 4;
        this_mv.as_mv.row += 4;
        thismse = vfp->svf_halfpix_hv(y - 1, d->pre_stride, z, b->src_stride, &sse);
        break;
    case 3:
    default:
        this_mv.as_mv.col += 4;
        this_mv.as_mv.row += 4;
        thismse = vfp->svf_halfpix_hv(y, d->pre_stride, z, b->src_stride, &sse);
        break;
    }

    diag = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);

    if (diag < bestmse)
    {
        *bestmv = this_mv;
        bestmse = diag;
        *distortion = thismse;
        *sse1 = sse;
    }

//  }


    // time to check quarter pels.
    if (bestmv->as_mv.row < startmv.as_mv.row)
        y -= d->pre_stride;

    if (bestmv->as_mv.col < startmv.as_mv.col)
        y--;

    startmv = *bestmv;



    // go left then right and check error
    this_mv.as_mv.row = startmv.as_mv.row;

    if (startmv.as_mv.col & 7)
    {
        this_mv.as_mv.col = startmv.as_mv.col - 2;
        thismse = vfp->svf(y, d->pre_stride, this_mv.as_mv.col & 7, this_mv.as_mv.row & 7, z, b->src_stride, &sse);
    }
    else
    {
        this_mv.as_mv.col = (startmv.as_mv.col - 8) | 6;
        thismse = vfp->svf(y - 1, d->pre_stride, 6, this_mv.as_mv.row & 7, z, b->src_stride, &sse);
    }

    left = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);

    if (left < bestmse)
    {
        *bestmv = this_mv;
        bestmse = left;
        *distortion = thismse;
        *sse1 = sse;
    }

    this_mv.as_mv.col += 4;
    thismse = vfp->svf(y, d->pre_stride, this_mv.as_mv.col & 7, this_mv.as_mv.row & 7, z, b->src_stride, &sse);
    right = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);

    if (right < bestmse)
    {
        *bestmv = this_mv;
        bestmse = right;
        *distortion = thismse;
        *sse1 = sse;
    }

    // go up then down and check error
    this_mv.as_mv.col = startmv.as_mv.col;

    if (startmv.as_mv.row & 7)
    {
        this_mv.as_mv.row = startmv.as_mv.row - 2;
        thismse = vfp->svf(y, d->pre_stride, this_mv.as_mv.col & 7, this_mv.as_mv.row & 7, z, b->src_stride, &sse);
    }
    else
    {
        this_mv.as_mv.row = (startmv.as_mv.row - 8) | 6;
        thismse = vfp->svf(y - d->pre_stride, d->pre_stride, this_mv.as_mv.col & 7, 6, z, b->src_stride, &sse);
    }

    up = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);

    if (up < bestmse)
    {
        *bestmv = this_mv;
        bestmse = up;
        *distortion = thismse;
        *sse1 = sse;
    }

    this_mv.as_mv.row += 4;
    thismse = vfp->svf(y, d->pre_stride, this_mv.as_mv.col & 7, this_mv.as_mv.row & 7, z, b->src_stride, &sse);
    down = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);

    if (down < bestmse)
    {
        *bestmv = this_mv;
        bestmse = down;
        *distortion = thismse;
        *sse1 = sse;
    }


    // now check 1 more diagonal
    whichdir = (left < right ? 0 : 1) + (up < down ? 0 : 2);

//  for(whichdir=0;whichdir<4;whichdir++)
//  {
    this_mv = startmv;

    switch (whichdir)
    {
    case 0:

        if (startmv.as_mv.row & 7)
        {
            this_mv.as_mv.row -= 2;

            if (startmv.as_mv.col & 7)
            {
                this_mv.as_mv.col -= 2;
                thismse = vfp->svf(y, d->pre_stride, this_mv.as_mv.col & 7, this_mv.as_mv.row & 7, z, b->src_stride, &sse);
            }
            else
            {
                this_mv.as_mv.col = (startmv.as_mv.col - 8) | 6;
                thismse = vfp->svf(y - 1, d->pre_stride, 6, this_mv.as_mv.row & 7, z, b->src_stride, &sse);;
            }
        }
        else
        {
            this_mv.as_mv.row = (startmv.as_mv.row - 8) | 6;

            if (startmv.as_mv.col & 7)
            {
                this_mv.as_mv.col -= 2;
                thismse = vfp->svf(y - d->pre_stride, d->pre_stride, this_mv.as_mv.col & 7, 6, z, b->src_stride, &sse);
            }
            else
            {
                this_mv.as_mv.col = (startmv.as_mv.col - 8) | 6;
                thismse = vfp->svf(y - d->pre_stride - 1, d->pre_stride, 6, 6, z, b->src_stride, &sse);
            }
        }

        break;
    case 1:
        this_mv.as_mv.col += 2;

        if (startmv.as_mv.row & 7)
        {
            this_mv.as_mv.row -= 2;
            thismse = vfp->svf(y, d->pre_stride, this_mv.as_mv.col & 7, this_mv.as_mv.row & 7, z, b->src_stride, &sse);
        }
        else
        {
            this_mv.as_mv.row = (startmv.as_mv.row - 8) | 6;
            thismse = vfp->svf(y - d->pre_stride, d->pre_stride, this_mv.as_mv.col & 7, 6, z, b->src_stride, &sse);
        }

        break;
    case 2:
        this_mv.as_mv.row += 2;

        if (startmv.as_mv.col & 7)
        {
            this_mv.as_mv.col -= 2;
            thismse = vfp->svf(y, d->pre_stride, this_mv.as_mv.col & 7, this_mv.as_mv.row & 7, z, b->src_stride, &sse);
        }
        else
        {
            this_mv.as_mv.col = (startmv.as_mv.col - 8) | 6;
            thismse = vfp->svf(y - 1, d->pre_stride, 6, this_mv.as_mv.row & 7, z, b->src_stride, &sse);
        }

        break;
    case 3:
        this_mv.as_mv.col += 2;
        this_mv.as_mv.row += 2;
        thismse = vfp->svf(y, d->pre_stride,  this_mv.as_mv.col & 7, this_mv.as_mv.row & 7, z, b->src_stride, &sse);
        break;
    }

    diag = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);

    if (diag < bestmse)
    {
        *bestmv = this_mv;
        bestmse = diag;
        *distortion = thismse;
        *sse1 = sse;
    }

    return bestmse;
}

int vp8_find_best_half_pixel_step(MACROBLOCK *mb, BLOCK *b, BLOCKD *d,
                                  int_mv *bestmv, int_mv *ref_mv,
                                  int error_per_bit,
                                  const vp8_variance_fn_ptr_t *vfp,
                                  int *mvcost[2], int *distortion,
                                  unsigned int *sse1)
{
    int bestmse = INT_MAX;
    int_mv startmv;
    int_mv this_mv;
    unsigned char *y = *(d->base_pre) + d->pre + (bestmv->as_mv.row) * d->pre_stride + bestmv->as_mv.col;
    unsigned char *z = (*(b->base_src) + b->src);
    int left, right, up, down, diag;
    unsigned int sse;
    int thismse;

    // Trap uncodable vectors
    if ((abs((bestmv->as_mv.col << 3) - ref_mv->as_mv.col) > MAX_FULL_PEL_VAL)
        || (abs((bestmv->as_mv.row << 3) - ref_mv->as_mv.row) > MAX_FULL_PEL_VAL))
    {
        bestmv->as_mv.row <<= 3;
        bestmv->as_mv.col <<= 3;
        *distortion = INT_MAX;
        return INT_MAX;
    }

    // central mv
    bestmv->as_mv.row <<= 3;
    bestmv->as_mv.col <<= 3;
    startmv = *bestmv;

    // calculate central point error
    bestmse = vfp->vf(y, d->pre_stride, z, b->src_stride, sse1);
    *distortion = bestmse;
    bestmse += mv_err_cost(bestmv, ref_mv, mvcost, error_per_bit);

    // go left then right and check error
    this_mv.as_mv.row = startmv.as_mv.row;
    this_mv.as_mv.col = ((startmv.as_mv.col - 8) | 4);
    thismse = vfp->svf_halfpix_h(y - 1, d->pre_stride, z, b->src_stride, &sse);
    left = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);

    if (left < bestmse)
    {
        *bestmv = this_mv;
        bestmse = left;
        *distortion = thismse;
        *sse1 = sse;
    }

    this_mv.as_mv.col += 8;
    thismse = vfp->svf_halfpix_h(y, d->pre_stride, z, b->src_stride, &sse);
    right = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);

    if (right < bestmse)
    {
        *bestmv = this_mv;
        bestmse = right;
        *distortion = thismse;
        *sse1 = sse;
    }

    // go up then down and check error
    this_mv.as_mv.col = startmv.as_mv.col;
    this_mv.as_mv.row = ((startmv.as_mv.row - 8) | 4);
    thismse = vfp->svf_halfpix_v(y - d->pre_stride, d->pre_stride, z, b->src_stride, &sse);
    up = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);

    if (up < bestmse)
    {
        *bestmv = this_mv;
        bestmse = up;
        *distortion = thismse;
        *sse1 = sse;
    }

    this_mv.as_mv.row += 8;
    thismse = vfp->svf_halfpix_v(y, d->pre_stride, z, b->src_stride, &sse);
    down = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);

    if (down < bestmse)
    {
        *bestmv = this_mv;
        bestmse = down;
        *distortion = thismse;
        *sse1 = sse;
    }

    // somewhat strangely not doing all the diagonals for half pel is slower than doing them.
#if 0
    // now check 1 more diagonal -
    whichdir = (left < right ? 0 : 1) + (up < down ? 0 : 2);
    this_mv = startmv;

    switch (whichdir)
    {
    case 0:
        this_mv.col = (this_mv.col - 8) | 4;
        this_mv.row = (this_mv.row - 8) | 4;
        diag = vfp->svf(y - 1 - d->pre_stride, d->pre_stride, 4, 4, z, b->src_stride, &sse);
        break;
    case 1:
        this_mv.col += 4;
        this_mv.row = (this_mv.row - 8) | 4;
        diag = vfp->svf(y - d->pre_stride, d->pre_stride, 4, 4, z, b->src_stride, &sse);
        break;
    case 2:
        this_mv.col = (this_mv.col - 8) | 4;
        this_mv.row += 4;
        diag = vfp->svf(y - 1, d->pre_stride, 4, 4, z, b->src_stride, &sse);
        break;
    case 3:
        this_mv.col += 4;
        this_mv.row += 4;
        diag = vfp->svf(y, d->pre_stride, 4, 4, z, b->src_stride, &sse);
        break;
    }

    diag += mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);

    if (diag < bestmse)
    {
        *bestmv = this_mv;
        bestmse = diag;
    }

#else
    this_mv.as_mv.col = (this_mv.as_mv.col - 8) | 4;
    this_mv.as_mv.row = (this_mv.as_mv.row - 8) | 4;
    thismse = vfp->svf_halfpix_hv(y - 1 - d->pre_stride, d->pre_stride, z, b->src_stride, &sse);
    diag = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);

    if (diag < bestmse)
    {
        *bestmv = this_mv;
        bestmse = diag;
        *distortion = thismse;
        *sse1 = sse;
    }

    this_mv.as_mv.col += 8;
    thismse = vfp->svf_halfpix_hv(y - d->pre_stride, d->pre_stride, z, b->src_stride, &sse);
    diag = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);

    if (diag < bestmse)
    {
        *bestmv = this_mv;
        bestmse = diag;
        *distortion = thismse;
        *sse1 = sse;
    }

    this_mv.as_mv.col = (this_mv.as_mv.col - 8) | 4;
    this_mv.as_mv.row = startmv.as_mv.row + 4;
    thismse = vfp->svf_halfpix_hv(y - 1, d->pre_stride, z, b->src_stride, &sse);
    diag = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);

    if (diag < bestmse)
    {
        *bestmv = this_mv;
        bestmse = diag;
        *distortion = thismse;
        *sse1 = sse;
    }

    this_mv.as_mv.col += 8;
    thismse = vfp->svf_halfpix_hv(y, d->pre_stride, z, b->src_stride, &sse);
    diag = thismse + mv_err_cost(&this_mv, ref_mv, mvcost, error_per_bit);

    if (diag < bestmse)
    {
        *bestmv = this_mv;
        bestmse = diag;
        *distortion = thismse;
        *sse1 = sse;
    }

#endif
    return bestmse;
}

#define CHECK_BOUNDS(range) \
{\
    all_in = 1;\
    all_in &= ((br-range) >= x->mv_row_min);\
    all_in &= ((br+range) <= x->mv_row_max);\
    all_in &= ((bc-range) >= x->mv_col_min);\
    all_in &= ((bc+range) <= x->mv_col_max);\
}

#define CHECK_POINT \
{\
    if (this_mv.as_mv.col < x->mv_col_min) continue;\
    if (this_mv.as_mv.col > x->mv_col_max) continue;\
    if (this_mv.as_mv.row < x->mv_row_min) continue;\
    if (this_mv.as_mv.row > x->mv_row_max) continue;\
}

#define CHECK_BETTER \
{\
    if (thissad < bestsad)\
    {\
        thissad += mvsad_err_cost(&this_mv, &fcenter_mv, mvsadcost, sad_per_bit);\
        if (thissad < bestsad)\
        {\
            bestsad = thissad;\
            best_site = i;\
        }\
    }\
}

static const MV next_chkpts[6][3] =
{
    {{ -2, 0}, { -1, -2}, {1, -2}},
    {{ -1, -2}, {1, -2}, {2, 0}},
    {{1, -2}, {2, 0}, {1, 2}},
    {{2, 0}, {1, 2}, { -1, 2}},
    {{1, 2}, { -1, 2}, { -2, 0}},
    {{ -1, 2}, { -2, 0}, { -1, -2}}
};

int vp8_hex_search
(
    MACROBLOCK *x,
    BLOCK *b,
    BLOCKD *d,
    int_mv *ref_mv,
    int_mv *best_mv,
    int search_param,
    int sad_per_bit,
    int *num00,
    const vp8_variance_fn_ptr_t *vfp,
    int *mvsadcost[2],
    int *mvcost[2],
    int_mv *center_mv
)
{
    MV hex[6] = { { -1, -2}, {1, -2}, {2, 0}, {1, 2}, { -1, 2}, { -2, 0} } ;
    MV neighbors[4] = {{0, -1}, { -1, 0}, {1, 0}, {0, 1}} ;
    int i, j;

    unsigned char *what = (*(b->base_src) + b->src);
    int what_stride = b->src_stride;
    int in_what_stride = d->pre_stride;
    int br = ref_mv->as_mv.row >> 3, bc = ref_mv->as_mv.col >> 3;
    int_mv this_mv;
    unsigned int bestsad = 0x7fffffff;
    unsigned int thissad;
    unsigned char *base_offset;
    unsigned char *this_offset;
    int k = -1;
    int all_in;
    int best_site = -1;

    int_mv fcenter_mv;
    fcenter_mv.as_mv.row = center_mv->as_mv.row >> 3;
    fcenter_mv.as_mv.col = center_mv->as_mv.col >> 3;

    // Work out the start point for the search
    base_offset = (unsigned char *)(*(d->base_pre) + d->pre);
    this_offset = base_offset + (br * (d->pre_stride)) + bc;
    this_mv.as_mv.row = br;
    this_mv.as_mv.col = bc;
    bestsad = vfp->sdf( what, what_stride, this_offset, in_what_stride, 0x7fffffff) + mvsad_err_cost(&this_mv, &fcenter_mv, mvsadcost, sad_per_bit);

    // hex search
    //j=0
    CHECK_BOUNDS(2)

    if(all_in)
    {
        for (i = 0; i < 6; i++)
        {
            this_mv.as_mv.row = br + hex[i].row;
            this_mv.as_mv.col = bc + hex[i].col;
            this_offset = base_offset + (this_mv.as_mv.row * in_what_stride) + this_mv.as_mv.col;
            thissad=vfp->sdf( what, what_stride, this_offset, in_what_stride, bestsad);
            CHECK_BETTER
        }
    }else
    {
        for (i = 0; i < 6; i++)
        {
            this_mv.as_mv.row = br + hex[i].row;
            this_mv.as_mv.col = bc + hex[i].col;
            CHECK_POINT
            this_offset = base_offset + (this_mv.as_mv.row * in_what_stride) + this_mv.as_mv.col;
            thissad=vfp->sdf( what, what_stride, this_offset, in_what_stride, bestsad);
            CHECK_BETTER
        }
    }

    if (best_site == -1)
        goto cal_neighbors;
    else
    {
        br += hex[best_site].row;
        bc += hex[best_site].col;
        k = best_site;
    }

    for (j = 1; j < 127; j++)
    {
        best_site = -1;
        CHECK_BOUNDS(2)

        if(all_in)
        {
            for (i = 0; i < 3; i++)
            {
                this_mv.as_mv.row = br + next_chkpts[k][i].row;
                this_mv.as_mv.col = bc + next_chkpts[k][i].col;
                this_offset = base_offset + (this_mv.as_mv.row * (in_what_stride)) + this_mv.as_mv.col;
                thissad = vfp->sdf( what, what_stride, this_offset, in_what_stride, bestsad);
                CHECK_BETTER
            }
        }else
        {
            for (i = 0; i < 3; i++)
            {
                this_mv.as_mv.row = br + next_chkpts[k][i].row;
                this_mv.as_mv.col = bc + next_chkpts[k][i].col;
                CHECK_POINT
                this_offset = base_offset + (this_mv.as_mv.row * (in_what_stride)) + this_mv.as_mv.col;
                thissad = vfp->sdf( what, what_stride, this_offset, in_what_stride, bestsad);
                CHECK_BETTER
            }
        }

        if (best_site == -1)
            break;
        else
        {
            br += next_chkpts[k][best_site].row;
            bc += next_chkpts[k][best_site].col;
            k += 5 + best_site;
            if (k >= 12) k -= 12;
            else if (k >= 6) k -= 6;
        }
    }

    // check 4 1-away neighbors
cal_neighbors:
    for (j = 0; j < 32; j++)
    {
        best_site = -1;
        CHECK_BOUNDS(1)

        if(all_in)
        {
            for (i = 0; i < 4; i++)
            {
                this_mv.as_mv.row = br + neighbors[i].row;
                this_mv.as_mv.col = bc + neighbors[i].col;
                this_offset = base_offset + (this_mv.as_mv.row * (in_what_stride)) + this_mv.as_mv.col;
                thissad = vfp->sdf( what, what_stride, this_offset, in_what_stride, bestsad);
                CHECK_BETTER
            }
        }else
        {
            for (i = 0; i < 4; i++)
            {
                this_mv.as_mv.row = br + neighbors[i].row;
                this_mv.as_mv.col = bc + neighbors[i].col;
                CHECK_POINT
                this_offset = base_offset + (this_mv.as_mv.row * (in_what_stride)) + this_mv.as_mv.col;
                thissad = vfp->sdf( what, what_stride, this_offset, in_what_stride, bestsad);
                CHECK_BETTER
            }
        }

        if (best_site == -1)
            break;
        else
        {
            br += neighbors[best_site].row;
            bc += neighbors[best_site].col;
        }
    }

    best_mv->as_mv.row = br;
    best_mv->as_mv.col = bc;
    this_mv.as_mv.row = br<<3;
    this_mv.as_mv.col = bc<<3;

    this_offset = (unsigned char *)(*(d->base_pre) + d->pre + (br * (in_what_stride)) + bc);
    return vfp->vf(what, what_stride, this_offset, in_what_stride, &bestsad) + mv_err_cost(&this_mv, center_mv, mvcost, x->errorperbit) ;
}
#undef CHECK_BOUNDS
#undef CHECK_POINT
#undef CHECK_BETTER

int vp8_diamond_search_sad
(
    MACROBLOCK *x,
    BLOCK *b,
    BLOCKD *d,
    int_mv *ref_mv,
    int_mv *best_mv,
    int search_param,
    int sad_per_bit,
    int *num00,
    vp8_variance_fn_ptr_t *fn_ptr,
    int *mvcost[2],
    int_mv *center_mv
)
{
    int i, j, step;

    unsigned char *what = (*(b->base_src) + b->src);
    int what_stride = b->src_stride;
    unsigned char *in_what;
    int in_what_stride = d->pre_stride;
    unsigned char *best_address;

    int tot_steps;
    int_mv this_mv;

    int bestsad = INT_MAX;
    int best_site = 0;
    int last_site = 0;

    int ref_row = ref_mv->as_mv.row >> 3;
    int ref_col = ref_mv->as_mv.col >> 3;
    int this_row_offset;
    int this_col_offset;
    search_site *ss;

    unsigned char *check_here;
    int thissad;

    int *mvsadcost[2] = {x->mvsadcost[0], x->mvsadcost[1]};
    int_mv fcenter_mv;
    fcenter_mv.as_mv.row = center_mv->as_mv.row >> 3;
    fcenter_mv.as_mv.col = center_mv->as_mv.col >> 3;

    *num00 = 0;

    best_mv->as_mv.row = ref_row;
    best_mv->as_mv.col = ref_col;

    // Work out the start point for the search
    in_what = (unsigned char *)(*(d->base_pre) + d->pre + (ref_row * (d->pre_stride)) + ref_col);
    best_address = in_what;

    // We need to check that the starting point for the search (as indicated by ref_mv) is within the buffer limits
    if ((ref_col > x->mv_col_min) && (ref_col < x->mv_col_max) &&
    (ref_row > x->mv_row_min) && (ref_row < x->mv_row_max))
    {
        // Check the starting position
        bestsad = fn_ptr->sdf(what, what_stride, in_what, in_what_stride, 0x7fffffff) + mvsad_err_cost(best_mv, &fcenter_mv, mvsadcost, sad_per_bit);
    }

    // search_param determines the length of the initial step and hence the number of iterations
    // 0 = initial step (MAX_FIRST_STEP) pel : 1 = (MAX_FIRST_STEP/2) pel, 2 = (MAX_FIRST_STEP/4) pel... etc.
    ss = &x->ss[search_param * x->searches_per_step];
    tot_steps = (x->ss_count / x->searches_per_step) - search_param;

    i = 1;

    for (step = 0; step < tot_steps ; step++)
    {
        for (j = 0 ; j < x->searches_per_step ; j++)
        {
            // Trap illegal vectors
            this_row_offset = best_mv->as_mv.row + ss[i].mv.row;
            this_col_offset = best_mv->as_mv.col + ss[i].mv.col;

            if ((this_col_offset > x->mv_col_min) && (this_col_offset < x->mv_col_max) &&
            (this_row_offset > x->mv_row_min) && (this_row_offset < x->mv_row_max))

            {
                check_here = ss[i].offset + best_address;
                thissad = fn_ptr->sdf(what, what_stride, check_here , in_what_stride, bestsad);

                if (thissad < bestsad)
                {
                    this_mv.as_mv.row = this_row_offset;
                    this_mv.as_mv.col = this_col_offset;
                    thissad += mvsad_err_cost(&this_mv, &fcenter_mv, mvsadcost, sad_per_bit);

                    if (thissad < bestsad)
                    {
                        bestsad = thissad;
                        best_site = i;
                    }
                }
            }

            i++;
        }

        if (best_site != last_site)
        {
            best_mv->as_mv.row += ss[best_site].mv.row;
            best_mv->as_mv.col += ss[best_site].mv.col;
            best_address += ss[best_site].offset;
            last_site = best_site;
        }
        else if (best_address == in_what)
            (*num00)++;
    }

    this_mv.as_mv.row = best_mv->as_mv.row << 3;
    this_mv.as_mv.col = best_mv->as_mv.col << 3;

    if (bestsad == INT_MAX)
        return INT_MAX;

    return fn_ptr->vf(what, what_stride, best_address, in_what_stride, (unsigned int *)(&thissad))
        + mv_err_cost(&this_mv, center_mv, mvcost, x->errorperbit);
}

int vp8_diamond_search_sadx4
(
    MACROBLOCK *x,
    BLOCK *b,
    BLOCKD *d,
    int_mv *ref_mv,
    int_mv *best_mv,
    int search_param,
    int sad_per_bit,
    int *num00,
    vp8_variance_fn_ptr_t *fn_ptr,
    int *mvcost[2],
    int_mv *center_mv
)
{
    int i, j, step;

    unsigned char *what = (*(b->base_src) + b->src);
    int what_stride = b->src_stride;
    unsigned char *in_what;
    int in_what_stride = d->pre_stride;
    unsigned char *best_address;

    int tot_steps;
    int_mv this_mv;

    int bestsad = INT_MAX;
    int best_site = 0;
    int last_site = 0;

    int ref_row = ref_mv->as_mv.row >> 3;
    int ref_col = ref_mv->as_mv.col >> 3;
    int this_row_offset;
    int this_col_offset;
    search_site *ss;

    unsigned char *check_here;
    unsigned int thissad;

    int *mvsadcost[2] = {x->mvsadcost[0], x->mvsadcost[1]};
    int_mv fcenter_mv;
    fcenter_mv.as_mv.row = center_mv->as_mv.row >> 3;
    fcenter_mv.as_mv.col = center_mv->as_mv.col >> 3;

    *num00 = 0;
    best_mv->as_mv.row = ref_row;
    best_mv->as_mv.col = ref_col;

    // Work out the start point for the search
    in_what = (unsigned char *)(*(d->base_pre) + d->pre + (ref_row * (d->pre_stride)) + ref_col);
    best_address = in_what;

    // We need to check that the starting point for the search (as indicated by ref_mv) is within the buffer limits
    if ((ref_col > x->mv_col_min) && (ref_col < x->mv_col_max) &&
    (ref_row > x->mv_row_min) && (ref_row < x->mv_row_max))
    {
        // Check the starting position
        bestsad = fn_ptr->sdf(what, what_stride, in_what, in_what_stride, 0x7fffffff) + mvsad_err_cost(best_mv, &fcenter_mv, mvsadcost, sad_per_bit);
    }

    // search_param determines the length of the initial step and hence the number of iterations
    // 0 = initial step (MAX_FIRST_STEP) pel : 1 = (MAX_FIRST_STEP/2) pel, 2 = (MAX_FIRST_STEP/4) pel... etc.
    ss = &x->ss[search_param * x->searches_per_step];
    tot_steps = (x->ss_count / x->searches_per_step) - search_param;

    i = 1;

    for (step = 0; step < tot_steps ; step++)
    {
        int all_in = 1, t;

        // To know if all neighbor points are within the bounds, 4 bounds checking are enough instead of
        // checking 4 bounds for each points.
        all_in &= ((best_mv->as_mv.row + ss[i].mv.row)> x->mv_row_min);
        all_in &= ((best_mv->as_mv.row + ss[i+1].mv.row) < x->mv_row_max);
        all_in &= ((best_mv->as_mv.col + ss[i+2].mv.col) > x->mv_col_min);
        all_in &= ((best_mv->as_mv.col + ss[i+3].mv.col) < x->mv_col_max);

        if (all_in)
        {
            unsigned int sad_array[4];

            for (j = 0 ; j < x->searches_per_step ; j += 4)
            {
                unsigned char *block_offset[4];

                for (t = 0; t < 4; t++)
                    block_offset[t] = ss[i+t].offset + best_address;

                fn_ptr->sdx4df(what, what_stride, block_offset, in_what_stride, sad_array);

                for (t = 0; t < 4; t++, i++)
                {
                    if (sad_array[t] < bestsad)
                    {
                        this_mv.as_mv.row = best_mv->as_mv.row + ss[i].mv.row;
                        this_mv.as_mv.col = best_mv->as_mv.col + ss[i].mv.col;
                        sad_array[t] += mvsad_err_cost(&this_mv, &fcenter_mv, mvsadcost, sad_per_bit);

                        if (sad_array[t] < bestsad)
                        {
                            bestsad = sad_array[t];
                            best_site = i;
                        }
                    }
                }
            }
        }
        else
        {
            for (j = 0 ; j < x->searches_per_step ; j++)
            {
                // Trap illegal vectors
                this_row_offset = best_mv->as_mv.row + ss[i].mv.row;
                this_col_offset = best_mv->as_mv.col + ss[i].mv.col;

                if ((this_col_offset > x->mv_col_min) && (this_col_offset < x->mv_col_max) &&
                (this_row_offset > x->mv_row_min) && (this_row_offset < x->mv_row_max))
                {
                    check_here = ss[i].offset + best_address;
                    thissad = fn_ptr->sdf(what, what_stride, check_here , in_what_stride, bestsad);

                    if (thissad < bestsad)
                    {
                        this_mv.as_mv.row = this_row_offset;
                        this_mv.as_mv.col = this_col_offset;
                        thissad += mvsad_err_cost(&this_mv, &fcenter_mv, mvsadcost, sad_per_bit);

                        if (thissad < bestsad)
                        {
                            bestsad = thissad;
                            best_site = i;
                        }
                    }
                }
                i++;
            }
        }

        if (best_site != last_site)
        {
            best_mv->as_mv.row += ss[best_site].mv.row;
            best_mv->as_mv.col += ss[best_site].mv.col;
            best_address += ss[best_site].offset;
            last_site = best_site;
        }
        else if (best_address == in_what)
            (*num00)++;
    }

    this_mv.as_mv.row = best_mv->as_mv.row << 3;
    this_mv.as_mv.col = best_mv->as_mv.col << 3;

    if (bestsad == INT_MAX)
        return INT_MAX;

    return fn_ptr->vf(what, what_stride, best_address, in_what_stride, (unsigned int *)(&thissad))
        + mv_err_cost(&this_mv, center_mv, mvcost, x->errorperbit);
}

int vp8_full_search_sad(MACROBLOCK *x, BLOCK *b, BLOCKD *d, int_mv *ref_mv,
                        int sad_per_bit, int distance,
                        vp8_variance_fn_ptr_t *fn_ptr, int *mvcost[2],
                        int_mv *center_mv)
{
    unsigned char *what = (*(b->base_src) + b->src);
    int what_stride = b->src_stride;
    unsigned char *in_what;
    int in_what_stride = d->pre_stride;
    int mv_stride = d->pre_stride;
    unsigned char *bestaddress;
    int_mv *best_mv = &d->bmi.mv;
    int_mv this_mv;
    int bestsad = INT_MAX;
    int r, c;

    unsigned char *check_here;
    int thissad;

    int ref_row = ref_mv->as_mv.row;
    int ref_col = ref_mv->as_mv.col;

    int row_min = ref_row - distance;
    int row_max = ref_row + distance;
    int col_min = ref_col - distance;
    int col_max = ref_col + distance;

    int *mvsadcost[2] = {x->mvsadcost[0], x->mvsadcost[1]};
    int_mv fcenter_mv;
    fcenter_mv.as_mv.row = center_mv->as_mv.row >> 3;
    fcenter_mv.as_mv.col = center_mv->as_mv.col >> 3;

    // Work out the mid point for the search
    in_what = *(d->base_pre) + d->pre;
    bestaddress = in_what + (ref_row * d->pre_stride) + ref_col;

    best_mv->as_mv.row = ref_row;
    best_mv->as_mv.col = ref_col;

    // We need to check that the starting point for the search (as indicated by ref_mv) is within the buffer limits
    if ((ref_col > x->mv_col_min) && (ref_col < x->mv_col_max) &&
    (ref_row > x->mv_row_min) && (ref_row < x->mv_row_max))
    {
        // Baseline value at the centre

        //bestsad = fn_ptr->sf( what,what_stride,bestaddress,in_what_stride) + (int)sqrt(mv_err_cost(ref_mv,ref_mv, mvcost,error_per_bit*14));
        bestsad = fn_ptr->sdf(what, what_stride, bestaddress, in_what_stride, 0x7fffffff) + mvsad_err_cost(best_mv, &fcenter_mv, mvsadcost, sad_per_bit);
    }

    // Apply further limits to prevent us looking using vectors that stretch beyiond the UMV border
    if (col_min < x->mv_col_min)
        col_min = x->mv_col_min;

    if (col_max > x->mv_col_max)
        col_max = x->mv_col_max;

    if (row_min < x->mv_row_min)
        row_min = x->mv_row_min;

    if (row_max > x->mv_row_max)
        row_max = x->mv_row_max;

    for (r = row_min; r < row_max ; r++)
    {
        this_mv.as_mv.row = r;
        check_here = r * mv_stride + in_what + col_min;

        for (c = col_min; c < col_max; c++)
        {
            thissad = fn_ptr->sdf(what, what_stride, check_here , in_what_stride, bestsad);

            this_mv.as_mv.col = c;
            thissad  += mvsad_err_cost(&this_mv, &fcenter_mv, mvsadcost, sad_per_bit);

            if (thissad < bestsad)
            {
                bestsad = thissad;
                best_mv->as_mv.row = r;
                best_mv->as_mv.col = c;
                bestaddress = check_here;
            }

            check_here++;
        }
    }

    this_mv.as_mv.row = best_mv->as_mv.row << 3;
    this_mv.as_mv.col = best_mv->as_mv.col << 3;

    if (bestsad < INT_MAX)
        return fn_ptr->vf(what, what_stride, bestaddress, in_what_stride, (unsigned int *)(&thissad))
        + mv_err_cost(&this_mv, center_mv, mvcost, x->errorperbit);
    else
        return INT_MAX;
}

int vp8_full_search_sadx3(MACROBLOCK *x, BLOCK *b, BLOCKD *d, int_mv *ref_mv,
                          int sad_per_bit, int distance,
                          vp8_variance_fn_ptr_t *fn_ptr, int *mvcost[2],
                          int_mv *center_mv)
{
    unsigned char *what = (*(b->base_src) + b->src);
    int what_stride = b->src_stride;
    unsigned char *in_what;
    int in_what_stride = d->pre_stride;
    int mv_stride = d->pre_stride;
    unsigned char *bestaddress;
    int_mv *best_mv = &d->bmi.mv;
    int_mv this_mv;
    int bestsad = INT_MAX;
    int r, c;

    unsigned char *check_here;
    unsigned int thissad;

    int ref_row = ref_mv->as_mv.row;
    int ref_col = ref_mv->as_mv.col;

    int row_min = ref_row - distance;
    int row_max = ref_row + distance;
    int col_min = ref_col - distance;
    int col_max = ref_col + distance;

    unsigned int sad_array[3];

    int *mvsadcost[2] = {x->mvsadcost[0], x->mvsadcost[1]};
    int_mv fcenter_mv;
    fcenter_mv.as_mv.row = center_mv->as_mv.row >> 3;
    fcenter_mv.as_mv.col = center_mv->as_mv.col >> 3;

    // Work out the mid point for the search
    in_what = *(d->base_pre) + d->pre;
    bestaddress = in_what + (ref_row * d->pre_stride) + ref_col;

    best_mv->as_mv.row = ref_row;
    best_mv->as_mv.col = ref_col;

    // We need to check that the starting point for the search (as indicated by ref_mv) is within the buffer limits
    if ((ref_col > x->mv_col_min) && (ref_col < x->mv_col_max) &&
    (ref_row > x->mv_row_min) && (ref_row < x->mv_row_max))
    {
        // Baseline value at the centre
        bestsad = fn_ptr->sdf(what, what_stride, bestaddress, in_what_stride, 0x7fffffff) + mvsad_err_cost(best_mv, &fcenter_mv, mvsadcost, sad_per_bit);
    }

    // Apply further limits to prevent us looking using vectors that stretch beyiond the UMV border
    if (col_min < x->mv_col_min)
        col_min = x->mv_col_min;

    if (col_max > x->mv_col_max)
        col_max = x->mv_col_max;

    if (row_min < x->mv_row_min)
        row_min = x->mv_row_min;

    if (row_max > x->mv_row_max)
        row_max = x->mv_row_max;

    for (r = row_min; r < row_max ; r++)
    {
        this_mv.as_mv.row = r;
        check_here = r * mv_stride + in_what + col_min;
        c = col_min;

        while ((c + 2) < col_max)
        {
            int i;

            fn_ptr->sdx3f(what, what_stride, check_here , in_what_stride, sad_array);

            for (i = 0; i < 3; i++)
            {
                thissad = sad_array[i];

                if (thissad < bestsad)
                {
                    this_mv.as_mv.col = c;
                    thissad  += mvsad_err_cost(&this_mv, &fcenter_mv, mvsadcost, sad_per_bit);

                    if (thissad < bestsad)
                    {
                        bestsad = thissad;
                        best_mv->as_mv.row = r;
                        best_mv->as_mv.col = c;
                        bestaddress = check_here;
                    }
                }

                check_here++;
                c++;
            }
        }

        while (c < col_max)
        {
            thissad = fn_ptr->sdf(what, what_stride, check_here , in_what_stride, bestsad);

            if (thissad < bestsad)
            {
                this_mv.as_mv.col = c;
                thissad  += mvsad_err_cost(&this_mv, &fcenter_mv, mvsadcost, sad_per_bit);

                if (thissad < bestsad)
                {
                    bestsad = thissad;
                    best_mv->as_mv.row = r;
                    best_mv->as_mv.col = c;
                    bestaddress = check_here;
                }
            }

            check_here ++;
            c ++;
        }

    }

    this_mv.as_mv.row = best_mv->as_mv.row << 3;
    this_mv.as_mv.col = best_mv->as_mv.col << 3;

    if (bestsad < INT_MAX)
        return fn_ptr->vf(what, what_stride, bestaddress, in_what_stride, (unsigned int *)(&thissad))
        + mv_err_cost(&this_mv, center_mv, mvcost, x->errorperbit);
    else
        return INT_MAX;
}

int vp8_full_search_sadx8(MACROBLOCK *x, BLOCK *b, BLOCKD *d, int_mv *ref_mv,
                          int sad_per_bit, int distance,
                          vp8_variance_fn_ptr_t *fn_ptr, int *mvcost[2],
                          int_mv *center_mv)
{
    unsigned char *what = (*(b->base_src) + b->src);
    int what_stride = b->src_stride;
    unsigned char *in_what;
    int in_what_stride = d->pre_stride;
    int mv_stride = d->pre_stride;
    unsigned char *bestaddress;
    int_mv *best_mv = &d->bmi.mv;
    int_mv this_mv;
    int bestsad = INT_MAX;
    int r, c;

    unsigned char *check_here;
    unsigned int thissad;

    int ref_row = ref_mv->as_mv.row;
    int ref_col = ref_mv->as_mv.col;

    int row_min = ref_row - distance;
    int row_max = ref_row + distance;
    int col_min = ref_col - distance;
    int col_max = ref_col + distance;

    DECLARE_ALIGNED_ARRAY(16, unsigned short, sad_array8, 8);
    unsigned int sad_array[3];

    int *mvsadcost[2] = {x->mvsadcost[0], x->mvsadcost[1]};
    int_mv fcenter_mv;
    fcenter_mv.as_mv.row = center_mv->as_mv.row >> 3;
    fcenter_mv.as_mv.col = center_mv->as_mv.col >> 3;

    // Work out the mid point for the search
    in_what = *(d->base_pre) + d->pre;
    bestaddress = in_what + (ref_row * d->pre_stride) + ref_col;

    best_mv->as_mv.row = ref_row;
    best_mv->as_mv.col = ref_col;

    // We need to check that the starting point for the search (as indicated by ref_mv) is within the buffer limits
    if ((ref_col > x->mv_col_min) && (ref_col < x->mv_col_max) &&
    (ref_row > x->mv_row_min) && (ref_row < x->mv_row_max))
    {
        // Baseline value at the centre
        bestsad = fn_ptr->sdf(what, what_stride, bestaddress, in_what_stride, 0x7fffffff) + mvsad_err_cost(best_mv, &fcenter_mv, mvsadcost, sad_per_bit);
    }

    // Apply further limits to prevent us looking using vectors that stretch beyiond the UMV border
    if (col_min < x->mv_col_min)
        col_min = x->mv_col_min;

    if (col_max > x->mv_col_max)
        col_max = x->mv_col_max;

    if (row_min < x->mv_row_min)
        row_min = x->mv_row_min;

    if (row_max > x->mv_row_max)
        row_max = x->mv_row_max;

    for (r = row_min; r < row_max ; r++)
    {
        this_mv.as_mv.row = r;
        check_here = r * mv_stride + in_what + col_min;
        c = col_min;

        while ((c + 7) < col_max)
        {
            int i;

            fn_ptr->sdx8f(what, what_stride, check_here , in_what_stride, sad_array8);

            for (i = 0; i < 8; i++)
            {
                thissad = (unsigned int)sad_array8[i];

                if (thissad < bestsad)
                {
                    this_mv.as_mv.col = c;
                    thissad  += mvsad_err_cost(&this_mv, &fcenter_mv, mvsadcost, sad_per_bit);

                    if (thissad < bestsad)
                    {
                        bestsad = thissad;
                        best_mv->as_mv.row = r;
                        best_mv->as_mv.col = c;
                        bestaddress = check_here;
                    }
                }

                check_here++;
                c++;
            }
        }

        while ((c + 2) < col_max)
        {
            int i;

            fn_ptr->sdx3f(what, what_stride, check_here , in_what_stride, sad_array);

            for (i = 0; i < 3; i++)
            {
                thissad = sad_array[i];

                if (thissad < bestsad)
                {
                    this_mv.as_mv.col = c;
                    thissad  += mvsad_err_cost(&this_mv, &fcenter_mv, mvsadcost, sad_per_bit);

                    if (thissad < bestsad)
                    {
                        bestsad = thissad;
                        best_mv->as_mv.row = r;
                        best_mv->as_mv.col = c;
                        bestaddress = check_here;
                    }
                }

                check_here++;
                c++;
            }
        }

        while (c < col_max)
        {
            thissad = fn_ptr->sdf(what, what_stride, check_here , in_what_stride, bestsad);

            if (thissad < bestsad)
            {
                this_mv.as_mv.col = c;
                thissad  += mvsad_err_cost(&this_mv, &fcenter_mv, mvsadcost, sad_per_bit);

                if (thissad < bestsad)
                {
                    bestsad = thissad;
                    best_mv->as_mv.row = r;
                    best_mv->as_mv.col = c;
                    bestaddress = check_here;
                }
            }

            check_here ++;
            c ++;
        }
    }

    this_mv.as_mv.row = best_mv->as_mv.row << 3;
    this_mv.as_mv.col = best_mv->as_mv.col << 3;

    if (bestsad < INT_MAX)
        return fn_ptr->vf(what, what_stride, bestaddress, in_what_stride, (unsigned int *)(&thissad))
        + mv_err_cost(&this_mv, center_mv, mvcost, x->errorperbit);
    else
        return INT_MAX;
}

int vp8_refining_search_sad(MACROBLOCK *x, BLOCK *b, BLOCKD *d, int_mv *ref_mv,
                            int error_per_bit, int search_range,
                            vp8_variance_fn_ptr_t *fn_ptr, int *mvcost[2],
                            int_mv *center_mv)
{
    MV neighbors[4] = {{-1, 0}, {0, -1}, {0, 1}, {1, 0}};
    int i, j;
    short this_row_offset, this_col_offset;

    int what_stride = b->src_stride;
    int in_what_stride = d->pre_stride;
    unsigned char *what = (*(b->base_src) + b->src);
    unsigned char *best_address = (unsigned char *)(*(d->base_pre) + d->pre +
        (ref_mv->as_mv.row * (d->pre_stride)) + ref_mv->as_mv.col);
    unsigned char *check_here;
    unsigned int thissad;
    int_mv this_mv;
    unsigned int bestsad = INT_MAX;

    int *mvsadcost[2] = {x->mvsadcost[0], x->mvsadcost[1]};
    int_mv fcenter_mv;

    fcenter_mv.as_mv.row = center_mv->as_mv.row >> 3;
    fcenter_mv.as_mv.col = center_mv->as_mv.col >> 3;

    bestsad = fn_ptr->sdf(what, what_stride, best_address, in_what_stride, 0x7fffffff) + mvsad_err_cost(ref_mv, &fcenter_mv, mvsadcost, error_per_bit);

    for (i=0; i<search_range; i++)
    {
        int best_site = -1;

        for (j = 0 ; j < 4 ; j++)
        {
            this_row_offset = ref_mv->as_mv.row + neighbors[j].row;
            this_col_offset = ref_mv->as_mv.col + neighbors[j].col;

            if ((this_col_offset > x->mv_col_min) && (this_col_offset < x->mv_col_max) &&
            (this_row_offset > x->mv_row_min) && (this_row_offset < x->mv_row_max))
            {
                check_here = (neighbors[j].row)*in_what_stride + neighbors[j].col + best_address;
                thissad = fn_ptr->sdf(what, what_stride, check_here , in_what_stride, bestsad);

                if (thissad < bestsad)
                {
                    this_mv.as_mv.row = this_row_offset;
                    this_mv.as_mv.col = this_col_offset;
                    thissad += mvsad_err_cost(&this_mv, &fcenter_mv, mvsadcost, error_per_bit);

                    if (thissad < bestsad)
                    {
                        bestsad = thissad;
                        best_site = j;
                    }
                }
            }
        }

        if (best_site == -1)
            break;
        else
        {
            ref_mv->as_mv.row += neighbors[best_site].row;
            ref_mv->as_mv.col += neighbors[best_site].col;
            best_address += (neighbors[best_site].row)*in_what_stride + neighbors[best_site].col;
        }
    }

    this_mv.as_mv.row = ref_mv->as_mv.row << 3;
    this_mv.as_mv.col = ref_mv->as_mv.col << 3;

    if (bestsad < INT_MAX)
        return fn_ptr->vf(what, what_stride, best_address, in_what_stride, (unsigned int *)(&thissad))
        + mv_err_cost(&this_mv, center_mv, mvcost, x->errorperbit);
    else
        return INT_MAX;
}

int vp8_refining_search_sadx4(MACROBLOCK *x, BLOCK *b, BLOCKD *d,
                              int_mv *ref_mv, int error_per_bit,
                              int search_range, vp8_variance_fn_ptr_t *fn_ptr,
                              int *mvcost[2], int_mv *center_mv)
{
    MV neighbors[4] = {{-1, 0}, {0, -1}, {0, 1}, {1, 0}};
    int i, j;
    short this_row_offset, this_col_offset;

    int what_stride = b->src_stride;
    int in_what_stride = d->pre_stride;
    unsigned char *what = (*(b->base_src) + b->src);
    unsigned char *best_address = (unsigned char *)(*(d->base_pre) + d->pre +
        (ref_mv->as_mv.row * (d->pre_stride)) + ref_mv->as_mv.col);
    unsigned char *check_here;
    unsigned int thissad;
    int_mv this_mv;
    unsigned int bestsad = INT_MAX;

    int *mvsadcost[2] = {x->mvsadcost[0], x->mvsadcost[1]};
    int_mv fcenter_mv;

    fcenter_mv.as_mv.row = center_mv->as_mv.row >> 3;
    fcenter_mv.as_mv.col = center_mv->as_mv.col >> 3;

    bestsad = fn_ptr->sdf(what, what_stride, best_address, in_what_stride, 0x7fffffff) + mvsad_err_cost(ref_mv, &fcenter_mv, mvsadcost, error_per_bit);

    for (i=0; i<search_range; i++)
    {
        int best_site = -1;
        int all_in = 1;

        all_in &= ((ref_mv->as_mv.row - 1) > x->mv_row_min);
        all_in &= ((ref_mv->as_mv.row + 1) < x->mv_row_max);
        all_in &= ((ref_mv->as_mv.col - 1) > x->mv_col_min);
        all_in &= ((ref_mv->as_mv.col + 1) < x->mv_col_max);

        if(all_in)
        {
            unsigned int sad_array[4];
            unsigned char *block_offset[4];
            block_offset[0] = best_address - in_what_stride;
            block_offset[1] = best_address - 1;
            block_offset[2] = best_address + 1;
            block_offset[3] = best_address + in_what_stride;

            fn_ptr->sdx4df(what, what_stride, block_offset, in_what_stride, sad_array);

            for (j = 0; j < 4; j++)
            {
                if (sad_array[j] < bestsad)
                {
                    this_mv.as_mv.row = ref_mv->as_mv.row + neighbors[j].row;
                    this_mv.as_mv.col = ref_mv->as_mv.col + neighbors[j].col;
                    sad_array[j] += mvsad_err_cost(&this_mv, &fcenter_mv, mvsadcost, error_per_bit);

                    if (sad_array[j] < bestsad)
                    {
                        bestsad = sad_array[j];
                        best_site = j;
                    }
                }
            }
        }
        else
        {
            for (j = 0 ; j < 4 ; j++)
            {
                this_row_offset = ref_mv->as_mv.row + neighbors[j].row;
                this_col_offset = ref_mv->as_mv.col + neighbors[j].col;

                if ((this_col_offset > x->mv_col_min) && (this_col_offset < x->mv_col_max) &&
                (this_row_offset > x->mv_row_min) && (this_row_offset < x->mv_row_max))
                {
                    check_here = (neighbors[j].row)*in_what_stride + neighbors[j].col + best_address;
                    thissad = fn_ptr->sdf(what, what_stride, check_here , in_what_stride, bestsad);

                    if (thissad < bestsad)
                    {
                        this_mv.as_mv.row = this_row_offset;
                        this_mv.as_mv.col = this_col_offset;
                        thissad += mvsad_err_cost(&this_mv, &fcenter_mv, mvsadcost, error_per_bit);

                        if (thissad < bestsad)
                        {
                            bestsad = thissad;
                            best_site = j;
                        }
                    }
                }
            }
        }

        if (best_site == -1)
            break;
        else
        {
            ref_mv->as_mv.row += neighbors[best_site].row;
            ref_mv->as_mv.col += neighbors[best_site].col;
            best_address += (neighbors[best_site].row)*in_what_stride + neighbors[best_site].col;
        }
    }

    this_mv.as_mv.row = ref_mv->as_mv.row << 3;
    this_mv.as_mv.col = ref_mv->as_mv.col << 3;

    if (bestsad < INT_MAX)
        return fn_ptr->vf(what, what_stride, best_address, in_what_stride, (unsigned int *)(&thissad))
        + mv_err_cost(&this_mv, center_mv, mvcost, x->errorperbit);
    else
        return INT_MAX;
}

#ifdef ENTROPY_STATS
void print_mode_context(void)
{
    FILE *f = fopen("modecont.c", "w");
    int i, j;

    fprintf(f, "#include \"entropy.h\"\n");
    fprintf(f, "const int vp8_mode_contexts[6][4] =\n");
    fprintf(f, "{\n");

    for (j = 0; j < 6; j++)
    {
        fprintf(f, "  { // %d \n", j);
        fprintf(f, "    ");

        for (i = 0; i < 4; i++)
        {
            int overal_prob;
            int this_prob;
            int count; // = mv_ref_ct[j][i][0]+mv_ref_ct[j][i][1];

            // Overall probs
            count = mv_mode_cts[i][0] + mv_mode_cts[i][1];

            if (count)
                overal_prob = 256 * mv_mode_cts[i][0] / count;
            else
                overal_prob = 128;

            if (overal_prob == 0)
                overal_prob = 1;

            // context probs
            count = mv_ref_ct[j][i][0] + mv_ref_ct[j][i][1];

            if (count)
                this_prob = 256 * mv_ref_ct[j][i][0] / count;
            else
                this_prob = 128;

            if (this_prob == 0)
                this_prob = 1;

            fprintf(f, "%5d, ", this_prob);
            //fprintf(f,"%5d, %5d, %8d,", this_prob, overal_prob, (this_prob << 10)/overal_prob);
            //fprintf(f,"%8d, ", (this_prob << 10)/overal_prob);
        }

        fprintf(f, "  },\n");
    }

    fprintf(f, "};\n");
    fclose(f);
}

/* MV ref count ENTROPY_STATS stats code */
#ifdef ENTROPY_STATS
void init_mv_ref_counts()
{
    vpx_memset(mv_ref_ct, 0, sizeof(mv_ref_ct));
    vpx_memset(mv_mode_cts, 0, sizeof(mv_mode_cts));
}

void accum_mv_refs(MB_PREDICTION_MODE m, const int ct[4])
{
    if (m == ZEROMV)
    {
        ++mv_ref_ct [ct[0]] [0] [0];
        ++mv_mode_cts[0][0];
    }
    else
    {
        ++mv_ref_ct [ct[0]] [0] [1];
        ++mv_mode_cts[0][1];

        if (m == NEARESTMV)
        {
            ++mv_ref_ct [ct[1]] [1] [0];
            ++mv_mode_cts[1][0];
        }
        else
        {
            ++mv_ref_ct [ct[1]] [1] [1];
            ++mv_mode_cts[1][1];

            if (m == NEARMV)
            {
                ++mv_ref_ct [ct[2]] [2] [0];
                ++mv_mode_cts[2][0];
            }
            else
            {
                ++mv_ref_ct [ct[2]] [2] [1];
                ++mv_mode_cts[2][1];

                if (m == NEWMV)
                {
                    ++mv_ref_ct [ct[3]] [3] [0];
                    ++mv_mode_cts[3][0];
                }
                else
                {
                    ++mv_ref_ct [ct[3]] [3] [1];
                    ++mv_mode_cts[3][1];
                }
            }
        }
    }
}

#endif/* END MV ref count ENTROPY_STATS stats code */

#endif
