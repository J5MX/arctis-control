# Arctis Control

Panel nativo para Linux para controlar auriculares **SteelSeries Arctis**, construido sobre
[HeadsetControl](https://github.com/Sapd/HeadsetControl).

*[Read in English](README.md)*

> **Proyecto no oficial**, sin relación con SteelSeries. «SteelSeries» y «Arctis» son marcas de
> sus propietarios.

La app detecta el auricular conectado y **muestra solo lo que ese modelo admite**: batería,
ecualizador (presets y bandas con el rango de cada modelo), apagado automático, limitador de
volumen, luces, avisos de voz, Bluetooth, sidetone, volumen y LED del micrófono, filtro de ruido…
Si hay varios auriculares conectados, permite elegir cuál controlar.

Si usas un micrófono aparte, puedes **desactivar el micrófono del auricular** para que el sistema
(y apps como Discord o los juegos) dejen de verlo. Cambia la tarjeta de sonido del auricular en
PipeWire a su perfil de solo salida; WirePlumber recuerda la elección.

Aplicación nativa en C++/Qt 6 (QML): sin navegador, sin servidor y sin conexiones de red. La
interfaz está en inglés y cambia sola a español en sistemas en español.

## Modelos compatibles

Todos los SteelSeries que soporta HeadsetControl: Arctis 1, 7, 7+, 7X, 7P, 9, Pro (2019),
Pro Wireless, GameBuds, Nova 3, Nova 3P, Nova 5/5X, Nova 7, Nova 7P y Nova Pro Wireless.

Probado en un Arctis Nova 5. El resto se ha probado con el auricular simulado de HeadsetControl
(`--test-device`), que tiene todas las funciones. Si tienes otro modelo, se agradecen informes.

## Instalación (Arch Linux / CachyOS)

1. Instala las herramientas para compilar paquetes (en CachyOS casi siempre vienen ya):
   ```
   sudo pacman -S --needed git base-devel
   ```
2. Descarga el código y compila el paquete:
   ```
   git clone https://github.com/J5MX/arctis-control.git
   cd arctis-control/packaging
   makepkg -si
   ```
   Ejecuta `makepkg` **sin** `sudo`: ya te pedirá la contraseña cuando la necesite. La opción `-s`
   instala sola las dependencias y `-i` instala la app al terminar de compilarla.
3. Abre **Arctis Control** desde el menú de aplicaciones (o con `arctis-control`).

Si el auricular no aparece, vuelve a conectar su receptor USB (al instalar `headsetcontrol` se
añaden las reglas de permisos, que se aplican al conectarlo) y comprueba que `headsetcontrol -b`
lo detecta.

**Actualizar:** `cd arctis-control && git pull && cd packaging && makepkg -si`
**Desinstalar:** `sudo pacman -R arctis-control`

Se instala `/usr/bin/arctis-control`, el lanzador **Arctis Control** y su icono.

- Dependencias (las instala `makepkg -s`): `headsetcontrol`, `qt6-base`, `qt6-declarative`,
  `qt6-svg`, `libpulse` (para `pactl`, que usa el interruptor del micrófono).
- Para compilar: `cmake`, `qt6-tools`.
- Opcional: `ttf-fira-sans` para la tipografía.

Si encuentras un fallo o tienes otro modelo, abre una incidencia indicando el modelo, tu
distribución y la salida de `headsetcontrol --version` y `headsetcontrol -o json` (no contiene
datos personales).

Los detalles de desarrollo, traducciones y seguridad están en el [README en inglés](README.md).

## Licencia

GPL-3.0-or-later (ver `LICENSE`).
