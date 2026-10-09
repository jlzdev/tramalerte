#include "portail.h"

#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include "config.h"
#include "ecran.h"
#include "portail_page.h"

static const char* RESEAU_NOM = "TramAlerte";
static const char* RESEAU_MDP = "tram1234";

static void afficherQrWifi(WiFiManager*) {
  String qr = String("WIFI:S:") + RESEAU_NOM + ";T:WPA;P:" + RESEAU_MDP + ";;";
  ecranAfficherQr(qr.c_str(), "Wi-Fi", {"1. Scanne le QR code", "2. Choisis ton Wi-Fi", "", "Réseau :", RESEAU_NOM, "Mot de passe :", RESEAU_MDP});
}

bool wifiConnecter(bool portailSiEchec) {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("tramalerte");
  WiFiManager wm;
  wm.setTitle("Tram alerte");
  std::vector<const char*> menu = {"wifi"};
  wm.setMenu(menu);
  wm.setAPCallback(afficherQrWifi);
  wm.setConnectTimeout(20);
  wm.setConfigPortalTimeout(wm.getWiFiIsSaved() ? 300 : 0);
  wm.setEnableConfigPortal(portailSiEchec);
  bool ok = wm.autoConnect(RESEAU_NOM, RESEAU_MDP);
  if (ok) WiFi.setAutoReconnect(true);
  return ok;
}

void wifiOublier() {
  WiFiManager wm;
  wm.resetSettings();
}

static WebServer serveur(80);
static bool configChangee = false;
static SimulationCallback simulation;
static EtatCallback etatCourant;

static void envoyerPage() {
  serveur.sendHeader("Cache-Control", "no-store");
  serveur.send_P(200, "text/html; charset=utf-8", PAGE_PORTAIL);
}

static void envoyerConfig() {
  JsonDocument doc;
  doc["cle"] = config.cleApi;
  doc["arret"] = config.arret;
  doc["tram"] = config.tram;
  JsonArray dirs = doc["directions"].to<JsonArray>();
  for (const Direction& d : config.directionsParsees()) {
    JsonObject o = dirs.add<JsonObject>();
    o["idLigne"] = d.idLigne;
    o["sensAller"] = d.sensAller;
    o["ligne"] = d.ligne;
    o["destination"] = d.destination;
  }
  doc["departMin"] = config.departMin;
  doc["margeMin"] = config.margeMin;
  doc["nuitDebut"] = config.nuitDebut;
  doc["nuitFin"] = config.nuitFin;
  doc["ip"] = WiFi.localIP().toString();
  String json;
  serializeJson(doc, json);
  serveur.sendHeader("Cache-Control", "no-store");
  serveur.send(200, "application/json", json);
}

static String nettoyer(const String& s, size_t max) {
  String out;
  for (size_t i = 0; i < s.length() && out.length() < max; i++) {
    unsigned char c = s[i];
    if (c >= 32 && c != 127) out += (char)c;
  }
  out.trim();
  return out;
}

static void enregistrer() {
  JsonDocument doc;
  DeserializationError e = deserializeJson(doc, serveur.arg("plain"));
  if (e) {
    serveur.send(400, "text/plain", "JSON illisible");
    return;
  }
  if (doc["cle"].is<const char*>()) config.cleApi = nettoyer(doc["cle"].as<String>(), 128);
  if (doc["arret"].is<const char*>()) config.arret = nettoyer(doc["arret"].as<String>(), 60);
  if (doc["tram"].is<bool>()) config.tram = doc["tram"].as<bool>();
  if (doc["directions"].is<JsonArray>()) {
    std::vector<Direction> dirs;
    for (JsonObject o : doc["directions"].as<JsonArray>()) {
      Direction d;
      d.idLigne = nettoyer(o["idLigne"] | "", 10);
      d.sensAller = o["sensAller"] | false;
      d.ligne = nettoyer(o["ligne"] | "", 10);
      d.destination = nettoyer(o["destination"] | "", 40);
      if (d.idLigne.length() && d.ligne.length() && dirs.size() < 8) dirs.push_back(d);
    }
    config.directions = directionsSerialiser(dirs);
  }
  if (doc["departMin"].is<int>()) config.departMin = doc["departMin"].as<int>();
  if (doc["margeMin"].is<int>()) config.margeMin = doc["margeMin"].as<int>();
  if (doc["nuitDebut"].is<int>()) config.nuitDebut = constrain(doc["nuitDebut"].as<int>(), 0, 24);
  if (doc["nuitFin"].is<int>()) config.nuitFin = constrain(doc["nuitFin"].as<int>(), 0, 24);
  depart::Reglages r = config.reglages();
  config.departMin = r.departMin;
  config.margeMin = r.margeMin;
  configEnregistrer();
  configChangee = true;
  Serial.printf("[CONFIG] arret %s, directions %s, depart %d min, marge %d min\n",
                config.arret.c_str(), config.libelleDirections().c_str(), config.departMin, config.margeMin);
  serveur.send(200, "text/plain", "ok");
}

static void simuler() {
  std::vector<int> secondes;
  String arg = serveur.arg("secondes");
  int debut = 0;
  while (debut <= (int)arg.length()) {
    int fin = arg.indexOf(',', debut);
    if (fin < 0) fin = arg.length();
    String n = arg.substring(debut, fin);
    n.trim();
    if (n.length()) secondes.push_back(n.toInt());
    debut = fin + 1;
  }
  if (simulation) simulation(secondes);
  serveur.send(200, "text/plain", secondes.empty() ? "simulation terminee" : "simulation lancee");
}

static void etat() {
  serveur.sendHeader("Cache-Control", "no-store");
  serveur.send(200, "application/json", etatCourant ? etatCourant() : String("{}"));
}

void serveurDemarrer(SimulationCallback simuler_, EtatCallback etat_) {
  simulation = simuler_;
  etatCourant = etat_;
  serveur.on("/", HTTP_GET, envoyerPage);
  serveur.on("/config", HTTP_GET, envoyerConfig);
  serveur.on("/enregistrer", HTTP_POST, enregistrer);
  serveur.on("/simuler", HTTP_GET, simuler);
  serveur.on("/etat", HTTP_GET, etat);
  serveur.onNotFound([] { serveur.send(404, "text/plain", "Page inconnue"); });
  serveur.begin();
}

void serveurTraiter() {
  serveur.handleClient();
}

bool serveurConfigChangee() {
  bool c = configChangee;
  configChangee = false;
  return c;
}
