#ifndef INTERNAL_H
#define INTERNAL_H

/*Smallest blocks are 4x4*/
# define OD_LOG_BSIZE0 (2)
/*There are 4 block sizes total (4x4, 8x8, 16x16, 32x32).*/
# define OD_NBSIZES    (4)
/*The maximum length of the side of a block.*/
# define OD_BSIZE_MAX  (1 << OD_LOG_BSIZE0 + OD_NBSIZES - 1)
# define OD_SUPERBLOCK_SIZE (32)

# define OD_MAXI(a, b) ((a) ^ (((a) ^ (b)) & -((b) > (a))))
# define OD_MINI(a, b) ((a) ^ (((b) ^ (a)) & -((b) < (a))))
# define OD_CLAMPI(a, b, c) (OD_MAXI(a, OD_MINI(b, c)))

# define OD_SIGNMASK(a) (-((a) < 0))
# define OD_FLIPSIGNI(a, b) (((a) + OD_SIGNMASK(b)) ^ OD_SIGNMASK(b))
# define OD_DIV_ROUND(x, y) (((x) + OD_FLIPSIGNI((y) >> 1, x))/(y))

# define OD_BLOCK_SIZE4x4(bsize, bstride, bx, by) \
 ((bsize)[((by) >> 1)*(bstride) + ((bx) >> 1)])
 
 # define OD_DIV_ROUND_POW2(dividend, shift, rval) \
  (((dividend) + OD_SIGNMASK(dividend) + (rval)) >> (shift))
 
 /*This should be a power of 2, and at least 8.*/
# define OD_UMV_PADDING (32)


# define OD_LOG_MVBSIZE_MIN (2)
# define OD_MVBSIZE_MIN (1 << OD_LOG_MVBSIZE_MIN)
 
typedef struct od_mv_grid_pt od_mv_grid_pt;
struct od_mv_grid_pt {
  /*The x, y offsets of the motion vector in units of 1/8th pixels.*/
  int mv[2];
  /*Whether or not this MV actually has a valid value.*/
  unsigned valid:1;
  unsigned ref:3;
};

#endif
