# OpenDash

OpenDash is a Qt-based infotainment center for your Linux OpenAuto installation!
The OpenDash project includes OpenAuto, AASDK, and Dash.

Main features of Dash include:

*	Embedded OpenAuto `Windowed/Fullscreen`
*	Wireless OpenAuto Capability
*	On-screen Volume, Brightness, & Theme Control
*	Responsive Scalable UI `Adjustable for screen size`
*	Bluetooth Media Control
*	Real-Time Vehicle OBD-II Data & SocketCAN Capabilities
*	Theming `Dark/Light mode` `Customizable RGB Accent Color`
*	Backlight brightness control (sysfs) or software dimming
*	Keyboard Shortcuts `GPIO Triggerable`

![](docs/imgs/opendash-ui.gif)

# Getting Started

This branch targets Debian 13 (trixie), Qt 6 and Wayland (labwc on Raspberry Pi OS).
aasdk, openauto and dash are expected side by side:

```
opendsh/
  aasdk/  openauto/  dash/
```

## Cross-build and deploy to a Raspberry Pi (recommended)

Needs Docker on the build machine only.

```
dash/docker/build.sh amd64 all      # native build + unit tests
dash/docker/deploy.sh pi@raspberrypi --install-packages
```

`deploy.sh` cross-builds for arm64 (`docker/build.sh arm64 all`), copies the staged
`bin/` and `lib/` to `/opt/opendsh` on the Pi, registers `/opt/opendsh/lib` with the
dynamic linker and installs the runtime packages listed by `docker/runtime-packages.sh`
(derived from the binaries' `NEEDED` entries plus run-time plugins; a snapshot is kept in
`docker/runtime-packages.txt`). `--no-build` deploys an existing `_stage/arm64`.

Then start it from the labwc session with `./autostart.sh`, or by hand:

```
QT_QPA_PLATFORM=wayland /opt/opendsh/bin/dash               # fullscreen
QT_QPA_PLATFORM=wayland /opt/opendsh/bin/dash --windowed 800x480
```

Settings live in `~/.config/openDsh/dash.conf`; openauto reads `openauto.ini` from the
working directory.

## Native build on Debian trixie

```
./install.sh                 # runtime + build packages, builds into /opt/opendsh
./install.sh --runtime       # only the runtime packages
```

## Adding a page

A page is a widget that also derives from `Page` (see `include/app/pages/page.hpp`):
construct the widgets in `init()`, then add an instance to the list in
`Session::Layout::Layout` (`src/app/session.cpp`). It gets a rail button, an entry in
Layout settings, a "Show ... Page" action and fullscreen support. CAN access goes through
`ICANBus` (`SocketCANBus::get_instance()`, `registerFrameHandler(id, callback)`); vehicle
plugins (`plugins/vehicle/`) receive the bus in `init()`.

Dialogs (`Dialog`) and the fullscreen button are drawn inside the main window, so pages
should not open top-level windows: under Wayland the compositor decides where those go.
