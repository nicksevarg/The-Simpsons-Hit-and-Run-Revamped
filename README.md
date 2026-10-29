# The Simpsons Hit & Run - Revamped! (Android)

[🇬🇧 English](#-english) | [🇫🇷 Français](#-français) | [🇩🇪 Deutsch](#-deutsch) | [🇪🇸 Español](#-español)


---

# 🇬🇧 English

## The Simpsons Hit & Run - Revamped! (Android)

An enhanced Android port of The Simpsons: Hit & Run that focuses on bringing the original game to Android devices with improved graphics, expanded customization, touch screen support, support for Android TV devices, as well as various bug fixes and quality of life improvements.

**Important:** You must provide your own PC version of the game and its required game data. Due to copyright reasons, the original game data is not included with this project.

The main vision this project is to push the original game and its original game assets to the limit while remaining optimized with a goal to modernize this legendary Simpsons game.

This version is based on the excellent Carlox33's fork of ZenoArrows's original Switch and Vita port of the game. A special thanks to everyone involved in making this port possible!

## ⚡ Recommended Specs

*(Stock Settings / Cel Shading ON)*

- Tegra X1 or Helio G99 [3/4GB RAM]   (1080p / 60FPS) 
- Snapdragon 8 Gen 1/2    [8GB RAM]   (2160p / 120FPS)

*(All Enhancements OFF)*

- Any older low/mid spec SOC [2GB RAM] (720p / 60FPS)

Stock settings are fine tuned to work nicely on most modern smartphones.

Please note: Motion Blur and CRT Filter can be quite GPU intensive, so they are toggled off by default.

## 📦 Installation

Select the appropriate game-data location when prompted, or allow the port to detect available data.

All game data must be sourced from the original PC install and consists of the following:

- `art` folder
- `movies` folder
- `scripts` folder
- `sound` folder
- All other `.rcf` files located in the parent install folder, including the language dialog `.rcf` file

Supported game-data locations:

- `Android\data\com.sevarg.simpsons\files`
- `Shared Internal\Simpsons`
- `External\Simpsons`

## 🌐 Languages

Support for English, Français, Deutsch and Español.

If PC game data is sourced from any European release, you can select any language you like and in-game text will also be supported. The European release has in-game text data and videos for all 4 languages. (This includes UK and Ireland)

If sourced from the North American release, for compatibility reasons the language will always default back to English.

As for in-game dialog, it will depend on the country and the language dialog files are as follows:

- English (`dialog.rcf`): UK, Ireland, USA, Canada, Netherlands, International
- Français (`dialogf.rcf`): France
- Deutsch (`dialogg.rcf`): Germany
- Español (`dialogs.rcf`): Spain

You can select your desired language when opening the game. If you do not have the correct respective dialog language file, and `dialog.rcf` is also present, then the game dialog will automatically default back to English. If no dialog file is found, the game will fail to load; this is to mitigate gameplay crashes.

Carefully copy all your files from the PC install folder.

## 🎮 Controls

Compatibility with Xbox One/Series, DualShock 4/DualSense, Switch 1/2, 8bitdo and various other compatible Bluetooth controllers.

Button actions are listed in this order: Xbox > PlayStation > Switch. On-foot / Driving controls are separated with `/`.

- **Start button** — Pause Menu
- **Top face button** — Interact / Interact with car — △ Y X
- **Left face button** — Attack / Handbrake — □ X Y
- **Right face button** — Toggle sprint / Reverse — ○ B A
- **Bottom face button** — Jump or Confirm / Accelerate — ✕ A B
- **D-pad** — Menu navigation and Movement/Steering
- **Left stick** — Menu navigation and Movement/Steering
- **Right stick** — Camera control
- **Left shoulder (L1)** — Switch camera zoom
- **Left trigger (L2)** — Zoom in / Reverse (Analog)
- **Right shoulder (R1)** — Centre camera / Car horn
- **Right trigger (R2)** — Zoom out / Accelerate (Analog)
- **Left stick click (L3)** — Toggle sprint

**Control Improvements:**

- Integrated touchscreen HUD and controls with customization options for touch-button placement, size and opacity.
- All the hard work was done by Carlox33, so please check out their repository for more details: https://github.com/Carlox33/The-Simpsons-Hit-and-Run-Android/blob/master/TOUCH_CONTROLS.md
- Improved camera system with faster response and more camera freedom.
- Improved responsiveness and tighter controls when on foot.

## ✨ Quality of Life Improvements

- In-game toggles for graphics and controller settings.
- Compatibility for Android TV devices with proper controller ordering and double-back support (tested on NVIDIA Shield TV).
- Flexible game-data location selection, with support for available internal, shared and external storage locations.
- First-launch game-data detection to simplify startup when the required files are already available.
- In-game cheats menu added.

## 🎨 Graphics & Visual Improvements

**Graphics options added:**

- Configurable cel-shading effect for a more aesthetically pleasing visual style, closely matching the original cartoon style of The Simpsons.
- In-game selectable rendering resolutions: 480p, 720p, 1080p, 1440p and 2160p.
- Configurable anisotropic texture filtering from 2× to 16×, improving texture clarity at distant and oblique angles.
- Adjustable image sharpening, with configurable strength and an enable/disable option.
- Lightweight bicubic upscaling for the game's low-resolution, 3D-rendered cutscenes.
- Configurable bloom to enhance the appearance of lights and special effects.
- Improved sunlight glare and lighting balance.
- Experimental animated cloud-shadow effects for outdoor daytime environments.
- Optional rim lighting for character and vehicle edges.
- **CRT filter:** An optional CRT-Geom-style effect that imitates the appearance of a 1990s television.
- **Motion blur:** A camera-aware effect focused on vehicle motion, tail separation and the lower portion of the screen.
- Increased scenery draw distance, allowing more of the game world to remain visible, with increased pedestrian and vehicle rendering distances and smoother animation of distant pedestrians.

## 🛠️ Default Graphics Configuration

| Setting | Default |
| --- | --- |
| Resolution | 1080p |
| Cel Shading | On |
| Anisotropic Filtering | 2× |
| Bloom | On |
| Cloud Shadows | On |
| Sharpening | 40% |
| CRT Filter | Off |
| Motion Blur | Off |
| Rim Lighting | Off |
| FPS Cap | 60 FPS |

## 🧪 Experimental Features

Some features remain experimental and may vary by device:

- Higher FPS caps (90, 120 and 144 FPS)
- Cloud-shadow rendering

## 🔬 Upcoming Features

- Refining compatibility for other languages.
- Researching possible compatibility with game data from PS2, GameCube and Xbox releases.
- Automatic extraction of game data from disc-image files.

---

# 🇫🇷 Français

## The Simpsons Hit & Run - Revamped! (Android)

Un portage Android amélioré de The Simpsons: Hit & Run qui vise à porter le jeu original sur les appareils Android avec des graphismes améliorés, une personnalisation étendue, le support des écrans tactiles, le support des appareils Android TV, ainsi que diverses corrections de bugs et améliorations du confort de jeu.

**Important :** Vous devez fournir votre propre version PC du jeu et ses données de jeu requises. Pour des raisons de droits d'auteur, les données du jeu original ne sont pas incluses dans ce projet.

La vision principale de ce projet est de pousser le jeu original et ses ressources graphiques/sonores d'origine dans leurs derniers retranchements tout en restant optimisé, dans le but de moderniser ce jeu Simpsons légendaire.

Cette version est basée sur l'excellent fork de Carlox33 du portage original Switch et Vita par ZenoArrows. Un remerciement particulier à toutes les personnes impliquées pour avoir rendu ce portage possible !

## ⚡ Spécifications recommandées

*(Paramètres par défaut / Cel Shading ACTIVÉ)*

- Tegra X1 ou Helio G99 [3/4 Go de RAM]   (1080p / 60 FPS)
- Snapdragon 8 Gen 1/2    [8 Go de RAM]    (2160p / 120 FPS)

*(Toutes les améliorations DÉSACTIVÉES)*

- Tout SoC ancien de faible/moyenne spécification [2 Go de RAM] (720p / 60 FPS)

Les paramètres par défaut sont ajustés pour fonctionner correctement sur la plupart des smartphones modernes.

Remarque : Le flou de bougé (Motion Blur) et le filtre CRT peuvent être très gourmands en ressources GPU, ils sont donc désactivés par défaut.

## 📦 Installation

Sélectionnez l'emplacement approprié des données de jeu lorsque vous y êtes invité, ou laissez le portage détecter les données disponibles.

Toutes les données de jeu doivent provenir de l'installation PC d'origine et se composent des éléments suivants :

- Dossier `art`
- Dossier `movies`
- Dossier `scripts`
- Dossier `sound`
- Tous les autres fichiers `.rcf` situés dans le dossier d'installation principal, y compris le fichier de langue dialog `.rcf`

Les emplacements de données de jeu pris en charge sont les suivants :

- `Android\data\com.sevarg.simpsons\files`
- `Stockage interne partagé\Simpsons`
- `Stockage externe\Simpsons`

## 🌐 Langues

Prise en charge de l'anglais, du français, de l'allemand et de l'espagnol.

Si les données du jeu PC proviennent d'une version européenne, vous pouvez sélectionner la langue de votre choix et le texte en jeu sera également pris en charge. La version européenne contient les données textuelles et vidéos en jeu pour les 4 langues. (Cela inclut le Royaume-Uni et l'Irlande)

