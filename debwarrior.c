#include <stdlib.h>
#include <notcurses/notcurses.h>

typedef struct player {
  struct notcurses* nc;
  int hp;
  enum {
    DIR_RIGHT, DIR_UP, DIR_LEFT, DIR_DOWN, DIR_MAX
  } direction;
  struct ncplane* splane; // stats plane
  struct ncvisual *ncvs[DIR_MAX];
  struct ncplane *ncps[DIR_MAX];
} player;

struct mapdata {
  struct ncplane* map;
  int y;
  int x;
  int toy;
  int tox;
};

static int
center_plane(struct ncplane* n){
  int dimy, dimx;
  const struct ncplane* p = ncplane_parent_const(n);
  ncplane_dim_yx(p, &dimy, &dimx);
  return ncplane_move_yx(n, ncplane_dim_y(p) / 2 - ncplane_dim_y(n) / 2,
                         ncplane_dim_x(p) / 2 - ncplane_dim_x(n) / 2 + 1);
}

static int
update_stats(player* p){
  ncplane_erase(p->splane);
  ncplane_set_fg_rgb(p->splane, 0x00ffff);
  ncplane_printf_aligned(p->splane, 0, NCALIGN_CENTER, "HP: %d", p->hp);
  return 0;
}

static int
load_celes(player* p){
  const char names[] = "rblf";
  char fn[11] = "celesX.gif";
  for(int i = 0 ; i < 4 ; ++i){
    fn[5] = names[i];
    p->ncvs[i] = ncvisual_from_file(fn);
    if(!p->ncvs[i]){
      return -1;
    }
    struct ncvisual_options vopts = {
      .blitter = NCBLIT_2x2,
    };
    p->ncps[i] = ncvisual_render(p->nc, p->ncvs[i], &vopts);
    ncplane_move_bottom(p->ncps[i]);
    center_plane(p->ncps[i]);
  }
  return 0;
}

static int
battle_loop(struct ncplane* plotp, player *p, struct ncselector* cmdsel){
  while(p->hp > 0){
    ncplane_set_fg_rgb(plotp, 0xf0f0f0);
    ncplane_printf(plotp, "Choose an action.\n");
    ncplane_set_fg_rgb(plotp, 0x00ff88);
    notcurses_render(p->nc);
    ncinput ni;
    char32_t ch;
    while((ch = notcurses_getc_blocking(p->nc, &ni))){
      if(!ncselector_offer_input(cmdsel, &ni)){
        if(ch == NCKEY_ENTER){
          ncplane_printf(plotp, "Your attack is ineffective against the almighty WarMECH!\n");
          break;
        }
      }
      notcurses_render(p->nc);
    }
    ncplane_printf(plotp, "WarMECH attacks with NUKE\n");
    int damage = random() % 5 + 2;
    ncplane_printf(plotp, "You are hit for %d HP\n", damage);
    if((p->hp -= damage) < 0){
      p->hp = 0;
    }
    update_stats(p);
  }
  ncplane_set_fg_rgb(plotp, 0xff8080);
  ncplane_printf(plotp, "Thou art dead, bitch 💣\n\n");
  notcurses_render(p->nc);
  return -1;
}

