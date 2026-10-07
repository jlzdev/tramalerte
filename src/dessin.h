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
  std::string destination;
  std::string quand;
  bool rate = false;
};

struct Contenu {
  std::string arret;
  std::string directions;
  std::string heure;
  depart::Etat etat = depart::Etat::Inconnu;
  std::string titre;
  int minutes = -1;
  int secondes = -1;
  std::string detail;
  std::vector<Prochain> prochains;
  std::string statut;
  bool nuit = false;
};

Contenu contenuDepuisVerdict(const depart::Verdict& v, int64_t nowMs);

void dessinerVerdict(Adafruit_GFX& g, const Contenu& c);
void dessinerQr(Adafruit_GFX& g, int modules, const std::function<bool(int, int)>& noir,
                const std::string& titre, const std::vector<std::string>& lignes);
void dessinerMessage(Adafruit_GFX& g, const std::string& titre, const std::string& texte);

std::string latin1(const std::string& utf8);

}  // namespace dessin
