#include <gtk/gtk.h>
#include "internal.h"
#include <math.h>
#include "jirojiro.h"
#include <daala/codec.h>
#include <string.h>

cairo_surface_t* draw(od_img *img) {
  unsigned char *y_row;
  unsigned char *u_row;
  unsigned char *v_row;
  unsigned char *xrgb;
  unsigned char *rgb;
  int w;
  int h;
  int x;
  int y;
  int i;
  int u_hdec;
  int u_vdec;
  int v_hdec;
  int v_vdec;
  cairo_surface_t *cs;
  unsigned char * pixels;

  w=img->width;
  h=img->height;
  u_hdec=img->planes[1].xdec;
  u_vdec=img->planes[1].ydec;
  v_hdec=img->planes[2].xdec;
  v_vdec=img->planes[2].ydec;
  y_row=img->planes[0].data;
  u_row=img->planes[1].data;
  v_row=img->planes[2].data;
  
  pixels = (unsigned char *)malloc(sizeof(*pixels)*4*w*h);

  /* convert the YUV image into our xRGB pixels buffer (Cairo requires
     unpacked 32 bit RGB with one unused byte).  This code works for
     420, 422 and 444 */
  xrgb=pixels;
  for(y=0;y<h;y++){
    for(x=0;x<w;x++){
      int r;
      int g;
      int b;
      r=OD_DIV_ROUND(2916394880000LL*y_row[x]+
                     4490222169144LL*v_row[x>>v_hdec]-
                     621410755730432LL, 9745792000LL);
      g=OD_DIV_ROUND(2916394880000LL*y_row[x]-
                     534117096223LL*u_row[x>>u_hdec]-
                     1334761232047LL*v_row[x>>v_hdec]+
                     192554107938560LL,9745792000LL);
      b=OD_DIV_ROUND(2916394880000LL*y_row[x]+
                     5290866304968LL*u_row[x>>u_hdec]-
                     723893205115904LL, 9745792000LL);
      xrgb[4*x+0]=OD_CLAMPI(0,b,65535)>>8;
      xrgb[4*x+1]=OD_CLAMPI(0,g,65535)>>8;
      xrgb[4*x+2]=OD_CLAMPI(0,r,65535)>>8;
      xrgb[4*x+3]=255;
    }
    y_row+=img->planes[0].ystride;
    u_row+=img->planes[1].ystride&-((y&1)|!u_vdec);
    v_row+=img->planes[2].ystride&-((y&1)|!v_vdec);
    xrgb+=4*w;
  }

  /* hand pixels to Cairo */
  cs=cairo_image_surface_create_for_data(pixels,CAIRO_FORMAT_ARGB32,w,h,w*4);
  if(cairo_surface_status(cs)!=CAIRO_STATUS_SUCCESS){
    cairo_surface_destroy(cs);
    return NULL;
  }
  return cs;
}

void draw_block_recursive(cairo_t *cr, jiro_ctx *j, int bx, int by, int l) {
  int n = OD_BLOCK_SIZE4x4(j->bsize, j->bstride, bx, by);
  if (n == l) {
    int block_flags = j->flags[by * j->fstride + bx];
    if (block_flags & 0x01) {
      cairo_set_source_rgba(cr, 0, 0, 0, 0);
    } else {
      cairo_set_source_rgba(cr, 1, 0, 0, 0.5);
    }
    cairo_set_line_width(cr, 1);
    cairo_rectangle(cr, bx*4 + 1, by*4 + 1, (4<<l) - 2, (4<<l) - 2);
    cairo_stroke(cr);
  } else if (n < l) {
    draw_block_recursive(cr, j, bx, by, l - 1);
    draw_block_recursive(cr, j, bx + (1<<l-1), by, l - 1);
    draw_block_recursive(cr, j, bx, by + (1<<l-1), l - 1);
    draw_block_recursive(cr, j, bx + (1<<l-1), by + (1<<l-1), l - 1);
  }
}

void draw_mvs(cairo_t *cr, jiro_ctx *j) {
  int vx;
  int vy;
  for (vy = 0; vy < j->nvmvbs; vy += 1) {
    for (vx = 0; vx < j->nhmvbs; vx += 1) {
      od_mv_grid_pt* mvp = &j->mv_grid[vy*j->mv_stride + vx];
      if (mvp->valid) {
        if (mvp->ref == 0) {
          cairo_set_source_rgba(cr, 1, 1, 1, 0.5);
        } else {
          cairo_set_source_rgba(cr, 0, 1, 0, 0.5);
        }
        cairo_move_to(cr, vx*4, vy*4);
        cairo_line_to(cr, vx*4 + (mvp->mv[0]>>3), vy*4 + (mvp->mv[1]>>3));
        cairo_stroke(cr);
        cairo_arc(cr, vx*4, vy*4, 1.5, 0, 2*M_PI);
        cairo_fill(cr);
      }
    }
  }
}
