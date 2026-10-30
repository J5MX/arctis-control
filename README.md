# nova5-panel

Panel gráfico local para los **SteelSeries Arctis Nova 5** en Linux, construido sobre
[HeadsetControl](https://github.com/Sapd/HeadsetControl).

- Batería (con aviso de color)
- Ecualizador: presets del auricular y curva personalizada de 10 bandas
- Limitador de volumen y apagado automático

## Uso

Desde el menú de aplicaciones: **Arctis Nova 5**. O a mano:

```
/usr/bin/python3 -I ~/workspace/nova5-panel/nova5_panel.py
```

Se abre en una ventana propia de Brave (o Chrome/Chromium, o el navegador por defecto) con un
perfil aparte. El servidor se cierra solo unos 2–3 minutos después de cerrar la ventana.
Cada vez que se abre el lanzador se crea una sesión nueva.

Con `--no-open` imprime la URL de un solo uso en lugar de abrir la ventana (útil para depurar).

## Seguridad

- **Sin red externa**: no hay CDNs, telemetría ni conexiones salientes; solo escucha en
  `127.0.0.1`, en un puerto aleatorio por sesión.
- **Autenticación por sesión**: el lanzador genera un código de un solo uso y se lo pasa a la
  ventana; el servidor lo canjea por una cookie `HttpOnly; SameSite=Strict` y lo invalida.
  Sin esa cookie el servidor responde 403 a todo, incluidos los archivos estáticos.
- **Contra webs maliciosas** (CSRF / DNS rebinding): se valida `Host` y `Origin`; los POST exigen
  `Content-Type: application/json` y la cabecera `X-Nova5-Panel`, lo que fuerza un preflight
  CORS que el servidor siempre rechaza.
- **Entrada validada**: lista blanca de acciones, tipos y rangos; cuerpo limitado a 4 KiB;
  `headsetcontrol` se llama con ruta absoluta y argumentos en lista (sin shell).
- **Cabeceras**: CSP estricta sin `unsafe-inline`, `frame-ancestors 'none'`, `nosniff`,
  `no-referrer`, `no-store`.
- **Aislamiento**: el servidor corre como servicio de usuario `nova5-panel-<id>` vía
  `systemd-run` con `ProtectSystem=strict`, `ProtectHome=read-only` (solo puede escribir en
  `~/.config/nova5-panel` y `$XDG_RUNTIME_DIR/nova5-panel`), `NoNewPrivileges`, seccomp,
  sin capacidades y `MemoryDenyWriteExecute`. Puntuación de `systemd-analyze --user security`: 1.8.
- **Ficheros privados**: estado en `~/.config/nova5-panel/state.json` (0600, escritura atómica);
  carpetas en 0700. Python se ejecuta con `-I` (ignora `PYTHONPATH` y paquetes de usuario).

Limitaciones conocidas:

- El cortafuegos IP de systemd (`IPAddressDeny`) no funciona en servicios de usuario sin root,
  así que no se usa; la ausencia de conexiones salientes depende del propio código.
- El código de un solo uso aparece en la línea de órdenes del navegador mientras la ventana está
  abierta, pero ya está gastado: no sirve para entrar.
- Cualquier programa que corra con tu mismo usuario podría usar `headsetcontrol` directamente,
  así que el panel no protege frente a eso (ni lo pretende).

## Detalles

- Requiere `headsetcontrol` (`sudo pacman -S headsetcontrol`) y systemd. Solo usa la librería
  estándar de Python.
- HeadsetControl no permite leer el ecualizador, el limitador ni el apagado automático del
  auricular, así que el panel guarda el último valor aplicado en el archivo de estado.
  Si se cambian desde otro sitio, el panel no se entera.
- El micrófono del auricular no aparece a propósito: se usa el HyperX SoloCast.
- Lanzador: `nova5-panel.desktop`, copiado en `~/.local/share/applications/`.
- Icono: `static/icon.svg` (logo de SteelSeries de Wikimedia Commons, recoloreado), también
  instalado como `~/.local/share/icons/hicolor/scalable/apps/nova5-panel.svg`.
- Ilustración del auricular: `static/headset.svg`, dibujo propio en SVG (sin fotos con copyright).
- Registro del servidor: `journalctl --user -u 'nova5-panel-*'`.
