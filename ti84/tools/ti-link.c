// ti-link: talk to a USB TI-84 Plus CE. Usage: ti-link info | ti-link send FILE...
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ticables.h>
#include <tifiles.h>
#include <ticalcs.h>

static int check(CalcHandle *c, int err, const char *what) {
  if (err) { fprintf(stderr, "%s failed (error %d)\n", what, err); }
  return err;
}

int main(int argc, char **argv) {
  if (argc < 2) { fprintf(stderr, "usage: ti-link info | send FILE...\n"); return 2; }
  ticables_library_init(); tifiles_library_init(); ticalcs_library_init();
  CableHandle *cable = ticables_handle_new(CABLE_USB, PORT_1);
  CalcHandle *calc = ticalcs_handle_new(CALC_TI84PCE_USB);
  if (!cable || !calc) { fprintf(stderr, "handle alloc failed\n"); return 1; }
  ticables_options_set_timeout(cable, 50);
  int rc = 0;
  if (check(calc, ticalcs_cable_attach(calc, cable), "attach")) return 1;
  if (!strcmp(argv[1], "reset")) { int e = ticables_cable_reset(cable); printf("reset %d\n", e); return e; }
  if (strcmp(argv[1], "info")) check(calc, ticalcs_calc_isready(calc), "isready");
  if (!strcmp(argv[1], "info")) {
    CalcInfos in; memset(&in, 0, sizeof in);
    if (check(calc, ticalcs_calc_get_version(calc, &in), "get_version")) rc = 1;
    else { char s[2048]; ticalcs_infos_to_string(&in, s, sizeof s); puts(s); }
  } else if (!strcmp(argv[1], "shot") && argc > 2) {
    CalcScreenCoord sc; memset(&sc, 0, sizeof sc); sc.format = SCREEN_FULL; uint8_t *bmp = NULL;
    if (check(calc, ticalcs_calc_recv_screen_rgb888(calc, &sc, &bmp), "screenshot")) rc = 1;
    else { FILE *f = fopen(argv[2], "wb"); fprintf(f, "P6\n%d %d\n255\n", sc.width, sc.height); fwrite(bmp, 3, sc.width * sc.height, f); fclose(f); printf("saved %dx%d\n", sc.width, sc.height); }
  } else if (!strcmp(argv[1], "run") && argc > 2) {
    VarEntry ve; memset(&ve, 0, sizeof ve); strncpy(ve.name, argv[2], sizeof ve.name - 1); ve.type = 0x06;
    if (check(calc, ticalcs_calc_execute(calc, &ve, ""), "execute")) rc = 1; else puts("started");
  } else if (!strcmp(argv[1], "get") && argc > 3) {
    VarEntry ve; memset(&ve, 0, sizeof ve); strncpy(ve.name, argv[2], sizeof ve.name - 1); ve.type = 0x15;
    if (check(calc, ticalcs_calc_recv_var2(calc, MODE_NORMAL, argv[3], &ve), "recv")) rc = 1; else printf("got %s\n", argv[2]);
  } else if (!strcmp(argv[1], "rm") && argc > 2) {
    for (int i = 2; i < argc; i++) {
      VarEntry ve; memset(&ve, 0, sizeof ve); strncpy(ve.name, argv[i], sizeof ve.name - 1); ve.type = 0x15;
      if (check(calc, ticalcs_calc_del_var(calc, &ve), argv[i])) rc = 1;
    }
  } else if (!strcmp(argv[1], "keys")) {
    for (int i = 2; i < argc && !rc; i++) if (check(calc, ticalcs_calc_send_key(calc, (uint32_t)strtoul(argv[i], NULL, 0)), "key")) rc = 1;
  } else if (!strcmp(argv[1], "romdump") && argc > 2) {
    if (check(calc, ticalcs_calc_dump_rom_1(calc), "dumper")) rc = 1;
    else if (check(calc, ticalcs_calc_dump_rom_2(calc, ROMSIZE_4MB, argv[2]), "dump")) rc = 1;
    else printf("dumped %s\n", argv[2]);
  } else if (!strcmp(argv[1], "ls")) {
    GNode *vars = NULL, *apps = NULL;
    if (check(calc, ticalcs_calc_get_dirlist(calc, &vars, &apps), "dirlist")) rc = 1; else ticalcs_dirlist_display(vars);
  } else if (!strcmp(argv[1], "send")) {
    for (int i = 2; i < argc; i++) {
      const char *f = argv[i];
      int e = tifiles_file_is_app(f) ? ticalcs_calc_send_app2(calc, f) : ticalcs_calc_send_var2(calc, MODE_NORMAL, f);
      if (check(calc, e, f)) { rc = 1; break; }
      printf("sent %s\n", f);
    }
  }
  ticalcs_cable_detach(calc); ticalcs_handle_del(calc); ticables_handle_del(cable);
  return rc;
}
