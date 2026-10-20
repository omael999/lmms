# NovaBeat Studio

DAW gratuito para PC con el **flujo de trabajo de FL Studio** y algunas ideas de **GarageBand**, construido sobre el motor de audio de [LMMS](https://lmms.io) (versión 1.3 en desarrollo).

NovaBeat Studio es un fork de LMMS y mantiene su licencia GPL. Todo el mérito del motor de audio, los instrumentos y los efectos es de los desarrolladores de LMMS.

## Qué cambia respecto a LMMS

**Todo en español**
- Toda la interfaz está en español por defecto (se puede cambiar en Ajustes), incluidos los diálogos de archivos y los botones Sí/No/Cancelar.
- Ventanas con los nombres de FL en español: **Arreglo** (Playlist), **Rack de canales** (Channel Rack), **Editor de piano** (Piano Roll) y **Mezclador** (Mixer).

**Aspecto y orden estilo FL Studio**
- Tema oscuro gris pizarra con acentos naranjas, botones de pasos nuevos y logo propio.
- **Ventanas acopladas**: el Arreglo ocupa todo el fondo, el Mezclador se acopla abajo (F9), el Editor de piano y el de automatización ocupan el sitio del Arreglo (F7 / F8, y F5 vuelve al Arreglo). Las ventanas acopladas no se pueden arrastrar por error.
- El Rack de canales flota encima y crece solo al añadir canales; la ventana de cada canal se abre a su lado.
- Solo una ventana de canal abierta a la vez.
- Todos los proyectos (nuevos o abiertos) se abren ordenados. Botón **Organizar ventanas** (Ctrl+Mayús+W) en la barra superior para volver a ordenar en cualquier momento. Se puede desactivar en *Ver > Ventanas acopladas (estilo FL)*.
- Proyecto nuevo con **Bombo, Palmas, Charles, Caja, Charles abierto, Percusión y 808**, cada uno en **su propia pista del Mezclador**, con nombre y color.

**Transporte estilo FL**
- Botones **PATRÓN / CANCIÓN** en la barra superior: en PATRÓN, *Reproducir* toca el patrón actual del Rack de canales; en CANCIÓN, toca todo el Arreglo.
- Botones grandes de Reproducir, Detener y Grabar.
- La barra espaciadora siempre sigue el modo PATRÓN/CANCIÓN (como en FL).

**Atajos de teclado de FL Studio**

| Tecla | Acción |
|---|---|
| Espacio | Reproducir / detener (según PATRÓN/CANCIÓN) |
| F5 | Arreglo |
| F6 | Rack de canales |
| F7 | Editor de piano |
| F8 | Editor de automatización |
| F9 | Mezclador |
| F10 | Ajustes |
| F11 | Pantalla completa |
| F12 | Cerrar todas las ventanas |
| Ctrl+Mayús+W | Organizar ventanas |

Los atajos originales de LMMS (Ctrl+1 … Ctrl+7) siguen funcionando.

**Rack de canales (la ventana de beats)**
- **Pinta pasos arrastrando** el ratón, como en FL (arrastra sobre pasos vacíos para encenderlos o sobre pasos encendidos para apagarlos).
- Mando **SWING** para dar balanceo al ritmo (se guarda con el proyecto).
- Clic derecho sobre los pasos de un canal: **Rellenar cada 2 / 4 / 8 pasos**, **Desplazar** a izquierda o derecha, **Invertir**, **Aleatorio**, **Humanizar la velocidad**, **Acentuar los tiempos**, **Copiar / Pegar pasos**.
- La rueda del ratón sobre un paso cambia su volumen (velocidad).
- Botón **+ Añadir canal**: cualquier instrumento o una **muestra de audio** de tu PC (WAV, MP3, OGG, FLAC...). Cada canal nuevo recibe automáticamente su propia pista en el Mezclador, también al arrastrar sonidos desde el navegador.
- Contador de pasos del patrón y botón para **vaciar el patrón**.

**Batería inteligente (como el Drummer de GarageBand)**
- Botón *Batería inteligente* en el Rack de canales.
- Eliges un estilo (Pop, Rock, Hip-Hop, Trap, Reguetón, House, Funk, Drill, Afrobeat, Cumbia, Lo-Fi) y mueves el punto en el pad: izquierda/derecha = sencillo/complejo, arriba/abajo = fuerte/suave.
- Escribe el ritmo en el patrón actual; si faltan canales de batería, los crea con samples incluidos.
- *Nueva variación* genera otra versión del mismo estilo.

**Separado de LMMS**
- Usa su propia configuración (`.novabeatrc.xml`) y su propia carpeta en *Documentos\NovaBeat Studio*, así que puede convivir con un LMMS instalado.

## Descargar el instalador para Windows

1. Abre la pestaña **Actions** de este repositorio.
2. Entra en la última ejecución verde de **NovaBeat Studio - Windows**.
3. En **Artifacts**, descarga `NovaBeatStudio-…-win64.exe` y ejecútalo.

Cada vez que se suben cambios a la rama `novabeat`, GitHub vuelve a compilar el instalador automáticamente (tarda unos 20–40 minutos).
