# Linux Rebuild Guide

This guide is for rebuilding and running the current `nm-tray-alt` checkout on your Linux install so the latest local changes are the version actually running in the tray.

## Goal

Build the current source tree, run it once manually, then optionally install it so the rebuilt binary is what starts in your tray on login.

## 1. Open a Terminal in Linux

Change into this project directory on your Linux partition or shared drive:

```bash
cd /path/to/nm-tray-2
```

If you want to confirm you are on the edited checkout:

```bash
git status
git log --oneline -n 3
```

## 2. Install Build Dependencies

For Ubuntu or Debian:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake pkg-config \
  qt6-base-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools
```

Check the tools are available:

```bash
cmake --version
pkg-config --version
```

## 3. Build

Configure:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

Compile:

```bash
cmake --build build -j"$(nproc)"
```

The binary should end up here:

```bash
./build/nm-tray
```

## 4. Stop Any Existing Tray Instance

Before testing the rebuilt binary, stop any already-running instance so you do not end up with two tray icons:

```bash
pkill -f '/nm-tray($| )' || true
pkill -x nm-tray || true
```

If another NetworkManager tray applet is also running, stop that too before testing.

## 5. Run the Rebuilt Version Manually First

Start the freshly built binary directly from the project folder:

```bash
./build/nm-tray
```

What to test immediately:

1. The tray icon appears.
2. Your saved home Wi-Fi is shown in the popup.
3. The popup and tooltip agree about the active connection.
4. `Connection information` shows the new `Connect to this network automatically` checkbox for saved Wi-Fi profiles.
5. Disconnecting from the tray disconnects the current Wi-Fi cleanly.
6. Reconnecting from the popup works.

## 6. If You Want Debug Output While Testing

Run it from the terminal and leave that terminal open:

```bash
./build/nm-tray
```

If something still looks wrong, capture:

```bash
journalctl -u NetworkManager -b --no-pager | tail -n 200
nmcli device status
nmcli connection show
nmcli -f GENERAL.STATE,GENERAL.CONNECTION,IP4.ADDRESS,IP6.ADDRESS device show
```

## 7. Install It System-Wide

If the manual run looks good, install this build:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local \
  -DNM_TRAY_XDG_AUTOSTART_DIR=/etc/xdg/autostart

cmake --build build -j"$(nproc)"
sudo cmake --install build
```

This installs the binary to:

```bash
/usr/local/bin/nm-tray
```

## 8. Start the Installed Version

After install, either log out and back in, or start it manually:

```bash
/usr/local/bin/nm-tray &
```

To confirm which binary is being used:

```bash
which nm-tray
```

Expected result:

```bash
/usr/local/bin/nm-tray
```

## 9. If an Older Installed Copy Still Runs

Check for duplicate desktop autostarts or older copies earlier in `PATH`:

```bash
which -a nm-tray
ls -l /etc/xdg/autostart /usr/local/share/applications /usr/share/applications 2>/dev/null | grep nm-tray
```

You want the rebuilt `/usr/local/bin/nm-tray` to be the one that launches.

## 10. Fast Rebuild After More Edits

If you change code again later:

```bash
cd /path/to/nm-tray-2
cmake --build build -j"$(nproc)"
pkill -x nm-tray || true
./build/nm-tray
```

## Notes

- `nm-tray-alt` should follow NetworkManager, not replace its policy.
- The tray now exposes the saved profile autoconnect setting instead of inventing its own reconnect behavior.
- The disconnect action now uses NetworkManager's device disconnect for normal device-backed links, so disconnecting Wi-Fi should be temporary and user-controlled.