Si les données proviennent de la version nord-américaine, la langue reviendra toujours par défaut à l'anglais pour des raisons de compatibilité.

Concernant les dialogues en jeu, cela dépendra du pays et les fichiers de dialogue linguistique sont les suivants :

- Anglais (`dialog.rcf`) : Royaume-Uni, Irlande, États-Unis, Canada, Pays-Bas, International
- Français (`dialogf.rcf`) : France
- Allemand (`dialogg.rcf`) : Allemagne
- Espagnol (`dialogs.rcf`) : Espagne

Vous pouvez sélectionner la langue souhaitée lors de l'ouverture du jeu. Si vous ne possédez pas le fichier de langue dialog correspondant, mais que le fichier `dialog.rcf` est présent, les dialogues du jeu repasseront automatiquement en anglais. Si aucun fichier de dialogue n'est trouvé, le jeu ne se chargera pas afin d'éviter les plantages pendant la partie.

Copiez soigneusement tous vos fichiers depuis le dossier d'installation PC.

## 🎮 Commandes

Compatibilité avec les manettes Xbox One/Series, DualShock 4/DualSense, Switch 1/2, 8bitdo et diverses autres manettes Bluetooth compatibles.

Les actions des boutons sont répertoriées dans cet ordre : Xbox > PlayStation > Switch. Les commandes À pied / Conduite sont séparées par `/`.

