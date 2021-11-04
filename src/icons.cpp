/*COPYRIGHT_HEADER

This file is a part of nm-tray.

Copyright (c)
    2015~now Palo Kisa <palo.kisa@gmail.com>

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
#include <QStringBuilder>

namespace icons
{
    namespace
    {
        class FallbackIcon : public QIcon
        {
        public:
            FallbackIcon()
            {
                static const QIcon fallback{QStringLiteral(":/images/default.svg")};
                // Note: using pixmap icon as the vector/svg one is not correctly transferred via the
                // D-Bus StatusNotifierItem interface
                addPixmap(fallback.pixmap({16, 16}));
                addPixmap(fallback.pixmap({32, 32}));
                addPixmap(fallback.pixmap({64, 64}));
                addPixmap(fallback.pixmap({128, 128}));
                addPixmap(fallback.pixmap({256, 256}));
            }
        };
    }

    QIcon getIcon(Icon ico, bool useSymbolic)
    {
        // final, bundled fallback
        static const FallbackIcon fallback;
        static const QStringList i_empty;
        QStringList const * icon_names = &i_empty;
        switch (ico)
        {
            case NETWORK_OFFLINE:
                static const QStringList i_network_offline = { QStringLiteral("network-offline") };
                icon_names = &i_network_offline;
                break;
            case NETWORK_WIRED:
                static const QStringList i_network_wired = { QStringLiteral("network-wired") };
                icon_names = &i_network_wired;
                break;
            case NETWORK_WIRED_DISCONNECTED:
                static const QStringList i_network_wired_disconnected = { QStringLiteral("network-wired-disconnected") };
                icon_names = &i_network_wired_disconnected;
                break;
            case NETWORK_WIFI_DISCONNECTED:
                static const QStringList i_wifi_disconnected = { QStringLiteral("network-wireless-disconnected") };
                icon_names = &i_wifi_disconnected;
                break;
            case NETWORK_WIFI_ACQUIRING:
                static const QStringList i_wifi_acquiring = { QStringLiteral("network-wireless-acquiring") };
                icon_names = &i_wifi_acquiring;
                break;
            case NETWORK_WIFI_NONE:
                static const QStringList i_wifi_none = { QStringLiteral("network-wireless-signal-none"), QStringLiteral("network-wireless-connected-00") };
                icon_names = &i_wifi_none;
                break;
            case NETWORK_WIFI_WEAK:
                static const QStringList i_wifi_weak = { QStringLiteral("network-wireless-signal-weak"), QStringLiteral("network-wireless-connected-25") };
                icon_names = &i_wifi_weak;
                break;
            case NETWORK_WIFI_OK:
                static const QStringList i_wifi_ok = { QStringLiteral("network-wireless-signal-ok"), QStringLiteral("network-wireless-connected-50") };
                icon_names = &i_wifi_ok;
                break;
            case NETWORK_WIFI_GOOD:
                static const QStringList i_wifi_good = { QStringLiteral("network-wireless-signal-good"),  QStringLiteral("network-wireless-connected-75") };
                icon_names = &i_wifi_good;
                break;
            case NETWORK_WIFI_EXCELENT:
                static const QStringList i_wifi_excelent = { QStringLiteral("network-wireless-signal-excellent"),  QStringLiteral("network-wireless-connected-100") };
                icon_names = &i_wifi_excelent;
                break;
            case NETWORK_VPN:
                static const QStringList i_network_vpn = { QStringLiteral("network-vpn") };
                icon_names = &i_network_vpn;
                break;
            case SECURITY_LOW:
                static const QStringList i_security_low = { QStringLiteral("security-low") };
                icon_names = &i_security_low;
                break;
            case SECURITY_HIGH:
                static const QStringList i_security_high = { QStringLiteral("security-high") };
                icon_names = &i_security_high;
                break;
            case PREFERENCES_NETWORK:
                static const QStringList i_preferences_network = { QStringLiteral("preferences-system-network") };
                icon_names = &i_preferences_network;
                break;
        };
        for (auto const & name : *icon_names)
        {
            QIcon icon{QIcon::fromTheme(useSymbolic ? name % QStringLiteral("-symbolic") : name)};
            if (!icon.isNull())
                return icon;
        }
        return QIcon::fromTheme(QStringLiteral("network-transmit"), fallback);
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
