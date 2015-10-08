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
                icon_names << QStringLiteral("network-wireless-signal-excelent-symbolic") << QStringLiteral("network-wireless-signal-excelent")
                    << QStringLiteral("network-wireless-connected-100-symbolic") << QStringLiteral("network-wireless-connected-100");
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
