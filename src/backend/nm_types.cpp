#include "nm_types.h"

namespace nm
{

bool isVpnType(const QString &type)
{
    return type == QStringLiteral("vpn") || type == QStringLiteral("wireguard");
}

QString connectionTypeLabel(const QString &type)
{
    if (type == QStringLiteral("802-11-wireless")) {
        return QStringLiteral("wireless");
    }
    if (type == QStringLiteral("802-3-ethernet")) {
        return QStringLiteral("ethernet");
    }
    if (type == QStringLiteral("wireguard")) {
        return QStringLiteral("wireguard");
    }
    if (type == QStringLiteral("vpn")) {
        return QStringLiteral("vpn");
    }
    return type;
}

} // namespace nm
