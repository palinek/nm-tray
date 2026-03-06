#include "nm_dbus_client.h"
#include "../log.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDateTime>
#include <QMetaObject>
#include <QSet>
#include <QThread>
#include <utility>

namespace
{
constexpr const char *kNmService = "org.freedesktop.NetworkManager";
constexpr const char *kNmPath = "/org/freedesktop/NetworkManager";
constexpr const char *kNmIface = "org.freedesktop.NetworkManager";
constexpr const char *kSettingsPath = "/org/freedesktop/NetworkManager/Settings";
constexpr const char *kSettingsIface = "org.freedesktop.NetworkManager.Settings";
constexpr const char *kDeviceIface = "org.freedesktop.NetworkManager.Device";
constexpr const char *kDeviceWirelessIface = "org.freedesktop.NetworkManager.Device.Wireless";
constexpr const char *kDeviceStatsIface = "org.freedesktop.NetworkManager.Device.Statistics";
constexpr const char *kAccessPointIface = "org.freedesktop.NetworkManager.AccessPoint";
constexpr const char *kActiveConnIface = "org.freedesktop.NetworkManager.Connection.Active";
constexpr const char *kSettingsConnIface = "org.freedesktop.NetworkManager.Settings.Connection";
constexpr const char *kDbusPropsIface = "org.freedesktop.DBus.Properties";

QVariantMap dbusGetAll(const QString &path, const QString &iface)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(kNmService), path, QString::fromLatin1(kDbusPropsIface), QStringLiteral("GetAll"));
    msg << iface;
    const QDBusMessage reply = QDBusConnection::systemBus().call(msg);
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
        return {};
    }
    return qdbus_cast<QVariantMap>(reply.arguments().at(0));
}

QList<QString> toObjectPathList(const QVariant &v)
{
    QList<QString> paths;
    QList<QDBusObjectPath> list;
    if (v.canConvert<QDBusArgument>()) {
        list = qdbus_cast<QList<QDBusObjectPath>>(v.value<QDBusArgument>());
    } else {
        list = qdbus_cast<QList<QDBusObjectPath>>(v);
    }
    paths.reserve(list.size());
    for (const auto &p : list) {
        paths.push_back(p.path());
    }
    return paths;
}

QString toObjectPath(const QVariant &v)
{
    if (v.canConvert<QDBusObjectPath>()) {
        return v.value<QDBusObjectPath>().path();
    }
    if (v.canConvert<QDBusArgument>()) {
        return qdbus_cast<QDBusObjectPath>(v.value<QDBusArgument>()).path();
    }
    return {};
}

QString decodeSsid(const QVariant &value)
{
    const auto bytes = value.toByteArray();
    if (bytes.isEmpty()) {
        return QObject::tr("<hidden>");
    }
    return QString::fromUtf8(bytes);
}

QVariantMap section(const QVariantMap &m, const QString &key)
{
    return m.value(key).toMap();
}

QStringList addressListFromData(const QVariant &value)
{
    QStringList out;
    const QVariantList list = value.toList();
    out.reserve(list.size());
    for (const QVariant &entry : list) {
        const QVariantMap map = entry.toMap();
        const QString address = map.value(QStringLiteral("address")).toString();
        if (!address.isEmpty()) {
            out.push_back(address);
        }
    }
    return out;
}