- **Bouton Start** — Menu Pause
- **Bouton du haut** — Interagir / Interagir avec le véhicule — △ Y X
- **Bouton de gauche** — Attaquer / Frein à main — □ X Y
- **Bouton de droite** — Basculer le sprint / Marche arrière — ○ B A
- **Bouton du bas** — Sauter ou Confirmer / Accélérer — ✕ A B
- **Croix directionnelle** — Navigation dans les menus et Déplacement/Direction
- **Stick gauche** — Navigation dans les menus et Déplacement/Direction
- **Stick droit** — Contrôle de la caméra
- **Gâchette haute gauche (L1)** — Changer le zoom de la caméra
- **Gâchette basse gauche (L2)** — Zoomer / Marche arrière (Analogique)
- **Gâchette haute droite (R1)** — Centrer la caméra / Klaxon
- **Gâchette basse droite (R2)** — Dézoomer / Accélérer (Analogique)
- **Clic stick gauche (L3)** — Basculer le sprint

**Autres améliorations :**

- HUD tactile et commandes intégrés avec options de personnalisation pour l'emplacement, la taille et l'opacité des boutons tactiles.
- Tout le travail de fond a été réalisé par Carlox33, n'hésitez pas à consulter son dépôt pour plus de détails: https://github.com/Carlox33/The-Simpsons-Hit-and-Run-Android/blob/master/TOUCH_CONTROLS.md
- Système de caméra amélioré avec une réponse plus rapide et une plus grande liberté d'orientation.
- Réactivité améliorée et commandes plus précises lors des déplacements à pied.

## ✨ Améliorations du confort de jeu (Quality of Life)

- Options en jeu pour les paramètres graphiques et de manette.
- Compatibilité avec les appareils Android TV avec gestion appropriée de l'ordre des manettes et prise en charge du double retour arrière (testé sur NVIDIA Shield TV).
- Sélection flexible de l'emplacement des données de jeu, avec prise en charge des emplacements de stockage interne, partagé et externe disponibles.
- Détection des données de jeu au premier lancement pour simplifier le démarrage lorsque les fichiers requis sont déjà disponibles.
- Menu de codes de triche (cheats) ajouté en jeu.

