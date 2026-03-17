#include "nmmodel.h"

#include "backend/nm_actions.h"
#include "icons.h"
#include "log.h"
#include "wifi_password_dialog.h"

#include <QLocale>
#include <QMetaObject>
#include <QMetaType>
#include <QApplication>
#include <algorithm>

namespace
{
int connTypeToInt(const QString &type)
{
    if (type == QStringLiteral("802-11-wireless")) {
        return 1;
    }
    if (type == QStringLiteral("802-3-ethernet")) {
        return 2;
    }
    if (nm::isVpnType(type)) {
        return 3;
    }
    return 4;
}

bool isWirelessType(const QString &type)
{
    return type == QStringLiteral("802-11-wireless");
}

QString formatBytes(qulonglong bytes)
{
    static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    return QLocale::system().toString(value, 'f', value >= 100.0 ? 0 : (value >= 10.0 ? 1 : 2))
        + QLatin1Char(' ')
        + QLatin1String(units[unit]);
}

NmModel::OverallState toOverallState(uint nmState, uint connectivity)
{
    // NM states: 20 disconnected, 40 connecting, 50 connected(local), 60 connected(site), 70 connected(global)
    if (nmState == 40) {
        return NmModel::OverallState::Connecting;
    }
    if (nmState == 50 || nmState == 60 || nmState == 70) {
        if (connectivity == 1 || connectivity == 2 || connectivity == 3) {
            return NmModel::OverallState::Limited;
        }
        return NmModel::OverallState::Connected;
    }
    return NmModel::OverallState::Disconnected;
}

bool managerStateEquivalent(const NmModel::ManagerState &a, const NmModel::ManagerState &b)
{
    return a.overallState == b.overallState
        && a.primaryKind == b.primaryKind
        && a.primaryName == b.primaryName
        && a.wifiStrength == b.wifiStrength
        && a.vpnActive == b.vpnActive
        && a.lastError == b.lastError
        && a.networkingEnabled == b.networkingEnabled
        && a.wirelessEnabled == b.wirelessEnabled
        && a.wirelessHardwareEnabled == b.wirelessHardwareEnabled
        && a.primaryConnectionPath == b.primaryConnectionPath;
}

bool activeEquivalent(const nm::ActiveConnectionRecord &a, const nm::ActiveConnectionRecord &b)
{
    return a.path == b.path
        && a.connectionPath == b.connectionPath
        && a.specificObjectPath == b.specificObjectPath
        && a.id == b.id
        && a.uuid == b.uuid
        && a.type == b.type
        && a.devices == b.devices
        && a.state == b.state
        && a.isVpn == b.isVpn
        && a.isDefault4 == b.isDefault4
        && a.isDefault6 == b.isDefault6
        && a.ip4ConfigPath == b.ip4ConfigPath
        && a.ip6ConfigPath == b.ip6ConfigPath
        && a.ip4Addresses == b.ip4Addresses
        && a.ip6Addresses == b.ip6Addresses
        && a.ip4Gateway == b.ip4Gateway
        && a.ip6Gateway == b.ip6Gateway
        && a.ip4Dns == b.ip4Dns
        && a.ip6Dns == b.ip6Dns
        && a.ip4RouteCount == b.ip4RouteCount
        && a.ip6RouteCount == b.ip6RouteCount;
}

bool connectionEquivalent(const nm::ConnectionViewRecord &a, const nm::ConnectionViewRecord &b)
{
    return a.connectionPath == b.connectionPath
        && a.id == b.id
        && a.uuid == b.uuid
        && a.type == b.type
        && a.active == b.active
        && a.autoconnect == b.autoconnect
        && a.stale == b.stale
        && a.lastUsedTimestamp == b.lastUsedTimestamp;
}

bool deviceEquivalent(const nm::DeviceRecord &a, const nm::DeviceRecord &b)
{
    return a.path == b.path
        && a.interfaceName == b.interfaceName
        && a.ipInterfaceName == b.ipInterfaceName
        && a.type == b.type
        && a.state == b.state
        && a.activeConnectionPath == b.activeConnectionPath
        && a.activeAccessPointPath == b.activeAccessPointPath
        && a.accessPointPaths == b.accessPointPaths
        && a.hardwareAddress == b.hardwareAddress
        && a.bitrateKbps == b.bitrateKbps
        && a.rxBytes == b.rxBytes
        && a.txBytes == b.txBytes;
}

bool wifiEquivalent(const nm::WifiViewRecord &a, const nm::WifiViewRecord &b)
{
    return a.apPath == b.apPath
        && a.ssid == b.ssid
        && a.devicePath == b.devicePath
        && a.strength == b.strength
        && a.secure == b.secure
        && a.active == b.active
        && a.savedConnectionPath == b.savedConnectionPath
        && a.autoconnect == b.autoconnect
        && a.autoconnectPriority == b.autoconnectPriority
        && a.lastUsedTimestamp == b.lastUsedTimestamp
        && a.stale == b.stale;
}

} // namespace

NmModel::NmModel(QObject *parent)
    : QAbstractItemModel(parent)
{
    qRegisterMetaType<nm::Snapshot>();

    mDbus = new nm::NmDbusClient;
    mDbus->moveToThread(&mDbusThread);
    connect(&mDbusThread, &QThread::finished, mDbus, &QObject::deleteLater);
    connect(mDbus, &nm::NmDbusClient::snapshotChanged, this, &NmModel::onSnapshotChanged, Qt::QueuedConnection);

    mDbusThread.start();
    QMetaObject::invokeMethod(mDbus, "start", Qt::QueuedConnection);

    rebuildFromSnapshot(nm::Snapshot{});
}

