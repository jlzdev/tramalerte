#include "config.h"

#include <Preferences.h>

Config config;
static Preferences memoire;
static const char* ESPACE = "tramalerte";

static int borner(int v, int min, int max) {
  return v < min ? min : (v > max ? max : v);
}

void configCharger() {
  memoire.begin(ESPACE, false);
  config.cleApi = memoire.getString("cle", "");
  config.arret = memoire.getString("arret", "");
  config.tram = memoire.getBool("tram", false);
  config.directions = memoire.getString("directions", "");
  config.departMin = memoire.getInt("departMin", depart::REGLAGES_DEFAUT.departMin);
  config.margeMin = memoire.getInt("margeMin", depart::REGLAGES_DEFAUT.margeMin);
  config.nuitDebut = borner(memoire.getInt("nuitDebut", 23), 0, 24);
  config.nuitFin = borner(memoire.getInt("nuitFin", 6), 0, 24);
  memoire.end();
  depart::Reglages r = config.reglages();
  config.departMin = r.departMin;
  config.margeMin = r.margeMin;
}

void configEnregistrer() {
  memoire.begin(ESPACE, false);
  memoire.putString("cle", config.cleApi);
  memoire.putString("arret", config.arret);
  memoire.putBool("tram", config.tram);
  memoire.putString("directions", config.directions);
  memoire.putInt("departMin", config.departMin);
  memoire.putInt("margeMin", config.margeMin);
  memoire.putInt("nuitDebut", config.nuitDebut);
  memoire.putInt("nuitFin", config.nuitFin);
  memoire.end();
}

void configEffacer() {
  memoire.begin(ESPACE, false);
  memoire.clear();
  memoire.end();
  config = Config();
}

std::vector<Direction> Config::directionsParsees() const {
  std::vector<Direction> out;
  int debut = 0;
  while (debut < (int)directions.length()) {
    int fin = directions.indexOf(';', debut);
    if (fin < 0) fin = directions.length();
    String item = directions.substring(debut, fin);
    debut = fin + 1;
    int a = item.indexOf('|');
    int b = a < 0 ? -1 : item.indexOf('|', a + 1);
    int c = b < 0 ? -1 : item.indexOf('|', b + 1);
    if (a < 0 || b < 0 || c < 0) continue;
    Direction d;
    d.idLigne = item.substring(0, a);
    d.sensAller = item.substring(a + 1, b) == "A";
    d.ligne = item.substring(b + 1, c);
    d.destination = item.substring(c + 1);
    if (d.idLigne.length()) out.push_back(d);
  }
  return out;
}

bool Config::directionChoisie(const String& idLigne, bool sensAller) const {
  for (const Direction& d : directionsParsees()) {
    if (d.idLigne == idLigne && d.sensAller == sensAller) return true;
  }
  return false;
}

String Config::libelleDirections() const {
  String s;
  for (const Direction& d : directionsParsees()) {
    if (s.length()) s += ", ";
    s += d.ligne + " > " + d.destination;
  }
  return s;
}

String directionsSerialiser(const std::vector<Direction>& directions) {
  String s;
  for (const Direction& d : directions) {
    if (s.length()) s += ";";
    s += d.idLigne + "|" + (d.sensAller ? "A" : "R") + "|" + d.ligne + "|" + d.destination;
  }
  return s;
}
