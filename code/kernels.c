#include <stdio.h>
#include <stdlib.h>
#include "defs.h"

/* 
 * Please fill in the following student struct 
 */
student_t student = {
  "Eric Waugh",     /* Full name */
  "ewaugh13@yahoo.com",  /* Email address */

};

/******************************************************
 * PINWHEEL KERNEL
 *
 * Your different versions of the pinwheel kernel go here
 ******************************************************/

/* 
 * naive_pinwheel - The naive baseline version of pinwheel 
 */
char naive_pinwheel_descr[] = "naive_pinwheel: baseline implementation";
void naive_pinwheel(pixel *src, pixel *dest)
{
  int qi, qj, i, j;

  /* qi & qj are column and row of quadrant
     i & j are column and row within quadrant */

  /* Loop over 4 quadrants: */
  for (qi = 0; qi < 2; qi++)
    for (qj = 0; qj < 2; qj++)
      /* Loop within quadrant: */
      for (i = 0; i < src->dim/2; i++)
        for (j = 0; j < src->dim/2; j++) {
          int s_idx = RIDX((qj * src->dim/2) + i,
                           j + (qi * src->dim/2), src->dim);
          int d_idx = RIDX((qj * src->dim/2) + src->dim/2 - 1 - j,
                           i + (qi * src->dim/2), src->dim);
          dest[d_idx].red = (src[s_idx].red
                             + src[s_idx].green
                             + src[s_idx].blue) / 3;
          dest[d_idx].green = (src[s_idx].red
                               + src[s_idx].green
                               + src[s_idx].blue) / 3;
          dest[d_idx].blue = (src[s_idx].red
                              + src[s_idx].green
                              + src[s_idx].blue) / 3;
        }
}

/* 
 * pinwheel - Your current working version of pinwheel
 * IMPORTANT: This is the version you will be graded on
 */
char pinwheel_descr[] = "pinwheel: Current working version";
void pinwheel(pixel *src, pixel *dest)
{
  int qi, qj, i, j, ii, jj;
  int quadrant = 16;
  int dimension = src->dim;
  int dimension_over_2 = dimension >> 1; //divide by 2

  /* Loop over 4 quadrants: */
  for (qi = 0; qi < 2; qi++)
  {
    int qi_dim = qi * dimension_over_2;
    for (qj = 0; qj < 2; qj++)
    {
      int qj_dim = qj * dimension_over_2;
      for(ii = 0; ii < dimension_over_2; ii+=quadrant)
      for(jj = 0; jj < dimension_over_2; jj+=quadrant)
      /* Loop within quadrant: */
      for (i = ii; i < ii + quadrant; i+=4)
      {
        for (j = jj; j < jj + quadrant; j++) //i = ii; i < ii + quadrant; i++
        {
          //j=1, i=1
          int s_idx_i1 = RIDX(qj_dim + i, j + qi_dim, dimension);
          int d_idx_i1 = RIDX(qj_dim + dimension_over_2 - 1 - j, i + qi_dim, dimension);

          //j=1, i=1
          pixel *current_src_i1 = &src[s_idx_i1];
          pixel *current_dest_i1 = &dest[d_idx_i1];
          unsigned short avg_color_i1 = (current_src_i1->red + current_src_i1->green + current_src_i1->blue) / 3;
          current_dest_i1->red = current_dest_i1->green = current_dest_i1->blue = avg_color_i1;
          //j=1, i=2
          pixel *current_src_i2 = &src[s_idx_i1 + dimension];
          pixel *current_dest_i2 = &dest[d_idx_i1 + 1];
          unsigned short avg_color_i2 = (current_src_i2->red + current_src_i2->green + current_src_i2->blue) / 3;
          current_dest_i2->red = current_dest_i2->green = current_dest_i2->blue = avg_color_i2;
          //j=1, i=3
          pixel *current_src_i3 = &src[s_idx_i1 + (dimension << 1)];
          pixel *current_dest_i3 = &dest[d_idx_i1 + 2];
          unsigned short avg_color_i3 = (current_src_i3->red + current_src_i3->green + current_src_i3->blue) / 3;
          current_dest_i3->red = current_dest_i3->green = current_dest_i3->blue = avg_color_i3;
          //j=1, i=4
          pixel *current_src_i4 = &src[s_idx_i1 + (dimension << 1) + dimension];
          pixel *current_dest_i4 = &dest[d_idx_i1 + 3];
          unsigned short avg_color_i4 = (current_src_i4->red + current_src_i4->green + current_src_i4->blue) / 3;
          current_dest_i4->red = current_dest_i4->green = current_dest_i4->blue = avg_color_i4;
        }
      }
    }
  }
}

