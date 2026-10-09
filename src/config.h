#pragma once

#include <Arduino.h>

#include <string>
#include <vector>

#include "depart.h"

struct Direction {
  String idLigne;
  bool sensAller = true;
  String ligne;
  String destination;
  String cle() const { return idLigne + "|" + (sensAller ? "A" : "R"); }
};

struct Config {
  String cleApi;
  String arret;
  bool tram = false;
  String directions;
  int departMin = depart::REGLAGES_DEFAUT.departMin;
  int nuitDebut = 23;
  int nuitFin = 6;
  float latitude = 0;
  float longitude = 0;

  bool complete() const { return cleApi.length() > 0 && arret.length() > 0 && directions.length() > 0; }
  bool positionConnue() const { return latitude != 0 || longitude != 0; }
  std::vector<Direction> directionsParsees() const;
  depart::Reglages reglages() const { return depart::normaliserReglages(departMin); }
  bool directionChoisie(const String& idLigne, bool sensAller) const;
  String libelleDirections() const;
};

extern Config config;

void configCharger();
void configEnregistrer();
void configEffacer();
String directionsSerialiser(const std::vector<Direction>& directions);