## 🎨 Améliorations graphiques & visuelles

**Options graphiques ajoutées :**

- Effet cel-shading configurable pour un style visuel plus esthétique, correspondant fidèlement au style de dessin animé original des Simpsons.
- Résolutions de rendu sélectionnables en jeu : 480p, 720p, 1080p, 1440p et 2160p.
- Filtrage anisotrope des textures configurable de 2× à 16×, améliorant la netteté des textures sous les angles éloignés et obliques.
- Netteté de l'image (Sharpening) ajustable, avec intensité configurable et option d'activation/désactivation.
- Upscaling bicubique léger pour les cinématiques 3D basse résolution du jeu.
- Effet Bloom configurable pour sublimer l'apparence des lumières et des effets spéciaux.
- Éblouissement du soleil et équilibre de l'éclairage améliorés.
- Effets d'ombres de nuages animés expérimentaux pour les environnements extérieurs en journée.
- Éclairage de contour (Rim lighting) optionnel pour les bords des personnages et des véhicules.
- **Filtre CRT :** Un effet optionnel de style CRT-Geom imitant l'apparence d'un téléviseur des années 1990.
- **Flou de mouvement (Motion blur) :** Un effet tenant compte de la caméra et axé sur le mouvement des véhicules, la séparation des feux arrière et la partie inférieure de l'écran.
- Distance d'affichage des décors augmentée, permettant à une plus grande partie du monde du jeu de rester visible avec des distances d'affichage accrues pour les piétons et les véhicules, ainsi qu'une animation plus fluide des piétons éloignés.

## 🛠️ Configuration graphique par défaut

| Paramètre | Valeur par défaut |
| --- | --- |
| Résolution | 1080p |
| Cel Shading | Activé |
| Filtrage anisotrope | 2× |
| Bloom | Activé |
| Ombres de nuages | Activé |
| Netteté (Sharpening) | 40 % |
| Filtre CRT | Désactivé |
| Flou de mouvement | Désactivé |
| Éclairage de contour | Désactivé |
| Limite de FPS | 60 FPS |

## 🧪 Fonctionnalités expérimentales

Certaines fonctionnalités restent expérimentales et peuvent varier selon l'appareil :

- Limites de FPS plus élevées (90, 120 et 144 FPS)
- Rendu des ombres de nuages

## 🔬 Fonctionnalités à venir

- Affinage de la compatibilité avec d'autres langues.
- Recherche sur une compatibilité possible avec les données de jeu des versions PS2, GameCube et Xbox.
- Extraction automatique des données de jeu à partir de fichiers d'images de disque.

---

# 🇩🇪 Deutsch

## The Simpsons Hit & Run - Revamped! (Android)

Ein verbesserter Android-Port von The Simpsons: Hit & Run, der darauf abzielt, das Originalspiel mit verbesserter Grafik, erweiterter Anpassung, Touchscreen-Unterstützung, Unterstützung für Android-TV-Geräte sowie verschiedenen Fehlerbehebungen und Qualitätsverbesserungen (Quality of Life) auf Android-Geräte zu bringen.

**Wichtig:** Sie müssen Ihre eigene PC-Version des Spiels und die erforderlichen Spieldaten bereitstellen. Aus Urheberrechtsgründen sind die ursprünglichen Spieldaten nicht in diesem Projekt enthalten.

Die Hauptvision dieses Projekts besteht darin, das Originalspiel und seine ursprünglichen Spieldateien an die Grenzen zu bringen und gleichzeitig optimiert zu bleiben, um diesen legendären Simpsons-Klassiker zu modernisieren.

Diese Version basiert auf dem hervorragenden Fork von Carlox33 des ursprünglichen Switch- und Vita-Ports von ZenoArrows. Ein besonderer Dank geht an alle Beteiligten, die diesen Port möglich gemacht haben!

## ⚡ Empfohlene Spezifikationen

*(Standardeinstellungen / Cel Shading EIN)*

- Tegra X1 oder Helio G99 [3/4 GB RAM]   (1080p / 60 FPS)
- Snapdragon 8 Gen 1/2    [8 GB RAM]     (2160p / 120 FPS)

*(Alle Verbesserungen AUS)*

- Beliebiger älterer Low/Mid-Spec SOC [2 GB RAM] (720p / 60 FPS)

