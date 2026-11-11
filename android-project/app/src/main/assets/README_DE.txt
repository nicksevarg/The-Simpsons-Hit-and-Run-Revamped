The Simpsons Hit & Run - Revamped! (Android)
https://github.com/nicksevarg/The-Simpsons-Hit-and-Run-Revamped
--------------------------------------------

Ein verbesserter Android-Port von The Simpsons: Hit & Run, der darauf abzielt, das Originalspiel mit verbesserter Grafik, erweiterter Anpassung, Touchscreen-Unterstützung, Unterstützung für Android TV-Geräte sowie verschiedenen Fehlerbehebungen und Lebensqualität-Verbesserungen auf Android-Geräte zu bringen.

Wichtig: Sie müssen Ihre eigene PC-Version des Spiels und die erforderlichen Spieldaten bereitstellen. Aus Urheberrechtsgründen sind die ursprünglichen Spieldaten nicht in diesem Projekt enthalten.

Die Hauptvision dieses Projekts ist es, das Originalspiel und seine ursprünglichen Spielressourcen an die Grenzen zu bringen und gleichzeitig optimiert zu bleiben, um diesen legendären Simpsons-Klassiker zu modernisieren.

Diese Version basiert auf dem hervorragenden Fork von Carlox33 des ursprünglichen Switch- und Vita-Ports von ZenoArrows. Ein besonderer Dank geht an alle Beteiligten, die diesen Port möglich gemacht haben!


⚡ Empfohlene Spezifikationen
--------------------------------------------

(Standardeinstellungen / Cel Shading "FULL")

Tegra X1 oder Helio G99 [3/4GB RAM]   (1080p / 60FPS) 
Snapdragon 8 Gen 1/2    [8GB RAM]   (2160p / 120FPS)

(Alle Verbesserungen AUS)

Jedes ältere Low/Mid-Spec SoC [2GB RAM] (720p / 60FPS)

Die Standardeinstellungen sind so feinabstimmend, dass sie auf den meisten modernen Smartphones gut funktionieren.

Bitte beachten Sie: Bewegungsschärfe (Motion Blur) und Wolkenschatten können sehr GPU-intensiv sein und sind daher standardmäßig deaktiviert. Auf älteren oder leistungsschwächeren CPU-SoCs wird der Cel Shading "LITE"-Modus empfohlen.
Die Unterstützung für Mali GPU SoCs ist nach wie vor nicht zu 100 % garantiert.


📦 Installation
--------------------------------------------

Wählen Sie den entsprechenden Spieldaten-Pfad aus, wenn Sie dazu aufgefordert werden, oder lassen Sie den Port verfügbare Daten automatisch erkennen.

Alle Spieldaten müssen aus der ursprünglichen PC-Installation stammen und bestehen aus Folgendem:

Ordner "art"
Ordner "movies"
Ordner "scripts"
Ordner "sound"

Und allen anderen .rcf-Dateien, die sich im übergeordneten Installationsordner befinden, einschließlich der Sprachdatei dialog.rcf.

Unterstützte Spieldaten-Pfade sind wie folgt:

Android\data\com.sevarg.simpsons\files
Shared Internal\Simpsons
External\Simpsons


🌐 Sprachen
--------------------------------------------

Unterstützung für Englisch, Französisch, Deutsch und Spanisch.

Wenn die PC-Spieldaten von einer europäischen Version stammen, können Sie jede beliebige Sprache auswählen, und der In-Game-Text wird ebenfalls unterstützt. Die europäische Version enthält Textdaten und Videos im Spiel für alle 4 Sprachen. (Dies schließt UK und Irland ein)

Wenn die Daten aus der nordamerikanischen Version stammen, wird die Sprache aus Kompatibilitätsgründen immer Standardmäßig auf Englisch zurückgesetzt.

Was die Dialoge im Spiel betrifft, hängt es vom Land ab, und die Sprach-Dialogdateien lauten wie folgt:

Englisch    (dialog.rcf)  UK, Irland, USA, Kanada, Niederlande, International
Französisch (dialogf.rcf) Frankreich
Deutsch     (dialogg.rcf) Deutschland
Spanisch    (dialogs.rcf) Spanien

Sie können Ihre gewünschte Sprache beim Öffnen des Spiels auswählen. Wenn Sie nicht über die entsprechende dialog.rcf-Sprachdatei verfügen und dialog.rcf vorhanden ist, wechselt der Spieldialog automatisch zu Englisch zurück. Wenn keine Dialogdatei gefunden wird, schlägt das Laden des Spiels fehl, um Abstürze während des Spiels zu vermeiden.

Kopieren Sie sorgfältig alle Ihre Dateien aus dem PC-Installationsordner.


🎮 Steuerung
--------------------------------------------

Kompatibilität mit Xbox One/Series, DualShock 4/DualSense, Switch 1/2, 8bitdo und verschiedenen anderen kompatiblen Bluetooth-Controllern.

Tastenaktionen sind in dieser Reihenfolge aufgeführt: Xbox > PlayStation > Switch
Steuerung zu Fuß / beim Fahren ist mit "/" getrennt

