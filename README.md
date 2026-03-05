# nm-tray-alt

`nm-tray-alt` is a usability-focused fork of `nm-tray`.

It keeps the lightweight Qt tray UI, but replaces the old backend integration with a direct `QtDBus` + NetworkManager model/cache pipeline.

## What Changed In This Fork

- Direct `QtDBus` backend (no `NetworkManagerQt` dependency)
- C++23 build
- Event-driven refresh with coalescing/debounce
- Better Wi-Fi menu behavior:
  - practical sorting
  - low-signal toggle
  - hidden/noise filtering
- Cleaner popup UX:
  - connected status at top
  - one-click disconnect from top status line
  - recent connections list
  - connection details/usage shortcut
- Better reconnect handling for saved Wi-Fi profiles
- Rebranded app text to `nm-tray-alt`

## License

This project remains licensed under **GNU GPLv2-or-later**.

## Build (Ubuntu/Debian Example)

Install dependencies:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake pkg-config \
  qt6-base-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools
```

Configure + build:

```bash
git clone https://github.com/LegeApp/nm-tray-alt.git
cd nm-tray-alt
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

Run without installing:

```bash
./build/nm-tray
```

## Install System-Wide

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local \
  -DNM_TRAY_XDG_AUTOSTART_DIR=/etc/xdg/autostart
cmake --build build -j"$(nproc)"
sudo cmake --install build
```

This installs:

- binary: `/usr/local/bin/nm-tray`
- desktop entry: `/usr/local/share/applications/nm-tray.desktop`
- autostart entry: `/etc/xdg/autostart/nm-tray-autostart.desktop`

## Notes

- Networking is still managed by NetworkManager (`network-manager` service).
- If another tray applet is already autostarting, disable it to avoid duplicate tray icons.
- `Edit connections...` tries multiple launch fallbacks (`nm-connection-editor`, terminal + `nmtui-edit`).