Die Standardeinstellungen sind so angepasst, dass sie auf den meisten modernen Smartphones gut funktionieren.

Bitte beachten: Bewegungsunschärfe (Motion Blur) und CRT-Filter können sehr GPU-intensiv sein und sind daher standardmäßig deaktiviert.

## 📦 Installation

Wählen Sie bei Aufforderung den entsprechenden Speicherort für die Spieldaten oder lassen Sie den Port die verfügbaren Daten automatisch erkennen.

Alle Spieldaten müssen aus der ursprünglichen PC-Installation stammen und bestehen aus Folgendem:

- Ordner `art`
- Ordner `movies`
- Ordner `scripts`
- Ordner `sound`
- Alle anderen `.rcf`-Dateien im Haupt-Installationsordner, einschließlich der Sprachdatei dialog `.rcf`

Unterstützte Spieldaten-Speicherorte sind wie folgt:

- `Android\data\com.sevarg.simpsons\files`
- `Gemeinsamer interner Speicher\Simpsons`
- `Externer Speicher\Simpsons`

## 🌐 Sprachen

Unterstützung für Englisch, Français, Deutsch und Español.

Wenn die PC-Spieldaten aus einer europäischen Version stammen, können Sie jede beliebige Sprache auswählen, und die Texte im Spiel werden ebenfalls unterstützt. Die europäische Version enthält Textdaten und Videos im Spiel für alle 4 Sprachen. (Dies umfasst auch Großbritannien und Irland)

Wenn die Daten aus der nordamerikanischen Version stammen, wird die Sprache aus Kompatibilitätsgründen immer auf Englisch zurückgesetzt.

Was die Sprachausgabe im Spiel betrifft, hängt sie vom Land ab und die Sprach-Dialogdateien lauten wie folgt:

- Englisch (`dialog.rcf`): Großbritannien, Irland, USA, Kanada, Niederlande, International
- Französisch (`dialogf.rcf`): Frankreich
- Deutsch (`dialogg.rcf`): Deutschland
- Spanisch (`dialogs.rcf`): Spanien

Sie können Ihre gewünschte Sprache beim Starten des Spiels auswählen. Wenn Sie nicht über die entsprechende Sprach-Dialogdatei verfügen, aber `dialog.rcf` vorhanden ist, wird die Sprachausgabe im Spiel automatisch auf Englisch zurückgesetzt. Wenn keine Dialogdatei gefunden wird, kann das Spiel nicht geladen werden; dies dient dazu, Abstürze während des Spiels zu vermeiden.

Kopieren Sie sorgfältig alle Ihre Dateien aus dem PC-Installationsordner.

## 🎮 Steuerung

Kompatibilität mit Xbox One/Series, DualShock 4/DualSense, Switch 1/2, 8bitdo und verschiedenen anderen kompatiblen Bluetooth-Controllern.

Tastenaktionen sind in dieser Reihenfolge aufgeführt: Xbox > PlayStation > Switch. Die Steuerungen Zu Fuß / Fahren sind durch `/` getrennt.

- **Start-Taste** — Pause-Menü
- **Obere Aktionstaste** — Interagieren / Mit Auto interagieren — △ Y X
- **Linke Aktionstaste** — Angriff / Handbremse — □ X Y
- **Rechte Aktionstaste** — Sprinten umschalten / Rückwärtsfahren — ○ B A
- **Untere Aktionstaste** — Springen oder Bestätigen / Beschleunigen — ✕ A B
- **Steuerkreuz** — Menünavigation und Bewegung/Lenken
- **Linker Stick** — Menünavigation und Bewegung/Lenken
- **Rechter Stick** — Kamerasteuerung
- **Linker Schulterbutton (L1)** — Kamera-Zoom umschalten
- **Linker Trigger (L2)** — Heranzoomen / Rückwärtsfahren (Analog)
- **Rechter Schulterbutton (R1)** — Kamera zentrieren / Hupe
- **Rechter Trigger (R2)** — Herauszoomen / Beschleunigen (Analog)
- **Linker Stick Klick (L3)** — Sprinten umschalten

**Weitere Verbesserungen:**

