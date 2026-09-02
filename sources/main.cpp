/**
 * @file main.cpp
 * @brief La borne : deux dossiers, tout le reste arrive par dlopen.
 *
 * Aucun vendor n'est linke ici, aucun jeu non plus. Le binaire ne sait que
 * deux choses : ou chercher les bibliotheques, et ou sont ses polices.
 *
 * launch<T> construit puis appelle run(). L'objet meurt AVANT le retour,
 * donc les dll se ferment avant que le code de sortie remonte au systeme.
 */

#include "ArcadeCore.hpp"

#include <string>

int main(int argc, char **argv) {
    const std::string libraries = (argc > 1) ? argv[1] : LIB_DIR;
    const std::string assets = (argc > 2) ? argv[2] : ASSETS_DIR;

    return launch<ArcadeCore>(libraries, assets);
}
