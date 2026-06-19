# Midi et demi 🎹

Un **looper multipiste minimaliste**, piloté au MIDI depuis un clavier maître,
pensé pour le *live looping* : empiler des boucles, jouer avec, enregistrer.

Né d'une frustration simple : les gros DAW sont des usines, et les loopers
fermés ne se laissent pas remapper comme on veut.

## En bref

- **Boucles MIDI multipistes** (8 pistes), chacune avec son propre instrument.
- **Overdub** (enrichir une boucle en cours) avec annulation de la dernière passe.
- **Mode performance** : déclencher / couper les pistes en live, et enregistrer
  le morceau final — en gardant l'arrangement rejouable **et** un export audio.
- **Mapping clavier entièrement configurable** (mode apprentissage).
- **Basse latence**, son produit par des **plugins instruments** (VST3 / AU).

## Cible & stack

- **Plateforme** : macOS (Monterey), via CoreAudio. Portage Linux/Windows
  envisageable plus tard (le code est multiplateforme).
- **Techno** : C++ + [JUCE](https://juce.com/) + CMake.
- **Contrôleur de référence** : M-Audio Oxygen Pro Mini.

## Documentation

- [`SPEC.md`](SPEC.md) — les spécifications fonctionnelles (le *quoi*).
- [`ARCHITECTURE.md`](ARCHITECTURE.md) — le découpage en couches (le *comment*).
- [`BUILD.md`](BUILD.md) — compiler et lancer l'app sur macOS.

## État

🚧 **En conception.** Specs et architecture posées ; le code arrive par étapes
(cf. la roadmap dans `ARCHITECTURE.md §8`).

## Licence

Sous licence **GNU General Public License v3.0** — voir [`LICENSE`](LICENSE).
(Ce projet utilise [JUCE](https://juce.com/) en mode open-source, ce qui impose
une licence GPL v3 compatible.)
