# nova5-panel

Panel gráfico no oficial para los **SteelSeries Arctis Nova 5** en Linux, construido sobre
[HeadsetControl](https://github.com/Sapd/HeadsetControl). No está afiliado a SteelSeries.

- Batería (con aviso de color)
- Ecualizador: presets del auricular y curva personalizada de 10 bandas
- Limitador de volumen y apagado automático

Aplicación nativa en C++/Qt6 (QML). Sin navegador, sin servidor y sin conexiones de red.

## Instalación (Arch / CachyOS)

```
cd packaging
makepkg -si
```

Instala `/usr/bin/nova5-panel`, el lanzador **Arctis Nova 5** y su icono. Dependencias:
`headsetcontrol`, `qt6-base`, `qt6-declarative`, `qt6-svg` (y `cmake` para compilar).
Opcional: `ttf-fira-sans` para la tipografía.

Desinstalar: `sudo pacman -R nova5-panel`.

## Compilar a mano

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/nova5-panel
```

`--screenshot archivo.png` guarda una captura y sale (con `QT_QPA_PLATFORM=offscreen` no abre
ventana); útil para revisar cambios de interfaz.

## Estructura

| Ruta | Contenido |
|---|---|
| `src/` | `HeadsetController`: ejecuta `headsetcontrol` y expone el estado a QML |
| `qml/` | Interfaz: ventana, tarjetas, botones, anillo de batería, bandas del EQ |
| `assets/` | Icono y la ilustración del auricular |
| `packaging/` | `PKGBUILD` y lanzador `.desktop` |
| `web/` | Versión anterior (servidor Python local + navegador), sustituida por la nativa |

## Seguridad

- No abre sockets ni hace conexiones de red: no hay nada que atacar desde fuera. Además, el
  motor QML usa un gestor de red que rechaza cualquier URL que no sea un recurso interno (qrc).
- Todos los textos se muestran como texto plano (`Text.PlainText`): un mensaje con HTML, por
  ejemplo un `<img src="http://…">` en un error de `headsetcontrol`, no se interpreta ni carga nada.
- `headsetcontrol` se ejecuta con ruta absoluta (`/usr/bin/headsetcontrol`), argumentos en
  lista (sin shell), entorno mínimo y un tiempo límite de 20 s; las órdenes van de una en una.
  Si su salida supera 1 MiB se corta y se descarta.
- Todos los valores se validan (lista cerrada de acciones, tipos y rangos) antes de llamarlo.
- La interfaz QML va compilada dentro del binario; no carga archivos externos.
- El estado se guarda en `~/.config/nova5-panel/state.json` (0600, escritura atómica) y al
  leerlo se acotan los valores. Es el mismo formato que usaba la versión web.
- El paquete se compila con las opciones de endurecimiento de `makepkg` (PIE, RELRO completo,
  `_FORTIFY_SOURCE`, protector de pila, CET) y sin RUNPATH.
- Al compilar con `makepkg` se usan las `CFLAGS` del sistema: en CachyOS pueden exigir una CPU
  concreta (p. ej. x86-64-v4). Para otro equipo, compila el paquete en ese equipo.

## Limitaciones

- HeadsetControl no permite leer el ecualizador, el limitador ni el apagado automático del
  auricular: el panel muestra el último valor aplicado desde él.
- Las frecuencias de las bandas son orientativas.
- El micrófono del auricular no aparece a propósito.

## Créditos

- El icono usa el logo de SteelSeries (Wikimedia Commons, `File:Steelseries_logo.svg`),
  recoloreado. Es una marca registrada de SteelSeries; si el repositorio se hace público,
  conviene sustituirlo por un icono propio.
- La ilustración del auricular (`assets/headset.svg`) es un dibujo propio.
- Licencia: pendiente de decidir (`license=('custom')` en el PKGBUILD).
