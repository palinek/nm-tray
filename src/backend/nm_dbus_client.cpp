#include "nm_dbus_client.h"
#include "../log.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QDateTime>
#include <QSet>

namespace
{
constexpr const char *kNmService = "org.freedesktop.NetworkManager";
constexpr const char *kNmPath = "/org/freedesktop/NetworkManager";
constexpr const char *kNmIface = "org.freedesktop.NetworkManager";
constexpr const char *kSettingsPath = "/org/freedesktop/NetworkManager/Settings";
constexpr const char *kSettingsIface = "org.freedesktop.NetworkManager.Settings";
constexpr const char *kDeviceIface = "org.freedesktop.NetworkManager.Device";
constexpr const char *kDeviceWirelessIface = "org.freedesktop.NetworkManager.Device.Wireless";
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

} // namespace

namespace nm
{

NmDbusClient::NmDbusClient(QObject *parent)
    : QObject(parent)
{
    mRefreshDebounce.setSingleShot(true);
    mRefreshDebounce.setInterval(120);
    connect(&mRefreshDebounce, &QTimer::timeout, this, &NmDbusClient::refreshSnapshot);

    registerSignals();
    refreshSnapshot();
}

const Snapshot &NmDbusClient::snapshot() const
{
    return mSnapshot;
}

void NmDbusClient::refreshNow()
{
    refreshSnapshot();
}

void NmDbusClient::scheduleRefresh()
{
    qCDebug(NM_TRAY) << "nm-dbus: scheduling snapshot refresh";
    mRefreshDebounce.start();
}

void NmDbusClient::registerSignals()
{
    QDBusConnection bus = QDBusConnection::systemBus();

    bus.connect(QString::fromLatin1(kNmService), QString::fromLatin1(kNmPath), QString::fromLatin1(kNmIface), QStringLiteral("StateChanged"), this, SLOT(onManagerSignal()));
    bus.connect(QString::fromLatin1(kNmService), QString::fromLatin1(kNmPath), QString::fromLatin1(kNmIface), QStringLiteral("DeviceAdded"), this, SLOT(onManagerSignal()));
    bus.connect(QString::fromLatin1(kNmService), QString::fromLatin1(kNmPath), QString::fromLatin1(kNmIface), QStringLiteral("DeviceRemoved"), this, SLOT(onManagerSignal()));
    bus.connect(QString::fromLatin1(kNmService), QString::fromLatin1(kNmPath), QString::fromLatin1(kDbusPropsIface), QStringLiteral("PropertiesChanged"), this, SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));
    bus.connect(QString::fromLatin1(kNmService), QString(), QString::fromLatin1(kDbusPropsIface), QStringLiteral("PropertiesChanged"), this, SLOT(onPropertiesChanged(QString,QVariantMap,QStringList)));
    bus.connect(QString::fromLatin1(kNmService), QString::fromLatin1(kSettingsPath), QString::fromLatin1(kSettingsIface), QStringLiteral("NewConnection"), this, SLOT(onManagerSignal()));
    bus.connect(QString::fromLatin1(kNmService), QString::fromLatin1(kSettingsPath), QString::fromLatin1(kSettingsIface), QStringLiteral("ConnectionRemoved"), this, SLOT(onManagerSignal()));
    bus.connect(QString::fromLatin1(kNmService), QString(), QString::fromLatin1(kDeviceWirelessIface), QStringLiteral("AccessPointAdded"), this, SLOT(onManagerSignal()));
    bus.connect(QString::fromLatin1(kNmService), QString(), QString::fromLatin1(kDeviceWirelessIface), QStringLiteral("AccessPointRemoved"), this, SLOT(onManagerSignal()));
}

void NmDbusClient::onManagerSignal()
{
    qCDebug(NM_TRAY) << "nm-dbus: manager signal received";
    scheduleRefresh();
}

void NmDbusClient::onPropertiesChanged(QString interfaceName, QVariantMap changedProperties, QStringList invalidatedProperties)
{
    Q_UNUSED(invalidatedProperties);
    static const QSet<QString> kRelevantIfaces{
        QString::fromLatin1(kNmIface),
        QString::fromLatin1(kDeviceIface),
        QString::fromLatin1(kDeviceWirelessIface),
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

    qCDebug(NM_TRAY) << "nm-dbus: properties changed on interface" << interfaceName;
    scheduleRefresh();
}

void NmDbusClient::refreshSnapshot()
{
    qCDebug(NM_TRAY) << "nm-dbus: refreshing snapshot";
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
            ap.ssid = decodeSsid(apProps.value(QStringLiteral("Ssid")));
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

    mSnapshot = std::move(next);
    qCDebug(NM_TRAY) << "nm-dbus: snapshot ready"
                     << "devices=" << mSnapshot.devices.size()
                     << "aps=" << mSnapshot.accessPoints.size()
                     << "saved=" << mSnapshot.savedConnections.size()
                     << "active=" << mSnapshot.activeConnections.size();
    if (managerChanged) {
        qCDebug(NM_TRAY) << "nm-dbus: manager state changed";
        emit managerStateChanged();
    }
    emit snapshotChanged();
}

} // namespace nm