NmModel::~NmModel()
{
    if (mDbusThread.isRunning()) {
        mDbusThread.quit();
        mDbusThread.wait();
    }
}

int NmModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid()) {
        return 1;
    }

    const ItemId id = static_cast<ItemId>(parent.internalId());
    if (id == ITEM_ROOT) {
        return 4;
    }
    if (id == ITEM_ACTIVE) {
        return mActive.size();
    }
    if (id == ITEM_CONNECTION) {
        return mConnections.size();
    }
    if (id == ITEM_DEVICE) {
        return mDevices.size();
    }
    if (id == ITEM_WIFINET) {
        return mWifi.size();
    }
    return 0;
}

int NmModel::columnCount(const QModelIndex &) const
{
    return 1;
}

QModelIndex NmModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!hasIndex(row, column, parent)) {
        return {};
    }

    if (!parent.isValid()) {
        return createIndex(row, column, ITEM_ROOT);
    }

    const ItemId id = static_cast<ItemId>(parent.internalId());
    if (id == ITEM_ROOT) {
        switch (row) {
        case 0:
            return createIndex(row, column, ITEM_ACTIVE);
        case 1:
            return createIndex(row, column, ITEM_CONNECTION);
        case 2:
            return createIndex(row, column, ITEM_DEVICE);
        case 3:
            return createIndex(row, column, ITEM_WIFINET);
        default:
            return {};
        }
    }

    if (id == ITEM_ACTIVE) {
        return createIndex(row, column, ITEM_ACTIVE_LEAF);
    }
    if (id == ITEM_CONNECTION) {
        return createIndex(row, column, ITEM_CONNECTION_LEAF);
    }
    if (id == ITEM_DEVICE) {
        return createIndex(row, column, ITEM_DEVICE_LEAF);
    }
    if (id == ITEM_WIFINET) {
        return createIndex(row, column, ITEM_WIFINET_LEAF);
    }
    return {};
}

QModelIndex NmModel::parent(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return {};
    }

    const ItemId id = static_cast<ItemId>(index.internalId());
    switch (id) {
    case ITEM_ROOT:
        return {};
    case ITEM_ACTIVE:
    case ITEM_CONNECTION:
    case ITEM_DEVICE:
    case ITEM_WIFINET:
        return createIndex(0, 0, ITEM_ROOT);
    case ITEM_ACTIVE_LEAF:
        return createIndex(0, 0, ITEM_ACTIVE);
    case ITEM_CONNECTION_LEAF:
        return createIndex(1, 0, ITEM_CONNECTION);
    case ITEM_DEVICE_LEAF:
        return createIndex(2, 0, ITEM_DEVICE);
    case ITEM_WIFINET_LEAF:
        return createIndex(3, 0, ITEM_WIFINET);
    }
    return {};
}

