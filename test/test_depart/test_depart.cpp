#include <unity.h>

#include <cstdlib>
#include <cstring>
#include <ctime>

#include "depart.h"

using namespace depart;

static const int64_t NOW = 1791450000000LL;
static const Reglages R = REGLAGES_DEFAUT;

static Passage passage(int secondes, bool fiable = true) {
  Passage p;
  p.ligne = "T1";
  p.idLigne = "101";
  p.destination = "Chalezeule";
  p.sensAller = true;
  p.secondes = secondes;
  p.fiable = fiable;
  return p;
}

static Verdict un(int secondes, int64_t now = NOW, int64_t fetchedAt = NOW) {
  return evaluer({passage(secondes)}, now, fetchedAt, R);
}

void setUp() {}
void tearDown() {}

void test_fenetres() {
  TEST_ASSERT_EQUAL(Etat::Inconnu, un(120).etat);
  TEST_ASSERT_EQUAL(Etat::Rate, un(120).candidats[0].etat);
  TEST_ASSERT_EQUAL(Etat::Inconnu, un(359).etat);
  TEST_ASSERT_EQUAL(Etat::Cours, un(360).etat);
  TEST_ASSERT_EQUAL(Etat::Cours, un(419).etat);
  TEST_ASSERT_EQUAL(Etat::Prepare, un(420).etat);
  TEST_ASSERT_EQUAL(Etat::Prepare, un(599).etat);
  TEST_ASSERT_EQUAL(Etat::Tranquille, un(600).etat);
  TEST_ASSERT_EQUAL(Etat::Tranquille, un(1500).etat);
}

void test_premier_rate_verdict_sur_le_second() {
  Verdict v = evaluer({passage(120), passage(1320)}, NOW, NOW, R);
  TEST_ASSERT_EQUAL(Etat::Tranquille, v.etat);
  TEST_ASSERT_EQUAL(1, v.cible);
  TEST_ASSERT_EQUAL(1320, v.cibleOuNull()->passage.secondes);
  TEST_ASSERT_EQUAL(Etat::Rate, v.candidats[0].etat);
  TEST_ASSERT_EQUAL(2, v.candidats.size());
}

void test_tri_par_heure_de_tram() {
  Verdict v = evaluer({passage(1320), passage(300)}, NOW, NOW, R);
  TEST_ASSERT_EQUAL(300, v.candidats[0].passage.secondes);
}

void test_decompte_local() {
  Verdict v = un(400, NOW + 90000, NOW);
  TEST_ASSERT_EQUAL(Etat::Inconnu, v.etat);
  TEST_ASSERT_EQUAL(310, (int)v.candidats[0].tramSec);
  Verdict v2 = un(600, NOW + 90000, NOW);
  TEST_ASSERT_EQUAL(150, (int)v2.cibleOuNull()->resteSec);
  TEST_ASSERT_EQUAL(Etat::Prepare, v2.etat);
}

void test_fetched_dans_le_futur_ignore() {
  Verdict v = un(600, NOW, NOW + 5000);
  TEST_ASSERT_EQUAL(240, (int)v.cibleOuNull()->resteSec);
}

void test_aucun_passage() {
  Verdict v = evaluer({}, NOW, NOW, R);
  TEST_ASSERT_EQUAL(Etat::Inconnu, v.etat);
  TEST_ASSERT_NULL(v.cibleOuNull());
  Libelles l = libelles(v, NOW);
  TEST_ASSERT_EQUAL_STRING("Pas de tram annoncé", l.titre.c_str());
  TEST_ASSERT_EQUAL_STRING("--", l.compte.c_str());
}

void test_libelle_theorique() {
  Verdict v = evaluer({passage(900, false)}, NOW, NOW, R);
  Libelles l = libelles(v, NOW);
  TEST_ASSERT_NOT_NULL(strstr(l.detail.c_str(), "(horaire théorique)"));
  TEST_ASSERT_EQUAL_STRING("Tu as le temps", l.titre.c_str());
  TEST_ASSERT_EQUAL_STRING("9 min", l.compte.c_str());
}

void test_libelle_cours() {
  Libelles l = libelles(un(390), NOW);
  TEST_ASSERT_EQUAL_STRING("Pars maintenant", l.titre.c_str());
  TEST_ASSERT_EQUAL_STRING("30 s", l.compte.c_str());
  TEST_ASSERT_EQUAL_STRING("Tram T1 vers Chalezeule dans 6 min 30.", l.detail.c_str());
}

void test_libelle_prepare() {
  Libelles l = libelles(un(520), NOW);
  TEST_ASSERT_EQUAL_STRING("Prépare-toi", l.titre.c_str());
  TEST_ASSERT_EQUAL_STRING("2 min 40", l.compte.c_str());
}

void test_fmt_duree() {
  TEST_ASSERT_EQUAL_STRING("45 s", fmtDuree(45, true).c_str());
  TEST_ASSERT_EQUAL_STRING("2 min 05", fmtDuree(125, true).c_str());
  TEST_ASSERT_EQUAL_STRING("2 min", fmtDuree(125).c_str());
  TEST_ASSERT_EQUAL_STRING("0 s", fmtDuree(-5, true).c_str());
}

void test_fmt_hm() {
  struct tm t = {};
  t.tm_year = 126;
  t.tm_mon = 9;
  t.tm_mday = 10;
  t.tm_hour = 9;
  t.tm_min = 5;
  t.tm_isdst = -1;
  TEST_ASSERT_EQUAL_STRING("9h05", fmtHM((int64_t)mktime(&t) * 1000).c_str());
  t.tm_hour = 23;
  t.tm_min = 0;
  t.tm_isdst = -1;
  TEST_ASSERT_EQUAL_STRING("23h", fmtHM((int64_t)mktime(&t) * 1000).c_str());
}

void test_normaliser_reglages() {
  Reglages n = normaliserReglages(7, -3);
  TEST_ASSERT_EQUAL(7, n.departMin);
  TEST_ASSERT_EQUAL(0, n.margeMin);
  Reglages g = normaliserReglages(99, 99);
  TEST_ASSERT_EQUAL(45, g.departMin);
  TEST_ASSERT_EQUAL(10, g.margeMin);
  Reglages z = normaliserReglages(0, 1);
  TEST_ASSERT_EQUAL(1, z.departMin);
}

int main() {
  setenv("TZ", "Europe/Paris", 1);
  tzset();
  UNITY_BEGIN();
  RUN_TEST(test_fenetres);
  RUN_TEST(test_premier_rate_verdict_sur_le_second);
  RUN_TEST(test_tri_par_heure_de_tram);
  RUN_TEST(test_decompte_local);
  RUN_TEST(test_fetched_dans_le_futur_ignore);
  RUN_TEST(test_aucun_passage);
  RUN_TEST(test_libelle_theorique);
  RUN_TEST(test_libelle_cours);
  RUN_TEST(test_libelle_prepare);
  RUN_TEST(test_fmt_duree);
  RUN_TEST(test_fmt_hm);
  RUN_TEST(test_normaliser_reglages);
  return UNITY_END();
}