- Integriertes Touchscreen-HUD und Steuerung mit Anpassungsoptionen für Position, Größe und Deckkraft der Touch-Tasten.
- Die gesamte Vorarbeit wurde von Carlox33 geleistet. Bitte besuchen Sie das Repository für weitere Details: https://github.com/Carlox33/The-Simpsons-Hit-and-Run-Android/blob/master/TOUCH_CONTROLS.md
- Verbessertes Kamerasystem mit schnellerer Reaktion und mehr Kamerafreiheit.
- Verbesserte Reaktionsfähigkeit und präzisere Steuerung zu Fuß.

## ✨ Qualitätsverbesserungen (Quality of Life)

- In-Game-Schalter für Grafik- und Controller-Einstellungen.
- Kompatibilität für Android-TV-Geräte mit ordnungsgemäßer Controller-Sortierung und Unterstützung für doppeltes Zurückdrücken (getestet auf NVIDIA Shield TV).
- Flexible Auswahl des Spieldaten-Speicherorts mit Unterstützung für verfügbare interne, gemeinsame und externe Speicherorte.
- Spieldaten-Erkennung beim ersten Start zur Vereinfachung der Inbetriebnahme, wenn die erforderlichen Dateien bereits vorhanden sind.
- In-Game-Cheat-Menü hinzugefügt.

## 🎨 Grafik- & visuelle Verbesserungen

**Hinzugefügte Grafikoptionen:**

- Konfigurierbarer Cel-Shading-Effekt für einen ästhetisch ansprechenderen visuellen Stil, der dem ursprünglichen Cartoon-Stil der Simpsons sehr nahe kommt.
- Im Spiel auswählbare Rendering-Auflösungen: 480p, 720p, 1080p, 1440p und 2160p.
- Konfigurierbare anisotrope Texturfilterung von 2× bis 16× zur Verbesserung der Texturschärfe bei entfernten und schrägen Blickwinkeln.
- Einstellbare Bildschärfung mit konfigurierbarer Stärke und Option zum Aktivieren/Deaktivieren.
- Leichtes bikubisches Upscaling für die niedrig aufgelösten 3D-Renderszenen des Spiels.
- Konfigurierbarer Bloom-Effekt zur Verbesserung der Darstellung von Lichtern und Spezialeffekten.
- Verbesserte Sonnenlicht-Blendeffekte und ausgewogenere Beleuchtung.
- Experimentelle animierte Wolkenschatten-Effekte für Außenbereiche bei Tageslicht.
- Optionale Randbeleuchtung (Rim lighting) für Charakter- und Fahrzeugkanten.
- **CRT-Filter:** Ein optionaler Effekt im CRT-Geom-Stil, der das Erscheinungsbild eines Fernsehers aus den 1990er Jahren imitiert.
- **Bewegungsunschärfe (Motion blur):** Ein kamerabezogener Effekt, der auf Fahrzeugbewegung, Rücklicht-Spuren und den unteren Bildschirmbereich fokussiert ist.
- Erhöhte Sichtweite der Umgebung, sodass mehr von der Spielwelt sichtbar bleibt, zusammen mit erhöhten Darstellungsdistanzen für Fußgänger und Fahrzeuge sowie flüssigeren Animationen entfernter Fußgänger.

## 🛠️ Standard-Grafikkonfiguration

| Einstellung | Standardwert |
| --- | --- |
| Auflösung | 1080p |
| Cel Shading | An |
| Anisotrope Filterung | 2× |
| Bloom | An |
| Wolkenschatten | An |
| Bildschärfung | 40 % |
| CRT-Filter | Aus |
| Bewegungsunschärfe | Aus |
| Randbeleuchtung (Rim lighting) | Aus |
| FPS-Begrenzung | 60 FPS |

## 🧪 Experimentelle Funktionen

Einige Funktionen bleiben experimentell und können je nach Gerät variieren:

- Höhere FPS-Begrenzungen (90, 120 und 144 FPS)
- Wolkenschatten-Rendering

## 🔬 Zukünftige Funktionen

- Verfeinerung der Kompatibilität für weitere Sprachen.
- Erforschung einer möglichen Kompatibilität mit Spieldaten der PS2-, GameCube- und Xbox-Releases.
- Automatische Extraktion von Spieldaten aus Disc-Image-Dateien.

---

# 🇪🇸 Español

## The Simpsons Hit & Run - Revamped! (Android)