QVariant NmModel::data(const QModelIndex &index, int role) const
{
    if (!isValidDataIndex(index)) {
        return {};
    }

    const ItemId id = static_cast<ItemId>(index.internalId());

    if (role == ItemTypeRole) {
        switch (id) {
        case ITEM_ACTIVE_LEAF:
            return ActiveConnectionType;
        case ITEM_CONNECTION_LEAF:
            return ConnectionType;
        case ITEM_DEVICE_LEAF:
            return DeviceType;
        case ITEM_WIFINET_LEAF:
            return WifiNetworkType;
        default:
            return HelperType;
        }
    }

    if (role == Qt::DisplayRole || role == NameRole) {
        switch (id) {
        case ITEM_ROOT:
            return tr("root");
        case ITEM_ACTIVE:
            return tr("active connection(s)");
        case ITEM_CONNECTION:
            return tr("connection(s)");
        case ITEM_DEVICE:
            return tr("device(s)");
        case ITEM_WIFINET:
            return tr("Wi-Fi network(s)");
        case ITEM_ACTIVE_LEAF:
            return mActive.at(index.row()).id;
        case ITEM_CONNECTION_LEAF:
            return mConnections.at(index.row()).id;
        case ITEM_DEVICE_LEAF:
            return mDevices.at(index.row()).interfaceName;
        case ITEM_WIFINET_LEAF:
            return mWifi.at(index.row()).ssid;
        }
    }

    if (role == Qt::ToolTipRole) {
        switch (id) {
        case ITEM_WIFINET_LEAF:
            return tr("Signal strength: %1%").arg(mWifi.at(index.row()).strength);
        case ITEM_ACTIVE_LEAF:
            if (isWirelessType(mActive.at(index.row()).type)) {
                const int signal = data(index, SignalRole).toInt();
                if (signal >= 0) {
                    return tr("Signal strength: %1%").arg(signal);
                }
            }
            break;
        case ITEM_CONNECTION_LEAF:
            if (isWirelessType(mConnections.at(index.row()).type)) {
                for (const auto &wifi : mWifi) {
                    if (wifi.ssid == mConnections.at(index.row()).id) {
                        return tr("Signal strength: %1%").arg(wifi.strength);
                    }
                }
            }
            break;
        default:
            break;
        }
    }

    if (role == ConnectionTypeRole) {
        switch (id) {
        case ITEM_ACTIVE_LEAF:
            return connTypeToInt(mActive.at(index.row()).type);
        case ITEM_CONNECTION_LEAF:
            return connTypeToInt(mConnections.at(index.row()).type);
        case ITEM_WIFINET_LEAF:
            return connTypeToInt(QStringLiteral("802-11-wireless"));
        case ITEM_DEVICE_LEAF:
            if (mDevices.at(index.row()).type == nm::DeviceType::Wifi) {
                return connTypeToInt(QStringLiteral("802-11-wireless"));
            }
            return connTypeToInt(QStringLiteral("802-3-ethernet"));
        default:
            return {};
        }
    }

    if (role == ConnectionTypeStringRole) {
        switch (id) {
        case ITEM_ACTIVE_LEAF:
            return nm::connectionTypeLabel(mActive.at(index.row()).type);
        case ITEM_CONNECTION_LEAF:
            return nm::connectionTypeLabel(mConnections.at(index.row()).type);
        case ITEM_WIFINET_LEAF:
            return QStringLiteral("wireless");
        case ITEM_DEVICE_LEAF:
            return mDevices.at(index.row()).type == nm::DeviceType::Wifi ? QStringLiteral("wireless") : QStringLiteral("ethernet");
        default:
            return {};
        }
    }

    if (role == ConnectionUuidRole) {
        if (id == ITEM_ACTIVE_LEAF) {
            return mActive.at(index.row()).uuid;
        }
        if (id == ITEM_CONNECTION_LEAF) {
            return mConnections.at(index.row()).uuid;
        }
    }

    if (role == ConnectionPathRole) {
        if (id == ITEM_ACTIVE_LEAF) {
            return mActive.at(index.row()).path;
        }
        if (id == ITEM_CONNECTION_LEAF) {
            return mConnections.at(index.row()).connectionPath;
        }
    }

    if (role == ActiveConnectionStateRole && id == ITEM_ACTIVE_LEAF) {
        return static_cast<int>(mActive.at(index.row()).state);
    }

    if (role == SavedConnectionPathRole) {
        switch (id) {
        case ITEM_ACTIVE_LEAF:
            return mActive.at(index.row()).connectionPath;
        case ITEM_CONNECTION_LEAF:
            return mConnections.at(index.row()).connectionPath;
        case ITEM_WIFINET_LEAF:
            return mWifi.at(index.row()).savedConnectionPath;
        default:
            return {};
        }
    }

    if (role == AutoConnectRole) {
        auto autoconnectForPath = [this](const QString &connectionPath) -> QVariant {
            const auto it = mCache.snapshot().savedConnections.find(connectionPath);
            if (it == mCache.snapshot().savedConnections.end()) {
                return {};
            }
            return it->autoconnect;
        };
        switch (id) {
        case ITEM_ACTIVE_LEAF:
            return autoconnectForPath(mActive.at(index.row()).connectionPath);
        case ITEM_CONNECTION_LEAF:
            return autoconnectForPath(mConnections.at(index.row()).connectionPath);
        case ITEM_WIFINET_LEAF:
            if (mWifi.at(index.row()).savedConnectionPath.isEmpty()) {
                return {};
            }
            return mWifi.at(index.row()).autoconnect;
        default:
            return {};
        }
    }

    if (role == AutoConnectSupportedRole) {
        switch (id) {
        case ITEM_ACTIVE_LEAF:
            return isWirelessType(mActive.at(index.row()).type) && !mActive.at(index.row()).connectionPath.isEmpty();
        case ITEM_CONNECTION_LEAF:
            return isWirelessType(mConnections.at(index.row()).type);
        case ITEM_WIFINET_LEAF:
            return !mWifi.at(index.row()).savedConnectionPath.isEmpty();
        default:
            return false;
        }
    }

    if (role == ActiveConnectionMasterRole && id == ITEM_ACTIVE_LEAF) {
        return QStringLiteral("/");
    }

    if (role == ActiveConnectionDevicesRole && id == ITEM_ACTIVE_LEAF) {
        return mActive.at(index.row()).devices;
    }

    if (role == SignalRole) {
        if (id == ITEM_WIFINET_LEAF) {
            return mWifi.at(index.row()).strength;
        }
        if (id == ITEM_ACTIVE_LEAF) {
            const auto &active = mActive.at(index.row());
            const auto apIt = mCache.snapshot().accessPoints.find(active.specificObjectPath);
            if (apIt != mCache.snapshot().accessPoints.end()) {
                return apIt->strength;
            }
            return -1;
        }
        return -1;
    }

    if (role == IconSecurityTypeRole) {
        if (id == ITEM_WIFINET_LEAF) {
            return mWifi.at(index.row()).secure ? icons::SECURITY_HIGH : icons::SECURITY_LOW;
        }
        return -1;
    }

    if (role == IconSecurityRole) {
        const int securityType = data(index, IconSecurityTypeRole).toInt();
        if (securityType >= 0) {
            return icons::getIcon(static_cast<icons::Icon>(securityType), true);
        }
        return {};
    }

    if (role == ActiveConnectionInfoRole && id == ITEM_ACTIVE_LEAF) {
        return buildActiveInfo(mActive.at(index.row()));
    }

    if (role == IconTypeRole) {
        if (id == ITEM_WIFINET_LEAF) {
            return icons::wifiSignalIcon(mWifi.at(index.row()).strength);
        }

        if (id == ITEM_ACTIVE_LEAF) {
            const auto &active = mActive.at(index.row());
            if (nm::isVpnType(active.type) || active.isVpn) {
                return icons::NETWORK_VPN;
            }
            if (isWirelessType(active.type)) {
                if (active.state == nm::ActiveState::Activating) {
                    return icons::NETWORK_WIFI_ACQUIRING;
                }
                if (active.state == nm::ActiveState::Activated) {
                    const int signal = data(index, SignalRole).toInt();
                    return icons::wifiSignalIcon(signal);
                }
                return icons::NETWORK_WIFI_DISCONNECTED;
            }
            return active.state == nm::ActiveState::Activated ? icons::NETWORK_WIRED : icons::NETWORK_WIRED_DISCONNECTED;
        }

        if (id == ITEM_CONNECTION_LEAF) {
            const auto &conn = mConnections.at(index.row());
            if (nm::isVpnType(conn.type)) {
                return icons::NETWORK_VPN;
            }
            if (isWirelessType(conn.type)) {
                if (conn.active) {
                    const QString acPath = mCache.activeConnectionForSettingsPath(conn.connectionPath);
                    for (int i = 0; i < mActive.size(); ++i) {
                        if (mActive.at(i).path == acPath) {
                            const int signal = data(createIndex(i, 0, ITEM_ACTIVE_LEAF), SignalRole).toInt();
                            return icons::wifiSignalIcon(signal);
                        }
                    }
                }
                return icons::NETWORK_WIFI_DISCONNECTED;
            }
            return conn.active ? icons::NETWORK_WIRED : icons::NETWORK_WIRED_DISCONNECTED;
        }

        if (id == ITEM_DEVICE_LEAF) {
            const auto &dev = mDevices.at(index.row());
            if (dev.type == nm::DeviceType::Wifi) {
                if (!dev.activeAccessPointPath.isEmpty()) {
                    const auto apIt = mCache.snapshot().accessPoints.find(dev.activeAccessPointPath);
                    if (apIt != mCache.snapshot().accessPoints.end()) {
                        return icons::wifiSignalIcon(apIt->strength);
                    }
                }
                return icons::NETWORK_WIFI_DISCONNECTED;
            }
            return dev.activeConnectionPath.isEmpty() ? icons::NETWORK_WIRED_DISCONNECTED : icons::NETWORK_WIRED;
        }

        return -1;
    }

    if (role == Qt::DecorationRole || role == IconRole) {
        const int iconType = data(index, IconTypeRole).toInt();
        if (iconType >= 0) {
            return icons::getIcon(static_cast<icons::Icon>(iconType), true);
        }
    }

    return {};
}

