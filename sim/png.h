#pragma once

#include <cstdint>
#include <string>

bool ecrirePngMonochrome(const std::string& chemin, const uint8_t* bits, int largeur, int hauteur);
