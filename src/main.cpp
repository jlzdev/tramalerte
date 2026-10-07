#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_task_wdt.h>

#include <ctime>
#include <sys/time.h>

#include "config.h"
#include "depart.h"
#include "dessin.h"
#include "ecran.h"
#include "ginko.h"
#include "portail.h"

static const int BOUTON_BOOT = 0;
static const unsigned long APPUI_LONG_MS = 3000;
static const unsigned long FETCH_MS = 30000;
static const unsigned long FETCH_PROCHE_MS = 15000;
static const unsigned long FETCH_NUIT_MS = 600000;
static const unsigned long FETCH_ERREUR_MS = 20000;
static const unsigned long COMPLET_MS = 30UL * 60 * 1000;
static const unsigned long PERIME_MS = 120000;
static const unsigned long NTP_MS = 12UL * 3600 * 1000;
static const double PROCHE_SEC = 300;

static std::vector<depart::Passage> passagesTous;
static int64_t fetchedAtMs = 0;
static unsigned long derniereTentative = 0;
static unsigned long dernierComplet = 0;
static unsigned long dernierNtp = 0;
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
    Serial.printf("[GINKO] %d passage(s) a %s, heap %u\n", (int)r.passages.size(), r.nomExact.c_str(), ESP.getFreeHeap());
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

static unsigned long intervalleFetch(const depart::Verdict& v) {
  if (enNuit()) return FETCH_NUIT_MS;
  if (!erreur.isEmpty() && !cleRefusee) return FETCH_ERREUR_MS;
  const depart::Candidat* c = v.cibleOuNull();
  return c && c->resteSec < PROCHE_SEC ? FETCH_PROCHE_MS : FETCH_MS;
}

static std::string statutLigne() {
  std::string s;
  if (WiFi.status() != WL_CONNECTED) s = "Wi-Fi perdu, reconnexion...";
  else if (simulation) s = "Simulation";
  else if (cleRefusee) s = "Clé API refusée";
  else if (!erreur.isEmpty()) s = "Erreur Ginko : " + std::string(erreur.c_str());
  else if (fetchedAtMs) s = "Temps réel Ginko " + depart::fmtHM(fetchedAtMs);
  else s = "Interrogation de Ginko...";
  if (WiFi.status() == WL_CONNECTED) s += ", " + std::string(WiFi.localIP().toString().c_str());
  return s;
}

static dessin::Contenu contenuCourant(const depart::Verdict& v) {
  const int64_t now = nowMs();
  dessin::Contenu c = dessin::contenuDepuisVerdict(v, now);
  c.arret = config.arret.c_str();
  c.directions = config.libelleDirections().c_str();
  c.heure = heureValide() ? depart::fmtHM(now) : "--h--";
  c.statut = statutLigne();
  if (v.etat == depart::Etat::Inconnu) {
    if (cleRefusee) c.detail = "Ouvre http://" + std::string(WiFi.localIP().toString().c_str()) + "/ pour coller une clé";
    else if (!erreur.isEmpty() && !fetchedAtMs) c.detail = "Ginko ne répond pas";
    else if (fetchedAtMs) c.detail = "Aucun passage annoncé pour tes directions";
  }
  if (fetchedAtMs && now - fetchedAtMs > (int64_t)PERIME_MS && v.etat != depart::Etat::Inconnu) {
    c.detail = "Données de " + depart::fmtHM(fetchedAtMs) + ", " + c.detail;
  }
  if (enNuit()) {
    c.nuit = true;
    const depart::Candidat* cible = v.cibleOuNull();
    c.detail = cible ? "Prochain tram connu : " + cible->passage.ligne + " à " + depart::fmtHM(cible->tramMs) : "";
    c.statut = "Mode nuit jusqu'à " + std::to_string(config.nuitFin) + "h, mise à jour toutes les 10 min";
  }
  return c;
}

static std::string cleRendu(const dessin::Contenu& c) {
  std::string s = c.arret + "|" + c.directions + "|" + c.heure + "|" + c.titre + "|" + std::to_string(c.minutes) + "|";
  s += std::to_string(c.secondes < 0 ? -1 : c.secondes / 10) + "|" + c.detail + "|" + c.statut + "|" + (c.nuit ? "n" : "j");
  for (const dessin::Prochain& p : c.prochains) s += "|" + p.ligne + p.destination + p.quand + (p.rate ? "r" : "");
  return s;
}

static void afficher(bool forcerComplet) {
  depart::Verdict v = verdictCourant();
  dessin::Contenu c = contenuCourant(v);
  if (c.secondes >= 0) c.secondes -= c.secondes % 10;
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
  if (!configPrete) afficherQrConfig();
}

static void gererBouton() {
  bool appuye = digitalRead(BOUTON_BOOT) == LOW;
  if (appuye && !appuiDepuis) appuiDepuis = millis();
  if (!appuye) appuiDepuis = 0;
  if (appuye && millis() - appuiDepuis > APPUI_LONG_MS) {
    Serial.println("[BOUTON] appui long : Wi-Fi oublie, redemarrage");
    ecranAfficherMessage("Réinitialisation", "Le Wi-Fi est oublié, l'afficheur redémarre en mode configuration.");
    wifiOublier();
    delay(500);
    ESP.restart();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BOUTON_BOOT, INPUT_PULLUP);
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
  appliquerConfig();
}

void loop() {
  serveurTraiter();
  gererBouton();
  if (serveurConfigChangee()) appliquerConfig();
  if (!configPrete) {
    delay(10);
    return;
  }
  if (millis() - dernierNtp > NTP_MS) reglerHeure();
  if (!simulation && (derniereTentative == 0 || millis() - derniereTentative > intervalleFetch(verdictCourant()))) {
    rafraichir();
    afficher(false);
  }
  static unsigned long dernierTick = 0;
  if (millis() - dernierTick >= 1000) {
    dernierTick = millis();
    afficher(false);
  }
  delay(10);
}