QModelIndex NmModel::indexTypeRoot(ItemType type) const
{
    switch (type) {
    case HelperType:
        return createIndex(0, 0, ITEM_ROOT);
    case ActiveConnectionType:
        return createIndex(0, 0, ITEM_ACTIVE);
    case ConnectionType:
        return createIndex(1, 0, ITEM_CONNECTION);
    case DeviceType:
        return createIndex(2, 0, ITEM_DEVICE);
    case WifiNetworkType:
        return createIndex(3, 0, ITEM_WIFINET);
    }
    return {};
}

bool NmModel::networkingEnabled() const
{
    return mManagerState.networkingEnabled;
}

bool NmModel::wirelessEnabled() const
{
    return mManagerState.wirelessEnabled;
}

bool NmModel::wirelessHardwareEnabled() const
{
    return mManagerState.wirelessHardwareEnabled;
}

QString NmModel::primaryConnectionPath() const
{
    return mManagerState.primaryConnectionPath;
}

bool NmModel::showLowSignalNetworks() const
{
    return mShowLowSignalNetworks;
}

NmModel::ManagerState NmModel::managerState() const
{
    return mManagerState;
}

QList<NmModel::RecentConnection> NmModel::recentConnections(int maxCount) const
{
    QList<RecentConnection> recent;
    QString currentConnectionPath;
    const auto activeIt = std::find_if(mActive.cbegin(), mActive.cend(), [this](const nm::ActiveConnectionRecord &active) {
        return active.path == mManagerState.primaryConnectionPath;
    });
    if (activeIt != mActive.cend()) {
        currentConnectionPath = activeIt->connectionPath;
    }

    for (const auto &conn : mConnections) {
        if (conn.lastUsedTimestamp <= 0) {
            continue;
        }
        if (!currentConnectionPath.isEmpty() && conn.connectionPath == currentConnectionPath) {
            continue;
        }
        RecentConnection r;
        r.id = conn.id;
        r.connectionPath = conn.connectionPath;
        r.lastUsedTimestamp = conn.lastUsedTimestamp;
        recent.push_back(r);
    }

    std::sort(recent.begin(), recent.end(), [](const RecentConnection &a, const RecentConnection &b) {
        if (a.lastUsedTimestamp != b.lastUsedTimestamp) {
            return a.lastUsedTimestamp > b.lastUsedTimestamp;
        }
        return QString::compare(a.id, b.id, Qt::CaseInsensitive) < 0;
    });
    if (maxCount > 0 && recent.size() > maxCount) {
        recent = recent.mid(0, maxCount);
    }
    return recent;
}

