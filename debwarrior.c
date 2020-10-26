#include <stdlib.h>
#include <notcurses/notcurses.h>

static int
ncplane_make_opaque(struct ncplane* ncp){
  return ncplane_set_base(ncp, " ", 0, CHANNELS_RGB_INITIALIZER(0, 0, 0, 0, 0, 0));
}

static int
center_plane(struct ncplane* n){
  int dimy, dimx;
  const struct ncplane* p = ncplane_parent_const(n);
  ncplane_dim_yx(p, &dimy, &dimx);
  return ncplane_move_yx(n, ncplane_dim_y(p) / 2 - ncplane_dim_y(n) / 2,
                         ncplane_dim_x(p) / 2 - ncplane_dim_x(n) / 2 + 1);
}

typedef struct player {
  int hp;
  struct ncplane* splane; // stats plane
} player;

static int
update_stats(player* p){
  ncplane_erase(p->splane);
  ncplane_set_fg_rgb(p->splane, 0x00ffff);
  ncplane_printf_aligned(p->splane, 0, NCALIGN_CENTER, "HP: %d", p->hp);
  return 0;
}

static int
load_celes(struct notcurses* nc, struct ncvisual** ncvs, struct ncplane** celes){
  const char names[] = "lfrb";
  char fn[11] = "celesX.gif";
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
    center_plane(celes[i]);
  }
  return 0;
}

static int
battle_loop(struct notcurses* nc, struct ncplane* plotp, player *p,
            struct ncselector* cmdsel){
  while(p->hp > 0){
    ncplane_set_fg_rgb(plotp, 0xf0f0f0);
    ncplane_printf(plotp, "Choose an action.\n");
    ncplane_set_fg_rgb(plotp, 0x00ff88);
    notcurses_render(nc);
    ncinput ni;
    char32_t ch;
    notcurses_render(nc);
    while((ch = notcurses_getc_blocking(nc, &ni))){
      if(!ncselector_offer_input(cmdsel, &ni)){
        if(ch == NCKEY_ENTER){
          ncplane_printf(plotp, "Your attack is ineffective against the almighty WarMECH!\n");
          break;
        }
      }
      notcurses_render(nc);
    }
    ncplane_printf(plotp, "WarMECH attacks with NUKE\n");
    int damage = random() % 5 + 2;
    ncplane_printf(plotp, "You are hit for %d HP\n", damage);
    if((p->hp -= damage) < 0){
      p->hp = 0;
    }
    update_stats(p);
    notcurses_render(nc);
  }
  ncplane_set_fg_rgb(plotp, 0xff8080);
  ncplane_printf(plotp, "Thou art dead, bitch 💣\n");
  notcurses_render(nc);
  return -1;
}

