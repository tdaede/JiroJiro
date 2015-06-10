#ifndef JIROJIRO_H
#define JIROJIRO_H
#include <daala/codec.h>
#include "internal.h"

typedef struct {
  unsigned char *bsize;
  int bstride;
  int selected_bx;
  int selected_by;
  int nvmvbs;
  int nhmvbs;
  od_mv_grid_pt *mv_grid;
  int mv_stride;
  unsigned int *flags;
  int fstride;
  od_img img;
  od_img mc_img;
  int valid;
} jiro_ctx;

#endif
