# Midi et demi — Spécifications (brouillon v0)

> Looper multipiste MIDI, calé au tempo, piloté depuis un clavier maître
> M-Audio Oxygen Pro Mini. Chaque piste est une boucle avec son propre
> instrument. Un mode « performance » enregistre le morceau final.
>
> _Document de travail — on itère dessus. Les points ouverts sont notés ⬜._

---

## 1. Vision en une phrase

Un mini-DAW **simple et entièrement mappable** dédié au *live looping* : on
empile des boucles MIDI (chacune avec son son), puis on joue avec ces boucles en
live, et on enregistre le résultat.

But explicite : retrouver le workflow de MPC Beats / vue Session d'Ableton, mais
**sans la complexité** et avec un **mapping clavier libre**.

---

## 2. Parcours utilisateur de référence

1. Choisir un son (instrument joué au clavier, ou sons assignés aux pads).
2. Chercher une mélodie librement, sans enregistrer.
3. Régler **tempo (BPM)** + **nombre de mesures**, lancer l'enregistrement en
   boucle, jouer la mélodie, arrêter → **Piste 1**.
4. Changer de son, lancer la **lecture** de la boucle, chercher une 2ᵉ mélodie
   par-dessus.
5. Enregistrer → **Piste 2**. Répéter jusqu'à ~10 pistes empilées.
6. **Mode performance** : lancer un enregistrement global, déclencher / couper
   les pistes en live → c'est le morceau final, exportable en audio.

---

## 3. Concepts

