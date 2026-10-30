# Arctis Control

Panel gráfico para controlar auriculares **SteelSeries Arctis** en Linux, construido sobre
[HeadsetControl](https://github.com/Sapd/HeadsetControl).

> Proyecto **no oficial**, sin relación con SteelSeries. «SteelSeries» y «Arctis» son marcas
> de sus propietarios.

La app detecta el auricular conectado y **muestra solo lo que ese modelo admite**: batería,
ecualizador (presets y bandas con el rango de cada modelo), apagado automático, limitador de
volumen, luces, avisos de voz, Bluetooth, sidetone, volumen y LED del micrófono, filtro de ruido…
Si hay varios auriculares conectados, permite elegir cuál controlar.

Aplicación nativa en C++/Qt6 (QML): sin navegador, sin servidor y sin conexiones de red.

## Modelos compatibles

Los que soporta HeadsetControl: Arctis 1, 7, 7+, 7X, 7P, 9, Pro (2019), Pro Wireless,
GameBuds, Nova 3, Nova 3P, Nova 5/5X, Nova 7, Nova 7P y Nova Pro Wireless.

Probado en un Arctis Nova 5. El resto se ha probado con el auricular simulado de HeadsetControl
(`--test-device`), que tiene todas las funciones; si tienes otro modelo, se agradecen informes.

## Instalación (Arch / CachyOS)

```
cd packaging
makepkg -si
```

Instala `/usr/bin/arctis-control`, el lanzador **Arctis Control** y su icono. Sustituye al
antiguo paquete `nova5-panel` y recupera sus ajustes. Dependencias: `headsetcontrol`,
`qt6-base`, `qt6-declarative`, `qt6-svg` (y `cmake` para compilar). Opcional: `ttf-fira-sans`.

Desinstalar: `sudo pacman -R arctis-control`.

Si el auricular no aparece, comprueba que HeadsetControl lo ve (`headsetcontrol -b`) y vuelve a
conectar el receptor tras instalar `headsetcontrol`, que añade las reglas udev de permisos.

## Desarrollo

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/arctis-control
```

| Opción | Uso |
|---|---|
| `--test-device` | Usa solo el auricular simulado de HeadsetControl. No toca los auriculares reales ni los ajustes guardados. |
| `--test-device --self-test` | Aplica todos los ajustes al simulado con el mismo código que la interfaz y sale con código 0 si todo va bien. |
| `--screenshot archivo.png` | Guarda una captura y sale. Con `QT_QPA_PLATFORM=offscreen` no abre ventana. |

| Ruta | Contenido |
|---|---|
| `src/` | `HeadsetController`: detecta auriculares, valida y ejecuta `headsetcontrol`, guarda ajustes |
| `qml/` | Interfaz: ventana, tarjetas, controles de ajustes, anillo de batería, bandas del EQ |
| `assets/` | Icono e ilustración del auricular |
| `packaging/` | `PKGBUILD` y lanzador `.desktop` |

## Seguridad

- No abre sockets ni hace conexiones de red. El motor QML usa un gestor de red que rechaza
  cualquier URL que no sea un recurso interno (qrc).
- Todos los textos se muestran como texto plano: un mensaje con HTML no se interpreta.
- `headsetcontrol` se ejecuta con ruta absoluta, argumentos en lista (sin shell), entorno
  mínimo, tiempo límite de 20 s y salida máxima de 1 MiB; las órdenes van de una en una.
- Cada orden lleva `-d` con el auricular elegido: nunca se aplica a otros conectados.
- Los valores se validan contra una tabla cerrada de ajustes y rangos, y contra las capacidades
  que anuncia el auricular. El resultado se comprueba en la respuesta de cada acción.
- Ajustes en `~/.config/arctis-control/state.json` (0600, escritura atómica), por auricular;
  al leerlos se descartan claves desconocidas y se acotan los valores.
- Binario con PIE, RELRO completo, `_FORTIFY_SOURCE`, protector de pila, CET y sin RUNPATH.
  `makepkg` usa las `CFLAGS` del sistema (en CachyOS pueden exigir la CPU del equipo que compila).

## Limitaciones

- HeadsetControl no permite leer la mayoría de ajustes del auricular: el panel muestra el último
  valor aplicado desde él (salvo el sidetone en los modelos que lo informan).
- El ecualizador paramétrico (Nova 3P, Nova 5) aún no está en la interfaz.
- ChatMix solo se muestra cuando la mezcla no está centrada.

## Licencia

GPL-3.0-or-later (ver `LICENSE`).

El icono usa el logo de SteelSeries (Wikimedia Commons, `File:Steelseries_logo.svg`),
recoloreado; es una marca registrada. Antes de publicar el repositorio conviene sustituirlo por
un icono propio. La ilustración del auricular (`assets/headset.svg`) es un dibujo propio.
