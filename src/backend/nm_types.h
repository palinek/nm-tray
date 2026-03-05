#ifndef NM_TYPES_H
#define NM_TYPES_H

#include <QDateTime>
#include <QList>
#include <QMap>
#include <QString>

namespace nm
{

enum class DeviceType : uint32_t
{
    Unknown = 0,
    Ethernet = 1,
    Wifi = 2,
};

enum class ActiveState : uint32_t
{
    Unknown = 0,
    Activating = 1,
    Activated = 2,
    Deactivating = 3,
    Deactivated = 4,
};

struct AccessPointRecord
{
    QString path;
    QString devicePath;
    QString ssid;
    int strength = 0;
    uint32_t flags = 0;
    uint32_t wpaFlags = 0;
    uint32_t rsnFlags = 0;
    uint32_t frequency = 0;
    bool privacy = false;
};

struct DeviceRecord
{
    QString path;
    QString interfaceName;
    QString ipInterfaceName;
    DeviceType type = DeviceType::Unknown;
    uint32_t state = 0;
    QString activeConnectionPath;
    QString activeAccessPointPath;
    QList<QString> accessPointPaths;
    QString hardwareAddress;
    int bitrateKbps = -1;
};

struct SavedConnectionRecord
{
    QString path;
    QString id;
    QString wifiSsid;
    QString uuid;
    QString type;
    QString interfaceName;
    qint64 timestamp = 0;
    int autoconnectPriority = 0;
    bool autoconnect = true;
};

struct ActiveConnectionRecord
{
    QString path;
    QString connectionPath;
    QString specificObjectPath;
    QString id;
    QString uuid;
    QString type;
    QList<QString> devices;
    ActiveState state = ActiveState::Unknown;
    bool isVpn = false;
    bool isDefault4 = false;
    bool isDefault6 = false;
    QString ip4ConfigPath;
    QString ip6ConfigPath;
};

struct ManagerState
{
    bool networkingEnabled = false;
    bool wirelessEnabled = false;
    bool wirelessHardwareEnabled = false;
    QString primaryConnectionPath;
    uint32_t state = 0;
    uint32_t connectivity = 0;
    QString lastError;
};

struct Snapshot
{
    ManagerState manager;
    QMap<QString, DeviceRecord> devices;
    QMap<QString, AccessPointRecord> accessPoints;
    QMap<QString, SavedConnectionRecord> savedConnections;
    QMap<QString, ActiveConnectionRecord> activeConnections;
    QDateTime collectedAt;
};

bool isVpnType(const QString &type);
QString connectionTypeLabel(const QString &type);

} // namespace nm

#endif
