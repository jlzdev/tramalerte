#include "ecran.h"

#include <GxEPD2_BW.h>
#include <qrcode.h>

#define EPD_CS 33
#define EPD_DC 25
#define EPD_RST 26
#define EPD_BUSY 27

#ifdef ECRAN_V1
using Panneau = GxEPD2_420;
#else
using Panneau = GxEPD2_420_GDEY042T81;
#endif

static GxEPD2_BW<Panneau, Panneau::HEIGHT> display(Panneau(EPD_CS, EPD_DC, EPD_RST, EPD_BUSY));
static bool premierAffichage = true;

void ecranInit() {
  display.init(0, true, 2, false);
  display.setRotation(0);
  display.clearScreen();
  premierAffichage = true;
}

template <typename F>
static void afficher(bool complet, F dessiner) {
  if (complet || premierAffichage) display.setFullWindow();
  else display.setPartialWindow(0, 0, display.width(), display.height());
  display.firstPage();
  do {
    dessiner();
  } while (display.nextPage());
  premierAffichage = false;
}

void ecranAfficherVerdict(const dessin::Contenu& contenu, bool complet) {
  afficher(complet, [&] { dessin::dessinerVerdict(display, contenu); });
}

void ecranAfficherQr(const char* contenu, const char* titre, const std::vector<String>& lignes) {
  QRCode qr;
  const uint8_t version = 6;
  std::vector<uint8_t> tampon(qrcode_getBufferSize(version));
  qrcode_initText(&qr, tampon.data(), version, ECC_LOW, contenu);
  std::vector<std::string> l;
  for (const String& s : lignes) l.push_back(s.c_str());
  afficher(true, [&] {
    dessin::dessinerQr(display, qr.size, [&](int x, int y) { return qrcode_getModule(&qr, x, y) != 0; }, titre, l);
  });
}

void ecranAfficherMessage(const char* titre, const char* texte) {
  afficher(true, [&] { dessin::dessinerMessage(display, titre, texte); });
}

void ecranDormir() {
  display.hibernate();
  premierAffichage = true;
}
