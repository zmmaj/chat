/*
 * Copyright (c) 2026 Ivica Jocic
 * SrBinOS Chat Application using native window and label bitmapped layout
 */

 #include <adt/list.h>
 #include <clipboard.h>
 #include <ctype.h>
 #include <errno.h>
 #include <io/kbd_event.h>
 #include <stdbool.h>
 #include <stdio.h>
 #include <stdlib.h>
 #include <str.h>
 #include <string.h>
 #include <ui/entry.h>
 #include <ui/fixed.h>
 #include <ui/label.h>
 #include <ui/menu.h>
 #include <ui/menubar.h>
 #include <ui/menudd.h>
 #include <ui/menuentry.h>
 #include <ui/pbutton.h>
 #include <ui/ui.h>
 #include <ui/window.h>
 #include "main.h"
 #include "chat_mreza.h"

 
 static chat_app_t chat;
 // Kreiramo globalnu instancu mrežne strukture
#define GEMINI_API_KEY "AIzaSyB38W3tqqjA-Z60x3z-5UfjUyVoSROA0WI"
static chat_mreza_handle_t chat_konekcija = NULL;
 static ui_entry_t *input_entry; // Ostaje samo donji unos za tastaturu
 
 static void chat_edit_copy(ui_menu_entry_t *, void *);
 static void chat_edit_paste(ui_menu_entry_t *, void *);
 static void chat_posalji_clicked(ui_pbutton_t *, void *);
 
 static ui_pbutton_cb_t pbutton_cb = {
     .clicked = chat_posalji_clicked
 };
 
 static void wnd_close(ui_window_t *, void *);
 static void wnd_kbd_event(ui_window_t *, void *, kbd_event_t *);
 
 static ui_window_cb_t window_cb = {
     .close = wnd_close,
     .kbd = wnd_kbd_event
 };
 
 static void wnd_close(ui_window_t *window, void *arg)
 {
     chat_app_t *c = (chat_app_t *) arg;
     ui_quit(c->ui);
 }
 
 static void wnd_kbd_event(ui_window_t *window, void *arg, kbd_event_t *event)
 {
     chat_app_t *c = (chat_app_t *) arg;
 
     if (ui_window_def_kbd(window, event) == ui_claimed)
         return;
 
     if (event->type == KEY_PRESS && (event->mods & KM_CTRL) != 0) {
         switch (event->key) {
         case KC_C:
             chat_edit_copy(NULL, c);
             break;
         case KC_V:
             chat_edit_paste(NULL, c);
             break;
         default:
             break;
         }
     }
 
     if (event->key == KC_ENTER) {
         if (event->type == KEY_PRESS)
             ui_pbutton_press(c->dugme_posalji);
         else
             ui_pbutton_release(c->dugme_posalji);
     }
 }
 

 // Pomoćna funkcija koja dodaje isključivo jedan red teksta u sistemske labele
 static void push_jedan_red_u_labele(chat_app_t *c, const char *ceo_red)
 {
     // Obezbeđujemo se da prosleđeni string nije NULL
     if (c == NULL || ceo_red == NULL) {
         return;
     }
 
     if (c->trenutni_red < PRIKAZ_REDOVA) {
         // Proveravamo da li je labele uopšte alocirana pre upisa
         if (c->labele_istorije[c->trenutni_red] != NULL) {
             str_ncpy(c->tekst_linija[c->trenutni_red], MAX_DUZINA_LINIJE, ceo_red, MAX_DUZINA_LINIJE - 1);
             ui_label_set_text(c->labele_istorije[c->trenutni_red], c->tekst_linija[c->trenutni_red]);
         }
         c->trenutni_red++;
     } else {
         // Ako je prozor pun, pomeramo tekstove za 1 mesto na gore
         for (int i = 1; i < PRIKAZ_REDOVA; i++) {
             str_ncpy(c->tekst_linija[i - 1], MAX_DUZINA_LINIJE, c->tekst_linija[i], MAX_DUZINA_LINIJE - 1);
             if (c->labele_istorije[i - 1] != NULL) {
                 ui_label_set_text(c->labele_istorije[i - 1], c->tekst_linija[i - 1]);
             }
         }
         // Upisujemo najnoviji prelomljeni red u poslednju labelu na dnu
         str_ncpy(c->tekst_linija[PRIKAZ_REDOVA - 1], MAX_DUZINA_LINIJE, ceo_red, MAX_DUZINA_LINIJE - 1);
         if (c->labele_istorije[PRIKAZ_REDOVA - 1] != NULL) {
             ui_label_set_text(c->labele_istorije[PRIKAZ_REDOVA - 1], c->tekst_linija[PRIKAZ_REDOVA - 1]);
         }
     }
 }
 
 // Glavna funkcija: Autor u svom redu, tekst ispod izlomljen na 44 karaktera
 void dodaj_poruku_u_bitmape(chat_app_t *c, const char *autor, const char *tekst)
 {
     if (c == NULL || autor == NULL || tekst == NULL) {
         return;
     }
 
     // POPRAVKA: Definišemo "red_autora" kao pravi fiksni niz karaktera na steku!
     char red_autora[MAX_DUZINA_LINIJE];
     snprintf(red_autora, sizeof(red_autora), "[%s]", autor);
     push_jedan_red_u_labele(c, red_autora);
 
     // Prelamamo sam tekst poruke nezavisno od autora
     char *ptr = (char *)tekst;
     char bafer_reda[MAX_DUZINA_LINIJE];
     int maksimalna_sirina_linije = 44; 
 
     while (*ptr != '\0') {
         int len = str_length(ptr);
 
         if (len <= maksimalna_sirina_linije) {
             push_jedan_red_u_labele(c, ptr);
             break;
         }
 
         int prelom = maksimalna_sirina_linije;
         while (prelom > 0 && ptr[prelom] != ' ' && ptr[prelom] != '\0') {
             prelom--;
         }
 
         if (prelom == 0) {
             prelom = maksimalna_sirina_linije;
         }
 
         str_ncpy(bafer_reda, sizeof(bafer_reda), ptr, prelom);
         bafer_reda[prelom] = '\0';
 
         push_jedan_red_u_labele(c, bafer_reda);
 
         ptr += prelom;
         
         if (*ptr == ' ') {
             ptr++;
         }
     }
 
     // Bezbedno osvežavamo prozor
     if (c->window != NULL) {
         (void) ui_window_paint(c->window);
     }
 }
 
 // Akcija kada se klikne na dugme "Posalji"
 static void chat_posalji_clicked(ui_pbutton_t *pbutton, void *arg)
{
	(void) pbutton;
	(void) arg;
	
	if (input_entry == NULL) {
		return;
	}
	
	const char *tekst_unosa = ui_entry_get_text(input_entry);
	
	if (tekst_unosa != NULL && str_cmp(tekst_unosa, "") != 0) {
		dodaj_poruku_u_bitmape(&chat, "Ti", tekst_unosa);
		
		(void) ui_entry_set_text(input_entry, (void *) "");
		ui_entry_paint(input_entry);

		dodaj_poruku_u_bitmape(&chat, "Sistem", "Povezujem se na Google...");

		// Otvaramo bezbednu HTTPS vezu preko stabilnog Pauk mrežnog sloja
		errno_t rc = chat_otvori_https_vezu(GEMINI_HOST, GEMINI_PORT, &chat_konekcija);
		if (rc != EOK) {
			char greska_poruka[128];
			snprintf(greska_poruka, sizeof(greska_poruka), "Mrezna greska! Kod: %d", rc);
			dodaj_poruku_u_bitmape(&chat, "Sistem", greska_poruka);
			return;
		}

		// POPRAVKA: Dinamički alociramo bafere na hipu umesto na steku dretve!
        char *json_payload = malloc(2048);
		char *http_zahtev = malloc(4096);
		char *prijemni_bafer = malloc(8192);

		if (!json_payload || !http_zahtev || !prijemni_bafer) {
			dodaj_poruku_u_bitmape(&chat, "Sistem", "Greska: Van memorije za bafere.");
			if (json_payload) free(json_payload);
			if (http_zahtev) free(http_zahtev);
			if (prijemni_bafer) free(prijemni_bafer);
			chat_oslobodi_https(chat_konekcija);
			chat_konekcija = NULL;
			return;
		}

      // 1. Čistimo bafere na nulu pre svakog upisa (Sinhronizovano)
	// 1. Čistimo bafere na nulu pre svakog upisa (Sinhronizovano)
    memset(json_payload, 0, 2048);
    memset(http_zahtev, 0, 4096);
    memset(prijemni_bafer, 0, 8192); // POPRAVLJENO: Sada je tačno ime promenljive!

    // 2. Čist i standardan snprintf na jednoj liniji
    snprintf(json_payload, 2048, "{\"contents\":[{\"parts\":[{\"text\":\"%s\"}]}]}", tekst_unosa);

    int payload_len = (int)strlen(json_payload);

    // 3. Formiramo HTTP POST zahtev sa preciznom dužinom sadržaja
    snprintf(http_zahtev, 4096,
             "POST /v1beta/models/gemini-3.5-flash:generateContent?key=%s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n\r\n"
             "%s",
             GEMINI_API_KEY, GEMINI_HOST, payload_len, json_payload);

		dodaj_poruku_u_bitmape(&chat, "Sistem", "Saljem upit...");

		if (chat_posalji_https(chat_konekcija, http_zahtev, strlen(http_zahtev)) != EOK) {
			dodaj_poruku_u_bitmape(&chat, "Sistem", "Greska pri slanju podataka.");
			free(json_payload); free(http_zahtev); free(prijemni_bafer);
			chat_oslobodi_https(chat_konekcija);
			chat_konekcija = NULL;
			return;
		}

		dodaj_poruku_u_bitmape(&chat, "Sistem", "Cekam odgovor...");

		memset(prijemni_bafer, 0, 4096);
		size_t procitano = 0;
		int pokusaji_citanja = 0;
		
		while (pokusaji_citanja < 50) {
			rc = chat_primi_https(chat_konekcija, prijemni_bafer, 4096, &procitano);
			if (rc == EOK && procitano > 0) {
				break;
			}
			fibril_usleep(10000);
			pokusaji_citanja++;
		}
		
		if (procitano > 0) {
			// USPEH: Prikazujemo sirovi odgovor na bitmapama labela!
			dodaj_poruku_u_bitmape(&chat, "Sistem", prijemni_bafer);
		} else {
			dodaj_poruku_u_bitmape(&chat, "Sistem", "Server je zatvorio vezu ili je doslo do greske.");
		}

		// Obavezno oslobađamo dinamičku memoriju sa hipa
		free(json_payload);
		free(http_zahtev);
		free(prijemni_bafer);

		chat_oslobodi_https(chat_konekcija);
		chat_konekcija = NULL;
	}
}
 
