#include "dessin.h"

#include <cmath>

#include "polices/Chiffres96.h"
#include "polices/Petit9.h"
#include "polices/Texte12.h"
#include "polices/TexteGras12.h"
#include "polices/Titre20.h"

namespace dessin {

static const uint16_t NOIR = 0;
static const uint16_t BLANC = 1;

static const int BANDEAU_BAS = 44;
static const int VERDICT_HAUT = 50;
static const int VERDICT_BAS = 208;
static const int PROCHAINS_Y = 229;
static const int PROCHAINS_PAS = 21;
static const int STATUT_LIGNE = 281;
static const int STATUT_Y = 295;

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
  g.setTextColor(couleur);
  g.setCursor(x, y);
  g.print(latin1(s).c_str());
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

static void texteCentre(Adafruit_GFX& g, int cx, int y, const std::string& s, uint16_t couleur = NOIR, int largeurMax = LARGEUR - 24) {
  std::string t = tronquer(g, s, largeurMax);
  texte(g, cx - largeur(g, t) / 2, y, t, couleur);
}

static void texteDroite(Adafruit_GFX& g, int xDroite, int y, const std::string& s, uint16_t couleur = NOIR) {
  texte(g, xDroite - largeur(g, s), y, s, couleur);
}

Contenu contenuDepuisVerdict(const depart::Verdict& v, int64_t nowMs) {
  Contenu c;
  c.etat = v.etat;
  depart::Libelles l = depart::libelles(v, nowMs);
  c.titre = l.titre;
  const depart::Candidat* cible = v.cibleOuNull();
  if (cible) {
    long reste = std::lround(std::fmax(0.0, cible->resteSec));
    c.minutes = reste / 60;
    c.secondes = (v.etat == depart::Etat::Tranquille) ? -1 : reste % 60;
    const depart::Passage& p = cible->passage;
    if (v.etat == depart::Etat::Cours) c.detail = "Tram " + p.ligne + " " + p.destination + " dans " + depart::fmtDuree(cible->tramSec, true);
    else c.detail = "Départ " + depart::fmtHM(std::max(nowMs, cible->departMs)) + ", tram " + p.ligne + " à " + depart::fmtHM(cible->tramMs);
    if (!p.fiable) c.detail += " (théorique)";
  }
  for (const depart::Candidat& cand : v.candidats) {
    if (c.prochains.size() >= 3) break;
    Prochain p;
    p.ligne = cand.passage.ligne;
    p.destination = cand.passage.destination;
    p.rate = cand.etat == depart::Etat::Rate;
    if (p.rate) p.quand = cand.tramSec < 60 ? "trop tard, tram imminent" : "trop tard, tram dans " + depart::fmtDuree(cand.tramSec);
    else p.quand = "pars " + depart::fmtHM(std::max(nowMs, cand.departMs)) + ", tram " + depart::fmtHM(cand.tramMs);
    if (!cand.passage.fiable) p.quand += " (th.)";
    c.prochains.push_back(p);
  }
  return c;
}

static void bandeau(Adafruit_GFX& g, const Contenu& c) {
  g.setFont(&Texte12);
  int wHeure = largeur(g, c.heure);
  texteDroite(g, LARGEUR - 10, 19, c.heure);
  g.setFont(&TexteGras12);
  texte(g, 10, 19, tronquer(g, c.arret, LARGEUR - 30 - wHeure));
  g.setFont(&Petit9);
  texte(g, 10, 36, tronquer(g, c.directions, LARGEUR - 20));
  g.drawFastHLine(10, BANDEAU_BAS - 2, LARGEUR - 20, NOIR);
}

static void verdictPrincipal(Adafruit_GFX& g, const Contenu& c) {
  const bool inverse = c.etat == depart::Etat::Cours;
  const int haut = VERDICT_HAUT, bas = VERDICT_BAS;
  if (inverse) g.fillRoundRect(6, haut, LARGEUR - 12, bas - haut, 10, NOIR);
  else if (c.etat == depart::Etat::Prepare) {
    for (int i = 0; i < 3; i++) g.drawRoundRect(6 + i, haut + i, LARGEUR - 12 - 2 * i, bas - haut - 2 * i, 10 - i, NOIR);
  }
  const uint16_t encre = inverse ? BLANC : NOIR;
  const int ligneChiffres = haut + 128;

  g.setFont(&Titre20);
  texteCentre(g, LARGEUR / 2, haut + 32, c.titre, encre);

  if (c.minutes < 0) {
    g.setFont(&Chiffres96);
    texteCentre(g, LARGEUR / 2, ligneChiffres, "--", encre);
  } else {
    const bool secondesSeules = c.minutes == 0 && c.secondes >= 0;
    std::string nombre = std::to_string(secondesSeules ? c.secondes : c.minutes);
    std::string unite = secondesSeules ? "s" : (c.secondes >= 0 ? "min " + std::string(c.secondes < 10 ? "0" : "") + std::to_string(c.secondes) + " s" : "min");
    g.setFont(&Chiffres96);
    int wNombre = largeur(g, nombre);
    g.setFont(&Titre20);
    int wUnite = largeur(g, unite);
    int x = (LARGEUR - wNombre - 12 - wUnite) / 2;
    g.setFont(&Chiffres96);
    texte(g, x, ligneChiffres, nombre, encre);
    g.setFont(&Titre20);
    texte(g, x + wNombre + 12, ligneChiffres, unite, encre);
  }

  g.setFont(&Texte12);
  texteCentre(g, LARGEUR / 2, bas - 14, c.detail, encre, LARGEUR - 36);
}

static void prochains(Adafruit_GFX& g, const Contenu& c) {
  int y = PROCHAINS_Y;
  if (c.prochains.empty() && c.etat != depart::Etat::Inconnu) {
    g.setFont(&Petit9);
    texte(g, 10, y, "Aucun passage annoncé.");
  }
  for (const Prochain& p : c.prochains) {
    g.setFont(&TexteGras12);
    int wLigne = std::max(28, largeur(g, p.ligne) + 10);
    if (p.rate) g.drawRoundRect(10, y - 13, wLigne, 18, 4, NOIR);
    else g.fillRoundRect(10, y - 13, wLigne, 18, 4, NOIR);
    texte(g, 10 + (wLigne - largeur(g, p.ligne)) / 2, y, p.ligne, p.rate ? NOIR : BLANC);
    g.setFont(p.rate ? &Petit9 : &Texte12);
    texte(g, 10 + wLigne + 8, y, tronquer(g, p.destination + ", " + p.quand, LARGEUR - 10 - wLigne - 28));
    y += PROCHAINS_PAS;
  }
}

static void statut(Adafruit_GFX& g, const Contenu& c) {
  g.setFont(&Petit9);
  g.drawFastHLine(10, STATUT_LIGNE, LARGEUR - 20, NOIR);
  texte(g, 10, STATUT_Y, tronquer(g, c.statut, LARGEUR - 20));
}

static void nuit(Adafruit_GFX& g, const Contenu& c) {
  bandeau(g, c);
  g.setFont(&Chiffres96);
  texteCentre(g, LARGEUR / 2, 170, c.heure);
  g.setFont(&Titre20);
  texteCentre(g, LARGEUR / 2, 218, "Bonne nuit");
  g.setFont(&Texte12);
  texteCentre(g, LARGEUR / 2, 250, c.detail);
  statut(g, c);
}

void dessinerVerdict(Adafruit_GFX& g, const Contenu& c) {
  g.fillScreen(BLANC);
  g.setTextWrap(false);
  if (c.nuit) {
    nuit(g, c);
    return;
  }
  bandeau(g, c);
  verdictPrincipal(g, c);
  prochains(g, c);
  statut(g, c);
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

  g.setFont(&Titre20);
  texte(g, 12, 48, tronquer(g, titre, x0 - 24));
  g.setFont(&Texte12);
  int y = 86;
  for (const std::string& l : lignes) {
    texte(g, 12, y, tronquer(g, l, x0 - 24));
    y += 22;
  }
}

void dessinerMessage(Adafruit_GFX& g, const std::string& titre, const std::string& texteMsg) {
  g.fillScreen(BLANC);
  g.setTextWrap(false);
  g.setFont(&Titre20);
  texteCentre(g, LARGEUR / 2, 120, titre);
  g.setFont(&Texte12);
  std::string reste = texteMsg;
  int y = 160;
  while (!reste.empty() && y < 280) {
    std::string ligne = reste;
    while (largeur(g, ligne) > LARGEUR - 40) {
      size_t coupe = ligne.rfind(' ');
      if (coupe == std::string::npos) break;
      ligne = ligne.substr(0, coupe);
    }
    texteCentre(g, LARGEUR / 2, y, ligne);
    reste = reste.size() > ligne.size() ? reste.substr(ligne.size() + 1) : "";
    y += 22;
  }
}

}  // namespace dessin