void NmModel::activateConnection(const QModelIndex &index)
{
    const ItemId id = static_cast<ItemId>(index.internalId());
    if (!isValidDataIndex(index)) {
        return;
    }

    if (id == ITEM_CONNECTION_LEAF) {
        const auto &conn = mConnections.at(index.row());
        QString devicePath = QStringLiteral("/");
        QString specificObject = QStringLiteral("/");

        if (isWirelessType(conn.type)) {
            QString ifaceHint;
            const auto sIt = mCache.snapshot().savedConnections.find(conn.connectionPath);
            if (sIt != mCache.snapshot().savedConnections.end()) {
                ifaceHint = sIt->interfaceName;
            }
            for (const auto &dev : mDevices) {
                if (dev.type != nm::DeviceType::Wifi) {
                    continue;
                }
                if (!ifaceHint.isEmpty() && dev.interfaceName != ifaceHint) {
                    continue;
                }
                if (dev.state >= 20) {
                    devicePath = dev.path;
                    break;
                }
            }
        }

        if (auto result = nm::NmActions::activateConnection(conn.connectionPath, devicePath, specificObject); !result) {
            qCWarning(NM_TRAY).noquote() << QStringLiteral("activateConnection failed for '%1': %2").arg(conn.id, result.error());
        }
        return;
    }

    if (id == ITEM_WIFINET_LEAF) {
        const auto &wifi = mWifi.at(index.row());
        QString savedConnectionPath = wifi.savedConnectionPath;
        if (savedConnectionPath.isEmpty()) {
            savedConnectionPath = mCache.connectionPathForSsid(wifi.ssid);
        }
        if (savedConnectionPath.isEmpty()) {
            // No saved connection, prompt to create one
            promptAndCreateWifiConnection(wifi.ssid, wifi.devicePath, wifi.secure);
            return;
        }
        if (auto result = nm::NmActions::activateConnection(savedConnectionPath, wifi.devicePath, wifi.apPath); !result) {
            qCWarning(NM_TRAY).noquote() << QStringLiteral("activateConnection failed for SSID '%1': %2").arg(wifi.ssid, result.error());
        }
    }
}

void NmModel::deactivateConnection(const QModelIndex &index)
{
    if (!isValidDataIndex(index) || static_cast<ItemId>(index.internalId()) != ITEM_ACTIVE_LEAF) {
        return;
    }
    disconnectActiveConnection(mActive.at(index.row()));
}

void NmModel::requestScan(const QModelIndex &index) const
{
    if (!isValidDataIndex(index) || static_cast<ItemId>(index.internalId()) != ITEM_DEVICE_LEAF) {
        return;
    }
    const auto &dev = mDevices.at(index.row());
    if (dev.type != nm::DeviceType::Wifi) {
        return;
    }
    if (auto result = nm::NmActions::requestScan(dev.path); !result) {
        qCWarning(NM_TRAY).noquote() << QStringLiteral("requestScan failed for '%1': %2").arg(dev.interfaceName, result.error());
    }
}

void NmModel::requestAllWifiScan() const
{
    for (int i = 0; i < mDevices.size(); ++i) {
        const auto &dev = mDevices.at(i);
        if (dev.type != nm::DeviceType::Wifi) {
            continue;
        }
        if (auto result = nm::NmActions::requestScan(dev.path); !result) {
            qCWarning(NM_TRAY).noquote() << QStringLiteral("requestScan failed for '%1': %2").arg(dev.interfaceName, result.error());
        }
    }
}

void NmModel::setNetworkingEnabled(bool enabled)
{
    if (auto result = nm::NmActions::setNetworkingEnabled(enabled); !result) {
        qCWarning(NM_TRAY).noquote() << QStringLiteral("setNetworkingEnabled failed: %1").arg(result.error());
    }
}

void NmModel::setWirelessEnabled(bool enabled)
{
    if (auto result = nm::NmActions::setWirelessEnabled(enabled); !result) {
        qCWarning(NM_TRAY).noquote() << QStringLiteral("setWirelessEnabled failed: %1").arg(result.error());
    }
}

void NmModel::setConnectionAutoconnect(const QString &connectionPath, bool enabled)
{
    if (connectionPath.isEmpty() || connectionPath == QStringLiteral("/")) {
        return;
    }

    if (auto result = nm::NmActions::setConnectionAutoconnect(connectionPath, enabled); !result) {
        qCWarning(NM_TRAY).noquote() << QStringLiteral("setConnectionAutoconnect failed for '%1': %2").arg(connectionPath, result.error());
        return;
    }

    QMetaObject::invokeMethod(mDbus, "refreshNow", Qt::QueuedConnection);
}

void NmModel::setShowLowSignalNetworks(bool enabled)
{
    if (mShowLowSignalNetworks == enabled) {
        return;
    }
    mShowLowSignalNetworks = enabled;
    rebuildFromSnapshot(mCache.snapshot());
}

void NmModel::disconnectPrimaryConnection()
{
    if (mManagerState.primaryConnectionPath.isEmpty() || mManagerState.primaryConnectionPath == QStringLiteral("/")) {
        return;
    }

    const auto activeIt = std::find_if(mActive.cbegin(), mActive.cend(), [this](const nm::ActiveConnectionRecord &active) {
        return active.path == mManagerState.primaryConnectionPath;
    });
    if (activeIt == mActive.cend()) {
        if (auto result = nm::NmActions::deactivateConnection(mManagerState.primaryConnectionPath); !result) {
            qCWarning(NM_TRAY).noquote() << QStringLiteral("disconnectPrimaryConnection failed: %1").arg(result.error());
        }
        return;
    }

    if (!disconnectActiveConnection(*activeIt)) {
        qCWarning(NM_TRAY).noquote() << QStringLiteral("disconnectPrimaryConnection failed for '%1'").arg(activeIt->id);
    }
}

