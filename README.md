# nm-tray

**nm-tray** is a simple [NetworkMamanger](https://wiki.gnome.org/Projects/NetworkManager) front end with information icon residing in system tray (like e.g. nm-applet).
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
