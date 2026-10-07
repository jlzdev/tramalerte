#include <Adafruit_GFX.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>

#include "depart.h"
#include "dessin.h"
#include "png.h"

using namespace depart;

static Passage passage(const char* ligne, const char* dest, int secondes, bool fiable = true) {
  Passage p;
  p.ligne = ligne;
  p.idLigne = ligne[0] == 'T' ? (ligne[1] == '1' ? "101" : "102") : "8";
  p.destination = dest;
  p.secondes = secondes;
  p.fiable = fiable;
  return p;
}

static int64_t maintenant() {
  struct tm t = {};
  t.tm_year = 126;
  t.tm_mon = 9;
  t.tm_mday = 10;
  t.tm_hour = 9;
  t.tm_min = 37;
  t.tm_sec = 12;
  t.tm_isdst = -1;
  return (int64_t)mktime(&t) * 1000;
}

static void sauver(GFXcanvas1& toile, const char* nom) {
  std::string chemin = std::string("sim/out/") + nom + ".png";
  if (ecrirePngMonochrome(chemin, toile.getBuffer(), dessin::LARGEUR, dessin::HAUTEUR)) printf("ecrit %s\n", chemin.c_str());
  else printf("ECHEC %s\n", chemin.c_str());
}

static dessin::Contenu base(const Verdict& v, int64_t now) {
  dessin::Contenu c = dessin::contenuDepuisVerdict(v, now);
  c.arret = "Battant";
  c.directions = "T1 > Chalezeule, T2 > Gare Viotte";
  c.heure = fmtHM(now);
  c.statut = "Temps réel Ginko 9h37, 192.168.1.42";
  return c;
}

int main() {
  setenv("TZ", "Europe/Paris", 1);
  tzset();
  GFXcanvas1 toile(dessin::LARGEUR, dessin::HAUTEUR);
  const int64_t now = maintenant();
  const Reglages r = REGLAGES_DEFAUT;

  {
    Verdict v = evaluer({passage("T1", "Chalezeule", 1500), passage("T2", "Gare Viotte", 1900), passage("T1", "Chalezeule", 2700)}, now, now, r);
    dessin::dessinerVerdict(toile, base(v, now));
    sauver(toile, "tranquille");
  }
  {
    Verdict v = evaluer({passage("T1", "Chalezeule", 520), passage("T2", "Gare Viotte", 1100), passage("T1", "Chalezeule", 1720)}, now, now, r);
    dessin::dessinerVerdict(toile, base(v, now));
    sauver(toile, "prepare");
  }
  {
    Verdict v = evaluer({passage("T1", "Chalezeule", 390), passage("T2", "Gare Viotte", 900), passage("T1", "Chalezeule", 1590, false)}, now, now, r);
    dessin::dessinerVerdict(toile, base(v, now));
    sauver(toile, "cours");
  }
  {
    Verdict v = evaluer({passage("T1", "Chalezeule", 200), passage("T2", "Gare Viotte", 1420)}, now, now, r);
    dessin::dessinerVerdict(toile, base(v, now));
    sauver(toile, "rate_puis_suivant");
  }
  {
    Verdict v = evaluer({}, now, now, r);
    dessin::Contenu c = base(v, now);
    c.detail = "Aucun passage annoncé pour tes directions.";
    c.statut = "Erreur Ginko : Ginko injoignable (read Timeout), 192.168.1.42";
    dessin::dessinerVerdict(toile, c);
    sauver(toile, "inconnu");
  }
  {
    Verdict v = evaluer({passage("T1", "Chalezeule", 1500)}, now, now, r);
    dessin::Contenu c = base(v, now);
    c.nuit = true;
    c.heure = "23h41";
    c.detail = "Prochain tram connu : T1 à 0h02";
    c.statut = "Mode nuit jusqu'à 6h, mise à jour toutes les 10 min";
    dessin::dessinerVerdict(toile, c);
    sauver(toile, "nuit");
  }
  {
    dessin::dessinerQr(toile, 41, [](int x, int y) { return ((x / 3) + (y / 3)) % 2 == 0 || (x < 7 && y < 7); },
                       "Wi-Fi", {"1. Scanne le QR code", "2. Choisis ton Wi-Fi", "", "Réseau :", "TramAlerte", "Mot de passe :", "tram1234"});
    sauver(toile, "qr_wifi");
  }
  {
    dessin::dessinerMessage(toile, "Clé API refusée", "Ouvre http://192.168.1.42/ sur ton téléphone pour coller une nouvelle clé Ginko.");
    sauver(toile, "message");
  }
  return 0;
}
