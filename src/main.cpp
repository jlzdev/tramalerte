#include <Arduino.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <WiFi.h>

#include <ctime>
#include <sys/time.h>

#include "config.h"
#include "depart.h"
#include "dessin.h"
#include "ecran.h"
#include "ginko.h"
#include "meteo.h"
#include "portail.h"

static const int BOUTON_BOOT = 0;
static const int LED_CARTE = 2;
static const unsigned long APPUI_LONG_MS = 3000;
static const unsigned long BOUTON_IGNORE_MS = 30000;
static const unsigned long FETCH_MS = 30000;
static const unsigned long FETCH_PROCHE_MS = 15000;
static const unsigned long FETCH_ERREUR_MS = 20000;
static const unsigned long METEO_MS = 30UL * 60 * 1000;
static const unsigned long METEO_ERREUR_MS = 5UL * 60 * 1000;
static const unsigned long COMPLET_MS = 30UL * 60 * 1000;
static const unsigned long PERIME_MS = 120000;
static const unsigned long NTP_MS = 12UL * 3600 * 1000;
static const double PROCHE_SEC = 300;
static const int ARRONDI_SEC = 10;

static std::vector<depart::Passage> passagesTous;
static int64_t fetchedAtMs = 0;
static unsigned long derniereTentative = 0;
static unsigned long dernierComplet = 0;
static unsigned long dernierNtp = 0;
static unsigned long derniereMeteo = 0;
static bool meteoEnEchec = false;
static dessin::Meteo meteoCourante;
static String erreur;
static bool cleRefusee = false;
static bool simulation = false;
static std::string dernierRendu;
static depart::Etat dernierEtat = depart::Etat::Inconnu;
static bool configPrete = false;
static unsigned long appuiDepuis = 0;

