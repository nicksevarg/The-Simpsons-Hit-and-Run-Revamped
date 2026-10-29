The Simpsons Hit & Run - Revamped! (Android)
https://github.com/nicksevarg/The-Simpsons-Hit-and-Run-Revamped
--------------------------------------------

Un portage Android amélioré de The Simpsons: Hit & Run qui vise à porter le jeu original sur les appareils Android avec des graphismes améliorés, une personnalisation étendue, la prise en charge des écrans tactiles, la compatibilité avec les appareils Android TV, ainsi que diverses corrections de bugs et améliorations de la qualité de vie.

Important : Vous devez fournir votre propre version PC du jeu ainsi que ses données de jeu requises. Pour des raisons de droits d'auteur, les données du jeu original ne sont pas incluses dans ce projet.

La vision principale de ce projet est de pousser le jeu original et ses ressources d'origine dans leurs derniers retranchements tout en restant optimisé, dans le but de moderniser ce jeu légendaire des Simpsons.

Cette version est basée sur l'excellent fork de Carlox33 du portage Switch et Vita original réalisé par ZenoArrows. Un remerciement particulier à toutes les personnes impliquées dans la réalisation de ce portage !


⚡ Spécifications recommandées
--------------------------------------------

(Paramètres par défaut / Cel Shading ACTIVÉ)

Tegra X1 ou Helio G99 [3/4 Go de RAM]   (1080p / 60 FPS) 
Snapdragon 8 Gen 1/2    [8 Go de RAM]    (2160p / 120 FPS)

(Toutes les améliorations DÉSACTIVÉES)

Tout SoC ancien de faible/moyenne spécification [2 Go de RAM] (720p / 60 FPS)

Les paramètres par défaut sont ajustés pour fonctionner correctement sur la plupart des smartphones modernes.

Remarque : Le flou de bougé (Motion Blur) et le filtre CRT peuvent être très gourmands en ressources GPU, ils sont donc désactivés par défaut.


📦 Installation
--------------------------------------------

Sélectionnez l'emplacement des données du jeu approprié lorsque vous y êtes invité, ou laissez le portage détecter les données disponibles.

Toutes les données du jeu doivent provenir de l'installation originale sur PC et se composent des éléments suivants :

Dossier "art"
Dossier "movies"
Dossier "scripts"
Dossier "sound"

Ainsi que tous les autres fichiers .rcf situés dans le dossier racine d'installation, y compris le fichier de langue dialog.rcf.

Les emplacements de données de jeu pris en charge sont les suivants :

Android\data\com.sevarg.simpsons\files
Stockage interne partagé\Simpsons
Stockage externe\Simpsons


🌐 Langues
--------------------------------------------

Prise en charge de l'anglais, du français, de l'allemand et de l'espagnol.

Si les données du jeu PC proviennent d'une version européenne, vous pouvez sélectionner la langue de votre choix et le texte en jeu sera également pris en charge. La version européenne contient les données de texte en jeu et les vidéos pour les 4 langues. (Cela inclut le Royaume-Uni et l'Irlande)

Si elles proviennent de la version nord-américaine, la langue reviendra toujours par défaut à l'anglais pour des raisons de compatibilité.

Concernant les dialogues en jeu, cela dépendra du pays et les fichiers de dialogue sont les suivants :

Anglais  (dialog.rcf)  Royaume-Uni, Irlande, USA, Canada, Pays-Bas, International
Français (dialogf.rcf) France
Allemand (dialogg.rcf) Allemagne
Espagnol (dialogs.rcf) Espagne

Vous pouvez sélectionner la langue souhaitée lors de l'ouverture du jeu. Si vous ne possédez pas le fichier de dialogue .rcf correspondant, mais que le fichier dialog.rcf par défaut est présent, les dialogues du jeu repasseront automatiquement en anglais. Si aucun fichier de dialogue n'est trouvé, le jeu ne se chargera pas afin d'éviter les plantages en cours de partie.

Copiez soigneusement tous vos fichiers depuis le dossier d'installation PC.


🎮 Commandes
--------------------------------------------

Compatibilité avec les manettes Xbox One/Series, DualShock 4/DualSense, Switch 1/2, 8bitdo et diverses autres manettes Bluetooth compatibles.

Les actions des boutons sont répertoriées dans cet ordre : Xbox > PlayStation > Switch
Les commandes À pied / Conduite sont séparées par "/"

