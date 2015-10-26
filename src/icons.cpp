/*COPYRIGHT_HEADER

This file is a part of nm-tray.

Copyright (c)
    2015 Palo Kisa <palo.kisa@gmail.com>

nm-tray is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

COPYRIGHT_HEADER*/
#include "icons.h"

#include <QStringList>
#include <QDebug>

namespace icons
{
    QIcon getIcon(Icon ico)
    {
        QStringList icon_names;
        switch (ico)
        {
            case NETWORK_OFFLINE:
                icon_names << QStringLiteral("network-offline");
                break;
            case NETWORK_WIRED:
                icon_names << QStringLiteral("network-wired-symbolic") << QStringLiteral("network-wired");
                break;
            case NETWORK_WIRED_DISCONNECTED:
                icon_names << QStringLiteral("network-wired-disconnected-symbolic") << QStringLiteral("network-wired-disconnected");
                break;
            case NETWORK_WIFI_DISCONNECTED:
                icon_names << QStringLiteral("network-wireless-disconnected-symbolic") << QStringLiteral("network-wireless-disconnected");
                break;
            case NETWORK_WIFI_ACQUIRING:
                icon_names << QStringLiteral("network-wireless-acquiring-symbolic") << QStringLiteral("network-wireless-acquiring");
                break;
            case NETWORK_WIFI_NONE:
                icon_names << QStringLiteral("network-wireless-signal-none-symbolic") << QStringLiteral("network-wireless-signal-none")
                    << QStringLiteral("network-wireless-connected-00-symbolic") << QStringLiteral("network-wireless-connected-00");
                break;
            case NETWORK_WIFI_WEAK:
                icon_names << QStringLiteral("network-wireless-signal-weak-symbolic") << QStringLiteral("network-wireless-signal-weak")
                    << QStringLiteral("network-wireless-connected-25-symbolic") << QStringLiteral("network-wireless-connected-25");
                break;
            case NETWORK_WIFI_OK:
                icon_names << QStringLiteral("network-wireless-signal-ok-symbolic") << QStringLiteral("network-wireless-signal-ok")
                    << QStringLiteral("network-wireless-connected-50-symbolic") << QStringLiteral("network-wireless-connected-50");
                break;
            case NETWORK_WIFI_GOOD:
                icon_names << QStringLiteral("network-wireless-signal-good-symbolic") << QStringLiteral("network-wireless-signal-good")
                    << QStringLiteral("network-wireless-connected-75-symbolic") << QStringLiteral("network-wireless-connected-75");
                break;
            case NETWORK_WIFI_EXCELENT:
                icon_names << QStringLiteral("network-wireless-signal-excellent-symbolic") << QStringLiteral("network-wireless-signal-excellent")
                    << QStringLiteral("network-wireless-connected-100-symbolic") << QStringLiteral("network-wireless-connected-100");
                break;
            case PREFERENCES_NETWORK:
                icon_names << QStringLiteral("preferences-system-network");
                break;
        };
        for (auto const & name : icon_names)
        {
            QIcon icon{QIcon::fromTheme(name)};
            if (!icon.isNull())
                return icon;
        }
        //TODO: fallback!?!
        return QIcon::fromTheme(QStringLiteral("preferences-system-network"));
    }

    Icon wifiSignalIcon(const int signal)
    {
        if (0 >= signal)
            return icons::NETWORK_WIFI_NONE;
        else if (25 >= signal)
            return icons::NETWORK_WIFI_WEAK;
        else if (50 >= signal)
            return icons::NETWORK_WIFI_OK;
        else if (75 >= signal)
            return icons::NETWORK_WIFI_GOOD;
        else
            return icons::NETWORK_WIFI_EXCELENT;
    }
}
