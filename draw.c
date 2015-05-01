#include <gtk/gtk.h>
#include "internal.h"
#include <math.h>
#include "jirojiro.h"

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