static int
do_battle(struct ncplane* ep, player* p){
  struct ncplane* stdn = notcurses_stdplane(p->nc);
  struct ncplane_options nopts = {
    .y = ncplane_dim_y(stdn) - 10,
    .horiz.x = 0,
    .rows = 10,
    .cols = 30,
  };
  struct ncplane* cmdp = ncplane_create(stdn, &nopts);
  ncplane_set_base(cmdp, " ", NCSTYLE_NONE, 0);
  static struct ncselector_item items[] = {
    { "Cast spell", "", },
    { "Vomit intensely", "", },
    { "Package it in AUR", "", },
    { "Shameful scurry", "", },
  };
  struct ncselector_options sopts = {
    .title = "Action",
    .items = items,
    .maxdisplay = 4,
    .boxchannels = CHANNELS_RGB_INITIALIZER(0x40, 0x80, 0x40, 0x0, 0x0, 0x60),
    .opchannels = CHANNELS_RGB_INITIALIZER(0xc0, 0xff, 0xc0, 0, 0, 0),
    .titlechannels = CHANNELS_RGB_INITIALIZER(0xff, 0x80, 0xff, 0, 0, 0x60),
  };
  struct ncselector* cmdsel = ncselector_create(cmdp, &sopts);
  if(cmdsel == NULL){
    ncplane_destroy(cmdp);
    return -1;
  }
  memset(&nopts, 0, sizeof(nopts));
  nopts.y = ncplane_dim_y(stdn) - 8;
  nopts.horiz.x = ncplane_dim_x(cmdp);
  nopts.rows = 8;
  nopts.cols = ncplane_dim_x(stdn) - ncplane_dim_x(cmdp);
  struct ncplane* plotp = ncplane_create(stdn, &nopts);
  ncplane_set_base(plotp, " ", 0, CHANNELS_RGB_INITIALIZER(0, 0, 0, 0, 0, 0));
  ncplane_set_scrolling(plotp, true);
  ncplane_set_fg_rgb(plotp, 0x40f0c0);
  ncplane_printf(plotp, "WarMECH approaches!\n");
  ncplane_move_top(p->splane);
  if(battle_loop(plotp, p, cmdsel)){
    return -1;
  }
  ncplane_destroy(plotp);
  ncselector_destroy(cmdsel, NULL);
  return 0;
}

static int
overworld_battle(struct ncplane* map, player *p){
  if(random() % 30 != 0){
    return 0;
  }
  struct ncplane* copy = ncplane_dup(map, NULL);
  if(!copy){
    return -1;
  }
  struct ncvisual* ncv = ncvisual_from_file("warmech.bmp");
  if(ncv == NULL){
    ncplane_destroy(copy);
    return -1;
  }
  ncplane_greyscale(copy);
  int dimy, dimx;
  struct ncplane* stdn = notcurses_stddim_yx(p->nc, &dimy, &dimx);
  struct ncplane_options nopts = {
    .y = 1,
    .horiz.align = NCALIGN_CENTER,
    .rows = dimy / 4 * 3,
    .cols = dimx / 4 * 3,
    .flags = NCPLANE_OPTION_HORALIGNED,
  };
  struct ncplane* ep = ncplane_create(stdn, &nopts);
  uint64_t channels = 0;
  channels_set_fg_alpha(&channels, CELL_ALPHA_TRANSPARENT);
  channels_set_bg_alpha(&channels, CELL_ALPHA_TRANSPARENT);
  ncplane_set_base(ep, "", 0, channels);
  struct ncvisual_options vopts = {
    .scaling = NCSCALE_SCALE,
    .blitter = NCBLIT_2x2,
    .n = ep,
  };
  ncvisual_render(p->nc, ncv, &vopts);
  int r = do_battle(ep, p);
  ncplane_destroy(ep);
  ncplane_destroy(copy);
  ncvisual_destroy(ncv);
  return r;
}

static int
advance_player(player* p){
  struct ncvisual_options vopts = {
    .blitter = NCBLIT_2x2,
    .n = p->ncps[p->direction],
  };
  if(ncvisual_decode_loop(p->ncvs[p->direction]) < 0){
    return -1;
  }
  ncplane_erase(vopts.n);
  if(ncvisual_render(p->nc, p->ncvs[p->direction], &vopts) == NULL){
    return -1;
  }
  ncplane_move_top(p->ncps[p->direction]);
  center_plane(p->ncps[p->direction]);
  update_stats(p);
  return 0;
}