Bouton Start — Menu Pause
Bouton du haut — Interagir / Interagir avec la voiture - △ Y X 
Bouton de gauche — Attaquer / Frein à main - □ X Y
Bouton de droite — Basculer le sprint / Marche arrière - ○ B A
Bouton du bas — Sauter ou Confirmer / Accélérer - ✕ A B
Croix directionnelle — Navigation dans les menus et Déplacement/Direction
Stick gauche — Navigation dans les menus et Déplacement/Direction
Stick droit — Contrôle de la caméra
Gâchette haute gauche (L1) — Changer le zoom de la caméra
Gâchette basse gauche (L2) — Zoom avant / Marche arrière (Analogique)
Gâchette haute droite (R1) — Centrer la caméra / Klaxon
Gâchette basse droite (R2) — Zoom arrière / Accélérer (Analogique)
Clic stick gauche (L3) — Basculer le sprint

Améliorations des commandes :

HUD tactile et commandes intégrés avec options de personnalisation pour l'emplacement, la taille et l'opacité des boutons tactiles.

Tout le travail de fond a été réalisé par Carlox33, n'hésitez pas à consulter son dépôt pour plus de détails :
https://github.com/Carlox33/The-Simpsons-Hit-and-Run-Android/blob/master/TOUCH_CONTROLS.md

Système de caméra amélioré avec une réponse plus rapide et une plus grande liberté d'orientation.
Réactivité améliorée et commandes plus précises lors des déplacements à pied.


✨ Améliorations de la qualité de vie
--------------------------------------------

Options intégrées en jeu pour les paramètres graphiques et de manette.

Compatibilité avec les appareils Android TV avec un ordonnancement correct des manettes et la prise en charge de la double pression sur retour (testé sur NVidia Shield TV).

Sélection flexible de l'emplacement des données du jeu, avec prise en charge des emplacements de stockage interne, partagé et externe disponibles.

Détection des données du jeu au premier lancement pour simplifier le démarrage lorsque les fichiers requis sont déjà disponibles.

Menu de codes de triche (cheats) ajouté en jeu.


🎨 Améliorations graphiques & visuelles
--------------------------------------------

Options graphiques ajoutées :

Effet de cel-shading configurable pour un style visuel plus agréable, correspondant fidèlement au style original du dessin animé Les Simpson.

Résolutions de rendu sélectionnables en jeu : 480p, 720p, 1080p, 1440p et 2160p.

Filtrage anisotrope des textures configurable de 2× à 16×, améliorant la clarté des textures sur les angles éloignés et obliques.

Netteté de l'image (sharpening) ajustable, avec intensité configurable et option d'activation/désactivation.

Upscaling bicubique léger pour les cinématiques rendues en 3D à basse résolution.

Effet de bloom configurable pour améliorer l'apparence des lumières et des effets spéciaux.

Amélioration des reflets du soleil et de l'équilibre de l'éclairage.

Effets expérimentaux d'ombres de nuages animées pour les environnements extérieurs en journée.

Éclairage de contour (rim lighting) optionnel pour les bords des personnages et des véhicules.

Filtre CRT : Un effet optionnel de style CRT-Geom qui imite l'apparence d'un téléviseur des années 1990.

Flou de bougé (Motion blur) : Un effet prenant en compte la caméra et axé sur le mouvement des véhicules, la séparation des feux arrière et la partie inférieure de l'écran.

Distance d'affichage (draw distance) accrue, permettant à une plus grande partie du monde du jeu de rester visible avec des distances de rendu des piétons et véhicules augmentées, ainsi que des animations plus fluides pour les piétons éloignés.


🛠️ Configuration graphique par défaut
--------------------------------------------

Résolution
1080p

Cel Shading
Activé

Filtrage anisotrope
2×

Bloom
Activé

Ombres des nuages
Activé

Netteté
40%

Filtre CRT
Désactivé

Flou de bougé (Motion Blur)
Désactivé

Éclairage de contour (Rim lighting)
Désactivé

Limite de FPS
60 FPS


🧪 Fonctionnalités expérimentales
--------------------------------------------

Certaines fonctionnalités restent expérimentales et peuvent varier selon l'appareil :
Limites de FPS plus élevées (90, 120 et 144 FPS)
Rendu des ombres de nuages.


🔬 Fonctionnalités à venir
--------------------------------------------

Affinage de la compatibilité avec d'autres langues.

Recherches sur une compatibilité possible avec les données de jeu des versions PS2, GameCube et Xbox.

Extraction automatique des données du jeu à partir des fichiers d'image disque.