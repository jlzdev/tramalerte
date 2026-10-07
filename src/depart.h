#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace depart {

struct Passage {
  std::string ligne;
  std::string idLigne;
  std::string destination;
  bool sensAller = true;
  int secondes = 0;
  bool fiable = true;
};

struct Reglages {
  int departMin = 5;
  int margeMin = 1;
};

const Reglages REGLAGES_DEFAUT{};
const int DEPART_MIN_MIN = 1;
const int DEPART_MIN_MAX = 45;
const int MARGE_MIN_MIN = 0;
const int MARGE_MIN_MAX = 10;

Reglages normaliserReglages(long departMin, long margeMin);

enum class Etat { Rate, Cours, Prepare, Tranquille, Inconnu };

const int COURS_FENETRE_SEC = 60;
const int PREPARE_FENETRE_SEC = 180;

struct Candidat {
  Passage passage;
  Etat etat = Etat::Inconnu;
  double resteSec = 0;
  double tramSec = 0;
  int64_t departMs = 0;
  int64_t tramMs = 0;
};

struct Verdict {
  Etat etat = Etat::Inconnu;
  int cible = -1;
  std::vector<Candidat> candidats;
  const Candidat* cibleOuNull() const { return cible >= 0 ? &candidats[cible] : nullptr; }
};

Verdict evaluer(const std::vector<Passage>& passages, int64_t nowMs, int64_t fetchedAtMs, const Reglages& r);

std::string fmtHM(int64_t ms);
std::string fmtDuree(double sec, bool precis = false);

struct Libelles {
  std::string titre;
  std::string compte;
  std::string detail;
};

Libelles libelles(const Verdict& v, int64_t nowMs);

}  // namespace depart
