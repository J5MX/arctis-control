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

Aplicación nativa en C++/Qt 6 (QML): sin navegador, sin servidor y sin conexiones de red. La
interfaz está en inglés y cambia sola a español en sistemas en español.

## Modelos compatibles

Todos los SteelSeries que soporta HeadsetControl: Arctis 1, 7, 7+, 7X, 7P, 9, Pro (2019),
Pro Wireless, GameBuds, Nova 3, Nova 3P, Nova 5/5X, Nova 7, Nova 7P y Nova Pro Wireless.

Probado en un Arctis Nova 5. El resto se ha probado con el auricular simulado de HeadsetControl
(`--test-device`), que tiene todas las funciones. Si tienes otro modelo, se agradecen informes.

## Instalación (Arch Linux / CachyOS)

```
cd packaging
makepkg -si
```

Instala `/usr/bin/arctis-control`, el lanzador **Arctis Control** y su icono. Sustituye al antiguo
paquete `nova5-panel` y recupera sus ajustes.

- Dependencias: `headsetcontrol`, `qt6-base`, `qt6-declarative`, `qt6-svg`.
- Para compilar: `cmake`, `qt6-tools`.
- Opcional: `ttf-fira-sans` para la tipografía.

Desinstalar: `sudo pacman -R arctis-control`.

Si el auricular no aparece, comprueba que HeadsetControl lo ve (`headsetcontrol -b`) y vuelve a
conectar el receptor tras instalar `headsetcontrol`, que añade las reglas udev de permisos.

Los detalles de desarrollo, traducciones y seguridad están en el [README en inglés](README.md).

## Licencia

GPL-3.0-or-later (ver `LICENSE`).