bool managerEquivalent(const nm::ManagerState &a, const nm::ManagerState &b)
{
    return a.networkingEnabled == b.networkingEnabled
        && a.wirelessEnabled == b.wirelessEnabled
        && a.wirelessHardwareEnabled == b.wirelessHardwareEnabled
        && a.primaryConnectionPath == b.primaryConnectionPath
        && a.state == b.state
        && a.connectivity == b.connectivity
        && a.lastError == b.lastError;
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

bool accessPointEquivalent(const nm::AccessPointRecord &a, const nm::AccessPointRecord &b)
{
    return a.path == b.path
        && a.devicePath == b.devicePath
        && a.ssid == b.ssid
        && a.ssidBytes == b.ssidBytes
        && a.bssid == b.bssid
        && a.strength == b.strength
        && a.flags == b.flags
        && a.wpaFlags == b.wpaFlags
        && a.rsnFlags == b.rsnFlags
        && a.frequency == b.frequency
        && a.privacy == b.privacy;
}

bool savedConnectionEquivalent(const nm::SavedConnectionRecord &a, const nm::SavedConnectionRecord &b)
{
    return a.path == b.path
        && a.id == b.id
        && a.wifiSsid == b.wifiSsid
        && a.wifiSsidBytes == b.wifiSsidBytes
        && a.uuid == b.uuid
        && a.type == b.type
        && a.interfaceName == b.interfaceName
        && a.timestamp == b.timestamp
        && a.autoconnectPriority == b.autoconnectPriority
        && a.autoconnect == b.autoconnect;
}

bool activeConnectionEquivalent(const nm::ActiveConnectionRecord &a, const nm::ActiveConnectionRecord &b)
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

template <typename T, typename Eq>
bool mapEquivalent(const QMap<QString, T> &a, const QMap<QString, T> &b, Eq eq)
{
    if (a.size() != b.size()) {
        return false;
    }
    auto itA = a.cbegin();
    auto itB = b.cbegin();
    while (itA != a.cend() && itB != b.cend()) {
        if (itA.key() != itB.key() || !eq(itA.value(), itB.value())) {
            return false;
        }
        ++itA;
        ++itB;
    }
    return true;
}

bool snapshotEquivalent(const nm::Snapshot &a, const nm::Snapshot &b)
{
    return managerEquivalent(a.manager, b.manager)
        && mapEquivalent(a.devices, b.devices, deviceEquivalent)
        && mapEquivalent(a.accessPoints, b.accessPoints, accessPointEquivalent)
        && mapEquivalent(a.savedConnections, b.savedConnections, savedConnectionEquivalent)
        && mapEquivalent(a.activeConnections, b.activeConnections, activeConnectionEquivalent);
}

} // namespace

namespace nm
{

NmDbusClient::NmDbusClient(QObject *parent)
    : QObject(parent)
{
    mRefreshDebounce.setParent(this);
}

void NmDbusClient::start()
{
    if (mStarted) {
        return;
    }
    mStarted = true;

    mRefreshDebounce.setSingleShot(true);
    mRefreshDebounce.setInterval(120);
    connect(&mRefreshDebounce, &QTimer::timeout, this, &NmDbusClient::refreshSnapshot);

    registerSignals();
    refreshSnapshot();
}

void NmDbusClient::refreshNow()
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "refreshNow", Qt::QueuedConnection);
        return;
    }
    refreshSnapshot();
}

void NmDbusClient::scheduleRefresh()
{
    if (QThread::currentThread() != thread()) {
        QMetaObject::invokeMethod(this, "scheduleRefresh", Qt::QueuedConnection);
        return;
    }
    if (!mStarted) {
        return;
    }
    if (mRefreshInProgress) {
        mRefreshQueued = true;
        return;
    }
    mRefreshDebounce.start();
}

