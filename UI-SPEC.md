# Midi et demi — Specs UI/UX (brouillon v0)

> Refonte de l'interface, à iterer ensemble avant de coder.
> Objectif : passer d'une fenêtre dense « piste active » à une vraie **table de
> mixage** lisible, avec une **visu des boucles agrandie** et des **boutons
> regroupés** par fonction.

---

## 1. Objectifs (priorités de Romain)

1. **Table de mixage** : voir les **8 pistes en parallèle** (nom, volume, mute, état).
2. **Hiérarchie & lisibilité** : regrouper, aérer, couleurs/libellés clairs.
3. **Visu des boucles agrandie** : plus de place pour les couloirs + tête de lecture.
4. **Regrouper les boutons souris** en blocs distincts (cf. §3).

Pas de mode performance plein écran pour l'instant (tout-en-un).

---

## 2. Principe : séparer « jouable » et « souris »

- **Contrôles jouables / mappables** (qu'on pilote surtout depuis l'Oxygen) :
  Lecture/Stop, BPM, Métronome, Mesures, Rec/Annuler/Refaire/Effacer, volumes,
  sélection de piste. → bien visibles, regroupés « transport » et « piste ».
- **Contrôles souris** (config, jamais mappés) : regroupés à part, en 2 blocs :
  - **Bloc Instrument** (piste active) : `Charger plugin` · `Éditeur` · `Pads` · `Mapping…`
  - **Bloc Session** : `Enregistrer` · `Ouvrir` · `Exporter (.wav)`

---

## 3. Maquette proposée

```
┌──────────────────────────────────────────────────────────────────────────┐
│  Midi et demi                                                             │
├──────────────────────────────────────────────────────────────────────────┤
│  TRANSPORT   [▶ Lecture]  BPM [──●──] 120   ☐ Métronome    Mesure 2 · 3   │  jouable
├───────────────────────────────────────┬──────────────────────────────────┤
│  INSTRUMENT (piste active) — souris    │  SESSION — souris                 │
│  [Charger plugin] [Éditeur] [Pads]     │  [Enregistrer] [Ouvrir]           │
│  [Mapping…]                            │  [Exporter (.wav)]                │
├───────────────────────────────────────┴──────────────────────────────────┤
│  MIXER                                                                     │
│  ┌────┐┌────┐┌────┐┌────┐┌────┐┌────┐┌────┐┌────┐                         │
│  │[1] ││ 2  ││ 3  ││ 4  ││ 5  ││ 6  ││ 7  ││ 8  │  ← clic = piste active   │
│  │Vital│Drums│ —  ││    ││    ││    ││    ││    │  nom de l'instrument     │
│  │ ▮  ││ ▮  ││ ▯  ││    ││    ││    ││    ││    │  fader de volume         │
│  │ ▮  ││ ▮  ││ ▯  ││    ││    ││    ││    ││    │                          │
│  │Mute││Mute││Mute││    ││    ││    ││    ││    │  bouton mute             │
│  │●REC││ ▶  ││vide││    ││    ││    ││    ││    │  état (couleur)          │
│  └────┘└────┘└────┘└────┘└────┘└────┘└────┘└────┘                         │
│  Piste active :  Mesures [4 ▾]   [● Rec] [Annuler] [Refaire] [Effacer]    │  jouable
├──────────────────────────────────────────────────────────────────────────┤
│  BOUCLES (grande)                                                          │
│  1 ▕ ■■──────■■──── ▏                                                      │
│  2 ▕ ──■■──■■──■■── ▏     (tête de lecture qui défile)                     │
│  …                                                                         │
├──────────────────────────────────────────────────────────────────────────┤
│  🎹  clavier à l'écran                                                     │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 4. Détails

### Voie de mixage (× 8)
- **Numéro + sélection** : la piste active est encadrée / `[N]`.
- **Nom de l'instrument** : plugin, « Sampler » si pads, sinon « — ».
- **Fader de volume vertical**.
- **Mute**.
- **Pastille d'état** : gris = vide · vert = a une boucle · rouge = enregistre.
- Cliquer la voie = la rendre active (ses contrôles d'édition apparaissent en bas).

### Bandeau « Piste active »
Les actions d'édition de la piste sélectionnée : Mesures, Rec, Annuler, Refaire,
Effacer. (Mappables aussi.)

### Visu des boucles agrandie
- Plus haute, 8 couloirs lisibles, grille des mesures, tête de lecture nette.
- (Option) cliquer un couloir = sélectionner la piste (cohérent avec le mixer).

### Code couleur unifié
- **Gris** vide · **Vert** boucle présente · **Rouge** enregistrement · accent =
  piste active. Même code partout (voies + visu).

---

## 5. Décisions

- ✅ **Écran cible** : 15,4" 2880×1800 (Retina) → ~1440×900 points logiques.
  Fenêtre confortable visée : ~1280 × 820 points.
- ✅ **Faders verticaux** (look table de mixage).
- ✅ **Une grande zone de visu** à 8 couloirs (pas de mini-visu par voie).
- ✅ **Clavier à l'écran optionnel** : masqué par défaut, bouton `Clavier` pour
  l'afficher/cacher (gagne de la place).
- ✅ **Thème** : on garde le look JUCE par défaut pour l'instant (palette à voir
  dans un second temps).

## 6. Ordre d'implémentation (par petits bouts)

Refonte **UI seulement** (le moteur ne change pas) → chaque étape compile et se
teste sur le Mac.

1. **R1 — Zones & blocs de boutons** : réorganiser en zones claires (Transport /
   Instrument / Session / Mixer / Visu / Clavier) ; regrouper les 2 blocs souris.
2. **R2 — Table de mixage** : 8 voies (sélection, nom instrument, fader vertical,
   mute, pastille d'état). Le bandeau « piste active » garde Mesures/Rec/Undo/
   Redo/Effacer.
3. **R3 — Visu agrandie** : agrandir la zone des couloirs + clic sur un couloir =
   sélection de piste.
4. **R4 — Clavier optionnel** : bouton afficher/masquer.

---

_Dernière mise à jour : 2026-06-29_
