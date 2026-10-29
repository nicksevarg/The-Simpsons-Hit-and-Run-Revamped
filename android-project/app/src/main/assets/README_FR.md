# The Simpsons Hit & Run - Revamped! (Android)
https://github.com/nicksevarg/The-Simpsons-Hit-and-Run-Revamped

Un portage Android amélioré de The Simpsons: Hit & Run qui vise à porter le jeu original sur les appareils Android avec des graphismes améliorés, une personnalisation étendue, le support des écrans tactiles, le support des appareils Android TV, ainsi que diverses corrections de bugs et améliorations du confort de jeu.

Important : Vous devez fournir votre propre version PC du jeu et ses données de jeu requises. Pour des raisons de droits d'auteur, les données du jeu original ne sont pas incluses dans ce projet.

La vision principale de ce projet est de pousser le jeu original et ses ressources graphiques/sonores d'origine dans leurs derniers retranchements tout en restant optimisé, dans le but de moderniser ce jeu Simpsons légendaire.

Cette version est basée sur l'excellent fork de Carlox33 du portage original Switch et Vita par ZenoArrows. Un remerciement particulier à toutes les personnes impliquées pour avoir rendu ce portage possible !

## ⚡ Spécifications recommandées

**(Cel Shading ACTIVÉ)**

- Tegra X1 ou Helio G99 (1080p / 60 FPS)
- Snapdragon 8 Gen 1   (2160p / 120 FPS)

## 📦 Installation

Sélectionnez l'emplacement approprié des données de jeu lorsque vous y êtes invité, ou laissez le portage détecter les données disponibles.

Toutes les données de jeu doivent provenir de l'installation PC d'origine et se composent des éléments suivants :

- Dossier "art"
- Dossier "movies"
- Dossier "scripts"
- Dossier "sound"

Et tous les autres fichiers .rcf situés dans le dossier d'installation principal, y compris le fichier de langue dialog.rcf.

Les emplacements de données de jeu pris en charge sont les suivants :

- Android\data\com.sevarg.simpsons\files
- Stockage interne partagé\Simpsons
- Stockage externe\Simpsons

## 🌐 Langues

Prise en charge de l'anglais, du français, de l'allemand et de l'espagnol.

Si les données du jeu PC proviennent d'une version européenne, vous pouvez sélectionner la langue de votre choix et le texte en jeu sera également pris en charge. La version européenne contient les données textuelles et vidéos en jeu pour les 4 langues. (Cela inclut le Royaume-Uni et l'Irlande)

Si les données proviennent de la version nord-américaine, la langue reviendra toujours par défaut à l'anglais pour des raisons de compatibilité.

Concernant les dialogues en jeu, cela dépendra du pays et les fichiers de dialogue linguistique sont les suivants :

- Anglais  (dialog.rcf)  Royaume-Uni, Irlande, États-Unis, Canada, Pays-Bas, International
- Français (dialogf.rcf) France
- Allemand (dialogg.rcf) Allemagne
- Espagnol (dialogs.rcf) Espagne

Vous pouvez sélectionner la langue souhaitée lors de l'ouverture du jeu. Si vous ne possédez pas le fichier de langue dialog.rcf correspondant, mais que le fichier dialog.rcf est présent, les dialogues du jeu repasseront automatiquement en anglais. Si aucun fichier de dialogue n'est trouvé, le jeu ne se chargera pas afin d'éviter les plantages pendant la partie.

Copiez soigneusement tous vos fichiers depuis le dossier d'installation PC.

## 🎮 Commandes

Compatibilité avec les manettes Xbox One/Series, DualShock 4/DualSense, Switch 1/2, 8bitdo et diverses autres manettes Bluetooth compatibles.

Les actions des boutons sont répertoriées dans cet ordre : Xbox > PlayStation > Switch
Les commandes À pied / Conduite sont séparées par "/"

