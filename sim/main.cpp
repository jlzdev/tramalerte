#include <Adafruit_GFX.h>

#include <cstdio>
#include <cstdlib>
#include <ctime>

#include "depart.h"
#include "dessin.h"
#include "png.h"

using namespace depart;

static Passage passage(const char* ligne, const char* dest, int secondes, bool fiable = true, bool bus = false) {
  Passage p;
  p.ligne = ligne;
  p.idLigne = ligne[0] == 'T' ? (ligne[1] == '1' ? "101" : "102") : "8";
  p.destination = dest;
  p.secondes = secondes;
  p.fiable = fiable;
  p.bus = bus;
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

static int problemes = 0;

static void sauver(GFXcanvas1& toile, const char* nom) {
  std::string chemin = std::string("sim/out/") + nom + ".png";
  if (ecrirePngMonochrome(chemin, toile.getBuffer(), dessin::LARGEUR, dessin::HAUTEUR)) printf("ecrit %s\n", chemin.c_str());
  else printf("ECHEC %s\n", chemin.c_str());
  if (getenv("ZONES")) for (const dessin::Zone& z : dessin::zonesCourantes()) printf("  zone %-28s x=%3d y=%3d w=%3d h=%3d\n", z.nom.c_str(), z.x, z.y, z.w, z.h);
  for (const std::string& p : dessin::zonesProblemes()) {
    printf("  PROBLEME %s : %s\n", nom, p.c_str());
    problemes++;
  }
  dessin::zonesReinitialiser();
}

static dessin::Meteo meteo(int64_t now) {
  dessin::Meteo m;
  m.valide = true;
  m.temperature = 12;
  m.code = 3;
  const int codes[3] = {2, 61, 3};
  const int mins[3] = {6, 11, 9};
  const int maxs[3] = {13, 16, 15};
  for (int i = 0; i < 3; i++) m.jours.push_back({dessin::nomJour(now, i), codes[i], mins[i], maxs[i]});
  return m;
}

static dessin::Contenu base(const Verdict& v, int64_t now) {
  dessin::Contenu c = dessin::contenuDepuisVerdict(v, now);
  c.departMin = 5;
  c.meteo = meteo(now);
  c.statut = "";
  return c;
}

int main() {
  setenv("TZ", "Europe/Paris", 1);
  tzset();
  GFXcanvas1 toile(dessin::LARGEUR, dessin::HAUTEUR);
  const int64_t now = maintenant();
  const Reglages r = REGLAGES_DEFAUT;

  {
    Verdict v = evaluer({passage("T1", "Chalezeule", 1500), passage("T1", "Chalezeule", 2700), passage("T1", "Chalezeule", 3900), passage("T1", "Chalezeule", 5100)}, now, now, r);
    dessin::dessinerVerdict(toile, base(v, now));
    sauver(toile, "tranquille");
  }
  {
    Verdict v = evaluer({passage("T1", "Chalezeule", 520), passage("T2", "Gare Viotte", 1100), passage("T1", "Chalezeule", 1720), passage("T2", "Gare Viotte", 2300)}, now, now, r);
    dessin::dessinerVerdict(toile, base(v, now));
    sauver(toile, "prepare");
  }
  {
    Verdict v = evaluer({passage("T1", "Chalezeule", 390), passage("T1", "Chalezeule", 1590, false), passage("T1", "Chalezeule", 2790)}, now, now, r);
    dessin::dessinerVerdict(toile, base(v, now));
    sauver(toile, "cours");
  }
  {
    Verdict v = evaluer({passage("8", "Espace Valentin", 200, true, true), passage("8", "Espace Valentin", 1420, true, true), passage("8", "Espace Valentin", 2620, true, true)}, now, now, r);
    dessin::dessinerVerdict(toile, base(v, now));
    sauver(toile, "bus_rate_puis_suivant");
  }
  {
    Verdict v = evaluer({}, now, now, r);
    dessin::Contenu c = base(v, now);
    c.message = "Ginko ne répond pas";
    c.statut = "Erreur Ginko : injoignable, 192.168.1.42";
    c.meteo.valide = false;
    dessin::dessinerVerdict(toile, c);
    sauver(toile, "inconnu");
  }
  {
    Verdict v = evaluer({passage("T1", "Chalezeule", 1500)}, now, now, r);
    dessin::Contenu c = base(v, now);
    c.nuit = true;
    c.heure = "23h41";
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
  printf(problemes ? "%d probleme(s) de mise en page\n" : "mise en page sans chevauchement\n", problemes);
  return problemes ? 1 : 0;
}
