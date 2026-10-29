The Simpsons Hit & Run - Revamped! (Android)
https://github.com/nicksevarg/The-Simpsons-Hit-and-Run-Revamped
--------------------------------------------

Ein verbesserter Android-Port von The Simpsons: Hit & Run, der darauf abzielt, das Originalspiel mit verbesserter Grafik, erweiterter Anpassung, Touchscreen-Unterstützung, Unterstützung für Android TV-Geräte sowie verschiedenen Fehlerbehebungen und Verbesserung der Benutzerfreundlichkeit auf Android-Geräte zu bringen.

Wichtig: Sie müssen Ihre eigene PC-Version des Spiels und die erforderlichen Spieldaten bereitstellen. Aus Urheberrechtsgründen sind die originalen Spieldaten nicht in diesem Projekt enthalten.

Die Hauptvision dieses Projekts ist es, das Originalspiel und seine originalen Spieldateien an ihre Grenzen zu bringen und gleichzeitig optimiert zu bleiben, um dieses legendäre Simpsons-Spiel zu modernisieren.

Diese Version basiert auf dem hervorragenden Fork von Carlox33 des ursprünglichen Switch- und Vita-Ports von ZenoArrows. Ein besonderer Dank geht an alle Beteiligten, die diesen Port möglich gemacht haben!


⚡ Empfohlene Spezifikationen
--------------------------------------------

(Standardeinstellungen / Cel Shading EIN)

Tegra X1 oder Helio G99 [3/4 GB RAM]   (1080p / 60 FPS) 
Snapdragon 8 Gen 1/2    [8 GB RAM]     (2160p / 120 FPS)

(Alle Verbesserungen AUS)

Beliebiger älterer Low/Mid-Spec SOC [2 GB RAM] (720p / 60 FPS)

Die Standardeinstellungen sind so angepasst, dass sie auf den meisten modernen Smartphones gut funktionieren.

Bitte beachten: Bewegungsunschärfe (Motion Blur) und CRT-Filter können sehr GPU-intensiv sein und sind daher standardmäßig deaktiviert.


📦 Installation
--------------------------------------------

Wählen Sie bei Aufforderung den entsprechenden Speicherort der Spieldaten aus oder lassen Sie den Port verfügbare Daten automatisch erkennen.

Alle Spieldaten müssen aus der ursprünglichen PC-Installation stammen und bestehen aus Folgendem:

Ordner "art"
Ordner "movies"
Ordner "scripts"
Ordner "sound"

Sowie allen anderen .rcf-Dateien, die sich im Hauptinstallationsordner befinden, einschließlich der Sprachdatei dialog.rcf.

Unterstützte Speicherorte für Spieldaten sind:

Android\data\com.sevarg.simpsons\files
Gemeinsamer interner Speicher\Simpsons
Externer Speicher\Simpsons


🌐 Sprachen
--------------------------------------------

Unterstützung für Englisch, Français, Deutsch und Español.

Wenn die PC-Spieldaten von einer europäischen Version stammen, können Sie jede beliebige Sprache auswählen, und der Text im Spiel wird ebenfalls unterstützt. Die europäische Version enthält In-Game-Textdaten und Videos für alle 4 Sprachen. (Dies schließt Großbritannien und Irland ein)

Wenn die Daten von der nordamerikanischen Version stammen, wird die Sprache aus Kompatibilitätsgründen immer auf Englisch zurückgesetzt.

Was die Dialoge im Spiel betrifft, hängt dies vom Land ab. Die Sprachdialogdateien sind wie folgt aufgebaut:

Englisch (dialog.rcf)  UK, Irland, USA, Kanada, Niederlande, International
Französisch (dialogf.rcf) Frankreich
Deutsch  (dialogg.rcf) Deutschland
Spanisch (dialogs.rcf) Spanien

Sie können Ihre gewünschte Sprache beim Starten des Spiels auswählen. Wenn Sie nicht über die entsprechende Sprachdatei dialog.rcf verfügen und die Standard-dialog.rcf vorhanden ist, wird der Spieldialog automatisch auf Englisch zurückgesetzt. Wenn keine Dialogdatei gefunden wird, wird das Spiel nicht geladen, um Abstürze während des Spiels zu vermeiden.

Kopieren Sie sorgfältig alle Ihre Dateien aus dem PC-Installationsordner.


🎮 Steuerung
--------------------------------------------

Kompatibilität mit Xbox One/Series, DualShock 4/DualSense, Switch 1/2, 8bitdo und verschiedenen anderen kompatiblen Bluetooth-Controllern.

Tastenaktionen sind in dieser Reihenfolge aufgelistet: Xbox > PlayStation > Switch
Zu Fuß / Fahren ist jeweils durch "/" getrennt

