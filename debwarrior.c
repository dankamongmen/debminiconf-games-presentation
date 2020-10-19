#include <errno.h>
#include <unistd.h>
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
center_plane(const struct notcurses* nc, struct ncplane* n){
  int dimy, dimx;
  // FIXME const struct ncplane* stdn = notcurses_stddim_yx_const(nc, &dimy, &dimx);
  const struct ncplane* stdn = notcurses_stdplane_const(nc);
  ncplane_dim_yx(stdn, &dimy, &dimx);
  return ncplane_move_yx(n, ncplane_dim_y(stdn) / 2 - ncplane_dim_y(n) / 2,
                         ncplane_dim_x(stdn) / 2 - ncplane_dim_x(n) / 2);
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
    };
    celes[i] = ncvisual_render(nc, ncvs[i], &vopts);
    ncplane_move_bottom(celes[i]);
    center_plane(nc, celes[i]);
  }
  return 0;
}

static int
do_battle(struct notcurses* nc, struct ncplane* ep, struct ncplane* player){
  struct ncplane* cmdp = ncplane_new(notcurses_stdplane(nc), 10, 40,
                                     ncplane_dim_y(notcurses_stdplane(nc)) - 10,
                                     0, NULL, NULL);
  static struct ncselector_item items[] = {
    { "Attack", "Attack", },
    { "Magic", "Magic", },
    { "Vomit intensely", "Vomit intensely", },
    { "Run", "Run", },
  };
  struct ncselector_options sopts = {
    .title = "Action",
    .items = items,
  };
  struct ncselector* cmdsel = ncselector_create(cmdp, &sopts);
  if(cmdsel == NULL){
    ncplane_destroy(cmdp);
    return -1;
  }
  // FIXME read input, throw up selector, etc.
  notcurses_render(nc);
sleep(40);
  ncselector_destroy(cmdsel, NULL);
  return 0;
}

static int
overworld_battle(struct notcurses* nc, struct ncplane* map, struct ncplane* player){
  if(random() % 10 != 0){
    return 0;
  }
  struct ncplane* copy = ncplane_dup(map, NULL);
  if(!copy){
    return -1;
  }
  struct ncvisual* ncv = ncvisual_from_file("Balsac.jpg");
  if(ncv == NULL){
    ncplane_destroy(copy);
    return -1;
  }
  ncplane_greyscale(copy);
  int dimy, dimx;
  ncplane_dim_yx(notcurses_stdplane(nc), &dimy, &dimx);
  const int ex = dimx / 4 * 3;
  const int exoff = ncplane_align(notcurses_stdplane(nc), NCALIGN_CENTER, ex);
  struct ncplane* ep = ncplane_new(notcurses_stdplane(nc), dimy / 4 * 3, ex, 1, exoff, NULL, NULL);
  struct ncvisual_options vopts = {
    .scaling = NCSCALE_STRETCH,
    .n = ep,
  };
  ncvisual_render(nc, ncv, &vopts);
  int r = do_battle(nc, ep, player);
  ncplane_destroy(ep);
  ncplane_destroy(copy);
  ncvisual_destroy(ncv);
  return r;
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
  int mapy = -1279, mapx = -1184;
  ncplane_move_yx(map, mapy, mapx);
  int dimy, dimx;
  ncplane_dim_yx(notcurses_stdplane(nc), &dimy, &dimx);
  int celidx = 1;
  ncplane_move_top(celes[celidx]);
  notcurses_render(nc);
  // FIXME add repeating timerfd
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
    center_plane(nc, celes[celidx]);
    if(overworld_battle(nc, map, celes[0])){
      break;
    }
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
