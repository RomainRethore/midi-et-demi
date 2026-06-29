#!/usr/bin/env bash
#
# Met à jour, compile et lance "Midi et demi" (macOS).
# Usage :  ./run.sh            (Debug)
#          ./run.sh Release    (optimisé)
#
# Tout le corps est dans une fonction : ainsi, même si "git pull" met à jour ce
# script en cours d'exécution, bash a déjà lu le fichier en entier (pas de bug).

set -euo pipefail

main() {
    cd "$(dirname "$0")"

    local config="${1:-Debug}"

    echo "==> Mise à jour (git pull)"
    git pull

    echo "==> Récupération de JUCE (sous-module)"
    git submodule update --init --recursive

    echo "==> Configuration CMake (${config})"
    cmake -B build -DCMAKE_BUILD_TYPE="${config}"

    echo "==> Compilation"
    cmake --build build

    local app
    app="$(find build -name '*.app' -maxdepth 4 | head -1)"
    if [[ -z "${app}" ]]; then
        echo "!! Application introuvable sous build/ — la compilation a-t-elle réussi ?"
        exit 1
    fi

    echo "==> Lancement : ${app}"
    /usr/bin/open "${app}"   # chemin complet pour contourner l'alias 'open' (VSCode)
}

main "$@"
