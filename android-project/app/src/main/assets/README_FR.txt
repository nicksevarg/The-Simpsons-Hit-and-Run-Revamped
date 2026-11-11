The Simpsons Hit & Run - Revamped! (Android)
https://github.com/nicksevarg/The-Simpsons-Hit-and-Run-Revamped
--------------------------------------------

Un portage Android amélioré de The Simpsons: Hit & Run qui vise à porter le jeu original sur les appareils Android avec des graphismes améliorés, une personnalisation étendue, la prise en charge des écrans tactiles, la prise en charge des appareils Android TV, ainsi que diverses corrections de bugs et améliorations de la qualité de vie.

Important : Vous devez fournir votre propre version PC du jeu et ses données de jeu requises. Pour des raisons de droits d'auteur, les données du jeu original ne sont pas incluses dans ce projet.

La vision principale de ce projet est de pousser le jeu original et ses ressources d'origine à leurs limites tout en restant optimisé, dans le but de moderniser ce jeu légendaire des Simpsons.

Cette version est basée sur l'excellent fork de Carlox33 du portage original Switch et Vita par ZenoArrows. Un remerciement particulier à toutes les personnes impliquées pour avoir rendu ce portage possible !


⚡ Spécifications recommandées
--------------------------------------------

(Paramètres par défaut / Cel Shading "FULL")

Tegra X1 ou Helio G99 [3/4 Go RAM]   (1080p / 60 FPS) 
Snapdragon 8 Gen 1/2    [8 Go RAM]   (2160p / 120 FPS)

(Toutes les améliorations DÉSACTIVÉES)

Tout SoC plus ancien bas/moyen de gamme [2 Go RAM] (720p / 60 FPS)

Les paramètres par défaut sont ajustés pour fonctionner correctement sur la plupart des smartphones modernes.

Remarque : Le flou de mouvement (Motion Blur) et les ombres des nuages peuvent être très gourmands en ressources GPU, ils sont donc désactivés par défaut. Sur les SoC CPU plus anciens ou bas de gamme, le mode Cel Shading "LITE" est recommandé.
La prise en charge des SoC Mali GPU n'est toujours pas garantie à 100 %.


📦 Installation
--------------------------------------------

Sélectionnez l'emplacement des données du jeu approprié lorsque vous y êtes invité, ou laissez le portage détecter les données disponibles.

Toutes les données du jeu doivent provenir de l'installation originale sur PC et se composer des éléments suivants :

Dossier "art"
Dossier "movies"
Dossier "scripts"
Dossier "sound"

Et tous les autres fichiers .rcf situés dans le dossier d'installation parent, y compris le fichier de langue dialog.rcf.

Les emplacements de données de jeu pris en charge sont les suivants :

Android\data\com.sevarg.simpsons\files
Shared Internal\Simpsons
External\Simpsons


🌐 Langues
--------------------------------------------

Prise en charge de l'anglais, du français, de l'allemand et de l'espagnol.

Si les données du jeu PC proviennent d'une version européenne, vous pouvez sélectionner la langue de votre choix et le texte du jeu sera également pris en charge. La version européenne contient des données de texte et des vidéos intégrées pour les 4 langues. (Cela inclut le Royaume-Uni et l'Irlande)

Si les données proviennent de la version nord-américaine, pour des raisons de compatibilité, la langue reviendra toujours à l'anglais par défaut.

En ce qui concerne les dialogues en jeu, cela dépendra du pays et les fichiers de dialogue de langue sont les suivants :

Anglais  (dialog.rcf)  Royaume-Uni, Irlande, États-Unis, Canada, Pays-Bas, International
Français (dialogf.rcf) France
Allemand (dialogg.rcf) Allemagne
Espagnol (dialogs.rcf) Espagne

Vous pouvez sélectionner la langue souhaitée lors de l'ouverture du jeu. Si vous ne disposez pas du fichier de dialogue dialog.rcf correspondant et que dialog.rcf est également présent, les dialogues du jeu reviendront automatiquement à l'anglais. Si aucun fichier de dialogue n'est trouvé, le jeu ne se chargera pas afin d'éviter les plantages en cours de jeu.

Copiez soigneusement tous vos fichiers depuis le dossier d'installation PC.


🎮 Contrôles
--------------------------------------------

Compatibilité avec Xbox One/Series, DualShock 4/DualSense, Switch 1/2, 8bitdo et diverses autres manettes Bluetooth compatibles.

Les actions des boutons sont répertoriées dans cet ordre : Xbox > PlayStation > Switch
Les contrôles à pied / en conduite sont séparés par "/"

