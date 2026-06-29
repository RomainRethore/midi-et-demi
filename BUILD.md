# Compiler & lancer (macOS)

Guide pour construire **Midi et demi** sur ton Mac (Monterey 12.7.6).
Le code est écrit côté Linux ; la compilation se fait ici.

---

## 1. Prérequis (à installer une seule fois)

### a) Les outils de compilation Apple
Dans le Terminal :
```bash
xcode-select --install
```
Ça installe les **Command Line Tools** (compilateur + SDK macOS). Léger, suffisant
pour compiler. *(Xcode complet n'est pas nécessaire pour l'instant ; si un jour tu
le veux pour déboguer, sur Monterey il faut la version **14.2**, à télécharger sur
developer.apple.com/download/all — l'App Store ne propose que la dernière, qui ne
tourne pas sur Monterey.)*

### b) CMake (≥ 3.22)
Le plus simple, au choix :
```bash
brew install cmake            # si tu as Homebrew
```
…ou bien télécharge l'installeur `.dmg` sur https://cmake.org/download/ puis, dans
le Terminal une fois installé :
```bash
sudo "/Applications/CMake.app/Contents/bin/cmake-gui" --install
```
Vérifie que ça répond :
```bash
cmake --version
```

---

## 2. Récupérer le code + JUCE

Dans le dossier du dépôt :
```bash
git pull
git submodule update --init --recursive   # télécharge JUCE (~73 Mo)
```

---