void NmModel::activateConnectionPath(const QString &connectionPath)
{
    if (connectionPath.isEmpty()) {
        return;
    }

    const auto connIt = std::find_if(mConnections.cbegin(), mConnections.cend(), [&connectionPath](const nm::ConnectionViewRecord &conn) {
        return conn.connectionPath == connectionPath;
    });
    if (connIt == mConnections.cend()) {
        return;
    }

    QString devicePath = QStringLiteral("/");
    QString specificObject = QStringLiteral("/");
    if (isWirelessType(connIt->type)) {
        QString ifaceHint;
        const auto sIt = mCache.snapshot().savedConnections.find(connIt->connectionPath);
        if (sIt != mCache.snapshot().savedConnections.end()) {
            ifaceHint = sIt->interfaceName;
        }
        for (const auto &dev : mDevices) {
            if (dev.type != nm::DeviceType::Wifi) {
                continue;
            }
            if (!ifaceHint.isEmpty() && dev.interfaceName != ifaceHint) {
                continue;
            }
            if (dev.state >= 20) {
                devicePath = dev.path;
                break;
            }
        }
    }

    if (auto result = nm::NmActions::activateConnection(connIt->connectionPath, devicePath, specificObject); !result) {
        qCWarning(NM_TRAY).noquote() << QStringLiteral("activateConnectionPath failed for '%1': %2").arg(connIt->id, result.error());
    }
}

void NmModel::onSnapshotChanged(const nm::Snapshot &snapshot)
{
    rebuildFromSnapshot(snapshot);
}

bool NmModel::isValidDataIndex(const QModelIndex &index) const
{
    if (!index.isValid()) {
        return false;
    }

    switch (static_cast<ItemId>(index.internalId())) {
    case ITEM_ROOT:
    case ITEM_ACTIVE:
    case ITEM_CONNECTION:
    case ITEM_DEVICE:
    case ITEM_WIFINET:
        return true;
    case ITEM_ACTIVE_LEAF:
        return index.row() >= 0 && index.row() < mActive.size() && index.column() == 0;
    case ITEM_CONNECTION_LEAF:
        return index.row() >= 0 && index.row() < mConnections.size() && index.column() == 0;
    case ITEM_DEVICE_LEAF:
        return index.row() >= 0 && index.row() < mDevices.size() && index.column() == 0;
    case ITEM_WIFINET_LEAF:
        return index.row() >= 0 && index.row() < mWifi.size() && index.column() == 0;
    }
    return false;
}