void NmDbusClient::registerSignals()
{
    QDBusConnection bus = QDBusConnection::systemBus();

    bus.connect(QString::fromLatin1(kNmService), QString::fromLatin1(kNmPath), QString::fromLatin1(kNmIface), QStringLiteral("StateChanged"), this, SLOT(onManagerSignal()));
    bus.connect(QString::fromLatin1(kNmService), QString::fromLatin1(kNmPath), QString::fromLatin1(kNmIface), QStringLiteral("DeviceAdded"), this, SLOT(onManagerSignal()));
    bus.connect(QString::fromLatin1(kNmService), QString::fromLatin1(kNmPath), QString::fromLatin1(kNmIface), QStringLiteral("DeviceRemoved"), this, SLOT(onManagerSignal()));
    bus.connect(QString::fromLatin1(kNmService), QString::fromLatin1(kNmPath), QString::fromLatin1(kDbusPropsIface), QStringLiteral("PropertiesChanged"), this, SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));
    bus.connect(QString::fromLatin1(kNmService), QString::fromLatin1(kSettingsPath), QString::fromLatin1(kSettingsIface), QStringLiteral("NewConnection"), this, SLOT(onManagerSignal()));
    bus.connect(QString::fromLatin1(kNmService), QString::fromLatin1(kSettingsPath), QString::fromLatin1(kSettingsIface), QStringLiteral("ConnectionRemoved"), this, SLOT(onManagerSignal()));
    bus.connect(QString::fromLatin1(kNmService), QString(), QString::fromLatin1(kSettingsConnIface), QStringLiteral("Updated"), this, SLOT(onManagerSignal()));
    bus.connect(QString::fromLatin1(kNmService), QString(), QString::fromLatin1(kDeviceWirelessIface), QStringLiteral("AccessPointAdded"), this, SLOT(onManagerSignal()));
    bus.connect(QString::fromLatin1(kNmService), QString(), QString::fromLatin1(kDeviceWirelessIface), QStringLiteral("AccessPointRemoved"), this, SLOT(onManagerSignal()));
}

void NmDbusClient::onManagerSignal()
{
    scheduleRefresh();
}

void NmDbusClient::onPropertiesChanged(QString interfaceName, QVariantMap changedProperties, QStringList invalidatedProperties)
{
    Q_UNUSED(invalidatedProperties);
    static const QSet<QString> kRelevantIfaces{
        QString::fromLatin1(kNmIface),
        QString::fromLatin1(kDeviceIface),
        QString::fromLatin1(kDeviceWirelessIface),
        QString::fromLatin1(kDeviceStatsIface),
        QString::fromLatin1(kAccessPointIface),
        QString::fromLatin1(kActiveConnIface),
        QString::fromLatin1(kSettingsConnIface),
    };
    if (!kRelevantIfaces.contains(interfaceName)) {
        return;
    }

    static const QSet<QString> kRelevantManagerKeys{
        QStringLiteral("PrimaryConnection"),
        QStringLiteral("ActiveConnections"),
        QStringLiteral("State"),
        QStringLiteral("Connectivity"),
        QStringLiteral("NetworkingEnabled"),
        QStringLiteral("WirelessEnabled"),
        QStringLiteral("WirelessHardwareEnabled"),
        QStringLiteral("AllDevices"),
    };
    static const QSet<QString> kRelevantDeviceKeys{
        QStringLiteral("State"),
        QStringLiteral("ActiveConnection"),
        QStringLiteral("Interface"),
        QStringLiteral("IpInterface"),
        QStringLiteral("DeviceType"),
    };
    static const QSet<QString> kRelevantWifiDevKeys{
        QStringLiteral("ActiveAccessPoint"),
        QStringLiteral("AccessPoints"),
        QStringLiteral("Bitrate"),
    };
    static const QSet<QString> kRelevantDeviceStatsKeys{
        QStringLiteral("RxBytes"),
        QStringLiteral("TxBytes"),
    };
    static const QSet<QString> kRelevantApKeys{
        QStringLiteral("Ssid"),
        QStringLiteral("Strength"),
        QStringLiteral("Flags"),
        QStringLiteral("WpaFlags"),
        QStringLiteral("RsnFlags"),
        QStringLiteral("Frequency"),
    };
    static const QSet<QString> kRelevantActiveConnKeys{
        QStringLiteral("Connection"),
        QStringLiteral("SpecificObject"),
        QStringLiteral("Id"),
        QStringLiteral("Uuid"),
        QStringLiteral("Type"),
        QStringLiteral("Devices"),
        QStringLiteral("State"),
        QStringLiteral("Vpn"),
        QStringLiteral("Default"),
        QStringLiteral("Default6"),
        QStringLiteral("Ip4Config"),
        QStringLiteral("Ip6Config"),
    };
    static const QSet<QString> kRelevantSettingsConnKeys{
        QStringLiteral("Unsaved"),
        QStringLiteral("Flags"),
    };

    const QSet<QString> *relevantKeys = nullptr;
    if (interfaceName == QString::fromLatin1(kNmIface)) {
        relevantKeys = &kRelevantManagerKeys;
    } else if (interfaceName == QString::fromLatin1(kDeviceIface)) {
        relevantKeys = &kRelevantDeviceKeys;
    } else if (interfaceName == QString::fromLatin1(kDeviceWirelessIface)) {
        relevantKeys = &kRelevantWifiDevKeys;
    } else if (interfaceName == QString::fromLatin1(kDeviceStatsIface)) {
        relevantKeys = &kRelevantDeviceStatsKeys;
    } else if (interfaceName == QString::fromLatin1(kAccessPointIface)) {
        relevantKeys = &kRelevantApKeys;
    } else if (interfaceName == QString::fromLatin1(kActiveConnIface)) {
        relevantKeys = &kRelevantActiveConnKeys;
    } else if (interfaceName == QString::fromLatin1(kSettingsConnIface)) {
        relevantKeys = &kRelevantSettingsConnKeys;
    }

    bool hasRelevantKey = false;
    for (auto it = changedProperties.cbegin(); it != changedProperties.cend(); ++it) {
        if (relevantKeys == nullptr || relevantKeys->contains(it.key())) {
            hasRelevantKey = true;
            break;
        }
    }
    if (!hasRelevantKey) {
        return;
    }

    scheduleRefresh();
}

