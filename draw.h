#include "jirojiro.h"

void draw_block_recursive(cairo_t *cr, jiro_ctx *j, int bx, int by, int l);
void draw_mvs(cairo_t *cr, jiro_ctx *j);
void draw_accounting(cairo_t *cr, jiro_ctx *j, short id);
cairo_surface_t* draw(od_img *img);
