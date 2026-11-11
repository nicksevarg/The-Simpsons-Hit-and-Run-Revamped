The Simpsons Hit & Run - Revamped! (Android)
https://github.com/nicksevarg/The-Simpsons-Hit-and-Run-Revamped
--------------------------------------------

Una versión mejorada de The Simpsons: Hit & Run para Android orientada a llevar el juego original a dispositivos Android con gráficos mejorados, personalización ampliada, soporte para pantalla táctil, soporte para dispositivos Android TV, así como varias correcciones de errores y mejoras en la calidad de vida.

Importante: Debes proporcionar tu propia versión de PC del juego y sus datos de juego requeridos. Por razones de derechos de autor, los datos originales del juego no se incluyen en este proyecto.

La visión principal de este proyecto es llevar el juego original y sus recursos originales al límite mientras se mantiene optimizado, con el objetivo de modernizar este legendario juego de Los Simpson.

Esta versión se basa en el excelente fork de Carlox33 del port original de ZenoArrows para Switch y Vita. ¡Un agradecimiento especial a todos los involucrados en hacer posible este port!


⚡ Especificaciones recomendadas
--------------------------------------------

(Ajustes por defecto / Cel Shading "FULL")

Tegra X1 o Helio G99 [3/4GB RAM]   (1080p / 60FPS) 
Snapdragon 8 Gen 1/2    [8GB RAM]   (2160p / 120FPS)

(Toda mejora DESACTIVADA)

Cualquier SoC antiguo de gama baja/media [2GB RAM] (720p / 60FPS)

Los ajustes por defecto están afinados para funcionar bien en la mayoría de los smartphones modernos.

Nota: El desenfoque de movimiento (Motion Blur) y las sombras de nubes pueden consumir bastante GPU, por lo que están desactivados por defecto. En SoCs con CPU más antiguas o de gama baja, se recomienda el modo Cel Shading "LITE".
El soporte para SoCs con GPU Mali aún no está garantizado al 100%.


📦 Instalación
--------------------------------------------

Selecciona la ubicación de datos de juego adecuada cuando se te solicite, o permite que el juego detecte los datos disponibles.

Todos los datos del juego deben proceder de la instalación original de PC y constan de lo siguiente:

Carpeta "art"
Carpeta "movies"
Carpeta "scripts"
Carpeta "sound"

Y todos los demás archivos .rcf ubicados en la carpeta raíz de instalación, incluido el archivo de idioma dialog.rcf.

Las ubicaciones de datos de juego compatibles son las siguientes:

Android\data\com.sevarg.simpsons\files
Shared Internal\Simpsons
External\Simpsons


🌐 Idiomas
--------------------------------------------

Soporte para inglés, francés, alemán y español.

Si los datos del juego de PC provienen de cualquier lanzamiento europeo, puedes seleccionar cualquier idioma que desees y el texto del juego también estará disponible. El lanzamiento europeo incluye datos de texto e imágenes de video en el juego para los 4 idiomas. (Esto incluye Reino Unido e Irlanda)

Si provienen del lanzamiento de Norteamérica, por razones de compatibilidad, el idioma siempre volverá por defecto al inglés.

En cuanto a los diálogos del juego, dependerá del país y los archivos de diálogo de idioma son los siguientes:

Inglés   (dialog.rcf)  Reino Unido, Irlanda, EE. UU., Canadá, Países Bajos, Internacional
Francés  (dialogf.rcf) Francia
Alemán   (dialogg.rcf) Alemania
Español  (dialogs.rcf) España

Puedes seleccionar el idioma deseado al abrir el juego. Si no tienes el archivo de diálogo dialog.rcf correspondiente y dialog.rcf estándar está presente, el diálogo del juego volverá automáticamente al inglés. Si no se encuentra ningún archivo de diálogo, el juego no se cargará, esto es para evitar cierres inesperados durante la partida.

Copia cuidadosamente todos tus archivos desde la carpeta de instalación de PC.


🎮 Controles
--------------------------------------------

Compatibilidad con mandos de Xbox One/Series, DualShock 4/DualSense, Switch 1/2, 8bitdo y varios otros mandos Bluetooth compatibles.

Las acciones de los botones se enumeran en este orden: Xbox > PlayStation > Switch
Los controles a pie / conduciendo se separan con "/"

