AM_CFLAGS = -Wall -Werror
bin_PROGRAMS = jirojiro jirovideo
jirojiro_SOURCES = jirojiro.c draw.c
jirojiro_CFLAGS = $(GTK_CFLAGS) $(DAALA_CFLAGS) $(OGG_CFLAGS)
jirojiro_LDFLAGS = $(GTK_LIBS) $(DAALA_LIBS) $(OGG_LIBS) -Wl,-rpath /home/thomas/daala/src/.libs/

jirovideo_SOURCES = jirovideo.c draw.c
jirovideo_CFLAGS = $(GTK_CFLAGS) $(DAALA_CFLAGS) $(OGG_CFLAGS)
jirovideo_LDFLAGS = $(GTK_LIBS) $(DAALA_LIBS) $(OGG_LIBS) -Wl,-rpath /home/thomas/daala/src/.libs/
