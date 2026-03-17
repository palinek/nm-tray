#include "nm_actions.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QDBusReply>
#include <QProcess>
#include <QVariant>
#include <utility>

namespace
{
constexpr const char *kNmService = "org.freedesktop.NetworkManager";
constexpr const char *kNmPath = "/org/freedesktop/NetworkManager";
constexpr const char *kNmIface = "org.freedesktop.NetworkManager";
constexpr const char *kSettingsPath = "/org/freedesktop/NetworkManager/Settings";
constexpr const char *kSettingsIface = "org.freedesktop.NetworkManager.Settings";
constexpr const char *kSettingsConnIface = "org.freedesktop.NetworkManager.Settings.Connection";
constexpr const char *kDeviceIface = "org.freedesktop.NetworkManager.Device";
constexpr const char *kDeviceWirelessIface = "org.freedesktop.NetworkManager.Device.Wireless";
constexpr const char *kDbusPropsIface = "org.freedesktop.DBus.Properties";

QString trimmedProcessOutput(const QByteArray &data)
{
    return QString::fromLocal8Bit(data).trimmed();
}

QString interfaceNameForDevicePath(const QString &devicePath)
{
    if (devicePath.isEmpty() || devicePath == QStringLiteral("/")) {
        return {};
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(kNmService), devicePath, QString::fromLatin1(kDbusPropsIface), QStringLiteral("Get"));
    msg << QString::fromLatin1(kDeviceIface) << QStringLiteral("Interface");

    const QDBusMessage reply = QDBusConnection::systemBus().call(msg);
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
        return {};
    }

    const auto variant = qvariant_cast<QDBusVariant>(reply.arguments().at(0));
    return variant.variant().toString();
}

bool callAndCheck(QDBusMessage &&msg, QString *errorMessage)
{
    const QDBusMessage reply = QDBusConnection::systemBus().call(msg);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        if (errorMessage) {
            *errorMessage = reply.errorMessage();
        }
        return false;
    }
    return true;
}

} // namespace

namespace nm
{

NmActions::Result callAndCheckExpected(QDBusMessage &&msg)
{
    QString error;
    if (!callAndCheck(std::move(msg), &error)) {
        return std::unexpected(std::move(error));
    }
    return {};
}

NmActions::Result NmActions::activateConnection(const QString &connectionPath, const QString &devicePath, const QString &specificObject)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(kNmService), QString::fromLatin1(kNmPath), QString::fromLatin1(kNmIface), QStringLiteral("ActivateConnection"));
    msg << QDBusObjectPath(connectionPath)
        << QDBusObjectPath(devicePath.isEmpty() ? QStringLiteral("/") : devicePath)
        << QDBusObjectPath(specificObject.isEmpty() ? QStringLiteral("/") : specificObject);
    return callAndCheckExpected(std::move(msg));
}

NmActions::Result NmActions::deactivateConnection(const QString &activeConnectionPath)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(kNmService), QString::fromLatin1(kNmPath), QString::fromLatin1(kNmIface), QStringLiteral("DeactivateConnection"));
    msg << QDBusObjectPath(activeConnectionPath);
    return callAndCheckExpected(std::move(msg));
}

NmActions::Result NmActions::disconnectDevice(const QString &devicePath)
{
    if (devicePath.isEmpty() || devicePath == QStringLiteral("/")) {
        return std::unexpected(QStringLiteral("Invalid device path"));
    }

    QDBusMessage msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(kNmService), devicePath, QString::fromLatin1(kDeviceIface), QStringLiteral("Disconnect"));
    return callAndCheckExpected(std::move(msg));
}

NmActions::Result NmActions::requestScan(const QString &devicePath)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(kNmService), devicePath, QString::fromLatin1(kDeviceWirelessIface), QStringLiteral("RequestScan"));
    msg << QVariantMap{};
    return callAndCheckExpected(std::move(msg));
}

NmActions::Result NmActions::setNetworkingEnabled(bool enabled)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(kNmService), QString::fromLatin1(kNmPath), QString::fromLatin1(kNmIface), QStringLiteral("Enable"));
    msg << enabled;
    return callAndCheckExpected(std::move(msg));
}

