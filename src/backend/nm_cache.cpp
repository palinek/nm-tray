#include "nm_cache.h"

#include <QDateTime>
#include <algorithm>

namespace
{
constexpr qint64 kStaleSeconds = 30LL * 24LL * 60LL * 60LL;
const QString kHiddenSsid = QStringLiteral("<hidden>");

bool hasConcreteSsid(const QString &ssid)
{
    return !ssid.isEmpty() && ssid != kHiddenSsid;
}

QString wifiGroupingKey(const nm::AccessPointRecord &ap)
{
    if (!ap.ssidBytes.isEmpty()) {
        return ap.devicePath + QLatin1Char('\x1f') + QString::fromLatin1(ap.ssidBytes.toBase64());
    }
    if (!ap.bssid.isEmpty()) {
        return ap.devicePath + QLatin1Char('\x1f') + ap.bssid.toLower();
    }
    return ap.devicePath + QLatin1Char('\x1f') + ap.path;
}

bool isUserFacingConnectionType(const QString &type)
{
    return type == QStringLiteral("802-11-wireless")
        || type == QStringLiteral("802-3-ethernet")
        || type == QStringLiteral("bluetooth");
}

} // namespace

namespace nm
{

void NmCache::setSnapshot(const Snapshot &snapshot)
{
    mSnapshot = snapshot;
}

const Snapshot &NmCache::snapshot() const
{
    return mSnapshot;
}

QList<ActiveConnectionRecord> NmCache::activeConnections() const
{
    QList<ActiveConnectionRecord> out;
    out.reserve(mSnapshot.activeConnections.size());
    for (const auto &it : mSnapshot.activeConnections) {
        if (!isUserFacingConnectionType(it.type)) {
            continue;
        }
        out.push_back(it);
    }
    return out;
}

QList<WifiViewRecord> NmCache::wifiEntries(bool hideStale) const
{
    QMap<QString, WifiViewRecord> merged;
    for (const auto &ap : mSnapshot.accessPoints) {
        WifiViewRecord item;
        item.apPath = ap.path;
        item.ssid = ap.ssid;
        if (!hasConcreteSsid(item.ssid)) {
            continue;
        }
        item.devicePath = ap.devicePath;
        item.strength = ap.strength;
        item.secure = ap.privacy || ap.wpaFlags != 0 || ap.rsnFlags != 0;

        const SavedConnectionRecord *bestConn = nullptr;
        for (const auto &conn : mSnapshot.savedConnections) {
            if (conn.type != QStringLiteral("802-11-wireless")) {
                continue;
            }
            const bool connHasSsidBytes = !conn.wifiSsidBytes.isEmpty();
            if (connHasSsidBytes) {
                if (conn.wifiSsidBytes != ap.ssidBytes) {
                    continue;
                }
            } else if (hasConcreteSsid(conn.wifiSsid)) {
                if (conn.wifiSsid != item.ssid) {
                    continue;
                }
            } else if (conn.id != item.ssid) {
                continue;
            }
            if (bestConn == nullptr
                || conn.autoconnectPriority > bestConn->autoconnectPriority
                || (conn.autoconnectPriority == bestConn->autoconnectPriority && conn.timestamp > bestConn->timestamp)) {
                bestConn = &conn;
            }
        }
        if (bestConn != nullptr) {
            item.savedConnectionPath = bestConn->path;
            item.autoconnectPriority = bestConn->autoconnectPriority;
            item.lastUsedTimestamp = bestConn->timestamp;
            item.stale = isConnectionStale(*bestConn);
        }

        const auto deviceIt = mSnapshot.devices.find(ap.devicePath);
        if (deviceIt != mSnapshot.devices.end()) {
            item.active = deviceIt->activeAccessPointPath == ap.path;
        }

        const QString key = wifiGroupingKey(ap);
        auto it = merged.find(key);
        if (it == merged.end()) {
            merged.insert(key, item);
            continue;
        }

        // Keep one deterministic entry per (device, SSID): prefer active AP, then strongest signal.
        const bool replace = (item.active && !it->active) || (item.active == it->active && item.strength > it->strength);
        if (replace) {
            item.savedConnectionPath = it->savedConnectionPath.isEmpty() ? item.savedConnectionPath : it->savedConnectionPath;
            item.autoconnectPriority = std::max(item.autoconnectPriority, it->autoconnectPriority);
            item.lastUsedTimestamp = std::max(item.lastUsedTimestamp, it->lastUsedTimestamp);
            item.stale = item.savedConnectionPath.isEmpty() ? item.stale : it->stale;
            *it = item;
        } else if (it->savedConnectionPath.isEmpty() && !item.savedConnectionPath.isEmpty()) {
            it->savedConnectionPath = item.savedConnectionPath;
            it->autoconnectPriority = item.autoconnectPriority;
            it->lastUsedTimestamp = item.lastUsedTimestamp;
            it->stale = item.stale;
        } else if (!item.savedConnectionPath.isEmpty() && item.savedConnectionPath == it->savedConnectionPath) {
            it->autoconnectPriority = std::max(it->autoconnectPriority, item.autoconnectPriority);
            it->lastUsedTimestamp = std::max(it->lastUsedTimestamp, item.lastUsedTimestamp);
        }
    }

    QList<WifiViewRecord> out;
    out.reserve(merged.size());
    for (const auto &item : merged) {
        if (hideStale && item.stale && !item.active) {
            continue;
        }
        out.push_back(item);
    }

    std::sort(out.begin(), out.end(), [](const WifiViewRecord &a, const WifiViewRecord &b) {
        if (a.active != b.active) {
            return a.active;
        }
        if (a.strength != b.strength) {
            return a.strength > b.strength;
        }
        if ((a.savedConnectionPath.isEmpty()) != (b.savedConnectionPath.isEmpty())) {
            return !a.savedConnectionPath.isEmpty();
        }
        if (a.autoconnectPriority != b.autoconnectPriority) {
            return a.autoconnectPriority > b.autoconnectPriority;
        }
        if (a.lastUsedTimestamp != b.lastUsedTimestamp) {
            return a.lastUsedTimestamp > b.lastUsedTimestamp;
        }
        return QString::compare(a.ssid, b.ssid, Qt::CaseInsensitive) < 0;
    });
    return out;
}

QList<ConnectionViewRecord> NmCache::knownConnections(bool hideStale) const
{
    QList<ConnectionViewRecord> out;
    for (const auto &conn : mSnapshot.savedConnections) {
        if (!isUserFacingConnectionType(conn.type)) {
            continue;
        }
        if (conn.id == kHiddenSsid) {
            continue;
        }
        ConnectionViewRecord item;
        item.connectionPath = conn.path;
        item.id = conn.id;
        item.uuid = conn.uuid;
        item.type = conn.type;
        item.stale = isConnectionStale(conn);
        item.lastUsedTimestamp = conn.timestamp;

        for (const auto &active : mSnapshot.activeConnections) {
            if (active.connectionPath == conn.path || (!conn.uuid.isEmpty() && active.uuid == conn.uuid)) {
                item.active = true;
                break;
            }
        }

        if (hideStale && item.stale && !item.active) {
            continue;
        }
        out.push_back(item);
    }

    std::sort(out.begin(), out.end(), [](const ConnectionViewRecord &a, const ConnectionViewRecord &b) {
        if (a.active != b.active) {
            return a.active;
        }
        if (a.stale != b.stale) {
            return !a.stale;
        }
        return QString::compare(a.id, b.id, Qt::CaseInsensitive) < 0;
    });
    return out;
}

QList<DeviceRecord> NmCache::devices() const
{
    QList<DeviceRecord> out;
    out.reserve(mSnapshot.devices.size());
    for (const auto &it : mSnapshot.devices) {
        out.push_back(it);
    }
    std::sort(out.begin(), out.end(), [](const DeviceRecord &a, const DeviceRecord &b) {
        return QString::compare(a.interfaceName, b.interfaceName, Qt::CaseInsensitive) < 0;
    });
    return out;
}

QString NmCache::activeConnectionForSettingsPath(const QString &connectionPath) const
{
    for (const auto &active : mSnapshot.activeConnections) {
        if (active.connectionPath == connectionPath) {
            return active.path;
        }
    }
    return {};
}

QString NmCache::connectionPathForUuid(const QString &uuid) const
{
    for (const auto &conn : mSnapshot.savedConnections) {
        if (conn.uuid == uuid) {
            return conn.path;
        }
    }
    return {};
}

QString NmCache::connectionPathForSsid(const QString &ssid) const
{
    for (const auto &conn : mSnapshot.savedConnections) {
        if ((hasConcreteSsid(conn.wifiSsid) && conn.wifiSsid == ssid) || conn.id == ssid) {
            return conn.path;
        }
    }
    return {};
}

QString NmCache::findWifiDeviceForAp(const QString &apPath) const
{
    const auto it = mSnapshot.accessPoints.find(apPath);
    if (it == mSnapshot.accessPoints.end()) {
        return {};
    }
    return it->devicePath;
}

bool NmCache::isConnectionStale(const SavedConnectionRecord &conn) const
{
    if (conn.timestamp <= 0) {
        return true;
    }
    const qint64 now = QDateTime::currentSecsSinceEpoch();
    return conn.timestamp <= (now - kStaleSeconds);
}

} // namespace nm