/*********************************************************************
 * register_pinwheel_functions - Register all of your different versions
 *     of the pinwheel kernel with the driver by calling the
 *     add_pinwheel_function() for each test function. When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.  
 *********************************************************************/

void register_pinwheel_functions() {
  add_pinwheel_function(&pinwheel, pinwheel_descr);
  add_pinwheel_function(&naive_pinwheel, naive_pinwheel_descr);
}


/***************************************************************
 * MOTION KERNEL
 * 
 * Starts with various typedefs and helper functions for the motion
 * function, and you may modify these any way you like.
 **************************************************************/

/* A struct used to compute averaged pixel value */
typedef struct {
  int red;
  int green;
  int blue;
} pixel_sum;

/* 
 * initialize_pixel_sum - Initializes all fields of sum to 0 
 */
static void initialize_pixel_sum(pixel_sum *sum) 
{
  sum->red = sum->green = sum->blue = 0;
}

/* 
 * accumulate_sum - Accumulates field values of p in corresponding 
 * fields of sum 
 */
static void accumulate_weighted_sum(pixel_sum *sum, pixel p, double weight) 
{
  sum->red += (int) p.red * weight;
  sum->green += (int) p.green * weight;
  sum->blue += (int) p.blue * weight;
}

/* 
 * assign_sum_to_pixel - Computes averaged pixel value in current_pixel 
 */
static void assign_sum_to_pixel(pixel *current_pixel, pixel_sum sum) 
{
  current_pixel->red = (unsigned short)sum.red;
  current_pixel->green = (unsigned short)sum.green;
  current_pixel->blue = (unsigned short)sum.blue;
}

/* 
 * weighted_combo - Returns new pixel value at (i,j) 
 */
