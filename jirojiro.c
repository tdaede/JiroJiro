#include <gtk/gtk.h>
#include <daala/codec.h>
#include <daala/daaladec.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

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
 
typedef struct od_mv_grid_pt od_mv_grid_pt;
struct od_mv_grid_pt {
  /*The x, y offsets of the motion vector in units of 1/8th pixels.*/
  int mv[2];
  /*Whether or not this MV actually has a valid value.*/
  unsigned valid:1;
  unsigned ref:3;
};
 
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
unsigned char *bsize;
int bstride;
int selected_bx;
int selected_by;
int nvmvbs;
int nhmvbs;
od_mv_grid_pt *mv_grid;
int mv_stride;

double scale_factor = 1.0;

unsigned int *flags;
int fstride;

FILE* input;

static void
step_frame (GtkWidget *widget,
             gpointer   data) {
  
}

static cairo_surface_t* draw(od_img *img) {
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

  GtkWidget *da;
  
static void draw_block_recursive(cairo_t *cr, int bx, int by, int l) {
  int n = OD_BLOCK_SIZE4x4(bsize, bstride, bx, by);
  if (n == l) {
    int block_flags = flags[by * fstride + bx];
    if (block_flags & 0x01) {
      cairo_set_source_rgba(cr, 0, 0, 0, 0.5);
    } else {
      cairo_set_source_rgba(cr, 1, 0, 0, 0.5);
    }
    cairo_set_line_width(cr, 1);
    cairo_rectangle(cr, bx*4 + 1, by*4 + 1, (4<<l) - 2, (4<<l) - 2);
    cairo_stroke(cr);
  } else if (n < l) {
    draw_block_recursive(cr, bx, by, l - 1);
    draw_block_recursive(cr, bx + (1<<l-1), by, l - 1);
    draw_block_recursive(cr, bx, by + (1<<l-1), l - 1);
    draw_block_recursive(cr, bx + (1<<l-1), by + (1<<l-1), l - 1);
  }
}

void draw_mvs(cairo_t *cr) {
  int vx;
  int vy;
  for (vy = 0; vy < nvmvbs; vy += 1) {
    for (vx = 0; vx < nhmvbs; vx += 1) {
      od_mv_grid_pt* mvp = &mv_grid[vy*mv_stride + vx];
      if (mvp->valid) {
        if (mvp->ref == 0) {
          cairo_set_source_rgba(cr, 0, 0, 1, 0.5);
        } else {
          cairo_set_source_rgba(cr, 0, 1, 0, 0.5);
        }
        cairo_move_to(cr, vx*4 -8, vy*4 - 8);
        cairo_line_to(cr, vx*4 + (mvp->mv[0]>>3) - 8, vy*4 + (mvp->mv[1]>>3) - 8);
        cairo_stroke(cr);
        cairo_arc(cr, vx*4 - 8, vy*4 - 8, 1.5, 0, 2*M_PI);
        cairo_fill(cr);
      }
    }
  }
}

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
  
  /* draw blocks */
  int nhsb = (di.pic_width + (OD_BSIZE_MAX - 1) & ~(OD_BSIZE_MAX - 1)) >> OD_LOG_BSIZE0 + OD_NBSIZES - 1;
  int nvsb = (di.pic_height + (OD_BSIZE_MAX - 1) & ~(OD_BSIZE_MAX - 1)) >> OD_LOG_BSIZE0 + OD_NBSIZES - 1;
  int sbx;
  int sby;
  
  for (sby = 0; sby < nvsb; sby++) {
    for (sbx = 0; sbx < nhsb; sbx++) {
      draw_block_recursive(cr, sbx*8, sby*8, 3);
    }
  }
  
  draw_mvs(cr);
  
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

static gboolean key_press_cb (GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
  ogg_packet packet;
  if (read_packet(&packet)) {
    if (daala_decode_packet_in(dctx, &img, &packet) != 0) {
      printf("Daala decode fail!\n");
      return -1;
    }
  }
  gtk_widget_queue_draw(da);
  while (gtk_events_pending()) {
    gtk_main_iteration_do(FALSE);
  }
  return FALSE;
}

static const char* band_desc[] = {
  "Coded",
  "Skipped",
  "NoRef",
  "Zeroed"
};

static gboolean pointer_motion_cb (GtkWidget *widget, GdkEventMotion *event, gpointer user_data) {
  char block_text[100];
  char flags_text[1000];
  int flags_text_i = 0;
  int bx = event->x / 4 / scale_factor;
  int by = event->y / 4 / scale_factor;
  int nhsb = (di.pic_width + (OD_BSIZE_MAX - 1) & ~(OD_BSIZE_MAX - 1)) >> OD_LOG_BSIZE0 + OD_NBSIZES - 1;
  int nvsb = (di.pic_height + (OD_BSIZE_MAX - 1) & ~(OD_BSIZE_MAX - 1)) >> OD_LOG_BSIZE0 + OD_NBSIZES - 1;
  if ((bx < nhsb*8) && (by < nvsb*8)) {
    int n = OD_BLOCK_SIZE4x4(bsize, bstride, bx, by);
    if (n <= 3) {
      int selected_bx = (bx >> n) << n;
      int selected_by = (by >> n) << n;
      int i;
      snprintf(block_text, 100, "Block: (%d, %d) Size: %d", selected_bx, selected_by, n);
      gtk_label_set_text(GTK_LABEL(coordinates), block_text);
      int max_band = n * 3;
      for (i = 0; i <= max_band; i++) {
        int band_flags = (flags[selected_by * fstride + selected_bx]>>(2*i)) & 0x03;
        flags_text_i += sprintf(flags_text + flags_text_i, "Band %d: %s\n", i, band_desc[band_flags]);
      }
      gtk_label_set_text(GTK_LABEL(flags_label), flags_text);
    }
  }
  return FALSE;
}

int
main (int   argc,
      char *argv[])
{
  GtkWidget *window;
  GtkWidget *headerbar;
  GtkWidget *topbox;
  GtkWidget *sidebar;
  int done = FALSE;

  gtk_init (&argc, &argv);
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
    while (!done && ogg_stream_packetpeek(&os, &packet) != 0) {
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
      if (!done) {
        if (ogg_stream_packetout(&os, &packet) != 1) {
          printf("???\n");
          return -1;
        }
      }
    }
  }
  
  int nhsb = (di.pic_width + (OD_BSIZE_MAX - 1) & ~(OD_BSIZE_MAX - 1)) >> OD_LOG_BSIZE0 + OD_NBSIZES - 1;
  int nvsb = (di.pic_height + (OD_BSIZE_MAX - 1) & ~(OD_BSIZE_MAX - 1)) >> OD_LOG_BSIZE0 + OD_NBSIZES - 1;
  bsize = (unsigned char *)malloc(sizeof(*bsize)*nhsb*4*nvsb*4);
  bstride = nhsb*4;
  flags = (unsigned int *)malloc(sizeof(*flags)*nhsb*8*nvsb*8);
  fstride = nhsb*8;
  daala_decode_ctl(dctx, OD_DECCTL_SET_BSIZE_BUFFER, bsize, sizeof(*bsize)*nhsb*4*nvsb*4);
  daala_decode_ctl(dctx, OD_DECCTL_SET_FLAGS_BUFFER, flags, sizeof(*flags)*nhsb*8*nvsb*8);
  int frame_width = (di.pic_width + (OD_SUPERBLOCK_SIZE - 1)) &
   ~(OD_SUPERBLOCK_SIZE - 1);
  int frame_height = (di.pic_height + (OD_SUPERBLOCK_SIZE - 1)) &
   ~(OD_SUPERBLOCK_SIZE - 1);
  int nhmbs = frame_width >> 4;
  int nvmbs = frame_height >> 4;
  nhmvbs = (nhmbs + 1) << 2;
  nvmvbs = (nvmbs + 1) << 2;
  mv_grid = (od_mv_grid_pt*)malloc(sizeof(od_mv_grid_pt)*(nhmvbs+1)*(nvmvbs+1));
  mv_stride = nhmvbs+1;
  int ret = daala_decode_ctl(dctx, OD_DECCTL_SET_MV_BUFFER, mv_grid, sizeof(od_mv_grid_pt)*(nhmvbs+1)*(nvmvbs+1));
  if (ret) {
    if (ret == OD_EINVAL) {
      printf("Allocated the wrong MV buffer size.\n");
    }
    printf("Daala build doesn't support MV buffer reads!\n");
    return -1;
  }
  /* create a new window, and set its title */
  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title (GTK_WINDOW (window), "JiroJiro - Daala Visualization Tool");
  gtk_window_set_icon_from_file(GTK_WINDOW(window), "hyperoats.jpg", NULL);
  g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);
  
  topbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
  gtk_container_add(GTK_CONTAINER(window), topbox);

  da = gtk_drawing_area_new ();
  gtk_widget_set_size_request (da, di.pic_width*scale_factor, di.pic_height*scale_factor);
  gtk_widget_add_events(da, GDK_POINTER_MOTION_MASK);
  gtk_container_add(GTK_CONTAINER(topbox), da);
  
  sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_widget_set_size_request(sidebar, 200, 200);
  gtk_container_add(GTK_CONTAINER(topbox), sidebar);
  gtk_widget_show(sidebar);
  
  coordinates = gtk_label_new("Block:");
  gtk_container_add(GTK_CONTAINER(sidebar), coordinates);
  
  flags_label = gtk_label_new("Flags:");
  gtk_container_add(GTK_CONTAINER(sidebar), flags_label);
  gtk_widget_show(flags_label);
  
  gtk_widget_show (da);
  gtk_widget_show(topbox);
  gtk_widget_show(coordinates);
  gtk_widget_show (window);
    /* Signals used to handle the backing surface */
  g_signal_connect (da, "draw",
                    G_CALLBACK (draw_cb), NULL);
  g_signal_connect (window, "key_press_event", G_CALLBACK (key_press_cb), NULL);
  g_signal_connect (da, "motion-notify-event", G_CALLBACK(pointer_motion_cb), NULL);

  gtk_main ();

  return 0;
}