Un port mejorado para Android de The Simpsons: Hit & Run que se enfoca en llevar el juego original a dispositivos Android con gráficos mejorados, personalización ampliada, soporte para pantalla táctil, soporte para dispositivos Android TV, así como varias correcciones de errores y mejoras de calidad de vida.

**Importante:** Debes proporcionar tu propia versión para PC del juego y sus datos de juego requeridos. Por razones de derechos de autor, los datos del juego original no se incluyen en este proyecto.

La visión principal de este proyecto es llevar al límite el juego original y sus archivos de recursos originales mientras se mantiene optimizado con el objetivo de modernizar este legendario juego de los Simpsons.

Esta versión se basa en el excelente fork de Carlox33 del port original para Switch y Vita realizado por ZenoArrows. ¡Un agradecimiento especial a todos los involucrados en hacer posible este port!

## ⚡ Especificaciones recomendadas

*(Ajustes por defecto / Cel Shading ACTIVADO)*

- Tegra X1 o Helio G99 [3/4GB RAM]   (1080p / 60FPS)
- Snapdragon 8 Gen 1/2    [8GB RAM]   (2160p / 120FPS)

*(Todas las mejoras DESACTIVADAS)*

- Cualquier SoC antiguo de gama baja/media [2 GB de RAM] (720p / 60 FPS)

Los ajustes por defecto están optimizados para funcionar bien en la mayoría de los smartphones modernos.

Nota: El desenfoque de movimiento (Motion Blur) y el filtro CRT pueden requerir bastante uso de la GPU, por lo que están desactivados por defecto.

## 📦 Instalación

Selecciona la ubicación adecuada de los datos del juego cuando se te solicite, o permite que el port detecte los datos disponibles.

Todos los datos del juego deben proceder de la instalación original de PC y constan de lo siguiente:

- Carpeta `art`
- Carpeta `movies`
- Carpeta `scripts`
- Carpeta `sound`
- Todos los demás archivos `.rcf` que se encuentran en la carpeta raíz de instalación, incluido el archivo de idioma dialog `.rcf`

Las ubicaciones de datos de juego compatibles son las siguientes:

- `Android\data\com.sevarg.simpsons\files`
- `Almacenamiento interno compartido\Simpsons`
- `Almacenamiento externo\Simpsons`

## 🌐 Idiomas

Soporte para inglés, francés, alemán y español.

Si los datos del juego para PC provienen de cualquier versión europea, puedes seleccionar cualquier idioma que desees y el texto del juego también será compatible. La versión europea incluye datos de texto e imágenes de vídeo en el juego para los 4 idiomas. (Esto incluye Reino Unido e Irlanda)

Si provienen de la versión norteamericana, por razones de compatibilidad el idioma siempre se restablecerá de forma predeterminada a inglés.

En cuanto a los diálogos del juego, dependerán del país y los archivos de diálogo por idioma son los siguientes:

- Inglés (`dialog.rcf`): Reino Unido, Irlanda, EE. UU., Canadá, Países Bajos, Internacional
- Francés (`dialogf.rcf`): Francia
- Alemán (`dialogg.rcf`): Alemania
- Español (`dialogs.rcf`): España

Puedes seleccionar el idioma deseado al abrir el juego. Si no tienes el archivo de idioma dialog correspondiente y el archivo `dialog.rcf` predeterminado está presente, los diálogos del juego cambiarán automáticamente a inglés. Si no se encuentra ningún archivo de diálogo, el juego no se cargará; esto es para mitigar bloqueos en la partida.

Copia cuidadosamente todos tus archivos desde la carpeta de instalación de PC.

## 🎮 Controles

Compatibilidad con mandos Xbox One/Series, DualShock 4/DualSense, Switch 1/2, 8bitdo y varios otros mandos Bluetooth compatibles.

Las acciones de los botones se enumeran en este orden: Xbox > PlayStation > Switch. Los controles A pie / Conducción están separados por `/`.

- **Botón Start** — Menú de pausa
- **Botón frontal superior** — Interactuar / Interactuar con el coche — △ Y X
- **Botón frontal izquierdo** — Ataque / Freno de mano — □ X Y
- **Botón frontal derecho** — Alternar esprint / Marcha atrás — ○ B A
- **Botón frontal inferior** — Saltar o Confirmar / Acelerar — ✕ A B
- **Cruceta (D-pad)** — Navegación por el menú y Movimiento/Dirección
- **Stick izquierdo** — Navegación por el menú y Movimiento/Dirección
- **Stick derecho** — Control de la cámara
- **Gatillo superior izquierdo (L1)** — Cambiar zoom de cámara
- **Gatillo inferior izquierdo (L2)** — Acercar zoom / Marcha atrás (Analógico)
- **Gatillo superior derecho (R1)** — Centrar cámara / Claxon del coche
- **Gatillo inferior derecho (R2)** — Alejar zoom / Acelerar (Analógico)
- **Pulsación stick izquierdo (L3)** — Alternar esprint

