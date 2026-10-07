#pragma once

#include <Arduino.h>

#include <functional>
#include <vector>

#include "depart.h"

bool wifiConnecter(bool portailSiEchec);
void wifiOublier();

using SimulationCallback = std::function<void(const std::vector<int>& secondes)>;
using EtatCallback = std::function<String()>;

void serveurDemarrer(SimulationCallback simuler, EtatCallback etat);
void serveurTraiter();
bool serveurConfigChangee();
