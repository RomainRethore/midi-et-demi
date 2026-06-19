# Midi et demi — Architecture (brouillon v0)

> Comment le code est découpé. Objectif : couches bien séparées, on peut
> toucher à une couche sans casser les autres. C++ moderne, SOLID, POO.
>
> _Document de travail — à lire avec `SPEC.md`. Itératif._

---

## 1. Principes directeurs

1. **Couches concentriques, dépendances vers l'intérieur.** Le code "métier"
   (le cœur) ne connaît rien des détails techniques (JUCE, l'OS, l'écran). Ce
   sont les couches *externes* qui dépendent du *cœur*, jamais l'inverse.
2. **Le cœur est en C++ pur, sans JUCE.** Donc compilable et testable **sur
   Linux**, sans carte son ni Mac. C'est ce qui rend l'Option 2 confortable.
3. **Inversion de dépendance (le "D" de SOLID).** Quand le cœur a besoin d'un
   service technique (lire un fichier, sortir du son), il définit une
   **interface** (classe abstraite) ; la couche technique en fournit
   l'implémentation. Le cœur dépend de l'abstraction, pas du détail.
4. **La frontière temps réel est sacrée** (cf. §3). Le fil audio ne bloque
   jamais ; il communique avec le reste par files lock-free.
5. **Une responsabilité par classe** (le "S" de SOLID). Une classe = une raison
   de changer.

---

## 2. Vue d'ensemble en couches

```
┌─────────────────────────────────────────────────────────────┐
│  5. UI (JUCE)                                                 │
│     Fenêtres, boutons, vumètres, écran de mapping.            │
│     Ne parle JAMAIS directement au fil audio.                 │
├─────────────────────────────────────────────────────────────┤
│  4. INFRASTRUCTURE (adaptateurs JUCE / OS)                    │
│     CoreAudio, entrée MIDI, fichiers, scan de plugins.        │
│     Implémente les interfaces définies par les couches < 4.   │
├─────────────────────────────────────────────────────────────┤
│  3. MOTEUR AUDIO (temps réel)            ⚡ zone contrainte    │
│     Callback audio, hôte de plugins, mixage.                  │
│     Pas d'alloc / pas de verrou / pas d'I-O.                  │
├─────────────────────────────────────────────────────────────┤
│  2. APPLICATION (cas d'usage / services)                      │
│     Séquenceur, enregistrement de boucle, overdub,            │
│     moteur de mapping, sauvegarde de session.                 │
├─────────────────────────────────────────────────────────────┤
│  1. DOMAINE (cœur, C++ pur)                                   │
│     Track, Boucle MIDI, Note, Tempo, Session (le modèle).     │
│     Aucune dépendance externe. 100% testable sur Linux.       │
└─────────────────────────────────────────────────────────────┘

         Règle : une flèche de dépendance pointe TOUJOURS vers le bas
         (l'extérieur connaît l'intérieur, jamais l'inverse).
```

---

## 3. La frontière temps réel (à ne jamais franchir)

Deux "mondes" qui ne partagent pas de mémoire mutable directement :

```
   MONDE NORMAL                        MONDE TEMPS RÉEL
   (UI, fichiers, plugins à charger)   (callback audio, ~3 ms)

   UI / Application  ──commandes──►   Moteur audio
                     ◄───état──────
                  (files lock-free, sans verrou)
```

- **Commandes** (UI → audio) : « arme la piste 2 », « lance la piste 3 »,
  « change le BPM ». Poussées dans une file lock-free, lues par le fil audio.
- **État** (audio → UI) : position de lecture, niveaux, « la piste 2 enregistre ».
  Renvoyé via une autre file / valeurs atomiques.
- Le **chargement d'un plugin** (lent, alloue de la mémoire) se fait dans le
  monde normal, *puis* l'objet prêt est passé au fil audio par échange de
  pointeur atomique.

> Implémentations JUCE utiles : `juce::AbstractFifo`, `std::atomic`,
> `juce::AudioProcessorGraph` pour le routage. (détails plus tard)

---

## 4. Les couches en détail

### Couche 1 — Domaine (C++ pur, testable sur Linux)
Le modèle, sans aucune logique technique :
- `NoteEvent` — une note (hauteur, vélocité, position en *ticks*, durée).
- `MidiClip` — la boucle d'une piste : une liste de `NoteEvent` + longueur en mesures.
- `Track` — une piste : référence d'instrument, volume, mute, son `MidiClip`.
- `Tempo` / `TimeSignature` — BPM, mesures, conversions temps musical ↔ échantillons.
- `Session` — l'agrégat : tempo, liste de `Track`, mapping, arrangement de perf.

### Couche 2 — Application (services, C++ pur ou presque)
Orchestration. Dépend du Domaine, expose des interfaces :
- `Sequencer` — sait quelles notes jouer à l'instant T, gère le bouclage.
- `LoopRecorder` — capture le MIDI entrant dans un `MidiClip` (count-in,
  quantification, **overdub** = accumulation, **undo** de la dernière passe).
- `MappingEngine` — traduit un message du clavier en *action* ; mode
  apprentissage (MIDI learn). Conçu pour ajouter des actions sans le modifier
  (le "O" de SOLID, ex. patron *Command*).
