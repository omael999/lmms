# NovaBeat Studio

DAW gratuito para PC con el **flujo de trabajo de FL Studio** y algunas ideas de **GarageBand**, construido sobre el motor de audio de [LMMS](https://lmms.io) (versión 1.3 en desarrollo).

NovaBeat Studio es un fork de LMMS y mantiene su licencia GPL. Todo el mérito del motor de audio, los instrumentos y los efectos es de los desarrolladores de LMMS.

## Qué cambia respecto a LMMS

**Aspecto estilo FL Studio**
- Tema oscuro gris pizarra con acentos naranjas, botones de pasos nuevos y logo propio.
- Nombres de FL: *Playlist* (antes Song Editor), *Channel Rack* (antes Pattern Editor) y *Mixer*.
- Al crear un proyecto: la Playlist ocupa el fondo, el Channel Rack aparece delante con **Kick, Clap, Hat y Snare** listos, y el navegador de sonidos se abre a la izquierda.

**Transporte estilo FL**
- Botones **PAT / SONG** en la barra superior: en PAT, *Reproducir* toca el patrón actual del Channel Rack; en SONG, toca toda la Playlist.
- Botones grandes de Reproducir, Detener y Grabar.
- La barra espaciadora siempre sigue el modo PAT/SONG (como en FL).

**Atajos de teclado de FL Studio**

| Tecla | Acción |
|---|---|
| Espacio | Reproducir / detener (según PAT/SONG) |
| F5 | Playlist |
| F6 | Channel Rack |
| F7 | Piano Roll |
| F8 | Editor de automatización |
| F9 | Mixer |
| F10 | Ajustes |
| F11 | Pantalla completa |
| F12 | Cerrar todas las ventanas |

Los atajos originales de LMMS (Ctrl+1 … Ctrl+7) siguen funcionando.

**Channel Rack**
- Botón **+ Añadir canal** con la lista de todos los instrumentos.

**Batería inteligente (como el Drummer de GarageBand)**
- Botón *Batería inteligente* en el Channel Rack.
- Eliges un estilo (Pop, Rock, Hip-Hop, Trap, Reguetón, House, Funk, Lo-Fi) y mueves el punto en el pad: izquierda/derecha = sencillo/complejo, arriba/abajo = fuerte/suave.
- Escribe el ritmo en el patrón actual; si faltan canales de batería, los crea con samples incluidos.
- *Nueva variación* genera otra versión del mismo estilo.

**Separado de LMMS**
- Usa su propia configuración (`.novabeatrc.xml`) y su propia carpeta en *Documentos\NovaBeat Studio*, así que puede convivir con un LMMS instalado.

## Descargar el instalador para Windows

1. Abre la pestaña **Actions** de este repositorio.
2. Entra en la última ejecución verde de **NovaBeat Studio - Windows**.
3. En **Artifacts**, descarga `NovaBeatStudio-…-win64.exe` y ejecútalo.

Cada vez que se suben cambios a la rama `novabeat`, GitHub vuelve a compilar el instalador automáticamente (tarda unos 20–40 minutos).
