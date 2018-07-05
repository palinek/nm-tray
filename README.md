# nm-tray

**nm-tray** is a simple [NetworkManager](https://wiki.gnome.org/Projects/NetworkManager) front end with information icon residing in system tray (like e.g. nm-applet).
It's a pure Qt application. For interaction with *NetworkManager* it uses API provided by [**KF5::NetworkManagerQt**](https://projects.kde.org/projects/frameworks/networkmanager-qt) -> plain DBus communication.

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

For [arch users](https://www.archlinux.org/) there is an AUR package [nm-tray-git](https://aur.archlinux.org/packages/nm-tray-git/) (thanks to [pmattern](https://github.com/pmattern)).

nm-tray is in the official repository of [openSUSE](https://www.opensuse.org/) since Leap 15.0. There is a also a [git package](https://build.opensuse.org/package/show/X11:LXQt:git/nm-tray) in the [X11:LXQt:git](https://build.opensuse.org/project/show/X11:LXQt:git) devel project of OBS.

## Translations

Thanks to [Weblate](https://weblate.org/) anyone can help us to localize nm-tray by using the [hosted weblate service](https://hosted.weblate.org/projects/nm-tray/translations/).