static int
input_loop(player* p, struct mapdata* mapd){
  struct ncplane* map = mapd->map;
  if(load_celes(p)){
    return -1;
  }
  int dimy, dimx;
  ncplane_dim_yx(notcurses_stdplane(p->nc), &dimy, &dimx);
  // coneria is at 2625, 2425
  int mapy = ((-2616.0 / mapd->y) * (mapd->y / mapd->toy)) + (dimy / mapd->toy);
  int mapx = ((-2438.0 / mapd->x) * (mapd->x / mapd->tox)) + (dimx / mapd->tox);
  ncplane_move_yx(map, mapy, mapx);
  p->direction = DIR_DOWN;
  ncplane_move_top(p->ncps[p->direction]);
  notcurses_render(p->nc);
  char32_t c;
  while((c = notcurses_getc_blocking(p->nc, NULL)) != -1){
    ncplane_move_bottom(p->ncps[p->direction]);
    if(c == 'q'){
      return 0;
    }else if(c == NCKEY_LEFT || c == 'h'){
      mapx += 8;
      p->direction = DIR_LEFT;
    }else if(c == NCKEY_RIGHT || c == 'l'){
      mapx -= 8;
      p->direction = DIR_RIGHT;
    }else if(c == NCKEY_UP || c == 'k'){
      mapy += 5;
      p->direction = DIR_UP;
    }else if(c == NCKEY_DOWN || c == 'j'){
      mapy -= 5;
      p->direction = DIR_DOWN;
    }else if(c == NCKEY_RESIZE){
      notcurses_refresh(p->nc, &dimy, &dimx);
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
    advance_player(p);
    if(overworld_battle(map, p)){
      break;
    }
    notcurses_render(p->nc);
  }
  return -1;
}

static int
display_logo(struct notcurses* nc){
  struct ncvisual* logo = ncvisual_from_file("logo.png");
  struct ncvisual_options vopts = {
    .scaling = NCSCALE_STRETCH,
  };
  struct ncplane* n = ncvisual_render(nc, logo, &vopts);
  if(n == NULL){
    return -1;
  }
  struct timespec ts = { .tv_sec = 1, .tv_nsec = 0, };
  ncplane_fadein(n, &ts, NULL, NULL);
  ncplane_set_fg_rgb(n, 0xffffff);
  ncplane_set_styles(n, NCSTYLE_BOLD | NCSTYLE_ITALIC);
  ncplane_putstr_aligned(n, ncplane_dim_y(n) - 1, NCALIGN_CENTER, "press enter");
  notcurses_render(nc);
  ncvisual_destroy(logo);
  return 0;
}

static int
debwarrior(player* p){
  display_logo(p->nc);
  struct ncvisual* ncv = ncvisual_from_file("ffi.png");
  if(!ncv){
    return -1;
  }
  struct ncvisual_options vopts = {
    .blitter = NCBLIT_2x2,
  };
  struct mapdata mapd = {
    .map = ncvisual_render(p->nc, ncv, &vopts),
  };
  if(mapd.map == NULL){
    return -1;
  }
  ncvisual_geom(p->nc, ncv, &vopts, &mapd.y, &mapd.x, &mapd.toy, &mapd.tox);
  struct ncplane_options nopts = {
    .horiz = { .align = NCALIGN_CENTER, },
    .rows = 1,
    .cols = 16,
    .flags = NCPLANE_OPTION_HORALIGNED,
  };
  p->hp = random() % 14 + 5;
  if((p->splane = ncplane_create(notcurses_stdplane(p->nc), &nopts)) == NULL){
    return -1;
  }
  ncplane_set_base(p->splane, " ", 0, CHANNELS_RGB_INITIALIZER(0, 0, 0, 0, 0, 0));
  int mapy, mapx;
  ncplane_yx(mapd.map, &mapy, &mapx);
  update_stats(p);
  char32_t c;
  while((c = notcurses_getc_blocking(p->nc, NULL)) != -1){
    if(c == NCKEY_ENTER){
      break;
    }
  }
  return input_loop(p, &mapd);
}

int main(void){
  player p = {0};
  p.nc = notcurses_init(NULL, NULL);
  if(p.nc == NULL){
    return EXIT_FAILURE;
  }
  int r = debwarrior(&p);
  notcurses_stop(p.nc);
  return r ? EXIT_FAILURE : EXIT_SUCCESS;
}
