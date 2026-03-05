#include "nmmodel.h"

#include "backend/nm_actions.h"
#include "icons.h"
#include "log.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusVariant>
#include <QLocale>
#include <algorithm>

namespace
{
constexpr const char *kNmService = "org.freedesktop.NetworkManager";
constexpr const char *kDbusPropsIface = "org.freedesktop.DBus.Properties";

QVariant dbusGet(const QString &path, const QString &iface, const QString &property)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(kNmService), path, QString::fromLatin1(kDbusPropsIface), QStringLiteral("Get"));
    msg << iface << property;
    const QDBusMessage reply = QDBusConnection::systemBus().call(msg);
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
        return {};
    }
    return reply.arguments().at(0).value<QDBusVariant>().variant();
}

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

} // namespace

NmModel::NmModel(QObject *parent)
    : QAbstractItemModel(parent)
{
    connect(&mDbus, &nm::NmDbusClient::snapshotChanged, this, &NmModel::rebuildFromSnapshot);
    connect(&mDbus, &nm::NmDbusClient::managerStateChanged, this, &NmModel::managerStateChanged);
    rebuildFromSnapshot();
}

NmModel::~NmModel() = default;

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

    QString error;
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

        if (!nm::NmActions::activateConnection(conn.connectionPath, devicePath, specificObject, &error)) {
            qCWarning(NM_TRAY).noquote() << QStringLiteral("activateConnection failed for '%1': %2").arg(conn.id, error);
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
            qCWarning(NM_TRAY).noquote() << QStringLiteral("No saved profile for SSID '%1'. Connection creation is intentionally delegated to NetworkManager secret agents.").arg(wifi.ssid);
            return;
        }
        if (!nm::NmActions::activateConnection(savedConnectionPath, wifi.devicePath, wifi.apPath, &error)) {
            qCWarning(NM_TRAY).noquote() << QStringLiteral("activateConnection failed for SSID '%1': %2").arg(wifi.ssid, error);
        }
    }
}

void NmModel::deactivateConnection(const QModelIndex &index)
{
    if (!isValidDataIndex(index) || static_cast<ItemId>(index.internalId()) != ITEM_ACTIVE_LEAF) {
        return;
    }
    QString error;
    const auto &active = mActive.at(index.row());
    if (!nm::NmActions::deactivateConnection(active.path, &error)) {
        qCWarning(NM_TRAY).noquote() << QStringLiteral("deactivateConnection failed for '%1': %2").arg(active.id, error);
    }
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
    QString error;
    if (!nm::NmActions::requestScan(dev.path, &error)) {
        qCWarning(NM_TRAY).noquote() << QStringLiteral("requestScan failed for '%1': %2").arg(dev.interfaceName, error);
    }
}

void NmModel::requestAllWifiScan() const
{
    for (int i = 0; i < mDevices.size(); ++i) {
        const auto &dev = mDevices.at(i);
        if (dev.type != nm::DeviceType::Wifi) {
            continue;
        }
        QString error;
        if (!nm::NmActions::requestScan(dev.path, &error)) {
            qCWarning(NM_TRAY).noquote() << QStringLiteral("requestScan failed for '%1': %2").arg(dev.interfaceName, error);
        }
    }
}

void NmModel::setNetworkingEnabled(bool enabled)
{
    QString error;
    if (!nm::NmActions::setNetworkingEnabled(enabled, &error)) {
        qCWarning(NM_TRAY).noquote() << QStringLiteral("setNetworkingEnabled failed: %1").arg(error);
    }
}

void NmModel::setWirelessEnabled(bool enabled)
{
    QString error;
    if (!nm::NmActions::setWirelessEnabled(enabled, &error)) {
        qCWarning(NM_TRAY).noquote() << QStringLiteral("setWirelessEnabled failed: %1").arg(error);
    }
}

void NmModel::setShowLowSignalNetworks(bool enabled)
{
    if (mShowLowSignalNetworks == enabled) {
        return;
    }
    mShowLowSignalNetworks = enabled;
    rebuildFromSnapshot();
}