**Otras mejoras:**

- HUD táctil integrado y controles en pantalla con opciones de personalización de posición, tamaño y opacidad de los botones táctiles.
- Todo el trabajo duro fue realizado por Carlox33, así que visita su repositorio para obtener más detalles: https://github.com/Carlox33/The-Simpsons-Hit-and-Run-Android/blob/master/TOUCH_CONTROLS.md
- Sistema de cámara mejorado con respuesta más rápida y mayor libertad de movimiento.
- Capacidad de respuesta mejorada y controles más precisos al ir a pie.

## ✨ Mejoras de calidad de vida (Quality of Life)

- Interruptores en el juego para la configuración gráfica y del mando.
- Compatibilidad con dispositivos Android TV con un orden correcto de mandos y soporte para doble pulsación atrás (probado en NVIDIA Shield TV).
- Selección flexible de la ubicación de datos del juego, compatible con ubicaciones de almacenamiento interno, compartido y externo disponibles.
- Detección de datos del juego en el primer lanzamiento para simplificar el inicio cuando los archivos requeridos ya están disponibles.
- Menú de trucos (cheats) integrado en el juego.

## 🎨 Mejoras gráficas y visuales

**Opciones gráficas añadidas:**

- Efecto cel-shading configurable para un estilo visual más estético, que se adapta fielmente al estilo de dibujo animado original de los Simpsons.
- Resoluciones de renderizado seleccionables en el juego: 480p, 720p, 1080p, 1440p y 2160p.
- Filtrado anisotrópico de texturas configurable de 2× a 16×, mejorando la claridad de las texturas en ángulos distantes u oblicuos.
- Enfoque de imagen (Sharpening) ajustable, con intensidad configurable y opción de activar/desactivar.
- Escalado bicúbico ligero para las cinemáticas renderizadas en 3D de baja resolución del juego.
- Efecto Bloom configurable para realzar la apariencia de las luces y efectos especiales.
- Brillo de la luz solar y balance de iluminación mejorados.
- Efectos experimentales de sombras de nubes animadas para entornos exteriores diurnos.
- Iluminación de borde (Rim lighting) opcional para los bordes de personajes y vehículos.
- **Filtro CRT:** Un efecto opcional estilo CRT-Geom que imita la apariencia de un televisor de la década de 1990.
- **Desenfoque de movimiento (Motion blur):** Un efecto adaptado a la cámara centrado en el movimiento del vehículo, la estela de las luces traseras y la parte inferior de la pantalla.
- Mayor distancia de dibujado del escenario, permitiendo que una mayor parte del mundo del juego permanezca visible con mayores distancias de renderizado de peatones y vehículos, y una animación más fluida de peatones distantes.

## 🛠️ Configuración gráfica predeterminada

| Ajuste | Valor predeterminado |
| --- | --- |
| Resolución | 1080p |
| Cel Shading | Activado |
| Filtrado anisotrópico | 2× |
| Bloom | Activado |
| Sombras de nubes | Activado |
| Enfoque (Sharpening) | 40 % |
| Filtro CRT | Desactivado |
| Desenfoque de movimiento | Desactivado |
| Iluminación de borde (Rim lighting) | Desactivado |
| Límite de FPS | 60 FPS |

## 🧪 Funciones experimentales

Algunas funciones siguen siendo experimentales y pueden variar según el dispositivo:

- Límites de FPS más altos (90, 120 y 144 FPS)
- Renderizado de sombras de nubes

## 🔬 Próximas funciones

- Perfeccionamiento de la compatibilidad con otros idiomas.
- Investigación de una posible compatibilidad con datos de juego de los lanzamientos de PS2, GameCube y Xbox.
- Extracción automática de datos del juego desde archivos de imagen de disco.

---

https://github.com/nicksevarg/The-Simpsons-Hit-and-Run-Revamped