void chat_file_exit(ui_menu_entry_t *mentry, void *arg)
 {
     chat_app_t *c = (chat_app_t *) arg;
     ui_quit(c->ui);
 }
 
 static void chat_edit_copy(ui_menu_entry_t *mentry, void *arg)
 {
     const char *str = ui_entry_get_text(input_entry);
     if (str != NULL)
         (void) clipboard_put_str(str);
 }
 
 static void chat_edit_paste(ui_menu_entry_t *mentry, void *arg)
 {
     char *str;
     errno_t rc;
 
     rc = clipboard_get_str(&str);
     if (rc != EOK)
         return;
 
     (void) ui_entry_set_text(input_entry, (void *) str);
     ui_entry_paint(input_entry);
     free(str);
 }
 
 int main(int argc, char *argv[])
 {
     const char *dspec = UI_ANY_DEFAULT;
     ui_t *ui = NULL;
     ui_wnd_params_t params;
     ui_window_t *window = NULL;
     ui_resource_t *ui_res;
     gfx_rect_t rect;
     errno_t rc;
 
     rc = ui_create(dspec, &ui);
     if (rc != EOK) {
         printf("Greska pri kreiranju UI na displeju %s.\n", dspec);
         return rc;
     }
 
     ui_wnd_params_init(&params);
     params.caption = "SrBinOS Ćaskalica (Bitmap Mode)";
     params.rect.p0.x = 0;
     params.rect.p0.y = 0;
     params.rect.p1.x = 400;
     params.rect.p1.y = 500;
 
     memset((void *) &chat, 0, sizeof(chat));
     chat.ui = ui;
     chat.trenutni_red = 0;
 
     rc = ui_window_create(ui, &params, &window);
     if (rc != EOK) {
         printf("Greska pri kreiranju prozora.\n");
         return rc;
     }
 
     ui_window_set_cb(window, &window_cb, (void *) &chat);
     chat.window = window;
 
     ui_res = ui_window_get_res(window);
 
     rc = ui_fixed_create(&chat.fixed);
     if (rc != EOK) return rc;
 
     // Podešavanje geometrije za meni bar, unos i dugme
     chat.geom.menubar_rect.p0.x = 4;
     chat.geom.menubar_rect.p0.y = 30;
     chat.geom.menubar_rect.p1.x = params.rect.p1.x - 4;
     chat.geom.menubar_rect.p1.y = 52;
 
     chat.geom.input_rect.p0.x = 10;
     chat.geom.input_rect.p0.y = 440;
     chat.geom.input_rect.p1.x = 290;
     chat.geom.input_rect.p1.y = 480;
 
     chat.geom.dugme_rect.p0.x = 300;
     chat.geom.dugme_rect.p0.y = 440;
     chat.geom.dugme_rect.p1.x = 390;
     chat.geom.dugme_rect.p1.y = 480;
 
     // Inicijalizacija Menu Bar-a
     rc = ui_menu_bar_create(ui, window, &chat.menubar);
     if (rc != EOK) return rc;
     ui_menu_bar_set_rect(chat.menubar, &chat.geom.menubar_rect);
     rc = ui_fixed_add(chat.fixed, ui_menu_bar_ctl(chat.menubar));
     if (rc != EOK) return rc;
 
     // --- NOVO: Dinamičko kreiranje X/Y Bitmap labela za istoriju ---
     for (int i = 0; i < PRIKAZ_REDOVA; i++) {
         chat.tekst_linija[i][0] = '\0';
         
         // Inicijalna poruka dobrodošlice na prvom redu
         if (i == 0) {
             strcpy(chat.tekst_linija[i], "Sistem: Dobrodosli u SrBinOS Chat!");
         }
 
         rc = ui_label_create(ui_res, chat.tekst_linija[i], &chat.labele_istorije[i]);
         if (rc != EOK) {
             printf("Greska pri kreiranju labele %d.\n", i);
             return rc;
         }
 
         // Računamo X i Y poziciju za svaki red teksta (od Y=65 na dole, razmak 28px)
         rect.p0.x = 15;
         rect.p0.y = 65 + (i * 28);
         rect.p1.x = 385;
         rect.p1.y = rect.p0.y + 22;
 
         ui_label_set_rect(chat.labele_istorije[i], &rect);
         ui_label_set_halign(chat.labele_istorije[i], gfx_halign_left);
 
         rc = ui_fixed_add(chat.fixed, ui_label_ctl(chat.labele_istorije[i]));
         if (rc != EOK) return rc;
     }
     chat.trenutni_red = 1; // Pošto smo zauzeli nulti red za dobrodošlicu
 
     // Kreiranje polja za unos teksta poruke na dnu
     rc = ui_entry_create(window, "", &input_entry);
     if (rc != EOK) return rc;
     ui_entry_set_rect(input_entry, &chat.geom.input_rect);
     ui_entry_set_halign(input_entry, gfx_halign_left);
     ui_entry_set_read_only(input_entry, false);
     rc = ui_fixed_add(chat.fixed, ui_entry_ctl(input_entry));
     if (rc != EOK) return rc;
 
     // Kreiranje dugmeta "Posalji"
     rc = ui_pbutton_create(ui_res, "Posalji", &chat.dugme_posalji);
     if (rc != EOK) return rc;
     ui_pbutton_set_cb(chat.dugme_posalji, &pbutton_cb, (void *) &chat);
     ui_pbutton_set_rect(chat.dugme_posalji, &chat.geom.dugme_rect);
     rc = ui_fixed_add(chat.fixed, ui_pbutton_ctl(chat.dugme_posalji));
     if (rc != EOK) return rc;
 
     ui_pbutton_set_default(chat.dugme_posalji, true);
 
     ui_window_add(window, ui_fixed_ctl(chat.fixed));
 
     rc = ui_window_paint(window);
     if (rc != EOK) {
         printf("Neuspelo bojanje prozora.\n");
         return rc;
     }
 
     ui_run(ui);
     ui_window_destroy(window);
     ui_destroy(ui);
 
     return 0;
 }
 