- Bouton Start - Menu Pause
- Bouton du haut — Interagir / Interagir avec le véhicule - △ Y X
- Bouton de gauche — Attaquer / Frein à main - □ X Y
- Bouton de droite — Basculer le sprint / Marche arrière - ○ B A
- Bouton du bas — Sauter ou Confirmer / Accélérer - ✕ A B
- Croix directionnelle - Navigation dans les menus et Déplacement/Direction
- Stick gauche - Navigation dans les menus et Déplacement/Direction
- Stick droit - Contrôle de la caméra
- Gâchette haute gauche (L1) - Changer le zoom de la caméra
- Gâchette basse gauche (L2) - Zoomer / Marche arrière (Analogique)
- Gâchette haute droite (R1) - Centrer la caméra / Klaxon
- Gâchette basse droite (R2) - Dézoomer / Accélérer (Analogique)
- Clic stick gauche (L3) - Basculer le sprint

### Autres améliorations :

- Système de caméra amélioré avec une réponse plus rapide et une plus grande liberté de vue.
- HUD et commandes sur écran tactile intégrés avec options de personnalisation de l'emplacement, de la taille et de l'opacité des boutons tactiles.
- Accès en jeu à la personnalisation de l'écran tactile depuis le menu Options.
- Réactivité améliorée et commandes plus précises lors des déplacements à pied.

## ✨ Améliorations du confort de jeu (Quality of Life)

- Options en jeu pour les paramètres graphiques et de manette.
- Compatibilité avec les appareils Android TV avec gestion appropriée de l'ordre des manettes et prise en charge du double retour arrière (Testé sur NVidia Shield TV).
- Sélection flexible de l'emplacement des données de jeu, avec prise en charge des emplacements de stockage interne, partagé et externe disponibles.
- Détection des données de jeu au premier lancement pour simplifier le démarrage lorsque les fichiers requis sont déjà disponibles.
- Menu de codes de triche (cheats) ajouté en jeu.

## 🎨 Améliorations graphiques & visuelles

Options graphiques ajoutées :

- Effet cel-shading configurable pour un style visuel plus esthétique, correspondant fidèlement au style de dessin animé original des Simpsons.
- Résolutions de rendu sélectionnables en jeu : 480p, 720p, 1080p, 1440p et 2160p.
- Filtrage anisotrope des textures configurable de 2× à 16×, améliorant la netteté des textures sous les angles éloignés et obliques.
- Netteté de l'image (Sharpening) ajustable, avec intensité configurable et option d'activation/désactivation.
- Upscaling bicubique léger pour les cinématiques 3D basse résolution du jeu.
- Effet Bloom configurable pour sublimer l'apparence des lumières et des effets spéciaux.
- Éblouissement du soleil et équilibre de l'éclairage améliorés.
- Effets d'ombres de nuages animés expérimentaux pour les environnements extérieurs en journée.
- Éclairage de contour (Rim lighting) optionnel pour les bords des personnages et des véhicules.
- Filtre CRT : Un effet optionnel de style CRT-Geom imitant l'apparence d'un téléviseur des années 1990.
- Flou de mouvement (Motion blur) : Un effet tenant compte de la caméra et axé sur le mouvement des véhicules, la séparation des feux arrière et la partie inférieure de l'écran.
- Distance d'affichage des décors augmentée, permettant à une plus grande partie du monde du jeu de rester visible avec des distances d'affichage accrues pour les piétons et les véhicules, ainsi qu'une animation plus fluide des piétons éloignés.

## 🛠️ Configuration graphique par défaut

- **Résolution**
  1080p

- **Cel Shading**
  Activé

- **Filtrage anisotrope**
  2×

- **Bloom**
  Activé

- **Ombres de nuages**
  Activé

- **Netteté (Sharpening)**
  40%

- **Filtre CRT**
  Désactivé

- **Flou de mouvement**
  Désactivé

- **Éclairage de contour**
  Désactivé

- **Limite de FPS**
  60 FPS

## 🧪 Fonctionnalités expérimentales

Certaines fonctionnalités restent expérimentales et peuvent varier selon l'appareil :
- Limites de FPS plus élevées (90, 120 et 144 FPS)
- Rendu des ombres de nuages.

## 🔬 Fonctionnalités à venir

- Affinage de la compatibilité avec d'autres langues.
- Recherche sur une compatibilité possible avec les données de jeu des versions PS2, GameCube et Xbox.
- Extraction automatique des données de jeu à partir de fichiers d'images de disque.
