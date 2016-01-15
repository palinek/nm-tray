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
#if !defined(ICONS_H)
#define ICONS_H

#include <QIcon>

namespace icons
{
    enum Icon {
        NETWORK_OFFLINE
            , NETWORK_WIRED
            , NETWORK_WIRED_DISCONNECTED
            , NETWORK_WIFI_ACQUIRING
            , NETWORK_WIFI_NONE
            , NETWORK_WIFI_WEAK
            , NETWORK_WIFI_OK
            , NETWORK_WIFI_GOOD
            , NETWORK_WIFI_EXCELENT
            , NETWORK_WIFI_DISCONNECTED

            , PREFERENCES_NETWORK
    };

    QIcon getIcon(Icon ico);
    Icon wifiSignalIcon(const int signal);
}

#endif