void NmDbusClient::refreshSnapshot()
{
    if (!mStarted) {
        return;
    }
    mRefreshInProgress = true;
    Snapshot next;
    next.collectedAt = QDateTime::currentDateTimeUtc();

    const QVariantMap nmProps = dbusGetAll(QString::fromLatin1(kNmPath), QString::fromLatin1(kNmIface));
    next.manager.networkingEnabled = nmProps.value(QStringLiteral("NetworkingEnabled")).toBool();
    next.manager.wirelessEnabled = nmProps.value(QStringLiteral("WirelessEnabled")).toBool();
    next.manager.wirelessHardwareEnabled = nmProps.value(QStringLiteral("WirelessHardwareEnabled")).toBool();
    next.manager.primaryConnectionPath = toObjectPath(nmProps.value(QStringLiteral("PrimaryConnection")));
    next.manager.state = nmProps.value(QStringLiteral("State")).toUInt();
    next.manager.connectivity = nmProps.value(QStringLiteral("Connectivity")).toUInt();

    const auto devicePaths = toObjectPathList(nmProps.value(QStringLiteral("AllDevices")));
    for (const QString &path : devicePaths) {
        DeviceRecord dev;
        dev.path = path;
        const QVariantMap devProps = dbusGetAll(path, QString::fromLatin1(kDeviceIface));
        dev.interfaceName = devProps.value(QStringLiteral("Interface")).toString();
        dev.ipInterfaceName = devProps.value(QStringLiteral("IpInterface")).toString();
        dev.type = static_cast<DeviceType>(devProps.value(QStringLiteral("DeviceType")).toUInt());
        dev.state = devProps.value(QStringLiteral("State")).toUInt();
        dev.activeConnectionPath = toObjectPath(devProps.value(QStringLiteral("ActiveConnection")));

        if (dev.type == DeviceType::Wifi) {
            const QVariantMap wifiProps = dbusGetAll(path, QString::fromLatin1(kDeviceWirelessIface));
            dev.activeAccessPointPath = toObjectPath(wifiProps.value(QStringLiteral("ActiveAccessPoint")));
            dev.accessPointPaths = toObjectPathList(wifiProps.value(QStringLiteral("AccessPoints")));
            dev.hardwareAddress = wifiProps.value(QStringLiteral("HwAddress")).toString();
            dev.bitrateKbps = wifiProps.value(QStringLiteral("Bitrate")).toInt();
        }
        const QVariantMap devStats = dbusGetAll(path, QString::fromLatin1(kDeviceStatsIface));
        dev.rxBytes = devStats.value(QStringLiteral("RxBytes")).toULongLong();
        dev.txBytes = devStats.value(QStringLiteral("TxBytes")).toULongLong();

        next.devices.insert(path, dev);
    }

    for (const auto &dev : next.devices) {
        if (dev.type != DeviceType::Wifi) {
            continue;
        }
        for (const QString &apPath : dev.accessPointPaths) {
            AccessPointRecord ap;
            ap.path = apPath;
            ap.devicePath = dev.path;
            const QVariantMap apProps = dbusGetAll(apPath, QString::fromLatin1(kAccessPointIface));
            ap.ssidBytes = apProps.value(QStringLiteral("Ssid")).toByteArray();
            ap.ssid = decodeSsid(apProps.value(QStringLiteral("Ssid")));
            ap.bssid = apProps.value(QStringLiteral("HwAddress")).toString();
            ap.strength = apProps.value(QStringLiteral("Strength")).toInt();
            ap.flags = apProps.value(QStringLiteral("Flags")).toUInt();
            ap.wpaFlags = apProps.value(QStringLiteral("WpaFlags")).toUInt();
            ap.rsnFlags = apProps.value(QStringLiteral("RsnFlags")).toUInt();
            ap.frequency = apProps.value(QStringLiteral("Frequency")).toUInt();
            ap.privacy = (ap.flags & 0x1U) != 0U;
            next.accessPoints.insert(apPath, ap);
        }
    }

    const QVariantMap settingsProps = dbusGetAll(QString::fromLatin1(kSettingsPath), QString::fromLatin1(kSettingsIface));
    const auto settingsPaths = toObjectPathList(settingsProps.value(QStringLiteral("Connections")));
    for (const QString &path : settingsPaths) {
        QDBusInterface connIf(QString::fromLatin1(kNmService), path, QString::fromLatin1(kSettingsConnIface), QDBusConnection::systemBus());
        QDBusMessage reply = connIf.call(QStringLiteral("GetSettings"));
        if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
            continue;
        }

        SavedConnectionRecord conn;
        conn.path = path;

        const QVariantMap settings = qdbus_cast<QVariantMap>(reply.arguments().at(0));
        const QVariantMap connection = section(settings, QStringLiteral("connection"));
        const QVariantMap wifi = section(settings, QStringLiteral("802-11-wireless"));

        conn.id = connection.value(QStringLiteral("id")).toString();
        conn.wifiSsidBytes = wifi.value(QStringLiteral("ssid")).toByteArray();
        conn.wifiSsid = decodeSsid(wifi.value(QStringLiteral("ssid")));
        conn.uuid = connection.value(QStringLiteral("uuid")).toString();
        conn.type = connection.value(QStringLiteral("type")).toString();
        conn.interfaceName = connection.value(QStringLiteral("interface-name")).toString();
        conn.timestamp = connection.value(QStringLiteral("timestamp")).toLongLong();
        conn.autoconnectPriority = connection.value(QStringLiteral("autoconnect-priority")).toInt();
        conn.autoconnect = connection.value(QStringLiteral("autoconnect"), true).toBool();
        if (conn.id.isEmpty() && !conn.wifiSsid.isEmpty()) {
            conn.id = conn.wifiSsid;
        }
        next.savedConnections.insert(path, conn);
    }

    const auto activePaths = toObjectPathList(nmProps.value(QStringLiteral("ActiveConnections")));
    for (const QString &path : activePaths) {
        ActiveConnectionRecord active;
        active.path = path;
        const QVariantMap activeProps = dbusGetAll(path, QString::fromLatin1(kActiveConnIface));
        active.connectionPath = toObjectPath(activeProps.value(QStringLiteral("Connection")));
        active.specificObjectPath = toObjectPath(activeProps.value(QStringLiteral("SpecificObject")));
        active.id = activeProps.value(QStringLiteral("Id")).toString();
        active.uuid = activeProps.value(QStringLiteral("Uuid")).toString();
        active.type = activeProps.value(QStringLiteral("Type")).toString();
        active.devices = toObjectPathList(activeProps.value(QStringLiteral("Devices")));
        active.state = static_cast<ActiveState>(activeProps.value(QStringLiteral("State")).toUInt());
        active.isVpn = activeProps.value(QStringLiteral("Vpn")).toBool();
        active.isDefault4 = activeProps.value(QStringLiteral("Default")).toBool();
        active.isDefault6 = activeProps.value(QStringLiteral("Default6")).toBool();
        active.ip4ConfigPath = toObjectPath(activeProps.value(QStringLiteral("Ip4Config")));
        active.ip6ConfigPath = toObjectPath(activeProps.value(QStringLiteral("Ip6Config")));
        const QVariantMap ip4Props = dbusGetAll(active.ip4ConfigPath, QStringLiteral("org.freedesktop.NetworkManager.IP4Config"));
        active.ip4Addresses = addressListFromData(ip4Props.value(QStringLiteral("AddressData")));
        active.ip4Gateway = ip4Props.value(QStringLiteral("Gateway")).toString();
        active.ip4Dns = addressListFromData(ip4Props.value(QStringLiteral("NameserverData")));
        active.ip4RouteCount = ip4Props.value(QStringLiteral("RouteData")).toList().size();

        const QVariantMap ip6Props = dbusGetAll(active.ip6ConfigPath, QStringLiteral("org.freedesktop.NetworkManager.IP6Config"));
        active.ip6Addresses = addressListFromData(ip6Props.value(QStringLiteral("AddressData")));
        active.ip6Gateway = ip6Props.value(QStringLiteral("Gateway")).toString();
        active.ip6Dns = addressListFromData(ip6Props.value(QStringLiteral("NameserverData")));
        active.ip6RouteCount = ip6Props.value(QStringLiteral("RouteData")).toList().size();

        next.activeConnections.insert(path, active);
    }

    next.manager.lastError.clear();

    const bool managerChanged =
        next.manager.networkingEnabled != mSnapshot.manager.networkingEnabled ||
        next.manager.wirelessEnabled != mSnapshot.manager.wirelessEnabled ||
        next.manager.wirelessHardwareEnabled != mSnapshot.manager.wirelessHardwareEnabled ||
        next.manager.primaryConnectionPath != mSnapshot.manager.primaryConnectionPath ||
        next.manager.state != mSnapshot.manager.state ||
        next.manager.connectivity != mSnapshot.manager.connectivity;

    const bool changed = !snapshotEquivalent(next, mSnapshot);
    updateDynamicPropertySubscriptions(next);
    mSnapshot = std::move(next);
    if (managerChanged) {
        emit managerStateChanged();
    }
    if (changed) {
        emit snapshotChanged(mSnapshot);
    }
    mRefreshInProgress = false;
    if (mRefreshQueued) {
        mRefreshQueued = false;
        scheduleRefresh();
    }
}

