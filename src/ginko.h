#pragma once

#include <Arduino.h>

#include <string>
#include <vector>

#include "depart.h"

namespace ginko {

struct Resultat {
  bool ok = false;
  bool cleRefusee = false;
  String erreur;
  String nomExact;
  float latitude = 0;
  float longitude = 0;
  std::vector<depart::Passage> passages;
};

Resultat tempsLieu(const String& cleApi, const String& nomArret);

}  // namespace ginko