- `ArrangementRecorder` — enregistre quelle piste joue à quel moment (perf).
- `SessionSerializer` — (dé)sérialise une `Session` en JSON.
- **Interfaces sortantes** (implémentées par l'infra) :
  `ISessionRepository`, `IPluginHost`, `IMidiSource`, `IAudioOutput`…

### Couche 3 — Moteur audio (temps réel ⚡)
- `AudioEngine` — le callback audio : lit les commandes, demande au `Sequencer`
  les notes dues, les envoie aux plugins, mixe les pistes, sort le son.
- `PluginInstance` — enveloppe un plugin chargé (son `processBlock`).
- Files lock-free entre ce monde et le reste.
- **Règle absolue** : ici, pas d'alloc, pas de verrou, pas d'I-O.

### Couche 4 — Infrastructure (JUCE / macOS)
Les adaptateurs concrets qui implémentent les interfaces des couches 1-2 :
- `CoreAudioOutput` → `IAudioOutput`.
- `JuceMidiInput` → `IMidiSource` (reçoit le MIDI du clavier).
- `JucePluginHost` → `IPluginHost` (scan + chargement VST3/AU via JUCE).
- `FileSessionRepository` → `ISessionRepository` (lire/écrire sur disque).

### Couche 5 — UI (JUCE)
- Composants : pistes, transport, vumètres, **écran de mapping**.
- Envoie des *commandes* à l'application/au moteur ; lit l'*état* publié.
- Jamais d'accès direct au fil audio.

---

## 5. SOLID, concrètement ici

| Principe | Application dans le projet |
|---|---|
| **S** — Responsabilité unique | `Track` stocke ; `Sequencer` ordonnance ; `PluginInstance` enveloppe un plugin. Chacun une seule raison de changer. |
| **O** — Ouvert/fermé | Ajouter une nouvelle *action* de mapping (ex. « tap tempo ») sans modifier le `MappingEngine` (patron Command). |
| **L** — Substitution | Toute implémentation de `IAudioOutput` (CoreAudio, ou un faux pour les tests) est interchangeable. |
| **I** — Interfaces fines | `IMidiSource`, `IAudioOutput`, `ISessionRepository` séparées plutôt qu'une grosse interface fourre-tout. |
| **D** — Inversion de dépendance | Le cœur définit les interfaces ; l'infra (JUCE) les implémente. Le cœur ignore JUCE. |

Le "D" est la clé de ton souhait : **on peut remplacer la couche technique sous
le cœur sans toucher au cœur** (ex. brancher un faux moteur audio pour tester
sur Linux, ou porter sous Linux plus tard).

---

## 6. Arborescence proposée

```
midi_et_demi/
├── CMakeLists.txt          # description unique du build (Mac + Linux)
├── SPEC.md
├── ARCHITECTURE.md
├── external/               # JUCE (sous-module git)
├── src/
│   ├── domain/             # couche 1 — C++ pur
│   ├── application/        # couche 2 — services + interfaces
│   ├── engine/             # couche 3 — temps réel
│   ├── infra/              # couche 4 — adaptateurs JUCE/macOS
│   ├── ui/                 # couche 5 — composants JUCE
│   └── app/                # main + "composition root" (le câblage)
└── tests/                  # tests unitaires couches 1-2 (lancés sur Linux)
```

> Le **"composition root"** (`src/app/`) est le seul endroit qui assemble tout :
> il crée les objets concrets (CoreAudio, etc.) et les injecte dans le cœur.
> C'est là que les dépendances sont « branchées ».

---

## 7. Qu'est-ce qu'on teste où ?

| | Sur Linux (ici, moi) | Sur le Mac (toi) |
|---|---|---|
| Domaine (couche 1) | ✅ tests unitaires | — |
| Application (couche 2) | ✅ tests unitaires (avec faux audio/MIDI) | — |
| Moteur audio (couche 3) | ✅ compile + logique testable | ✅ ressenti latence |
| Infra CoreAudio / AU | compile seulement | ✅ test réel |
| Clavier MIDI réel | — | ✅ |
| UI | compile + lance | ✅ ergonomie |

---

## 8. Ordre de construction proposé (roadmap)

Principe : un **« squelette qui marche »** d'abord (le plus court chemin vers
« j'entends une note »), puis on étoffe couche par couche.

1. **Squelette audio** : projet CMake + JUCE qui s'ouvre, sort du son,
   charge **un** plugin, et joue une note du clavier. → *preuve que la chaîne
   audio/MIDI/plugin fonctionne sur ton Mac.*
2. **Tempo + transport** : horloge musicale, métronome, play/stop.
3. **Une boucle** : enregistrer puis rejouer en boucle sur **une** piste.
4. **Multipiste** : 8 pistes, chacune son plugin et son volume.
5. **Overdub + undo**.
6. **Mapping configurable** (MIDI learn).
7. **Mode performance + arrangement**.
8. **Sauvegarde/chargement de session**.
9. **Export audio `.wav`**.

Chaque étape donne un truc *utilisable*, et s'appuie sur la précédente sans la
casser — c'est tout l'intérêt du découpage en couches.

---

_Dernière mise à jour : 2026-06-19_
