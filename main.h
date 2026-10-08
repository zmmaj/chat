
#include <adt/list.h>
#include <clipboard.h>
#include <ctype.h>
#include <io/kbd_event.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <str.h>



#define NAME  "srbin_chat"
#define NULL_DISPLAY  "Sistem: Dobrodosli u SrBinOS Chat!"

 // Broj redova istorije koje prikazujemo na ekranu odjednom
 #define PRIKAZ_REDOVA 12
 #define MAX_DUZINA_LINIJE 128

typedef struct {
    gfx_rect_t menubar_rect;
    gfx_rect_t input_rect;    // Polje za unos na dnu
    gfx_rect_t dugme_rect;    // Dugme "Posalji" pored unosa
} chat_geom_t;

typedef struct {
    ui_t *ui;
    ui_window_t *window;
    ui_fixed_t *fixed;
    ui_pbutton_t *dugme_posalji;
    ui_menu_bar_t *menubar;
    chat_geom_t geom;
    
    // Niz labela koje direktno renderuju tekst na prozor
    ui_label_t *labele_istorije[PRIKAZ_REDOVA];
    char tekst_linija[PRIKAZ_REDOVA][MAX_DUZINA_LINIJE];
    int trenutni_red;
} chat_app_t;

// --- NOVO: Strukture za bafer istorije poruka ---
#define MAX_ISTORIJA_LINIJA 15   // Koliko linija teksta staje u prozor odjednom
#define MAX_DUZINA_LINIJE   128  // Maksimalna dužina jedne poruke
#define VELICINA_EKRANSKOG_BAFERA (MAX_ISTORIJA_LINIJA * (MAX_DUZINA_LINIJE + 1))

typedef struct {
   char linije[MAX_ISTORIJA_LINIJA][MAX_DUZINA_LINIJE];
   int broj_linija;
} chat_istorija_t;


void chat_file_exit(ui_menu_entry_t *mentry, void *arg);
void dodaj_u_istoriju(const char *format, const char *autor, const char *tekst);
void dodaj_poruku_u_bitmape(chat_app_t *c, const char *autor, const char *tekst);