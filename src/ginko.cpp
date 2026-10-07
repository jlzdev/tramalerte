#include "ginko.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "ginko_racine.h"

namespace ginko {

static const char* BASE = "https://api.ginko.voyage/";
static const int DELAI_MS = 10000;

static String encoder(const String& s) {
  static const char* HEXA = "0123456789ABCDEF";
  String out;
  for (size_t i = 0; i < s.length(); i++) {
    unsigned char c = s[i];
    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') out += (char)c;
    else {
      out += '%';
      out += HEXA[c >> 4];
      out += HEXA[c & 15];
    }
  }
  return out;
}

static bool messageCleRefusee(const char* msg) {
  if (!msg) return false;
  return strstr(msg, "cl\xC3\xA9") || strstr(msg, "Cl\xC3\xA9") || strstr(msg, "cle ") || strstr(msg, "apiKey");
}

Resultat tempsLieu(const String& cleApi, const String& nomArret) {
  Resultat r;
  WiFiClientSecure client;
  client.setCACert(GINKO_RACINE);
  client.setTimeout(DELAI_MS / 1000);
  HTTPClient http;
  http.useHTTP10(true);
  http.setTimeout(DELAI_MS);
  http.setReuse(false);
  if (!http.begin(client, String(BASE) + "TR/getTempsLieu.do")) {
    r.erreur = "Connexion Ginko impossible";
    return r;
  }
  http.addHeader("Content-Type", "application/x-www-form-urlencoded");
  int code = http.POST("apiKey=" + encoder(cleApi) + "&nom=" + encoder(nomArret));
  if (code != 200) {
    r.erreur = code < 0 ? "Ginko injoignable (" + HTTPClient::errorToString(code) + ")" : "Ginko HTTP " + String(code);
    http.end();
    return r;
  }

  JsonDocument filtre;
  filtre["ok"] = true;
  filtre["msg"] = true;
  filtre["objets"]["nomExact"] = true;
  JsonObject f = filtre["objets"]["listeTemps"][0].to<JsonObject>();
  f["numLignePublic"] = true;
  f["idLigne"] = true;
  f["destination"] = true;
  f["sensAller"] = true;
  f["tempsEnSeconde"] = true;
  f["fiable"] = true;

  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filtre));
  http.end();
  if (e) {
    r.erreur = String("Reponse Ginko illisible (") + e.c_str() + ")";
    return r;
  }
  if (!doc["ok"].as<bool>()) {
    const char* msg = doc["msg"];
    r.cleRefusee = messageCleRefusee(msg);
    r.erreur = r.cleRefusee ? "Cle API refusee" : (msg ? String(msg) : String("Erreur Ginko"));
    return r;
  }
  r.nomExact = doc["objets"]["nomExact"] | nomArret.c_str();
  for (JsonObject t : doc["objets"]["listeTemps"].as<JsonArray>()) {
    if (!t["tempsEnSeconde"].is<int>()) continue;
    depart::Passage p;
    p.ligne = t["numLignePublic"] | "";
    p.idLigne = t["idLigne"] | "";
    p.destination = t["destination"] | "";
    p.sensAller = t["sensAller"] | false;
    p.secondes = t["tempsEnSeconde"].as<int>();
    p.fiable = t["fiable"] | true;
    r.passages.push_back(p);
  }
  r.ok = true;
  return r;
}

}  // namespace ginko
