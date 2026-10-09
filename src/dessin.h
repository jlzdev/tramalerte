#pragma once

#include <Adafruit_GFX.h>

#include <functional>
#include <string>
#include <vector>

#include "depart.h"

namespace dessin {

const int LARGEUR = 400;
const int HAUTEUR = 300;

struct Prochain {
  std::string ligne;
  std::string heure;
  bool rate = false;
  bool cible = false;
};

struct Jour {
  std::string nom;
  int code = -1;
  int tMin = 0;
  int tMax = 0;
};

struct Meteo {
  bool valide = false;
  int temperature = 0;
  int code = -1;
  std::vector<Jour> jours;
};

struct Contenu {
  std::string heure;
  depart::Etat etat = depart::Etat::Inconnu;
  bool bus = false;
  std::string ligne;
  std::string direction;
  std::string arrivee;
  std::string depart;
  std::string dans;
  std::string compteNombre;
  std::string compteUnite;
  int departMin = 5;
  std::vector<Prochain> prochains;
  Meteo meteo;
  std::string statut;
  bool nuit = false;
  std::string message;
};

Contenu contenuDepuisVerdict(const depart::Verdict& v, int64_t nowMs, int arrondiSec = 1);
char iconeMeteo(int code, bool nuit);
std::string nomJour(int64_t ms, int decalageJours);

void dessinerVerdict(Adafruit_GFX& g, const Contenu& c);
void dessinerQr(Adafruit_GFX& g, int modules, const std::function<bool(int, int)>& noir,
                const std::string& titre, const std::vector<std::string>& lignes);
void dessinerMessage(Adafruit_GFX& g, const std::string& titre, const std::string& texte);

std::string latin1(const std::string& utf8);

enum class TypeZone { Contenu, Cadre, Ligne };
struct Zone {
  int x, y, w, h;
  TypeZone type;
  std::string nom;
};

#ifdef NATIVE
void zonesReinitialiser();
std::vector<std::string> zonesProblemes();
const std::vector<Zone>& zonesCourantes();
#endif

}  // namespace dessin