static int64_t nowMs() {
  struct timeval tv;
  gettimeofday(&tv, nullptr);
  return (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static bool heureValide() {
  return time(nullptr) > 1700000000;
}

static void reglerHeure() {
  configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "fr.pool.ntp.org", "pool.ntp.org");
  for (int i = 0; i < 100 && !heureValide(); i++) delay(100);
  dernierNtp = millis();
  Serial.printf("[HEURE] %s\n", heureValide() ? depart::fmtHM(nowMs()).c_str() : "NTP en attente");
}

static bool enNuit() {
  if (!heureValide()) return false;
  time_t t = time(nullptr);
  struct tm local;
  localtime_r(&t, &local);
  int h = local.tm_hour;
  if (config.nuitDebut == config.nuitFin) return false;
  if (config.nuitDebut < config.nuitFin) return h >= config.nuitDebut && h < config.nuitFin;
  return h >= config.nuitDebut || h < config.nuitFin;
}

static std::vector<depart::Passage> passagesChoisis() {
  std::vector<depart::Passage> out;
  for (const depart::Passage& p : passagesTous) {
    if (config.directionChoisie(p.idLigne.c_str(), p.sensAller)) out.push_back(p);
  }
  return out;
}

static depart::Verdict verdictCourant() {
  const int64_t now = nowMs();
  return depart::evaluer(passagesChoisis(), now, fetchedAtMs ? fetchedAtMs : now, config.reglages());
}

static void rafraichir() {
  derniereTentative = millis();
  if (simulation || WiFi.status() != WL_CONNECTED) return;
  ginko::Resultat r = ginko::tempsLieu(config.cleApi, config.arret);
  if (r.ok) {
    passagesTous = r.passages;
    fetchedAtMs = nowMs();
    erreur = "";
    cleRefusee = false;
    if (!config.positionConnue() && (r.latitude != 0 || r.longitude != 0)) {
      config.latitude = r.latitude;
      config.longitude = r.longitude;
      configEnregistrer();
      derniereMeteo = 0;
      Serial.println("[GINKO] position de l'arret memorisee pour la meteo");
    }
    Serial.printf("[GINKO] %d passage(s), heap %u\n", (int)r.passages.size(), ESP.getFreeHeap());
  } else {
    erreur = r.erreur;
    cleRefusee = r.cleRefusee;
    if (cleRefusee) {
      passagesTous.clear();
      fetchedAtMs = 0;
    }
    Serial.printf("[GINKO] erreur : %s\n", r.erreur.c_str());
  }
}

static void rafraichirMeteo() {
  derniereMeteo = millis();
  if (WiFi.status() != WL_CONNECTED || !config.positionConnue() || !heureValide()) return;
  dessin::Meteo m;
  meteoEnEchec = !meteo::recuperer(config.latitude, config.longitude, nowMs(), m);
  if (!meteoEnEchec) meteoCourante = m;
}

static unsigned long intervalleFetch(const depart::Verdict& v) {
  if (!erreur.isEmpty() && !cleRefusee) return FETCH_ERREUR_MS;
  const depart::Candidat* c = v.cibleOuNull();
  return c && c->resteSec < PROCHE_SEC ? FETCH_PROCHE_MS : FETCH_MS;
}

static std::string ip() {
  return std::string(WiFi.localIP().toString().c_str());
}

static dessin::Contenu contenuCourant(const depart::Verdict& v) {
  const int64_t now = nowMs();
  dessin::Contenu c = dessin::contenuDepuisVerdict(v, now, ARRONDI_SEC);
  c.heure = heureValide() ? depart::fmtHM(now) : "--h--";
  c.arret = config.arret.c_str();
  c.departMin = config.departMin;
  c.meteo = meteoCourante;
  if (WiFi.status() != WL_CONNECTED) c.statut = "Wi-Fi perdu, reconnexion...";
  else if (simulation) c.statut = "Simulation";
  else if (cleRefusee) c.statut = "Clé API refusée, voir http://" + ip() + "/";
  else if (!erreur.isEmpty()) c.statut = "Ginko : " + std::string(erreur.c_str());
  else if (fetchedAtMs && now - fetchedAtMs > (int64_t)PERIME_MS) c.statut = "Données de " + depart::fmtHM(fetchedAtMs);
  if (v.etat == depart::Etat::Inconnu) {
    if (cleRefusee) c.message = "Clé API à renouveler";
    else if (!erreur.isEmpty() && !fetchedAtMs) c.message = "Ginko ne répond pas";
    else if (fetchedAtMs) c.message = "Rien d'annoncé pour tes directions";
    else c.message = "Interrogation de Ginko...";
  }
  c.nuit = enNuit();
  c.nuitFin = config.nuitFin;
  return c;
}

static std::string cleRendu(const dessin::Contenu& c) {
  std::string s = c.heure + "|" + c.arret + "|" + std::to_string((int)c.etat) + "|" + c.ligne + "|" + c.direction + "|" + c.arrivee + "|" + c.depart + "|" + c.dans + "|";
  s += c.message + "|" + c.statut + "|" + (c.nuit ? "n" : "j") + "|" + std::to_string(c.departMin) + "|";
  s += std::to_string(c.meteo.valide) + std::to_string(c.meteo.temperature) + std::to_string(c.meteo.code);
  for (const dessin::Jour& j : c.meteo.jours) s += "|" + j.nom + std::to_string(j.code) + std::to_string(j.tMin) + std::to_string(j.tMax);
  for (const dessin::Prochain& p : c.prochains) s += "|" + p.ligne + p.heure + (p.rate ? "r" : "") + (p.cible ? "c" : "");
  return s;
}

static void afficher(bool forcerComplet) {
  depart::Verdict v = verdictCourant();
  dessin::Contenu c = contenuCourant(v);
  std::string cle = cleRendu(c);
  if (!forcerComplet && cle == dernierRendu) return;
  bool complet = forcerComplet || v.etat != dernierEtat || millis() - dernierComplet > COMPLET_MS;
  ecranAfficherVerdict(c, complet);
  if (complet) dernierComplet = millis();
  dernierRendu = cle;
  dernierEtat = v.etat;
}

static void afficherQrConfig() {
  String url = "http://" + WiFi.localIP().toString() + "/";
  ecranAfficherQr(url.c_str(), "Réglages", {"1. Scanne le QR code", "2. Colle ta clé Ginko,", "   choisis ton arrêt", "   et tes directions", "", url});
  dernierRendu = "";
}

static void simuler(const std::vector<int>& secondes) {
  simulation = !secondes.empty();
  passagesTous.clear();
  std::vector<Direction> dirs = config.directionsParsees();
  if (!dirs.empty()) {
    for (int s : secondes) {
      depart::Passage p;
      p.ligne = dirs[0].ligne.c_str();
      p.idLigne = dirs[0].idLigne.c_str();
      p.destination = dirs[0].destination.c_str();
      p.sensAller = dirs[0].sensAller;
      p.secondes = s;
      p.bus = !config.tram;
      passagesTous.push_back(p);
    }
  }
  fetchedAtMs = nowMs();
  erreur = "";
  cleRefusee = false;
  if (!simulation) derniereTentative = 0;
  Serial.printf("[SIMU] %s\n", simulation ? "activee" : "terminee");
}

static String etatJson() {
  JsonDocument doc;
  depart::Verdict v = verdictCourant();
  doc["etat"] = (int)v.etat;
  doc["titre"] = depart::libelles(v, nowMs()).titre;
  doc["passages"] = passagesTous.size();
  doc["fetchedAt"] = fetchedAtMs;
  doc["erreur"] = erreur;
  doc["simulation"] = simulation;
  doc["nuit"] = enNuit();
  doc["meteo"] = meteoCourante.valide;
  doc["heap"] = ESP.getFreeHeap();
  doc["uptimeS"] = millis() / 1000;
  doc["ip"] = WiFi.localIP().toString();
  String s;
  serializeJson(doc, s);
  return s;
}

static void appliquerConfig() {
  configPrete = config.complete();
  passagesTous.clear();
  fetchedAtMs = 0;
  erreur = "";
  cleRefusee = false;
  dernierRendu = "";
  derniereTentative = 0;
  derniereMeteo = 0;
  if (!configPrete) afficherQrConfig();
}

static void gererBouton() {
  static bool appuiAIgnorer = false;
  static bool avertissement = false;
  bool appuye = digitalRead(BOUTON_BOOT) == LOW;
  if (millis() < BOUTON_IGNORE_MS) {
    appuiAIgnorer = appuye;
    appuiDepuis = 0;
    return;
  }
  if (appuiAIgnorer) {
    if (!appuye) appuiAIgnorer = false;
    return;
  }
  if (appuye && !appuiDepuis) {
    appuiDepuis = millis();
    avertissement = false;
    digitalWrite(LED_CARTE, HIGH);
  }
  if (!appuye && appuiDepuis) {
    bool longAppui = millis() - appuiDepuis > APPUI_LONG_MS;
    appuiDepuis = 0;
    digitalWrite(LED_CARTE, LOW);
    if (longAppui) {
      Serial.println("[BOUTON] appui long relache : Wi-Fi oublie, redemarrage");
      ecranAfficherMessage("Réinitialisation", "Wi-Fi oublié, l'afficheur redémarre en mode configuration.");
      wifiOublier();
      delay(500);
      ESP.restart();
    }
    return;
  }
  if (appuye && millis() - appuiDepuis > APPUI_LONG_MS) {
    if (!avertissement) {
      avertissement = true;
      ecranAfficherMessage("Bouton BOOT", "Relâche le bouton pour oublier le Wi-Fi et relancer la configuration.");
      dernierRendu = "";
    }
    digitalWrite(LED_CARTE, (millis() / 200) % 2);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BOUTON_BOOT, INPUT_PULLUP);
  pinMode(LED_CARTE, OUTPUT);
  digitalWrite(LED_CARTE, LOW);
  Serial.println("\n[TRAMALERTE] demarrage");
  ecranInit();
  configCharger();
  if (!wifiConnecter(true)) {
    ecranAfficherMessage("Wi-Fi injoignable", "Nouvelle tentative dans une minute.");
    delay(60000);
    ESP.restart();
  }
  Serial.printf("[WIFI] connecte, IP %s\n", WiFi.localIP().toString().c_str());
  reglerHeure();
  serveurDemarrer(simuler, etatJson);
  ArduinoOTA.setHostname("tramalerte");
  ArduinoOTA.onStart([] { ecranAfficherMessage("Mise à jour", "Nouveau firmware en cours d'installation par le Wi-Fi, ne pas débrancher."); });
  ArduinoOTA.onError([](ota_error_t e) { Serial.printf("[OTA] erreur %u\n", (unsigned)e); });
  ArduinoOTA.begin();
  Serial.println("[OTA] pret, cible tramalerte.local");
  appliquerConfig();
}

void loop() {
  serveurTraiter();
  ArduinoOTA.handle();
  gererBouton();
  if (serveurConfigChangee()) appliquerConfig();
  if (!configPrete) {
    delay(10);
    return;
  }
  if (millis() - dernierNtp > NTP_MS) reglerHeure();
  static bool nuitPrecedente = false;
  const bool nuit = enNuit();
  if (nuit != nuitPrecedente) {
    nuitPrecedente = nuit;
    if (nuit) {
      passagesTous.clear();
      fetchedAtMs = 0;
      Serial.println("[NUIT] debut, Ginko en pause");
    } else {
      Serial.println("[NUIT] fin, reprise des requetes Ginko");
    }
    derniereTentative = 0;
    dernierRendu = "";
  }
  if (!simulation && !nuit && (derniereTentative == 0 || millis() - derniereTentative > intervalleFetch(verdictCourant()))) {
    rafraichir();
    afficher(false);
  }
  if (derniereMeteo == 0 || millis() - derniereMeteo > (meteoEnEchec ? METEO_ERREUR_MS : METEO_MS)) {
    rafraichirMeteo();
    afficher(false);
  }
  static unsigned long dernierTick = 0;
  if (millis() - dernierTick >= 1000) {
    dernierTick = millis();
    afficher(false);
  }
  delay(10);
}