Botón Start - Menú de pausa
Botón superior — Interactuar / Interactuar con el coche - △ Y X 
Botón izquierdo — Ataque / Freno de mano - □ X Y
Botón derecho — Alternar esprint / Marcha atrás - ○ B A
Botón inferior — Saltar o Confirmar / Acelerar - ✕ A B
D-pad - Navegación por menús y Movimiento/Dirección
Stick izquierdo - Navegación por menús y Movimiento/Dirección
Stick derecho - Control de cámara
Gatillo superior izquierdo (L1) - Cambiar zoom de cámara 
Gatillo inferior izquierdo (L2) - Acercar zoom / Marcha atrás (Analógico)
Gatillo superior derecho (R1) - Centrar cámara / Claxon del coche
Gatillo inferior derecho (R2) - Alejar zoom / Acelerar (Analógico)
Pulsar stick izquierdo (L3) - Alternar esprint

Mejoras en el control:

HUD y controles táctiles integrados en pantalla con opciones de personalización para la posición, el tamaño y la opacidad de los botones táctiles.

Todo el trabajo duro fue realizado por Carlox33, por favor visita su repositorio para obtener más detalles: https://github.com/Carlox33/The-Simpsons-Hit-and-Run-Android/blob/master/TOUCH_CONTROLS.md

Sistema de cámara mejorado con respuesta más rápida y mayor libertad de movimiento.
Mejor respuesta y controles más precisos al desplazarse a pie.


✨ Mejoras en la calidad de vida
--------------------------------------------

Ajustes integrados en el juego para la configuración de gráficos y mandos.

Compatibilidad con dispositivos Android TV con orden correcto de mandos y soporte para doble toque hacia atrás (Probado en NVidia Shield TV)

Selección flexible de la ubicación de los datos del juego, compatible con ubicaciones de almacenamiento interno, compartido y externo.

Detección de datos del juego en el primer inicio para simplificar la carga cuando los archivos requeridos ya están disponibles.

Menú de trucos integrado añadido.


🎨 Mejoras gráficas y visuales
--------------------------------------------

Opciones gráficas añadidas:

Efecto cel-shading ligero basado en objetos para un estilo visual más atractivo, muy similar al estilo de dibujos animados original de Los Simpson.

Resoluciones de renderizado seleccionables en el juego: 480p, 720p, 1080p, 1440p y 2160p, con escalado dinámico de resolución opcional.

Filtrado anisotrópico de texturas configurable de 4× a 16×, mejorando la claridad de las texturas a larga distancia y en ángulos oblicuos.

Nitidez de imagen ajustable, con intensidad configurable y opción de activar/desactivar.

Escalado bicúbico ligero para las cinemáticas renderizadas en 3D de baja resolución del juego.

Efecto de resplandor (Bloom) configurable para mejorar la apariencia de las luces y los efectos especiales.

Brillo solar y equilibrio de iluminación mejorados.

Efectos de sombras de nubes animadas experimentales para entornos exteriores durante el día.

Iluminación de bordes (Rim lighting) opcional para los contornos de personajes y vehículos.

Filtro CRT: Un efecto opcional estilo CRT-Geom que imita la apariencia de un televisor de los años 90.

Desenfoque de movimiento (Motion blur): Un efecto adaptado a la cámara centrado en el movimiento del vehículo, la separación de la estela trasera y la parte inferior de la pantalla.

Mayor distancia de dibujado del escenario, permitiendo que más partes del mundo del juego sigan siendo visibles con mayores distancias de renderizado de peatones y vehículos, y animaciones más fluidas de peatones lejanos.


🛠️ Configuración gráfica por defecto
--------------------------------------------

Resolución
1080p

Cel Shading
Activado [Full]

Filtrado anisotrópico
4×

Bloom
Activado

Sombras de nubes
Desactivado

Nitidez
Desactivado

Filtro CRT
Desactivado

Desenfoque de movimiento
Desactivado

Iluminación de bordes
Desactivado

Límite de FPS
60 FPS

Escalado dinámico de resolución
Desactivado


🧪 Funciones experimentales
--------------------------------------------

Algunas funciones siguen siendo experimentales y pueden variar según el dispositivo:
Límites de FPS más altos (90, 120 y 144 FPS)
Renderizado de sombras de nubes.


🔬 Próximas funciones
--------------------------------------------

Perfeccionamiento de la compatibilidad con otros idiomas.

Investigación de una posible compatibilidad con los datos de juego de las versiones de PS2, GameCube y Xbox.

Extracción automática de los datos del juego a partir de archivos de imagen de disco.