Start-Taste - Pause-Menü
Obere Aktionstaste — Interagieren / Mit Auto interagieren - △ Y X 
Linke Aktionstaste — Angriff / Handbremse - □ X Y
Rechte Aktionstaste — Sprint umschalten / Rückwärts - ○ B A
Untere Aktionstaste — Springen oder Bestätigen / Beschleunigen - ✕ A B
D-Pad - Menünavigation und Bewegung/Lenken
Linker Stick - Menünavigation und Bewegung/Lenken
Rechter Stick - Kamerasteuerung
Linke Schultertaste (L1) - Kamerazoom umschalten 
Linker Trigger (L2) - Heranzoomen / Rückwärts (Analog)
Rechte Schultertaste (R1) - Kamera zentrieren / Autohupe
Rechter Trigger (R2) - Herauszoomen / Beschleunigen (Analog)
Linker Stick Klick (L3) - Sprint umschalten

Steuerungsverbesserungen:

Integriertes Touchscreen-HUD und Steuerung mit Anpassungsoptionen für Platzierung, Größe und Deckkraft der Touch-Tasten.

Die gesamte harte Arbeit wurde von Carlox33 geleistet, bitte besuchen Sie deren Repository für weitere Details: https://github.com/Carlox33/The-Simpsons-Hit-and-Run-Android/blob/master/TOUCH_CONTROLS.md

Verbessertes Kamerasystem mit schnellerer Reaktion und mehr Kamerafreiheit.
Verbesserte Reaktionsfreudigkeit / präzisere Steuerung zu Fuß.


✨ Lebensqualität-Verbesserungen
--------------------------------------------

In-Game-Umschalter für Grafik- und Controller-Einstellungen.

Kompatibilität für Android TV-Geräte mit korrekter Controller-Sortierung und Unterstützung für doppeltes Zurückdrücken (Getestet auf NVidia Shield TV)

Flexible Auswahl des Spieldaten-Pfads mit Unterstützung für verfügbare interne, geteilte und externe Speicherorte.

Erkennung von Spieldaten beim ersten Start zur Vereinfachung des Starts, wenn die erforderlichen Dateien bereits vorhanden sind.

In-Game-Cheat-Menü hinzugefügt.


🎨 Grafik- & Visuelle Verbesserungen
--------------------------------------------

Hinzugefügte Grafikoptionen:

Leichter objektbasierter Cel-Shading-Effekt für einen ästhetisch ansprechenderen visuellen Stil, der dem ursprünglichen Cartoon-Stil der Simpsons sehr nahe kommt.

Im Spiel auswählbare Rendering-Auflösungen: 480p, 720p, 1080p, 1440p und 2160p, mit optionaler dynamischer Auflösungsskalierung.

Konditionierbare anisotrope Texturfilterung von 4× bis 16×, die die Texturklarheit bei entfernten und schrägen Winkeln verbessert.

Einstellbare Bildschärfe mit konfigurierbarer Stärke und Aktivierungs-/Deaktivierungsoption.

Leichtes bikubisches Hochskalieren für die niedrig auflösenden, 3D-gerenderten Zwischensequenzen des Spiels.

Konfigurierbares Bloom zur Verbesserung des Erscheinungsbilds von Lichtern und Spezialeffekten.

Verbesserte Sonnenblendung und Ausgewogenheit der Beleuchtung.

Experimentelle animierte Wolkenschatten-Effekte für Außenbereiche bei Tageslicht.

Optionale Randbeleuchtung (Rim Lighting) für Kanten von Charakteren und Fahrzeugen.

CRT-Filter: Ein optionaler Effekt im CRT-Geom-Stil, der das Erscheinungsbild eines Fernsehers aus den 1990er Jahren imitiert.

Bewegungsunschärfe (Motion Blur): Ein kamerabewusster Effekt, der auf Fahrzeugbewegung, Trennung der Rücklichter und den unteren Bildschirmbereich fokussiert ist.

Erhöhte Sichtweite der Szenerie, sodass mehr von der Spielwelt sichtbar bleibt, mit erhöhter Darstellungsdistanz für Fußgänger und Fahrzeuge sowie flüssigerer Animation entfernter Fußgänger.


🛠️ Standard-Grafikkonfiguration
--------------------------------------------

Auflösung
1080p

Cel Shading
Ein [Full]

Anisotrope Filterung
4×

Bloom
Ein

Wolkenschatten
Aus

Schärfung
Aus

CRT-Filter
Aus

Bewegungsunschärfe
Aus

Randbeleuchtung
Aus

FPS-Begrenzung
60 FPS

Dynamische Auflösungsskalierung
Aus


🧪 Experimentelle Funktionen
--------------------------------------------

Einige Funktionen bleiben experimentell und können je nach Gerät variieren:
Höhere FPS-Begrenzungen (90, 120 und 144 FPS)
Wolkenschatten-Rendering.


🔬 Kommende Funktionen
--------------------------------------------

Verfeinerung der Kompatibilität für andere Sprachen.

Erforschung einer möglichen Kompatibilität mit Spieldaten von PS2-, GameCube- und Xbox-Veröffentlichungen.

Automatische Extraktion von Spieldaten aus Disc-Image-Dateien.
