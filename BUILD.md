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
> l'enregistrement à la main en recliquant **« ● REC »**.

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
