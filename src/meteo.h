#pragma once

#include <Arduino.h>

#include "dessin.h"

namespace meteo {

bool recuperer(float latitude, float longitude, int64_t nowMs, dessin::Meteo& out);

}  // namespace meteo