void NmModel::rebuildFromSnapshot(const nm::Snapshot &snapshot)
{
    static constexpr int kMinShownSignalPercent = 25;
    const ManagerState oldManagerState = mManagerState;

    mCache.setSnapshot(snapshot);
    QList<nm::ActiveConnectionRecord> nextActive = mCache.activeConnections();
    QList<nm::ConnectionViewRecord> nextConnections = mCache.knownConnections(false);
    QList<nm::DeviceRecord> nextDevices = mCache.devices();
    QList<nm::WifiViewRecord> nextWifi = mCache.wifiEntries(false);
    if (!mShowLowSignalNetworks) {
        QList<nm::WifiViewRecord> filtered;
        filtered.reserve(nextWifi.size());
        for (const auto &wifi : nextWifi) {
            if (wifi.active || wifi.strength >= kMinShownSignalPercent) {
                filtered.push_back(wifi);
            }
        }
        nextWifi = std::move(filtered);
    }

    auto applySection = [this](ItemId sectionId, auto &current, const auto &next, auto keyOf, auto isSameItem) {
        QStringList currentKeys;
        QStringList nextKeys;
        currentKeys.reserve(current.size());
        nextKeys.reserve(next.size());
        for (const auto &item : current) {
            currentKeys.push_back(keyOf(item));
        }
        for (const auto &item : next) {
            nextKeys.push_back(keyOf(item));
        }

        int sectionRow = -1;
        ItemId leafId = ITEM_ROOT;
        switch (sectionId) {
        case ITEM_ACTIVE:
            sectionRow = 0;
            leafId = ITEM_ACTIVE_LEAF;
            break;
        case ITEM_CONNECTION:
            sectionRow = 1;
            leafId = ITEM_CONNECTION_LEAF;
            break;
        case ITEM_DEVICE:
            sectionRow = 2;
            leafId = ITEM_DEVICE_LEAF;
            break;
        case ITEM_WIFINET:
            sectionRow = 3;
            leafId = ITEM_WIFINET_LEAF;
            break;
        default:
            break;
        }
        if (sectionRow < 0 || leafId == ITEM_ROOT) {
            current = next;
            return;
        }

        const QModelIndex parentIdx = createIndex(sectionRow, 0, sectionId);
        if (currentKeys != nextKeys) {
            if (!current.isEmpty()) {
                beginRemoveRows(parentIdx, 0, current.size() - 1);
                current.clear();
                endRemoveRows();
            }
            if (!next.isEmpty()) {
                beginInsertRows(parentIdx, 0, next.size() - 1);
                current = next;
                endInsertRows();
            }
            return;
        }

        if (current.isEmpty()) {
            return;
        }

        QList<QPair<int, int>> changedRanges;
        int rangeStart = -1;
        for (int i = 0; i < current.size(); ++i) {
            if (!isSameItem(current.at(i), next.at(i))) {
                if (rangeStart < 0) {
                    rangeStart = i;
                }
                continue;
            }
            if (rangeStart >= 0) {
                changedRanges.push_back({rangeStart, i - 1});
                rangeStart = -1;
            }
        }
        if (rangeStart >= 0) {
            changedRanges.push_back({rangeStart, current.size() - 1});
        }

        if (changedRanges.isEmpty()) {
            return;
        }

        current = next;
        for (const auto &[first, last] : changedRanges) {
            emit dataChanged(createIndex(first, 0, leafId), createIndex(last, 0, leafId));
        }
    };

    applySection(ITEM_ACTIVE, mActive, nextActive, [](const nm::ActiveConnectionRecord &item) { return item.path; }, activeEquivalent);
    applySection(ITEM_CONNECTION, mConnections, nextConnections, [](const nm::ConnectionViewRecord &item) { return item.connectionPath; }, connectionEquivalent);
    applySection(ITEM_DEVICE, mDevices, nextDevices, [](const nm::DeviceRecord &item) { return item.path; }, deviceEquivalent);
    applySection(ITEM_WIFINET, mWifi, nextWifi, [](const nm::WifiViewRecord &item) { return item.apPath; }, wifiEquivalent);

    ManagerState nextManagerState;
    nextManagerState.networkingEnabled = mCache.snapshot().manager.networkingEnabled;
    nextManagerState.wirelessEnabled = mCache.snapshot().manager.wirelessEnabled;
    nextManagerState.wirelessHardwareEnabled = mCache.snapshot().manager.wirelessHardwareEnabled;
    nextManagerState.primaryConnectionPath = mCache.snapshot().manager.primaryConnectionPath;
    nextManagerState.overallState = toOverallState(mCache.snapshot().manager.state, mCache.snapshot().manager.connectivity);
    nextManagerState.primaryKind = PrimaryKind::Unknown;
    nextManagerState.primaryName.clear();
    nextManagerState.wifiStrength = -1;
    nextManagerState.vpnActive.clear();
    nextManagerState.lastError = mCache.snapshot().manager.lastError;

    for (const auto &active : mActive) {
        if (active.isVpn || nm::isVpnType(active.type)) {
            nextManagerState.vpnActive.push_back(active.id);
        }
    }

    const auto primaryIt = std::find_if(mActive.cbegin(), mActive.cend(), [this, &nextManagerState](const nm::ActiveConnectionRecord &active) {
        return active.path == nextManagerState.primaryConnectionPath;
    });
    if (primaryIt != mActive.cend()) {
        nextManagerState.primaryName = primaryIt->id;
        if (isWirelessType(primaryIt->type)) {
            nextManagerState.primaryKind = PrimaryKind::Wifi;
            const auto apIt = mCache.snapshot().accessPoints.find(primaryIt->specificObjectPath);
            if (apIt != mCache.snapshot().accessPoints.end()) {
                nextManagerState.primaryName = apIt->ssid;
                nextManagerState.wifiStrength = apIt->strength;
            } else if (!primaryIt->devices.isEmpty()) {
                const auto devIt = mCache.snapshot().devices.find(primaryIt->devices.first());
                if (devIt != mCache.snapshot().devices.end()) {
                    const auto activeAp = mCache.snapshot().accessPoints.find(devIt->activeAccessPointPath);
                    if (activeAp != mCache.snapshot().accessPoints.end()) {
                        nextManagerState.primaryName = activeAp->ssid;
                        nextManagerState.wifiStrength = activeAp->strength;
                    }
                }
            }
        } else {
            nextManagerState.primaryKind = PrimaryKind::Wired;
        }
    }

    mManagerState = nextManagerState;
    if (!managerStateEquivalent(oldManagerState, nextManagerState)) {
        emit managerStateChanged();
    }
}

bool NmModel::disconnectActiveConnection(const nm::ActiveConnectionRecord &active)
{
    const bool isDeviceBacked = !active.isVpn
        && (active.type == QStringLiteral("802-11-wireless")
            || active.type == QStringLiteral("802-3-ethernet")
            || active.type == QStringLiteral("bluetooth"));

    if (isDeviceBacked && !active.devices.isEmpty()) {
        const QString devicePath = active.devices.front();
        if (auto result = nm::NmActions::disconnectDevice(devicePath); !result) {
            qCWarning(NM_TRAY).noquote()
                << QStringLiteral("disconnectDevice failed for '%1' on '%2': %3").arg(active.id, devicePath, result.error());
            return false;
        }
        return true;
    }

    if (auto result = nm::NmActions::deactivateConnection(active.path); !result) {
        qCWarning(NM_TRAY).noquote()
            << QStringLiteral("deactivateConnection failed for '%1': %2").arg(active.id, result.error());
        return false;
    }
    return true;
}