Bouton Start - Menu Pause
Bouton du haut — Interagir / Interagir avec la voiture - △ Y X 
Bouton de gauche — Attaque / Frein à main - □ X Y
Bouton de droite — Basculer le sprint / Marche arrière - ○ B A
Bouton du bas — Sauter ou Confirmer / Accélérer - ✕ A B
Croix directionnelle - Navigation dans les menus et Déplacement/Direction
Stick gauche - Navigation dans les menus et Déplacement/Direction
Stick droit - Contrôle de la caméra
Gâchette haute gauche (L1) - Changer le zoom de la caméra 
Gâchette basse gauche (L2) - Zoom avant / Marche arrière (Analogique)
Gâchette haute droite (R1) - Centrer la caméra / Klaxon
Gâchette basse droite (R2) - Zoom arrière / Accélérer (Analogique)
Clic sur le stick gauche (L3) - Basculer le sprint

Améliorations des contrôles :

HUD et contrôles tactiles intégrés avec options de personnalisation pour le placement, la taille et l'opacité des boutons tactiles.

Tout le travail difficile a été réalisé par Carlox33, veuillez donc consulter leur dépôt pour plus de détails : https://github.com/Carlox33/The-Simpsons-Hit-and-Run-Android/blob/master/TOUCH_CONTROLS.md

Système de caméra amélioré avec une réponse plus rapide et une plus grande liberté de mouvement.
Réactivité améliorée et contrôles plus précis à pied.


✨ Améliorations de la qualité de vie
--------------------------------------------

Options intégrées pour les graphismes et les paramètres de manette.

Compatibilité avec les appareils Android TV avec un ordre de manette correct et prise en charge du double retour (Testé sur NVidia Shield TV)

Sélection flexible de l'emplacement des données de jeu, avec prise en charge des emplacements de stockage interne, partagé et externe disponibles.

Détection des données du jeu au premier lancement pour simplifier le démarrage lorsque les fichiers requis sont déjà disponibles.

Menu de triche (cheats) intégré ajouté.


🎨 Améliorations graphiques et visuelles
--------------------------------------------

Options graphiques ajoutées :

Effet de cel-shading léger basé sur les objets pour un style visuel plus agréable, correspondant étroitement au style de dessin animé original des Simpsons.

Résolutions de rendu sélectionnables en jeu : 480p, 720p, 1080p, 1440p et 2160p, avec mise à l'échelle dynamique de la résolution en option.

Filtrage anisotrope des textures configurable de 4× à 16×, améliorant la clarté des textures à distance et sous des angles obliques.

Netteté de l'image ajustable, avec intensité configurable et option d'activation/désactivation.

Mise à l'échelle bicubique légère pour les cinématiques rendues en 3D à basse résolution du jeu.

Bloom configurable pour améliorer l'apparence des lumières et des effets spéciaux.

Amélioration des reflets du soleil et de l'équilibre de l'éclairage.

Effets d'ombres de nuages animés expérimentaux pour les environnements extérieurs de jour.

Éclairage de contour (Rim lighting) optionnel pour les bords des personnages et des véhicules.

Filtre CRT : Un effet optionnel de style CRT-Geom qui imite l'apparence d'un téléviseur des années 1990.

Flou de mouvement (Motion blur) : Un effet adapté à la caméra axé sur le mouvement du véhicule, la séparation de la traînée et la partie inférieure de l'écran.

Distance d'affichage des décors accrue, permettant de voir une plus grande partie du monde du jeu avec des distances d'affichage accrues pour les piétons et les véhicules, et une animation plus fluide des piétons éloignés.


🛠️ Configuration graphique par défaut
--------------------------------------------

Résolution
1080p

Cel Shading
Activé [Full]

Filtrage anisotrope
4×

Bloom
Activé

Ombres des nuages
Désactivé

Netteté
Désactivé

Filtre CRT
Désactivé

Flou de mouvement
Désactivé

Éclairage de contour
Désactivé

Limite de FPS
60 FPS

Mise à l'échelle dynamique de la résolution
Désactivé


🧪 Fonctionnalités expérimentales
--------------------------------------------

Certaines fonctionnalités restent expérimentales et peuvent varier selon l'appareil :
Limites de FPS plus élevées (90, 120 et 144 FPS)
Rendu des ombres de nuages.


🔬 Fonctionnalités à venir
--------------------------------------------

Affinage de la compatibilité pour d'autres langues.

Recherche d'une compatibilité possible avec les données de jeu des versions PS2, GameCube et Xbox.

Extraction automatique des données du jeu à partir des fichiers d'image disque.