NmActions::Result NmActions::setWirelessEnabled(bool enabled)
{
    QDBusMessage setMsg = QDBusMessage::createMethodCall(
        QString::fromLatin1(kNmService), QString::fromLatin1(kNmPath), QString::fromLatin1(kDbusPropsIface), QStringLiteral("Set"));
    setMsg << QString::fromLatin1(kNmIface) << QStringLiteral("WirelessEnabled") << QVariant::fromValue(QDBusVariant(enabled));
    return callAndCheckExpected(std::move(setMsg));
}

NmActions::Result NmActions::setConnectionAutoconnect(const QString &connectionPath, bool enabled)
{
    if (connectionPath.isEmpty() || connectionPath == QStringLiteral("/")) {
        return std::unexpected(QStringLiteral("Invalid connection path"));
    }

    QDBusMessage getSettings = QDBusMessage::createMethodCall(
        QString::fromLatin1(kNmService), connectionPath, QString::fromLatin1(kSettingsConnIface), QStringLiteral("GetSettings"));
    const QDBusMessage reply = QDBusConnection::systemBus().call(getSettings);
    if (reply.type() == QDBusMessage::ErrorMessage || reply.arguments().isEmpty()) {
        return std::unexpected(reply.errorMessage().isEmpty() ? QStringLiteral("GetSettings failed") : reply.errorMessage());
    }

    const QVariantMap settings = qdbus_cast<QVariantMap>(reply.arguments().at(0));
    if (settings.isEmpty()) {
        return std::unexpected(QStringLiteral("Connection settings payload was empty"));
    }

    QVariantMap updated = settings;
    QVariantMap connection = updated.value(QStringLiteral("connection")).toMap();
    connection.insert(QStringLiteral("autoconnect"), enabled);
    updated.insert(QStringLiteral("connection"), QVariant::fromValue(connection));

    QDBusMessage update = QDBusMessage::createMethodCall(
        QString::fromLatin1(kNmService), connectionPath, QString::fromLatin1(kSettingsConnIface), QStringLiteral("Update"));
    update << updated;
    return callAndCheckExpected(std::move(update));
}

NmActions::Result NmActions::addWifiConnection(const QString &ssid, const QString &password, const QString &devicePath, bool secure)
{
    if (ssid.isEmpty() || devicePath.isEmpty()) {
        return std::unexpected(QStringLiteral("SSID and device path are required"));
    }

    const QString trimmedPassword = password.trimmed();
    if (secure && trimmedPassword.isEmpty()) {
        return std::unexpected(QStringLiteral("Password is required for this Wi-Fi network"));
    }

    QStringList args{
        QStringLiteral("--wait"),
        QStringLiteral("20"),
        QStringLiteral("device"),
        QStringLiteral("wifi"),
        QStringLiteral("connect"),
        ssid,
    };

    const QString interfaceName = interfaceNameForDevicePath(devicePath);
    if (!interfaceName.isEmpty()) {
        args << QStringLiteral("ifname") << interfaceName;
    }

    if (secure) {
        args << QStringLiteral("password") << trimmedPassword;
    }

    QProcess nmcli;
    nmcli.start(QStringLiteral("nmcli"), args, QIODevice::ReadOnly);
    if (!nmcli.waitForStarted(5000)) {
        return std::unexpected(QStringLiteral("Failed to start nmcli"));
    }
    if (!nmcli.waitForFinished(30000)) {
        nmcli.kill();
        nmcli.waitForFinished(2000);
        return std::unexpected(QStringLiteral("Timed out while connecting to '%1'").arg(ssid));
    }

    if (nmcli.exitStatus() != QProcess::NormalExit || nmcli.exitCode() != 0) {
        const QString stderrText = trimmedProcessOutput(nmcli.readAllStandardError());
        const QString stdoutText = trimmedProcessOutput(nmcli.readAllStandardOutput());
        const QString message = !stderrText.isEmpty() ? stderrText : (!stdoutText.isEmpty() ? stdoutText : QStringLiteral("nmcli failed"));
        return std::unexpected(message);
    }

    return {};
}

} // namespace nm