## 3. Compiler

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
```
La 1ʳᵉ compilation est longue (JUCE se compile entièrement) : compte plusieurs
minutes. Les suivantes seront rapides.

---

## 4. Lancer

L'app est un bundle `.app` quelque part sous `build/`. Pour le retrouver et
l'ouvrir :
```bash
open "$(find build -name '*.app' -maxdepth 4 | head -1)"
```

---

## 5. Ce que tu dois voir / entendre ✅

- Une fenêtre **« Midi et demi »** avec un clavier de piano à l'écran.
- Une ligne de statut indiquant le **périphérique audio** et les **claviers MIDI
  détectés** (ton Oxygen Pro Mini doit y apparaître).
- En cliquant sur le piano à l'écran **ou** en jouant sur l'Oxygen → **du son**
  (une onde sinusoïdale toute simple).

Si tu as ça : l'incrément **1a est validé**, toute la chaîne fonctionne. 🎉

### Étape 1b — charger un vrai plugin
- Clique **« Charger un plugin… »** et choisis un instrument dans
  `/Library/Audio/Plug-Ins/VST3` (ex. `Vital.vst3`, `Surge XT.vst3`, `Dexed.vst3`).
- La ligne `Instrument :` du statut doit afficher le nom du plugin.
- Joue → tu entends **le plugin** (plus le sinus).
- **« Ouvrir l'editeur »** affiche l'interface du plugin (pour choisir un preset).

> Les `.vst3` / `.component` sont des « paquets » : le sélecteur macOS les
> présente comme des fichiers cliquables.

### Étape 3 — enregistrer et boucler une piste
1. Charge un plugin et choisis le **nombre de mesures** (sélecteur « Mesures »).
2. Clique **« Enregistrer »** : la lecture démarre **immédiatement depuis le
   début**, le métronome te cale → joue ta mélodie.
3. À la fin des mesures, la boucle **rejoue automatiquement** en boucle ce que
   tu as joué. L'état « Boucle : … » suit la progression.
4. **« Effacer »** vide la boucle. **« Re-enregistrer »** en refait une.

> Le bouton **« Lecture/Stop »** suit l'état réel ; tu peux aussi arrêter
> l'enregistrement à la main en recliquant **« Stop REC »**.

### Étapes 4-5 — multipiste + overdub
1. Boutons **1-8** : choisis la piste active (numéro entre crochets `[N]`).
   Couleur : gris = vide, vert = a une boucle, rouge = enregistre.
2. Charge un plugin, règle ses **Mesures**, **Enregistrer** : la boucle tourne
   **en continu**, tu joues quand tu veux, tu **réentends** ce que tu as déjà
   posé et tu **ajoutes** par-dessus (overdub). **Stop REC** quand tu as fini.
3. Sur une piste qui a déjà une boucle, le bouton devient **« Overdub »** :
   il enrichit la boucle (ne la remplace pas).
4. **Annuler** retire la **dernière note**, **Refaire** la remet (note par note) ;
   **Effacer** vide la piste. (Les notes ET la **molette de modulation / pitch
   bend** sont enregistrées et bouclées.)
5. Change de piste, charge un autre son, recommence → empile tes pistes.

### Étape 6 — mapping MIDI
1. Clique **« Mapping… »** : actions disponibles = Lecture/Stop, Enregistrer,
   Effacer, **Annuler (undo)**, **Refaire (redo)**, Métronome,
   **Volume piste active** (potard), **Sélecteur de piste** (potard → piste 1-8),
   **Mesures piste active** (potard → 1/2/4/8), **BPM** (potard → 40-240),
   **Volume piste 1-8**, **Éditeur piste active**. Liste défilante + moniteur du
   dernier contrôle reçu. Seul le **chargement de plugin** reste à la souris.
2. Clique **« Apprendre »** sur une action, puis **bouge un potard / bouton**
   de l'Oxygen → l'association s'affiche (« CC n »). Le **X** efface.
3. Le contrôle déclenche l'action.

> Utilise des **potards / boutons (CC)** : ils ne se confondent jamais avec les
> touches du clavier. Évite les **pads qui envoient des notes** pour piloter des
> actions (même n° de note qu'une touche → collision).

> Le mapping est **sauvegardé automatiquement** (fichier global
> `~/Library/Application Support/Midi et demi/mapping.json`) et rechargé au démarrage.

### Pads / samples
- Sur la piste active, clique **« Pads… »** : charge un **sample par pad**
  (16 pads = 2 banques). « Note pad 1 » = note MIDI du 1er pad (défaut 36) ;
  les pads suivants sont base+1, base+2, … (vérifie avec le moniteur du mapping).
- Tape les pads → ils jouent les samples. Comme c'est une **piste**, tu peux
  **enregistrer/boucler** tes patterns (overdub, undo, etc.) comme le reste.
- Priorité instrument de la piste : **sampler** (si des pads chargés) > plugin > synthé.

### Étape 8 — sauvegarde de session
- **Enreg. session** / **Ouvrir session** (rangée des pistes) : fichier `.mdemi`.
- Sauvegarde : tempo + par piste (instrument **et son preset/réglages**, notes,
  contrôles molette/pitch, volume, mesures). À la réouverture, la session
  resonne à l'identique (les plugins sont rechargés et restaurés).
- L'audio se met brièvement en pause pendant save/load (normal).

### Étape 9 — export audio (.wav)
- **Exporter (.wav)** (rangée des pistes) : choisis un fichier, l'app **capture
  en temps réel** ce que tu joues (mix des pistes). Joue ta performance
  (déclenche/coupe les pistes, joue en live), puis reclique **Stop export**.
- Le **métronome n'est PAS** dans l'export. L'écriture disque se fait sur un
  thread séparé (aucune coupure de son).

---

## 6. Si ça coince

| Symptôme | Piste |
|---|---|
| `cmake: command not found` | CMake pas installé / pas dans le PATH (cf. §1b). |
| `JUCE est introuvable` | Lance `git submodule update --init --recursive`. |
| Erreur de compilation | **Copie-moi le message d'erreur complet**, je corrige. |
| Fenêtre OK mais pas de son | Vérifie le volume + le périphérique de sortie dans la ligne de statut. |
| L'Oxygen n'apparaît pas en MIDI | Branche-le **avant** de lancer l'app, puis relance. |

> En cas d'erreur de build, le plus utile est de me coller **tout** le texte
> affiché par `cmake --build build` (même long).