QString NmModel::buildActiveInfo(const nm::ActiveConnectionRecord &active) const
{
    QString info;
    QDebug str(&info);
    str.noquote();
    str.nospace();

    str << QStringLiteral("<table>")
        << QStringLiteral("<tr><td colspan='2'><big><strong>") << tr("General", "Active connection information") << QStringLiteral("</strong></big></td></tr>")
        << QStringLiteral("<tr><td><strong>") << tr("Connection", "Active connection information") << QStringLiteral("</strong>: </td><td>") << active.id << QStringLiteral("</td></tr>")
        << QStringLiteral("<tr><td><strong>") << tr("Type", "Active connection information") << QStringLiteral("</strong>: </td><td>") << nm::connectionTypeLabel(active.type) << QStringLiteral("</td></tr>")
        << QStringLiteral("<tr><td><strong>") << tr("UUID", "Active connection information") << QStringLiteral("</strong>: </td><td>") << active.uuid << QStringLiteral("</td></tr>");

    if (!active.devices.isEmpty()) {
        const auto devIt = mCache.snapshot().devices.find(active.devices.front());
        if (devIt != mCache.snapshot().devices.end()) {
            str << QStringLiteral("<tr><td><strong>") << tr("Interface", "Active connection information") << QStringLiteral("</strong>: </td><td>")
                << devIt->interfaceName << QStringLiteral("</td></tr>");
            if (!devIt->hardwareAddress.isEmpty()) {
                str << QStringLiteral("<tr><td><strong>") << tr("Hardware Address", "Active connection information") << QStringLiteral("</strong>: </td><td>")
                    << devIt->hardwareAddress << QStringLiteral("</td></tr>");
            }
            if (devIt->bitrateKbps > 0) {
                str << QStringLiteral("<tr><td><strong>") << tr("Speed", "Active connection information") << QStringLiteral("</strong>: </td><td>")
                    << QLocale::system().toString(static_cast<double>(devIt->bitrateKbps) / 1000.0, 'g', 5) << QStringLiteral(" Mb/s</td></tr>");
            }
            if (devIt->rxBytes > 0 || devIt->txBytes > 0) {
                str << QStringLiteral("<tr><td><strong>") << tr("Data received", "Active connection information") << QStringLiteral("</strong>: </td><td>")
                    << formatBytes(devIt->rxBytes) << QStringLiteral("</td></tr>")
                    << QStringLiteral("<tr><td><strong>") << tr("Data transmitted", "Active connection information") << QStringLiteral("</strong>: </td><td>")
                    << formatBytes(devIt->txBytes) << QStringLiteral("</td></tr>");
            }
        }
    }

    auto appendIpConfig = [&str](const QString &title,
                                 const QStringList &addresses,
                                 const QString &gateway,
                                 const QStringList &dns,
                                 int routeCount) {
        if (addresses.isEmpty() && gateway.isEmpty() && dns.isEmpty() && routeCount <= 0) {
            return;
        }

        str << QStringLiteral("<tr/><tr><td colspan='2'><big><strong>") << title << QStringLiteral("</strong></big></td></tr>");

        for (int i = 0; i < addresses.size(); ++i) {
            const QString suffix = i > 0 ? QStringLiteral("(%1)").arg(i + 1) : QString{};
            str << QStringLiteral("<tr><td><strong>") << tr("IP Address", "Active connection information") << suffix
                << QStringLiteral("</strong>: </td><td>") << addresses.at(i) << QStringLiteral("</td></tr>");
        }

        if (!gateway.isEmpty()) {
            str << QStringLiteral("<tr><td><strong>") << tr("Default route", "Active connection information")
                << QStringLiteral("</strong>: </td><td>") << gateway << QStringLiteral("</td></tr>");
        }

        for (int i = 0; i < dns.size(); ++i) {
            str << QStringLiteral("<tr><td><strong>") << tr("DNS(%1)", "Active connection information").arg(i + 1)
                << QStringLiteral("</strong>: </td><td>") << dns.at(i) << QStringLiteral("</td></tr>");
        }

        if (routeCount > 0) {
            str << QStringLiteral("<tr><td><strong>") << tr("Routes", "Active connection information")
                << QStringLiteral("</strong>: </td><td>") << routeCount << QStringLiteral("</td></tr>");
        }
    };

    appendIpConfig(tr("IPv4", "Active connection information"),
                   active.ip4Addresses,
                   active.ip4Gateway,
                   active.ip4Dns,
                   active.ip4RouteCount);
    appendIpConfig(tr("IPv6", "Active connection information"),
                   active.ip6Addresses,
                   active.ip6Gateway,
                   active.ip6Dns,
                   active.ip6RouteCount);

    str << QStringLiteral("</table>");
    return info;
}

void NmModel::promptAndCreateWifiConnection(const QString &ssid, const QString &devicePath, bool secure)
{
    auto *dialog = new WifiPasswordDialog(ssid, secure, nullptr);
    dialog->setAttribute(Qt::WA_DeleteOnClose);

    connect(dialog, &QDialog::accepted, this, [this, ssid, devicePath, secure, dialog]() {
        const QString password = dialog->password();
        if (auto result = nm::NmActions::addWifiConnection(ssid, password, devicePath, secure); !result) {
            qCWarning(NM_TRAY).noquote() << QStringLiteral("Failed to create Wi-Fi connection for '%1': %2").arg(ssid, result.error());
        } else if (mDbus != nullptr) {
            QMetaObject::invokeMethod(mDbus, "refreshNow", Qt::QueuedConnection);
        }
    });

    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}
