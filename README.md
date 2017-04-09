# nm-tray

**nm-tray** is a simple [NetworkManager](https://wiki.gnome.org/Projects/NetworkManager) front end with information icon residing in system tray (like e.g. nm-applet).
It's a pure Qt application. For interaction with *NetworkManager* it uses API provided by [**KF5::NetworkManagerQt**](https://projects.kde.org/projects/frameworks/networkmanager-qt) -> plain DBus communication.

**! This is work in progress and there are still many TODOs and debugs all around in the code !**

## License

This software is licensed under [GNU GPLv2 or later](https://www.gnu.org/licenses/old-licenses/gpl-2.0.html)

## Build example

    git clone https://github.com/palinek/nm-tray.git
    cd nm-tray
    mkdir build
    cd build
    cmake ..
    make
    ./nm-tray

## Packages

For [arch users](https://www.archlinux.org/) there is an AUR packge [nm-tray-git](https://aur.archlinux.org/packages/nm-tray-git/) (thanks to [pmattern](https://github.com/pmattern)).

For [openSUSE users](https://www.opensuse.org/) there is a [package](https://build.opensuse.org/package/show/X11:LXQt:git/nm-tray) in the [X11:LXQt:git](https://build.opensuse.org/project/show/X11:LXQt:git) devel project of OBS.
