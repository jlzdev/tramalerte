#include "meteo.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <cmath>

#include "ginko_racine.h"

namespace meteo {

static const int DELAI_MS = 10000;

bool recuperer(float latitude, float longitude, int64_t nowMs, dessin::Meteo& out) {
  WiFiClientSecure client;
  client.setCACert(GINKO_RACINE);
  client.setTimeout(DELAI_MS / 1000);
  HTTPClient http;
  http.useHTTP10(true);
  http.setTimeout(DELAI_MS);
  String url = "https://api.open-meteo.com/v1/forecast?latitude=" + String(latitude, 4) + "&longitude=" + String(longitude, 4) +
               "&current=temperature_2m,weather_code&daily=weather_code,temperature_2m_min,temperature_2m_max&timezone=Europe%2FParis&forecast_days=3";
  if (!http.begin(client, url)) return false;
  int code = http.GET();
  if (code != 200) {
    Serial.printf("[METEO] HTTP %d\n", code);
    http.end();
    return false;
  }
  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, http.getStream());
  http.end();
  if (e) {
    Serial.printf("[METEO] reponse illisible (%s)\n", e.c_str());
    return false;
  }
  dessin::Meteo m;
  m.valide = true;
  m.temperature = (int)std::lround(doc["current"]["temperature_2m"] | 0.0);
  m.code = doc["current"]["weather_code"] | -1;
  JsonArray codes = doc["daily"]["weather_code"].as<JsonArray>();
  JsonArray mins = doc["daily"]["temperature_2m_min"].as<JsonArray>();
  JsonArray maxs = doc["daily"]["temperature_2m_max"].as<JsonArray>();
  for (size_t i = 0; i < codes.size() && i < 3; i++) {
    dessin::Jour j;
    j.nom = dessin::nomJour(nowMs, (int)i);
    j.code = codes[i] | -1;
    j.tMin = (int)std::lround(mins[i] | 0.0);
    j.tMax = (int)std::lround(maxs[i] | 0.0);
    m.jours.push_back(j);
  }
  out = m;
  Serial.printf("[METEO] %d degres, code %d, %d jour(s)\n", m.temperature, m.code, (int)m.jours.size());
  return true;
}

}  // namespace meteo