| Concept | Définition |
|---|---|
| **Projet / Session** | L'ensemble du travail : tempo, pistes, mapping, réglages. Sauvegardable et réouvrable. |
| **Piste** | Une boucle MIDI. ~10 par session. Chacune a son **instrument**, son volume, son état (lecture/mute). |
| **Instrument** | Un plugin (VST3 / LV2 / CLAP) chargé par l'app, qui produit le son d'une piste. |
| **Boucle / Clip** | Les **notes MIDI** enregistrées sur une piste, d'une longueur de N mesures, qui se rejouent en boucle. |
| **Pads** | Les 8 pads du clavier : déclencher/couper des pistes en performance, et/ou jouer des sons. |
| **Tempo + mesures** | Définissent la longueur d'une boucle. L'enregistrement et la lecture sont **calés** dessus. |
| **Enregistrement boucle** | Capture des **notes** jouées (pas l'audio) → le son reste modifiable après coup. |
| **Enregistrement performance** | Capture du **morceau final** rendu en audio quand on joue avec les pistes. |

---

## 4. Périmètre

### 4.1 MVP (v1) — ce qu'on construit d'abord

- [ ] **Entrée MIDI** depuis l'Oxygen Pro Mini (notes, vélocité, potards, pads, transport).
- [ ] **Hébergement d'un plugin instrument par piste** (VST3 / LV2 / CLAP).
- [ ] **Multipiste** : 8 pistes, chacune avec son instrument, son volume, sa longueur de boucle.
- [ ] **Tempo global (BPM)** + **métronome / clic**.
- [ ] **Longueur de boucle par piste** en nombre de mesures.
- [ ] **Enregistrement de boucle calé** : décompte (count-in), enregistrement
      quantifié sur la longueur choisie, bouclage automatique.
- [ ] **Overdub** : rejouer par-dessus une boucle déjà lancée pour l'enrichir
      (les notes s'accumulent passe après passe), avec **annulation de la dernière passe**.
- [ ] **Lecture / arrêt par piste**, synchronisés (les boucles redémarrent ensemble).
- [ ] **Mode performance** : déclencher/couper les pistes en live, en capturant
      **l'arrangement** (quelle piste joue à quel moment) → rejouable et modifiable après coup.
- [ ] **Export audio** du rendu final en `.wav`.
- [ ] **Sauvegarde / réouverture de session** (projet complet).
- [ ] **Mapping MIDI configurable** avec mode « apprentissage » (MIDI learn).
- [ ] **Latence basse** via backend audio Linux (JACK / PipeWire). Cible : ~5 ms ⬜ à valider.

### 4.2 Plus tard (v2+)

- Sons assignés aux pads (échantillons / one-shots, kits de batterie).
- Plugins d'effets par piste (reverb, delay…).
- Édition fine des notes / quantification a posteriori.
- Export `.mp3`, export MIDI.
- Undo/redo.
- Bibliothèque de sons fournie avec l'app.

### 4.3 Hors périmètre (pour l'instant)

- Édition audio façon studio (montage, automation détaillée).
- Portage Linux / Windows — on cible **macOS (Monterey 12.7.6)** d'abord ;
  les autres OS viendront « gratuitement » via JUCE plus tard.
- Collaboration / cloud.

---

## 5. Mapping clavier (le cœur de la valeur)

Tout doit être réassignable. Mapping **par défaut** proposé (à ajuster) :

| Contrôle Oxygen Pro Mini | Action par défaut |
|---|---|
| Touches (32) | Jouer l'instrument de la piste sélectionnée |
| Bouton **REC** | Armer / lancer l'enregistrement de boucle sur la piste sélectionnée |
| Bouton **PLAY / STOP** | Transport global |
| Pads 1-8 | Lancer / couper les pistes 1-8 (mode performance) |
| Potards | Volume des pistes |
| Flèches / preset | Sélectionner la piste active |

**Mode apprentissage** : on clique sur une action dans l'UI → on bouge un
contrôle du clavier → l'association est mémorisée. Mapping sauvegardé dans la session.

---

## 6. Pistes techniques (à confirmer)

- **Cible principale** : **macOS Monterey 12.7.6**. JUCE étant multiplateforme,
  le même code C++ pourra être recompilé pour Linux/Windows plus tard.
- **Langage / framework** : C++ + **JUCE** (gère audio bas-niveau, MIDI,
  hébergement de plugins VST3/AU, et GUI). ⬜ à valider.
- **Outils de build (Mac)** : Xcode (version compatible Monterey : 14.2) + CMake.
- **Audio backend** : **CoreAudio** (basse latence native sur Mac, sans config).
- **Formats de plugins supportés** : **VST3 + AU** sur Mac (LV2 réservé au futur
  portage Linux). Instruments gratuits de départ : Vital, Surge XT, Dexed, sfizz.
- **Format de session** : dossier de projet (JSON + données MIDI + arrangement
  de performance + état des plugins + rendus audio). ⬜ à concevoir.

---

## 6 bis. Workflow de développement (contrainte importante)

Claude Code tourne sur une **machine Linux** ; le Mac (cible) **ne peut pas
recevoir Claude Code**. On adopte donc :

- **Écriture du code + compilation de vérification + tests automatiques** sur le
  Linux (toute la logique OS-indépendante : séquenceur, boucles, overdub,
  session, mapping).
- **Build final + tests audio/MIDI réels** (CoreAudio, plugins AU, clavier) sur
  le **Mac**, par l'utilisateur.
- **CMake** comme description unique du projet → génère un projet **Xcode** sur
  Mac et une build Linux ici, sans double maintenance.
- **Git** comme moyen de transfert Linux → Mac (`git pull` sur le Mac).
- Objectif : maximiser ce qui est validé côté Linux pour que le code **compile
  du premier coup** sur le Mac et limiter les allers-retours.

## 7. Décisions prises

- ✅ **Type de boucle** : MIDI (on enregistre les notes, le son reste modifiable).
- ✅ **Latence** : très basse → app native Linux (pas de web).
- ✅ **Cible** : macOS Monterey 12.7.6 (CoreAudio, VST3/AU) ; portage autres OS plus tard.
- ✅ **Workflow** : dév + tests logiques sur Linux, build/tests audio finaux sur le Mac (cf. §6 bis).
- ✅ **Plugins** : on part de zéro avec des instruments gratuits ; cibles VST3 + AU (Mac).
- ✅ **Performance** : on garde l'arrangement (rejouable/modifiable) **+** un export audio.
- ✅ **Overdub** : oui, dès le MVP, avec **annulation de la dernière passe** (undo live).
- ✅ **Session** : sauvegardable et réouvrable.
- ✅ **Nombre de pistes** : **8** au départ (1 par pad), extensible plus tard.
- ✅ **Performance** : **on/off piste par piste** (pas de scènes au MVP).
- ✅ **Longueurs de boucle** : différentes autorisées par piste, en gardant des
  longueurs **multiples** (4 / 8 / 16 mesures) pour que tout reste calé.

## 8. Points encore ouverts ⬜

_Aucun pour l'instant — la spec fonctionnelle v1 est complète._
_Prochaine étape : document d'architecture (le « comment »)._

---

_Dernière mise à jour : 2026-06-19_
