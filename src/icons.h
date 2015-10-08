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
    };

    QIcon getIcon(Icon ico);
    Icon wifiSignalIcon(const int signal);
}

#endif