static pixel weighted_combo(int dim, int i, int j, pixel *src) 
{
  //int ii, jj;
  pixel_sum sum;
  pixel current_pixel;

  unsigned short red, green, blue;
  red = green = blue = 0;
  initialize_pixel_sum(&sum);

  int ridx = RIDX(i, j, dim);
  int dim_2 = dim + dim;
  pixel pixel_at_ridx;

  if((i + 2 < dim) && (j + 2 < dim))
  {
    pixel_at_ridx = src[ridx];
    red += (int) pixel_at_ridx.red * 0.60;
    green += (int) pixel_at_ridx.green * 0.60;
    blue += (int) pixel_at_ridx.blue * 0.60;
    //accumulate_weighted_sum(&sum, src[ridx], 0.60);

    pixel_at_ridx = src[ridx + 1];
    red += (int) pixel_at_ridx.red * 0.03;
    green += (int) pixel_at_ridx.green * 0.03;
    blue += (int) pixel_at_ridx.blue * 0.03;
    //accumulate_weighted_sum(&sum, src[ridx + 1], 0.03);

    pixel_at_ridx = src[ridx + dim];
    red += (int) pixel_at_ridx.red * 0.03;
    green += (int) pixel_at_ridx.green * 0.03;
    blue += (int) pixel_at_ridx.blue * 0.03;
    //accumulate_weighted_sum(&sum, src[ridx + dim], 0.03);

    pixel_at_ridx = src[ridx + dim + 1];
    red += (int) pixel_at_ridx.red * 0.30;
    green += (int) pixel_at_ridx.green * 0.30;
    blue += (int) pixel_at_ridx.blue * 0.30;
    //accumulate_weighted_sum(&sum, src[ridx + dim + 1], 0.30);

    pixel_at_ridx = src[ridx + dim + 2];
    red += (int) pixel_at_ridx.red * 0.03;
    green += (int) pixel_at_ridx.green * 0.03;
    blue += (int) pixel_at_ridx.blue * 0.03;
    //accumulate_weighted_sum(&sum, src[ridx + dim + 2], 0.03);

    pixel_at_ridx = src[ridx + dim_2 + 1];
    red += (int) pixel_at_ridx.red * 0.03;
    green += (int) pixel_at_ridx.green * 0.03;
    blue += (int) pixel_at_ridx.blue * 0.03;
    //accumulate_weighted_sum(&sum, src[ridx + dim_2 + 1], 0.03);

    pixel_at_ridx = src[ridx + dim_2 + 2];
    red += (int) pixel_at_ridx.red * 0.10;
    green += (int) pixel_at_ridx.green * 0.10;
    blue += (int) pixel_at_ridx.blue * 0.10;
    //accumulate_weighted_sum(&sum, src[ridx + dim_2 + 2], 0.10);

    sum.red = red; sum.blue = blue; sum.green = green;
  
    assign_sum_to_pixel(&current_pixel, sum);
  }
  else
  {
    if((i < dim) && (j < dim))
    {
      accumulate_weighted_sum(&sum, src[ridx], 0.60);
    }

    if((i < dim) && (j + 1 < dim))
    {
      accumulate_weighted_sum(&sum, src[ridx + 1], 0.03);
    }

    if((i + 1 < dim) && (j < dim))
    {
      accumulate_weighted_sum(&sum, src[ridx + dim], 0.03);
    }

    if((i + 1 < dim) && (j + 1 < dim))
    {
      accumulate_weighted_sum(&sum, src[ridx + dim + 1], 0.30);
    }

    if((i + 1 < dim) && (j + 2 < dim))
    {
      accumulate_weighted_sum(&sum, src[ridx + dim + 2], 0.03);
    }

    if((i + 2 < dim) && (j + 1 < dim))
    {
      accumulate_weighted_sum(&sum, src[ridx + dim_2 + 1], 0.03);
    }

    if((i + 2 < dim) && (j + 2 < dim))
    {
      accumulate_weighted_sum(&sum, src[ridx + dim_2 + 2], 0.10);
    }
    
    assign_sum_to_pixel(&current_pixel, sum);
  }

  return current_pixel;
}

/******************************************************
 * Your different versions of the motion kernel go here
 ******************************************************/

/*
 * naive_motion - The naive baseline version of motion 
 */
char naive_motion_descr[] = "naive_motion: baseline implementation";
void naive_motion(pixel *src, pixel *dst) 
{
  int i, j;
    
  for (i = 0; i < src->dim; i++)
    for (j = 0; j < src->dim; j++)
      dst[RIDX(i, j, src->dim)] = weighted_combo(src->dim, i, j, src);
}

/*
 * motion - Your current working version of motion. 
 * IMPORTANT: This is the version you will be graded on
 */
char motion_descr[] = "motion: Current working version";
void motion(pixel *src, pixel *dst) 
{
  int i, j;
  int dimension = src->dim;
    
  for (i = 0; i < dimension; i++)
  {
    for (j = 0; j < dimension; j++)
    {
      dst[RIDX(i, j, dimension)] = weighted_combo(dimension, i, j, src);
    }
  }
}

/********************************************************************* 
 * register_motion_functions - Register all of your different versions
 *     of the motion kernel with the driver by calling the
 *     add_motion_function() for each test function.  When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.  
 *********************************************************************/

void register_motion_functions() {
  add_motion_function(&motion, motion_descr);
  add_motion_function(&naive_motion, naive_motion_descr);
}
