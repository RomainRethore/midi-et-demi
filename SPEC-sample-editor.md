# Spec — Éditeur / découpeur de samples (brouillon v0)

> Charger un fichier audio (même un morceau entier), visualiser la forme d'onde,
> sélectionner une portion, l'écouter, puis la **découper et l'assigner à un pad**.
> Le bouclage se fait en réutilisant le moteur (slice = sample de pad, calé au tempo).

---

## 1. Décisions

- ✅ **Bouclage** : le segment découpé devient un **sample de pad**. Pour le
  boucler, on le déclenche une fois par boucle (moteur existant). Une aide
  **« Caler le BPM sur la sélection »** ajuste le tempo du projet pour que la
  sélection fasse exactement N mesures → bouclage propre, sans time-stretch.
- ✅ **Sélection libre** sur la forme d'onde, avec affichage de la **durée**
  (secondes + équivalent en temps/mesures au BPM courant).
- ✅ Le slice est **écrit en fichier `.wav`** (dossier dédié sous Application
  Support), puis assigné à un pad comme n'importe quel sample → **persistant en
  session** (le pad référence ce fichier).

---

## 2. Fenêtre « Éditeur de sample »

Ouverte depuis un bouton (bloc *Instrument*). Agit sur la **piste active**.

```
┌──────────────────────────────────────────────────────────────┐
│ [Charger un fichier...]            Fichier : groove.wav        │
├──────────────────────────────────────────────────────────────┤
│  FORME D'ONDE  ▕▂▃▅▇▆▃▂▁▂▅▇▆▃▁ ... ▏                          │
│                 [─────█████████─────]  ← sélection (glisser)   │
├──────────────────────────────────────────────────────────────┤
│ Sélection : 1.92 s  (~4 temps / 1 mesure à 125 BPM)           │
│ [Écouter sélection] [Écouter tout] [Stop]                     │
│ Caler le BPM : [1 mesure ▾]  [Appliquer]                      │
│ Découper vers : Pad [1 ▾]   [Découper → pad]                  │
└──────────────────────────────────────────────────────────────┘
```

---

## 3. Détails

### Forme d'onde
- `juce::AudioThumbnail` (+ cache + AudioFormatManager) pour dessiner l'onde.
- Sélection : glisser à la souris → début/fin (poignées ajustables ensuite).
- Affiche un curseur de lecture pendant la pré-écoute.

### Pré-écoute
- « Écouter sélection » / « Écouter tout » → réutilise la pré-écoute du moteur
  (un buffer one-shot, hors export). « Stop » coupe.

### Durée / tempo
- Affiche la durée de la sélection en **secondes** et en **temps/mesures** au
  BPM courant.
- **Caler le BPM** : choisir un nombre de mesures cible → BPM calculé pour que la
  sélection = ces mesures, appliqué au projet (`setTempo`). Ainsi un déclenchement
  par boucle reboucle pile.

### Découpe → pad
- Extrait la région du fichier (au sample-rate natif), écrit un `.wav` dans
  `~/Library/Application Support/Midi et demi/slices/`, puis l'assigne au pad
  choisi de la piste active (réutilise `loadSampleToActiveTrack`).

---

## 4. Ordre d'implémentation

1. **E1** — fenêtre + chargement de fichier + **forme d'onde** (AudioThumbnail).
2. **E2** — **sélection** à la souris + affichage durée (s + temps/mesures).
3. **E3** — **pré-écoute** sélection / tout / stop (région via le moteur).
4. **E4** — **découpe → pad** (écriture du `.wav` + assignation).
5. **E5** — **caler le BPM sur la sélection** (N mesures → tempo).

---

_Dernière mise à jour : 2026-06-29_