void NmDbusClient::updateDynamicPropertySubscriptions(const Snapshot &snapshot)
{
    QSet<QString> nextPaths;
    for (const auto &device : snapshot.devices) {
        nextPaths.insert(device.path);
    }
    for (const auto &ap : snapshot.accessPoints) {
        nextPaths.insert(ap.path);
    }
    for (const auto &active : snapshot.activeConnections) {
        nextPaths.insert(active.path);
    }
    for (const auto &saved : snapshot.savedConnections) {
        nextPaths.insert(saved.path);
    }

    QDBusConnection bus = QDBusConnection::systemBus();

    for (const QString &path : nextPaths) {
        if (mDynamicPropertyPaths.contains(path)) {
            continue;
        }
        bus.connect(QString::fromLatin1(kNmService),
                    path,
                    QString::fromLatin1(kDbusPropsIface),
                    QStringLiteral("PropertiesChanged"),
                    this,
                    SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));
    }

    for (const QString &path : std::as_const(mDynamicPropertyPaths)) {
        if (nextPaths.contains(path)) {
            continue;
        }
        bus.disconnect(QString::fromLatin1(kNmService),
                       path,
                       QString::fromLatin1(kDbusPropsIface),
                       QStringLiteral("PropertiesChanged"),
                       this,
                       SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));
    }

    mDynamicPropertyPaths = std::move(nextPaths);
}

} // namespace nm