Start-Taste — Pause-Menü
Obere Aktionstaste — Interagieren / In Auto einsteigen - △ Y X 
Linke Aktionstaste — Angriff / Handbremse - □ X Y
Rechte Aktionstaste — Sprintfunktion umschalten / Rückwärts - ○ B A
Untere Aktionstaste — Springen oder Bestätigen / Beschleunigen - ✕ A B
Steuerkreuz — Menünavigation und Bewegung/Lenken
Linker Stick — Menünavigation und Bewegung/Lenken
Rechter Stick — Kamerasteuerung
L1 (Schultertaste links) — Kamerazoom umschalten
L2 (Trigger links) — Heranzoomen / Rückwärts (Analog)
R1 (Schultertaste rechts) — Kamera zentrieren / Hupe
R2 (Trigger rechts) — Herauszoomen / Beschleunigen (Analog)
L3 (Linken Stick drücken) — Sprintfunktion umschalten

Verbesserung der Steuerung:

Integriertes Touchscreen-HUD und Steuerung mit Anpassungsoptionen für Position, Größe und Deckkraft der Touch-Tasten.

Die gesamte Vorarbeit wurde von Carlox33 geleistet. Bitte besuchen Sie das Repository für weitere Details:
https://github.com/Carlox33/The-Simpsons-Hit-and-Run-Android/blob/master/TOUCH_CONTROLS.md

Verbessertes Kamerasystem mit schnellerer Reaktion und mehr Kamerafreiheit.
Verbesserte Reaktionsfähigkeit/präzisere Steuerung zu Fuß.


✨ Lebensqualität-Verbesserungen (QoL)
--------------------------------------------

Optionen im Spiel für Grafik- und Controller-Einstellungen.

Kompatibilität für Android TV-Geräte mit korrekter Controller-Sortierung und Unterstützung für doppeltes Zurückdrücken (Getestet auf NVidia Shield TV).

Flexible Auswahl des Speicherorts für Spieldaten mit Unterstützung für internen, gemeinsamen und externen Speicher.

Erkennung der Spieldaten beim ersten Start, um die Einrichtung zu vereinfachen, wenn die erforderlichen Dateien bereits vorhanden sind.

In-Game-Cheat-Menü hinzugefügt.


🎨 Grafik- und visuelle Verbesserungen
--------------------------------------------

Hinzugefügte Grafikoptionen:

Einstellbarer Cel-Shading-Effekt für einen ästhetisch ansprechenderen visuellen Stil, der dem originalen Zeichentrickstil der Simpsons sehr nahe kommt.

Im Spiel auswählbare Rendering-Auflösungen: 480p, 720p, 1080p, 1440p und 2160p.

Einstellbare anisotrope Texturfilterung von 2× bis 16×, die die Texturschärfe bei entfernten und schrägen Blickwinkeln verbessert.

Anpassbare Bildschärfe mit konfigurierbarer Stärke und einer Ein/Aus-Option.

Leichtes bikubisches Hochskalieren für die niedrig auflösenden 3D-Cutscenes des Spiels.

Konfigurierbarer Bloom-Effekt zur Verbesserung des Erscheinungsbilds von Lichtern und Spezialeffekten.

Verbesserte Sonnenblendung und ausgewogenere Beleuchtung.

Experimentelle animierte Wolkenschatten-Effekte für Außenbereiche bei Tageslicht.

Optionale Randbeleuchtung (Rim Lighting) für Kanten von Charakteren und Fahrzeugen.

CRT-Filter: Ein optionaler Effekt im CRT-Geom-Stil, der das Erscheinungsbild eines Fernsehers aus den 1990er Jahren imitiert.

Bewegungsunschärfe (Motion Blur): Ein kamerabezogener Effekt, der sich auf Fahrzeugbewegungen, Rücklichtstreuung und den unteren Bildschirmbereich konzentriert.

Erhöhte Sichtweite (Draw Distance), sodass mehr von der Spielwelt sichtbar bleibt, mit größeren Darstellungsdistanzen für Fußgänger und Fahrzeuge sowie flüssigeren Animationen für entfernte Fußgänger.


🛠️ Standard-Grafikkonfiguration
--------------------------------------------

Auflösung
1080p

Cel Shading
Ein

Anisotrope Filterung
2×

Bloom
Ein

Wolkenschatten
Ein

Schärfe
40%

CRT-Filter
Aus

Bewegungsunschärfe
Aus

Randbeleuchtung (Rim lighting)
Aus

FPS-Begrenzung
60 FPS


🧪 Experimentelle Funktionen
--------------------------------------------

Einige Funktionen bleiben experimentell und können je nach Gerät variieren:
Höhere FPS-Begrenzungen (90, 120 und 144 FPS)
Wolkenschatten-Rendering.


🔬 Zukünftige Funktionen
--------------------------------------------

Verfeinerung der Kompatibilität für andere Sprachen.

Erforschung einer möglichen Kompatibilität mit Spieldaten von PS2-, GameCube- und Xbox-Versionen.

Automatisches Entpacken von Spieldaten aus Disc-Image-Dateien.