static int
do_battle(struct notcurses* nc, struct ncplane* ep, player* p){
  struct ncplane* cmdp = ncplane_new(notcurses_stdplane(nc), 10, 30,
                                     ncplane_dim_y(notcurses_stdplane(nc)) - 10,
                                     0, NULL, "acts");
  ncplane_set_base(cmdp, " ", NCSTYLE_NONE, 0);
  static struct ncselector_item items[] = {
    { "Salaminize", "", },
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
  struct ncplane* plotp = ncplane_new(notcurses_stdplane(nc), 8,
                                      ncplane_dim_x(notcurses_stdplane(nc)) - ncplane_dim_x(cmdp),
                                      ncplane_dim_y(notcurses_stdplane(nc)) - 8,
                                      ncplane_dim_x(cmdp), NULL, "plot");
  ncplane_make_opaque(plotp);
  ncplane_set_scrolling(plotp, true);
  ncplane_set_fg_rgb(plotp, 0x40f0c0);
  ncplane_printf(plotp, "WarMECH approaches!\n");
  ncplane_move_top(p->splane);
  if(battle_loop(nc, plotp, p, cmdsel)){
    return -1;
  }
  ncplane_destroy(plotp);
  ncselector_destroy(cmdsel, NULL);
  return 0;
}

static int
overworld_battle(struct notcurses* nc, struct ncplane* map, player *p){
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
  ncplane_dim_yx(notcurses_stdplane(nc), &dimy, &dimx);
  const int ex = dimx / 4 * 3;
  const int exoff = ncplane_align(notcurses_stdplane(nc), NCALIGN_CENTER, ex);
  struct ncplane* ep = ncplane_new(notcurses_stdplane(nc), dimy / 4 * 3, ex, 1, exoff, NULL, NULL);
  uint64_t channels = 0;
  channels_set_fg_alpha(&channels, CELL_ALPHA_TRANSPARENT);
  channels_set_bg_alpha(&channels, CELL_ALPHA_TRANSPARENT);
  ncplane_set_base(ep, "", 0, channels);
  struct ncvisual_options vopts = {
    .scaling = NCSCALE_SCALE,
    .blitter = NCBLIT_2x2,
    .n = ep,
  };
  ncvisual_render(nc, ncv, &vopts);
  int r = do_battle(nc, ep, p);
  ncplane_destroy(ep);
  ncplane_destroy(copy);
  ncvisual_destroy(ncv);
  return r;
}

static int
advance_player(struct notcurses* nc, player* p, struct ncvisual* ncv, struct ncplane* ncp){
  struct ncvisual_options vopts = {
    .scaling = NCSCALE_NONE,
    .blitter = NCBLIT_2x2,
    .n = ncp,
  };
  if(ncvisual_decode_loop(ncv) < 0){
    return -1;
  }
  if(ncvisual_render(nc, ncv, &vopts) == NULL){
    return -1;
  }
  ncplane_move_top(ncp);
  center_plane(ncp);
  update_stats(p);
  return 0;
}

static int
input_loop(struct notcurses* nc, player* p, struct ncplane* map){
  struct ncvisual* ncvs[4];
  struct ncplane* celes[4];
  if(load_celes(nc, ncvs, celes)){
    return -1;
  }
  int mapy = -1279, mapx = -1184; // FIXME define in terms of screen size
  ncplane_move_yx(map, mapy, mapx);
  int dimy, dimx;
  ncplane_dim_yx(notcurses_stdplane(nc), &dimy, &dimx);
  int celidx = 1;
  ncplane_move_top(celes[celidx]);
  notcurses_render(nc);
  char32_t c;
  while((c = notcurses_getc_blocking(nc, NULL)) != -1){
    ncplane_move_bottom(celes[celidx]);
    if(c == 'q'){
      return 0;
    }else if(c == NCKEY_LEFT || c == 'h'){
      mapx += 8;
      celidx = 0;
    }else if(c == NCKEY_RIGHT || c == 'l'){
      mapx -= 8;
      celidx = 2;
    }else if(c == NCKEY_UP || c == 'k'){
      mapy += 8;
      celidx = 3;
    }else if(c == NCKEY_DOWN || c == 'j'){
      mapy -= 8;
      celidx = 1;
    }else if(c == NCKEY_RESIZE){
      notcurses_refresh(nc, &dimy, &dimx);
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
    advance_player(nc, p, ncvs[celidx], celes[celidx]);
    if(overworld_battle(nc, map, p)){
      break;
    }
    notcurses_render(nc);
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
  notcurses_render(nc);
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
debwarrior(struct notcurses* nc){
  display_logo(nc);
  struct ncvisual* ncv = ncvisual_from_file("ffi.png");
  if(!ncv){
    return -1;
  }
  struct ncvisual_options vopts = {
    .scaling = NCSCALE_NONE,
    .blitter = NCBLIT_2x2,
  };
  struct ncplane* map = ncvisual_render(nc, ncv, &vopts);
  struct ncplane_options nopts = {
    .horiz = { .align = NCALIGN_CENTER, },
    .rows = 1,
    .cols = 16,
    .flags = NCPLANE_OPTION_HORALIGNED,
  };
  player p = {
    .hp = random() % 14 + 5,
    .splane = ncplane_create(notcurses_stdplane(nc), &nopts),
  };
  if(!p.splane){
    return -1;
  }
  ncplane_make_opaque(p.splane);
  int mapy, mapx;
  ncplane_yx(map, &mapy, &mapx);
  update_stats(&p);
  char32_t c;
  while((c = notcurses_getc_blocking(nc, NULL)) != -1){
    if(c == NCKEY_ENTER){
      break;
    }
  }
  return input_loop(nc, &p, map);
}

int main(void){
  struct notcurses_options nopts = {
    .flags = NCOPTION_NO_ALTERNATE_SCREEN | NCOPTION_SUPPRESS_BANNERS,
  };
  struct notcurses* nc = notcurses_init(&nopts, NULL);
  if(nc == NULL){
    return EXIT_FAILURE;
  }
  int r = debwarrior(nc);
  notcurses_stop(nc);
  return r ? EXIT_FAILURE : EXIT_SUCCESS;
}
