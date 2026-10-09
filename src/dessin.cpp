#include "dessin.h"

#include <cmath>
#include <ctime>

#include "polices/Chiffres26.h"
#include "polices/Gras11.h"
#include "polices/Gras13.h"
#include "polices/Gras18.h"
#include "polices/Gras9.h"
#include "polices/Icones12.h"
#include "polices/Icones16.h"
#include "polices/Icones24.h"
#include "polices/Petit7.h"
#include "polices/Texte9.h"

namespace dessin {

static const uint16_t NOIR = 0;
static const uint16_t BLANC = 1;
static const int MARGE = 12;
static const int SEP1 = 84, SEP2 = 196, SEP3 = 254;

#ifdef NATIVE
static std::vector<Zone> zones;
static void noter(int x, int y, int w, int h, TypeZone type, const std::string& nom) {
  if (w > 0 && h > 0) zones.push_back({x, y, w, h, type, nom});
}
void zonesReinitialiser() { zones.clear(); }
const std::vector<Zone>& zonesCourantes() { return zones; }
static bool croise(const Zone& a, const Zone& b) {
  return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}
static bool contient(const Zone& cadre, const Zone& z) {
  return z.x >= cadre.x && z.y >= cadre.y && z.x + z.w <= cadre.x + cadre.w && z.y + z.h <= cadre.y + cadre.h;
}
std::vector<std::string> zonesProblemes() {
  std::vector<std::string> out;
  for (size_t i = 0; i < zones.size(); i++) {
    const Zone& a = zones[i];
    if (a.type == TypeZone::Contenu && (a.x < 0 || a.y < 0 || a.x + a.w > LARGEUR || a.y + a.h > HAUTEUR)) out.push_back("hors ecran : " + a.nom);
    for (size_t j = i + 1; j < zones.size(); j++) {
      const Zone& b = zones[j];
      if (!croise(a, b)) continue;
      if (a.type == TypeZone::Contenu && b.type == TypeZone::Contenu) out.push_back("chevauchement : " + a.nom + " / " + b.nom);
      else if ((a.type == TypeZone::Contenu && b.type == TypeZone::Ligne) || (a.type == TypeZone::Ligne && b.type == TypeZone::Contenu)) out.push_back("ligne traversee : " + a.nom + " / " + b.nom);
      else if (a.type == TypeZone::Cadre && b.type == TypeZone::Contenu && !contient(a, b)) out.push_back("deborde du cadre : " + b.nom + " / " + a.nom);
      else if (b.type == TypeZone::Cadre && a.type == TypeZone::Contenu && !contient(b, a)) out.push_back("deborde du cadre : " + a.nom + " / " + b.nom);
    }
  }
  return out;
}
#else
static void noter(int, int, int, int, TypeZone, const std::string&) {}
#endif

std::string latin1(const std::string& utf8) {
  std::string out;
  for (size_t i = 0; i < utf8.size(); i++) {
    unsigned char c = utf8[i];
    if (c < 0x80) out += (char)c;
    else if ((c & 0xE0) == 0xC0 && i + 1 < utf8.size()) {
      unsigned code = ((c & 0x1F) << 6) | (utf8[i + 1] & 0x3F);
      out += code < 256 ? (char)code : '?';
      i++;
    } else if ((c & 0xF0) == 0xE0) {
      out += '?';
      i += 2;
    } else if ((c & 0xF8) == 0xF0) {
      out += '?';
      i += 3;
    }
  }
  return out;
}

static int largeur(Adafruit_GFX& g, const std::string& utf8) {
  int16_t x, y;
  uint16_t w, h;
  g.getTextBounds(latin1(utf8).c_str(), 0, 0, &x, &y, &w, &h);
  return w;
}

static void texte(Adafruit_GFX& g, int x, int y, const std::string& s, uint16_t couleur = NOIR) {
  std::string l = latin1(s);
  int16_t bx, by;
  uint16_t bw, bh;
  g.getTextBounds(l.c_str(), x, y, &bx, &by, &bw, &bh);
  noter(bx - 1, by - 1, bw + 2, bh + 2, TypeZone::Contenu, s);
  g.setTextColor(couleur);
  g.setCursor(x, y);
  g.print(l.c_str());
}

static std::string tronquer(Adafruit_GFX& g, const std::string& s, int largeurMax) {
  if (largeur(g, s) <= largeurMax) return s;
  std::string t = s;
  while (t.size() > 1) {
    t.pop_back();
    while (!t.empty() && (t.back() & 0xC0) == 0x80) t.pop_back();
    if (largeur(g, t + "...") <= largeurMax) return t + "...";
  }
  return t;
}

static void texteCentre(Adafruit_GFX& g, int cx, int y, const std::string& s, uint16_t couleur, int largeurMax) {
  std::string t = tronquer(g, s, largeurMax);
  texte(g, cx - largeur(g, t) / 2, y, t, couleur);
}

static void texteDroite(Adafruit_GFX& g, int xDroite, int y, const std::string& s, uint16_t couleur = NOIR) {
  texte(g, xDroite - largeur(g, s), y, s, couleur);
}

static void icone(Adafruit_GFX& g, const GFXfont& police, int x, int y, char lettre, const std::string& nom, uint16_t couleur = NOIR) {
  g.setFont(&police);
  char s[2] = {lettre, 0};
  int16_t bx, by;
  uint16_t bw, bh;
  g.getTextBounds(s, x, y, &bx, &by, &bw, &bh);
  noter(bx - 1, by - 1, bw + 2, bh + 2, TypeZone::Contenu, "icone " + nom);
  g.setTextColor(couleur);
  g.setCursor(x, y);
  g.write(lettre);
}

static void ligneH(Adafruit_GFX& g, int y, const std::string& nom) {
  g.drawFastHLine(MARGE, y, LARGEUR - 2 * MARGE, NOIR);
  noter(MARGE, y, LARGEUR - 2 * MARGE, 1, TypeZone::Ligne, nom);
}

static void cadre(Adafruit_GFX& g, int x, int y, int w, int h, const std::string& nom) {
  g.drawRoundRect(x, y, w, h, 8, NOIR);
  noter(x, y, w, h, TypeZone::Cadre, nom);
}

char iconeMeteo(int code, bool nuit) {
  if (code == 0) return nuit ? 'N' : 'D';
  if (code <= 2) return nuit ? 'O' : 'E';
  if (code == 3) return 'F';
  if (code == 45 || code == 48) return 'K';
  if (code >= 51 && code <= 57) return 'H';
  if (code == 61 || code == 63 || code == 80 || code == 81) return 'G';
  if (code == 65 || code == 82) return 'I';
  if (code == 66 || code == 67) return 'M';
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return 'J';
  if (code >= 95) return 'L';
  return 'F';
}

std::string nomJour(int64_t ms, int decalageJours) {
  static const char* JOURS[] = {"Dimanche", "Lundi", "Mardi", "Mercredi", "Jeudi", "Vendredi", "Samedi"};
  if (decalageJours == 0) return "Aujourd'hui";
  if (decalageJours == 1) return "Demain";
  time_t t = (time_t)(ms / 1000) + (time_t)decalageJours * 86400;
  struct tm local;
  localtime_r(&t, &local);
  return JOURS[local.tm_wday];
}

Contenu contenuDepuisVerdict(const depart::Verdict& v, int64_t nowMs, int arrondiSec) {
  Contenu c;
  c.etat = v.etat;
  c.heure = depart::fmtHM(nowMs);
  const depart::Candidat* cible = v.cibleOuNull();
  if (cible) {
    const depart::Passage& p = cible->passage;
    c.bus = p.bus;
    c.ligne = p.ligne;
    c.direction = p.destination;
    c.arrivee = depart::fmtHM(cible->tramMs);
    c.depart = depart::fmtHM(std::max(nowMs, cible->departMs));
    long reste = std::lround(std::fmax(0.0, cible->resteSec));
    if (arrondiSec > 1) reste -= reste % arrondiSec;
    if (v.etat == depart::Etat::Tranquille) {
      c.compteNombre = std::to_string(reste / 60);
      c.compteUnite = "min";
    } else if (reste >= 60) {
      c.compteNombre = std::to_string(reste / 60);
      c.compteUnite = "min " + std::string(reste % 60 < 10 ? "0" : "") + std::to_string(reste % 60) + " s";
    } else {
      c.compteNombre = std::to_string(reste);
      c.compteUnite = "s";
    }
    c.dans = "dans " + c.compteNombre + " " + c.compteUnite;
    if (!p.fiable) c.dans += " (théorique)";
  }
  for (const depart::Candidat& cand : v.candidats) {
    if (c.prochains.size() >= 4) break;
    Prochain p;
    p.ligne = cand.passage.ligne;
    p.heure = depart::fmtHM(cand.tramMs);
    p.rate = cand.etat == depart::Etat::Rate;
    p.cible = &cand == cible;
    c.prochains.push_back(p);
  }
  return c;
}

static void entete(Adafruit_GFX& g, const Contenu& c) {
  const int xDroit = LARGEUR - MARGE;
  g.setFont(&Gras9);
  int wHeure = largeur(g, c.heure) + 16;
  g.fillRoundRect(xDroit - wHeure, 8, wHeure, 22, 6, NOIR);
  noter(xDroit - wHeure, 8, wHeure, 22, TypeZone::Cadre, "pastille heure");
  texte(g, xDroit - wHeure + 8, 24, c.heure, BLANC);
  int xLimite = xDroit - 14;
  if (!c.nuit) {
    g.setFont(&Petit7);
    texteDroite(g, xDroit, 46, "Arrivée à l'arrêt");
    g.setFont(&Gras13);
    std::string arrivee = c.arrivee.empty() ? "--h--" : c.arrivee;
    int wArrivee = largeur(g, arrivee);
    texteDroite(g, xDroit, 72, arrivee);
    xLimite = xDroit - std::max(wArrivee, 90) - 14;
  }

  icone(g, Icones24, MARGE, 62, c.bus ? 'B' : 'A', "transport");
  const int xTexte = MARGE + 56;
  g.setFont(&Texte9);
  texte(g, xTexte, 34, tronquer(g, c.arret.empty() ? (c.bus ? "Prochain bus" : "Prochain tram") : c.arret, xLimite - xTexte));
  g.setFont(&Gras11);
  std::string ligne = c.nuit ? "Reprise à " + std::to_string(c.nuitFin) + "h" : (c.ligne.empty() ? "--" : c.ligne + " > " + c.direction);
  texte(g, xTexte, 64, tronquer(g, ligne, xLimite - xTexte));
  ligneH(g, SEP1, "separateur 1");
}

static void blocDepart(Adafruit_GFX& g, const Contenu& c) {
  const int wBox = 150, xBox = LARGEUR - MARGE - wBox;
  const int yBox = SEP1 + 10, hBox = SEP2 - SEP1 - 20;
  const int largeurTexte = xBox - MARGE - 10;
  const bool inconnu = c.etat == depart::Etat::Inconnu;
  g.setFont(&Texte9);
  texte(g, MARGE, SEP1 + 26, tronquer(g, inconnu ? "Aucun passage annoncé" : "Partir de chez soi dans", largeurTexte));
  g.setFont(&Chiffres26);
  std::string nombre = inconnu ? "--" : c.compteNombre;
  int wNombre = largeur(g, nombre);
  texte(g, MARGE, SEP1 + 72, nombre);
  if (!inconnu) {
    g.setFont(&Gras13);
    texte(g, MARGE + wNombre + 10, SEP1 + 72, tronquer(g, c.compteUnite, largeurTexte - wNombre - 10));
  }
  g.setFont(inconnu ? &Texte9 : &Gras13);
  std::string bas = inconnu ? c.message : "à " + c.depart + (c.dans.find("théorique") != std::string::npos ? " (théorique)" : "");
  texte(g, MARGE, SEP1 + 100, tronquer(g, bas, largeurTexte));

  cadre(g, xBox, yBox, wBox, hBox, "encadre marche");
  icone(g, Icones24, xBox + 8, yBox + 64, 'C', "pieton");
  g.setFont(&Gras13);
  texte(g, xBox + 48, yBox + 42, std::to_string(c.departMin) + " min");
  g.setFont(&Texte9);
  texte(g, xBox + 48, yBox + 64, "de marche");
  ligneH(g, SEP2, "separateur 2");
}

static void frise(Adafruit_GFX& g, const Contenu& c) {
  const int yTexte = SEP2 + 24, yLigne = SEP2 + 42;
  g.setFont(&Texte9);
  texte(g, MARGE, yTexte, "Prochains");
  if (c.prochains.empty()) {
    texte(g, 112, yTexte, "aucun passage annoncé");
  } else {
    const int n = (int)c.prochains.size();
    const int x0 = 150, x1 = LARGEUR - MARGE - 26;
    const int pas = n > 1 ? (x1 - x0) / (n - 1) : 0;
    bool plusieursLignes = false;
    for (const Prochain& p : c.prochains) plusieursLignes |= p.ligne != c.prochains[0].ligne;
    g.drawFastHLine(x0, yLigne, x1 - x0, NOIR);
    for (int i = 0; i < n; i++) {
      const Prochain& p = c.prochains[i];
      int x = x0 + i * pas;
      g.setFont(&Gras9);
      if (p.cible) {
        int w = largeur(g, p.heure) + 14;
        g.fillRoundRect(x - w / 2, yTexte - 16, w, 22, 6, NOIR);
        noter(x - w / 2, yTexte - 16, w, 22, TypeZone::Cadre, "pastille " + p.heure);
        texte(g, x - (w - 14) / 2, yTexte, p.heure, BLANC);
      } else {
        g.setFont(&Texte9);
        texteCentre(g, x, yTexte, p.heure, NOIR, pas > 0 ? pas - 4 : 80);
      }
      if (p.rate) {
        g.drawCircle(x, yLigne, 5, NOIR);
        g.drawLine(x - 3, yLigne - 3, x + 3, yLigne + 3, NOIR);
      } else {
        g.fillCircle(x, yLigne, 6, BLANC);
        g.drawCircle(x, yLigne, 6, NOIR);
        g.drawCircle(x, yLigne, 5, NOIR);
        if (p.cible) g.fillCircle(x, yLigne, 3, NOIR);
      }
      if (plusieursLignes) {
        g.setFont(&Petit7);
        texte(g, x + 10, yLigne + 4, p.ligne);
      }
    }
  }
  ligneH(g, SEP3, "separateur 3");
}

static void pied(Adafruit_GFX& g, const Contenu& c) {
  const int yBase = SEP3 + 40;
  if (c.meteo.valide) {
    icone(g, Icones16, MARGE, yBase - 2, iconeMeteo(c.meteo.code, c.nuit), "meteo actuelle");
    g.setFont(&Gras13);
    texte(g, MARGE + 36, yBase - 9, std::to_string(c.meteo.temperature) + "°");
  } else if (c.statut.empty()) {
    g.setFont(&Texte9);
    texte(g, MARGE, yBase - 8, "Météo indisponible");
  }
  if (!c.statut.empty()) {
    g.setFont(&Petit7);
    texteDroite(g, LARGEUR - MARGE, yBase - 8, tronquer(g, c.statut, 260));
    return;
  }
  if (!c.meteo.valide) return;
  const int x = 104;
  const int wCol = (LARGEUR - MARGE - x) / 3;
  for (size_t i = 0; i < c.meteo.jours.size() && i < 3; i++) {
    const Jour& j = c.meteo.jours[i];
    int xCol = x + wCol * (int)i;
    if (i > 0) {
      g.drawFastVLine(xCol, SEP3 + 6, HAUTEUR - SEP3 - 10, NOIR);
      noter(xCol, SEP3 + 6, 1, HAUTEUR - SEP3 - 10, TypeZone::Ligne, "colonne " + std::to_string(i));
    }
    g.setFont(&Petit7);
    texteCentre(g, xCol + wCol / 2, SEP3 + 14, j.nom, NOIR, wCol - 8);
    icone(g, Icones12, xCol + 4, yBase + 2, iconeMeteo(j.code, false), "meteo " + j.nom);
    g.setFont(&Texte9);
    texte(g, xCol + 30, yBase - 3, std::to_string(j.tMin) + "-" + std::to_string(j.tMax) + "°");
  }
}

static void nuit(Adafruit_GFX& g, const Contenu& c) {
  entete(g, c);
  g.setFont(&Chiffres26);
  texteCentre(g, LARGEUR / 2, SEP1 + 64, c.heure, NOIR, LARGEUR);
  g.setFont(&Gras13);
  texteCentre(g, LARGEUR / 2, SEP1 + 96, "Bonne nuit", NOIR, LARGEUR);
  ligneH(g, SEP2, "separateur 2");
  g.setFont(&Texte9);
  texteCentre(g, LARGEUR / 2, SEP2 + 34, "Pas de passage la nuit", NOIR, LARGEUR - 2 * MARGE);
  ligneH(g, SEP3, "separateur 3");
  pied(g, c);
}

void dessinerVerdict(Adafruit_GFX& g, const Contenu& c) {
  g.fillScreen(BLANC);
  g.setTextWrap(false);
  if (c.nuit) {
    nuit(g, c);
    return;
  }
  entete(g, c);
  blocDepart(g, c);
  frise(g, c);
  pied(g, c);
}

void dessinerQr(Adafruit_GFX& g, int modules, const std::function<bool(int, int)>& noir,
                const std::string& titre, const std::vector<std::string>& lignes) {
  g.fillScreen(BLANC);
  g.setTextWrap(false);
  const int echelle = std::max(1, std::min(5, 190 / modules));
  const int cote = echelle * modules;
  const int x0 = LARGEUR - cote - 16, y0 = (HAUTEUR - cote) / 2;
  for (int y = 0; y < modules; y++)
    for (int x = 0; x < modules; x++)
      if (noir(x, y)) g.fillRect(x0 + x * echelle, y0 + y * echelle, echelle, echelle, NOIR);

  g.setFont(&Gras18);
  texte(g, 12, 48, tronquer(g, titre, x0 - 24));
  g.setFont(&Texte9);
  int y = 84;
  for (const std::string& l : lignes) {
    texte(g, 12, y, tronquer(g, l, x0 - 24));
    y += 22;
  }
}

void dessinerMessage(Adafruit_GFX& g, const std::string& titre, const std::string& texteMsg) {
  g.fillScreen(BLANC);
  g.setTextWrap(false);
  g.setFont(&Gras18);
  texteCentre(g, LARGEUR / 2, 120, titre, NOIR, LARGEUR - 24);
  g.setFont(&Texte9);
  std::string reste = texteMsg;
  int y = 160;
  while (!reste.empty() && y < 280) {
    std::string ligne = reste;
    while (largeur(g, ligne) > LARGEUR - 40) {
      size_t coupe = ligne.rfind(' ');
      if (coupe == std::string::npos) break;
      ligne = ligne.substr(0, coupe);
    }
    texteCentre(g, LARGEUR / 2, y, ligne, NOIR, LARGEUR - 24);
    reste = reste.size() > ligne.size() ? reste.substr(ligne.size() + 1) : "";
    y += 22;
  }
}

}  // namespace dessin
