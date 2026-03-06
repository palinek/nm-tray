#ifndef NM_CACHE_H
#define NM_CACHE_H

#include "nm_types.h"

namespace nm
{

struct WifiViewRecord
{
    QString apPath;
    QString ssid;
    QString devicePath;
    int strength = 0;
    bool secure = false;
    bool active = false;
    QString savedConnectionPath;
    bool autoconnect = false;
    int autoconnectPriority = 0;
    qint64 lastUsedTimestamp = 0;
    bool stale = false;
};

struct ConnectionViewRecord
{
    QString connectionPath;
    QString id;
    QString uuid;
    QString type;
    bool active = false;
    bool autoconnect = false;
    bool stale = false;
    qint64 lastUsedTimestamp = 0;
};

class NmCache
{
public:
    void setSnapshot(const Snapshot &snapshot);

    const Snapshot &snapshot() const;
    QList<ActiveConnectionRecord> activeConnections() const;
    QList<WifiViewRecord> wifiEntries(bool hideStale) const;
    QList<ConnectionViewRecord> knownConnections(bool hideStale) const;
    QList<DeviceRecord> devices() const;

    QString activeConnectionForSettingsPath(const QString &connectionPath) const;
    QString connectionPathForUuid(const QString &uuid) const;
    QString connectionPathForSsid(const QString &ssid) const;
    QString findWifiDeviceForAp(const QString &apPath) const;

private:
    bool isConnectionStale(const SavedConnectionRecord &conn) const;

    Snapshot mSnapshot;
};

} // namespace nm

#endif