void NmModel::disconnectPrimaryConnection()
{
    if (mManagerState.primaryConnectionPath.isEmpty() || mManagerState.primaryConnectionPath == QStringLiteral("/")) {
        return;
    }
    QString error;
    if (!nm::NmActions::deactivateConnection(mManagerState.primaryConnectionPath, &error)) {
        qCWarning(NM_TRAY).noquote() << QStringLiteral("disconnectPrimaryConnection failed: %1").arg(error);
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
    QString error;
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

    if (!nm::NmActions::activateConnection(connIt->connectionPath, devicePath, specificObject, &error)) {
        qCWarning(NM_TRAY).noquote() << QStringLiteral("activateConnectionPath failed for '%1': %2").arg(connIt->id, error);
    }
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

void NmModel::rebuildFromSnapshot()
{
    static constexpr int kMinShownSignalPercent = 25;
    beginResetModel();
    mCache.setSnapshot(mDbus.snapshot());
    mActive = mCache.activeConnections();
    mConnections = mCache.knownConnections(true);
    mDevices = mCache.devices();
    mWifi = mCache.wifiEntries(true);
    if (!mShowLowSignalNetworks) {
        QList<nm::WifiViewRecord> filtered;
        filtered.reserve(mWifi.size());
        for (const auto &wifi : mWifi) {
            if (wifi.active || wifi.strength >= kMinShownSignalPercent) {
                filtered.push_back(wifi);
            }
        }
        mWifi = std::move(filtered);
    }
    mManagerState.networkingEnabled = mCache.snapshot().manager.networkingEnabled;
    mManagerState.wirelessEnabled = mCache.snapshot().manager.wirelessEnabled;
    mManagerState.wirelessHardwareEnabled = mCache.snapshot().manager.wirelessHardwareEnabled;
    mManagerState.primaryConnectionPath = mCache.snapshot().manager.primaryConnectionPath;
    mManagerState.overallState = toOverallState(mCache.snapshot().manager.state, mCache.snapshot().manager.connectivity);
    mManagerState.primaryKind = PrimaryKind::Unknown;
    mManagerState.primaryName.clear();
    mManagerState.wifiStrength = -1;
    mManagerState.vpnActive.clear();
    mManagerState.lastError = mCache.snapshot().manager.lastError;

    for (const auto &active : mActive) {
        if (active.isVpn || nm::isVpnType(active.type)) {
            mManagerState.vpnActive.push_back(active.id);
        }
    }

    const auto primaryIt = std::find_if(mActive.cbegin(), mActive.cend(), [this](const nm::ActiveConnectionRecord &active) {
        return active.path == mManagerState.primaryConnectionPath;
    });
    if (primaryIt != mActive.cend()) {
        mManagerState.primaryName = primaryIt->id;
        if (isWirelessType(primaryIt->type)) {
            mManagerState.primaryKind = PrimaryKind::Wifi;
            const auto apIt = mCache.snapshot().accessPoints.find(primaryIt->specificObjectPath);
            if (apIt != mCache.snapshot().accessPoints.end()) {
                mManagerState.primaryName = apIt->ssid;
                mManagerState.wifiStrength = apIt->strength;
            } else if (!primaryIt->devices.isEmpty()) {
                const auto devIt = mCache.snapshot().devices.find(primaryIt->devices.first());
                if (devIt != mCache.snapshot().devices.end()) {
                    const auto activeAp = mCache.snapshot().accessPoints.find(devIt->activeAccessPointPath);
                    if (activeAp != mCache.snapshot().accessPoints.end()) {
                        mManagerState.primaryName = activeAp->ssid;
                        mManagerState.wifiStrength = activeAp->strength;
                    }
                }
            }
        } else {
            mManagerState.primaryKind = PrimaryKind::Wired;
        }
    }
    endResetModel();
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

            const QString statsPath = devIt->path;
            const QString statsIface = QStringLiteral("org.freedesktop.NetworkManager.Device.Statistics");
            const qulonglong rxBytes = dbusGet(statsPath, statsIface, QStringLiteral("RxBytes")).toULongLong();
            const qulonglong txBytes = dbusGet(statsPath, statsIface, QStringLiteral("TxBytes")).toULongLong();
            if (rxBytes > 0 || txBytes > 0) {
                str << QStringLiteral("<tr><td><strong>") << tr("Data received", "Active connection information") << QStringLiteral("</strong>: </td><td>")
                    << formatBytes(rxBytes) << QStringLiteral("</td></tr>")
                    << QStringLiteral("<tr><td><strong>") << tr("Data transmitted", "Active connection information") << QStringLiteral("</strong>: </td><td>")
                    << formatBytes(txBytes) << QStringLiteral("</td></tr>");
            }
        }
    }

    auto appendIpConfig = [this, &str](const QString &title, const QString &path, const QString &iface) {
        if (path.isEmpty() || path == QStringLiteral("/")) {
            return;
        }

        const QVariant addressData = dbusGet(path, iface, QStringLiteral("AddressData"));
        const QVariant routeData = dbusGet(path, iface, QStringLiteral("RouteData"));
        const QVariant nameservers = dbusGet(path, iface, QStringLiteral("NameserverData"));
        const QString gateway = dbusGet(path, iface, QStringLiteral("Gateway")).toString();

        str << QStringLiteral("<tr/><tr><td colspan='2'><big><strong>") << title << QStringLiteral("</strong></big></td></tr>");

        const QVariantList addrList = addressData.toList();
        for (int i = 0; i < addrList.size(); ++i) {
            const QVariantMap addr = addrList.at(i).toMap();
            const QString suffix = i > 0 ? QStringLiteral("(%1)").arg(i + 1) : QString{};
            str << QStringLiteral("<tr><td><strong>") << tr("IP Address", "Active connection information") << suffix
                << QStringLiteral("</strong>: </td><td>") << addr.value(QStringLiteral("address")).toString() << QStringLiteral("</td></tr>");
        }

        if (!gateway.isEmpty()) {
            str << QStringLiteral("<tr><td><strong>") << tr("Default route", "Active connection information")
                << QStringLiteral("</strong>: </td><td>") << gateway << QStringLiteral("</td></tr>");
        }

        const QVariantList dnsList = nameservers.toList();
        for (int i = 0; i < dnsList.size(); ++i) {
            const QVariantMap dns = dnsList.at(i).toMap();
            str << QStringLiteral("<tr><td><strong>") << tr("DNS(%1)", "Active connection information").arg(i + 1)
                << QStringLiteral("</strong>: </td><td>") << dns.value(QStringLiteral("address")).toString() << QStringLiteral("</td></tr>");
        }

        const QVariantList routeList = routeData.toList();
        if (!routeList.isEmpty()) {
            str << QStringLiteral("<tr><td><strong>") << tr("Routes", "Active connection information")
                << QStringLiteral("</strong>: </td><td>") << routeList.size() << QStringLiteral("</td></tr>");
        }
    };

    appendIpConfig(tr("IPv4", "Active connection information"), active.ip4ConfigPath, QStringLiteral("org.freedesktop.NetworkManager.IP4Config"));
    appendIpConfig(tr("IPv6", "Active connection information"), active.ip6ConfigPath, QStringLiteral("org.freedesktop.NetworkManager.IP6Config"));

    str << QStringLiteral("</table>");
    return info;
}
