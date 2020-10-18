#include <errno.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <notcurses/notcurses.h>

static struct ncplane*
legendplane(struct notcurses* nc){
  const int dimx = ncplane_dim_x(notcurses_stdplane(nc));
  struct ncplane* lp = ncplane_new(notcurses_stdplane(nc), 1, dimx, 0, 0, NULL, "lgnd");
  if(lp){
    uint64_t channels = 0;
    channels_set_fg_alpha(&channels, CELL_ALPHA_TRANSPARENT);
    channels_set_bg_alpha(&channels, CELL_ALPHA_TRANSPARENT);
    ncplane_set_base(lp, "", 0, channels);
    ncplane_set_fg_rgb(lp, 0);
    ncplane_set_fg_alpha(lp, CELL_ALPHA_HIGHCONTRAST);
  }
  return lp;
}

static int
load_celes(struct notcurses* nc, struct ncvisual** ncvs, struct ncplane** celes){
  const char names[] = "LFRB";
  char fn[11] = "CelesX.png";
  for(int i = 0 ; i < 4 ; ++i){
    fn[5] = names[i];
    ncvs[i] = ncvisual_from_file(fn);
    if(!ncvs[i]){
      return -1;
    }
    struct ncvisual_options vopts = {
      .scaling = NCSCALE_NONE,
      .blitter = NCBLIT_2x2,
      .y = ncplane_dim_y(notcurses_stdplane(nc)) / 2,
      .x = ncplane_dim_x(notcurses_stdplane(nc)) / 2,
    };
    celes[i] = ncvisual_render(nc, ncvs[i], &vopts);
    ncplane_move_bottom(celes[i]);
  }
  return 0;
}

static int
input_loop(struct notcurses* nc, struct ncplane* map, struct ncplane* legend){
  struct ncvisual* ncvs[4];
  struct ncplane* celes[4];
  if(load_celes(nc, ncvs, celes)){
    return -1;
  }
  int fd = notcurses_inputready_fd(nc);
  int evs;
  struct pollfd pfds[1] = {
    { .fd = fd, .events = POLLIN, },
  };
  int mapy = 0, mapx = 0;
  int dimy, dimx;
  ncplane_dim_yx(notcurses_stdplane(nc), &dimy, &dimx);
  int celidx = 1;
  ncplane_move_top(celes[celidx]);
  notcurses_render(nc);
  while((evs = poll(pfds, sizeof(pfds) / sizeof(*pfds), -1)) >= 0 || errno == EINTR){
    if(evs > 0){
      ncplane_move_bottom(celes[celidx]);
      if(pfds[0].revents){
        ncinput ni;
        char32_t ch = notcurses_getc_nblock(nc, &ni);
        if(ch <= 0){
          break;
        }else if(ch == 'q'){
          ncplane_destroy(legend);
          return 0;
        }else if(ch == NCKEY_LEFT || ch == 'h'){
          mapx += 8;
          celidx = 0;
        }else if(ch == NCKEY_RIGHT || ch == 'l'){
          mapx -= 8;
          celidx = 2;
        }else if(ch == NCKEY_UP || ch == 'k'){
          mapy += 8;
          celidx = 3;
        }else if(ch == NCKEY_DOWN || ch == 'j'){
          mapy -= 8;
          celidx = 1;
        }else if(ch == NCKEY_RESIZE){
          notcurses_refresh(nc, &dimy, &dimx);
        }
      }
    }
    if(mapx < -(2048 - dimx)){
      mapx = -(2048 - dimx);
    }else if(mapx > 0){
      mapx = 0;
    }else if(mapy > 0){
      mapy = 0;
    }else if(mapy < -(2048 - dimy)){
      mapy = -(2048 - dimy);
    }
    ncplane_move_yx(map, mapy, mapx);
    ncplane_printf_aligned(legend, 0, NCALIGN_RIGHT, "x: %d y: %d", -mapx, -mapy);
    ncplane_move_top(celes[celidx]);
    ncplane_move_yx(celes[celidx], dimy / 2, dimx / 2);
    notcurses_render(nc);
  }
  ncplane_destroy(legend);
  return -1;
}

static int
debwarrior(struct notcurses* nc){
  struct ncvisual* ncv = ncvisual_from_file("FinalFantasyOverworld.png");
  if(!ncv){
    return -1;
  }
  struct ncvisual_options vopts = {
    .scaling = NCSCALE_NONE,
    .blitter = NCBLIT_2x2,
  };
  struct ncplane* map = ncvisual_render(nc, ncv, &vopts);
  struct ncplane* legend = legendplane(nc);
  if(legend == NULL){
    return -1;
  }
  int mapy, mapx;
  ncplane_yx(map, &mapy, &mapx);
  if(ncplane_printf_aligned(legend, 0, NCALIGN_RIGHT, "x: %d y: %d", -mapx, -mapy) <= 0){
    return -1;
  }
  return input_loop(nc, map, legend);
}

int main(void){
  struct notcurses* nc = notcurses_init(NULL, NULL);
  if(nc == NULL){
    return EXIT_FAILURE;
  }
  int r = debwarrior(nc);
  notcurses_stop(nc);
  return r ? EXIT_FAILURE : EXIT_SUCCESS;
}
