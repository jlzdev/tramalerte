#pragma once

#include <Arduino.h>

#include <string>
#include <vector>

#include "dessin.h"

void ecranInit();
void ecranAfficherVerdict(const dessin::Contenu& contenu, bool complet);
void ecranAfficherQr(const char* contenu, const char* titre, const std::vector<String>& lignes);
void ecranAfficherMessage(const char* titre, const char* texte);
void ecranDormir();
