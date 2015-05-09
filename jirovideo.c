#include <gtk/gtk.h>
#include <daala/codec.h>
#include <daala/daaladec.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "draw.h"
#include "internal.h"
#include "jirojiro.h"
 
 extern const int        OD_VERT_D[];
/*The vector offsets in the X direction for each motion comepnsation block
   vertex from the upper-left.*/
# define OD_VERT_DX (OD_VERT_D+1)
/*The vector offsets in the Y direction for each motion compensation block
   vertex from the upper-left.*/
# define OD_VERT_DY (OD_VERT_D+0)
extern const int *const OD_VERT_SETUP_DX[4][4];
extern const int *const OD_VERT_SETUP_DY[4][4];

static cairo_surface_t *surface = NULL;
GtkWidget *coordinates;
GtkWidget *flags_label;
ogg_page page;
ogg_sync_state oy;
ogg_stream_state os;
daala_info di;
daala_comment dc;
daala_setup_info *dsi;
daala_dec_ctx *dctx;
od_img img;

double scale_factor = 1.0;

FILE* input;

jiro_ctx j;

static gboolean
draw_cb (GtkWidget *widget,
         cairo_t   *cr,
         gpointer   data)
{
  cairo_scale(cr, scale_factor, scale_factor);
  /* draw video */
  cairo_surface_t* cs = draw(&img);
  cairo_set_source_surface (cr, cs, 0, 0);
  cairo_paint (cr);
 
  
  return FALSE;
}

int read_page() {
  while (ogg_sync_pageout(&oy, &page) != 1) {
    char *buffer = ogg_sync_buffer(&oy, 4096);
    if (buffer == NULL) {
      return FALSE;
    }
    int bytes = fread(buffer, 1, 4096, input);
    // End of file
    if (bytes == 0) {
      return FALSE;
    }
    if (ogg_sync_wrote(&oy, bytes) != 0) {
      return FALSE;
    }
  }
  return TRUE;
}

int read_packet(ogg_packet *packet) {
  while (ogg_stream_packetout(&os, packet) != 1) {
    if (!read_page()) {
      return FALSE;
    }
    if (ogg_stream_pagein(&os, &page) != 0) {
      return TRUE;
    }
  }
  return TRUE;
}

int
main (int   argc,
      char *argv[])
{
  ogg_packet packet;
  cairo_surface_t* cs;
  cairo_t* cr;
  int done = FALSE;
  
  if (argc < 2) {
    printf("Usage: %s *.ogv\n",argv[0]);
    return -1;
  }
  
  ogg_sync_init(&oy);
  daala_info_init(&di);
  daala_comment_init(&dc);
  input = fopen(argv[1],"rb");
  while (!done && read_page()) {
    int serial = ogg_page_serialno(&page);
    if (ogg_page_bos(&page)) {
      if (ogg_stream_init(&os, serial) != 0) {
        printf("Could not initialize stream!\n");
        return -1;
      }
    }
    if (ogg_stream_pagein(&os, &page) != 0) {
      return -1;
    }
    ogg_packet packet;
    while (!done && ogg_stream_packetout(&os, &packet) != 0) {
      int ret = daala_decode_header_in(&di, &dc, &dsi, &packet);
      if (ret < 0) {
        if (memcmp(packet.packet, "fishead", packet.bytes)) {
          fprintf(stderr, "Ogg Skeleton streams not supported\n");
        }
        printf("Failed to decode Daala header!\n");
        return -1;
      }
      if (ret == 0) {
        done = TRUE;
        dctx = daala_decode_alloc(&di, dsi);
        if (dctx == NULL) {
          printf("Failed to allocate Daala decoder!\n");
          return -1;
        }
      }
    }
  }
  
  int nhsb = (di.pic_width + (OD_BSIZE_MAX - 1) & ~(OD_BSIZE_MAX - 1)) >> OD_LOG_BSIZE0 + OD_NBSIZES - 1;
  int nvsb = (di.pic_height + (OD_BSIZE_MAX - 1) & ~(OD_BSIZE_MAX - 1)) >> OD_LOG_BSIZE0 + OD_NBSIZES - 1;
  j.bsize = (unsigned char *)malloc(sizeof(*j.bsize)*nhsb*4*nvsb*4);
  j.bstride = nhsb*4;
  j.flags = (unsigned int *)malloc(sizeof(*j.flags)*nhsb*8*nvsb*8);
  j.fstride = nhsb*8;
  daala_decode_ctl(dctx, OD_DECCTL_SET_BSIZE_BUFFER, j.bsize, sizeof(*j.bsize)*nhsb*4*nvsb*4);
  daala_decode_ctl(dctx, OD_DECCTL_SET_FLAGS_BUFFER, j.flags, sizeof(*j.flags)*nhsb*8*nvsb*8);
  int frame_width = (di.pic_width + (OD_SUPERBLOCK_SIZE - 1)) &
   ~(OD_SUPERBLOCK_SIZE - 1);
  int frame_height = (di.pic_height + (OD_SUPERBLOCK_SIZE - 1)) &
   ~(OD_SUPERBLOCK_SIZE - 1);
  int nhmbs = frame_width >> 4;
  int nvmbs = frame_height >> 4;
  j.nhmvbs = frame_width >> OD_LOG_MVBSIZE_MIN;
  j.nvmvbs = frame_height >> OD_LOG_MVBSIZE_MIN;
  j.mv_grid = (od_mv_grid_pt*)malloc(sizeof(od_mv_grid_pt)*(j.nhmvbs+1)*(j.nvmvbs+1));
  j.mv_stride = j.nhmvbs+1;
  int ret = daala_decode_ctl(dctx, OD_DECCTL_SET_MV_BUFFER, j.mv_grid, sizeof(od_mv_grid_pt)*(j.nhmvbs+1)*(j.nvmvbs+1));
  if (ret) {
    if (ret == OD_EINVAL) {
      printf("Allocated the wrong MV buffer size.\n");
    }
    printf("Daala build doesn't support MV buffer reads!\n");
    return -1;
  }
  int frame_number = 0;
  while(read_packet(&packet)) {
    if (daala_decode_packet_in(dctx, &img, &packet) != 0) {
      printf("Daala decode fail!\n");
      return -1;
    }
    cs = draw(&img);
    cr = cairo_create (cs);
      /* draw blocks */
    int nhsb = (di.pic_width + (OD_BSIZE_MAX - 1) & ~(OD_BSIZE_MAX - 1)) >> OD_LOG_BSIZE0 + OD_NBSIZES - 1;
    int nvsb = (di.pic_height + (OD_BSIZE_MAX - 1) & ~(OD_BSIZE_MAX - 1)) >> OD_LOG_BSIZE0 + OD_NBSIZES - 1;
    int sbx;
    int sby;
    
    for (sby = 0; sby < nvsb; sby++) {
      for (sbx = 0; sbx < nhsb; sbx++) {
        draw_block_recursive(cr, &j, sbx*8, sby*8, 3);
      }
    }
    char filename[50];
    sprintf(filename, "out%05d.png", frame_number);
    draw_mvs(cr, &j);
    cairo_surface_write_to_png (cs,
                            filename);
    cairo_destroy(cr);
    cairo_surface_destroy(cs);
    frame_number++;
  }

  return 0;
}

