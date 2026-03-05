#include "nm_actions.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QVariant>

namespace
{
constexpr const char *kNmService = "org.freedesktop.NetworkManager";
constexpr const char *kNmPath = "/org/freedesktop/NetworkManager";
constexpr const char *kNmIface = "org.freedesktop.NetworkManager";
constexpr const char *kDeviceWirelessIface = "org.freedesktop.NetworkManager.Device.Wireless";

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

bool NmActions::activateConnection(const QString &connectionPath, const QString &devicePath, const QString &specificObject, QString *errorMessage)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(kNmService), QString::fromLatin1(kNmPath), QString::fromLatin1(kNmIface), QStringLiteral("ActivateConnection"));
    msg << QDBusObjectPath(connectionPath)
        << QDBusObjectPath(devicePath.isEmpty() ? QStringLiteral("/") : devicePath)
        << QDBusObjectPath(specificObject.isEmpty() ? QStringLiteral("/") : specificObject);
    return callAndCheck(std::move(msg), errorMessage);
}

bool NmActions::deactivateConnection(const QString &activeConnectionPath, QString *errorMessage)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(kNmService), QString::fromLatin1(kNmPath), QString::fromLatin1(kNmIface), QStringLiteral("DeactivateConnection"));
    msg << QDBusObjectPath(activeConnectionPath);
    return callAndCheck(std::move(msg), errorMessage);
}

bool NmActions::requestScan(const QString &devicePath, QString *errorMessage)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(kNmService), devicePath, QString::fromLatin1(kDeviceWirelessIface), QStringLiteral("RequestScan"));
    msg << QVariantMap{};
    return callAndCheck(std::move(msg), errorMessage);
}

bool NmActions::setNetworkingEnabled(bool enabled, QString *errorMessage)
{
    QDBusMessage msg = QDBusMessage::createMethodCall(
        QString::fromLatin1(kNmService), QString::fromLatin1(kNmPath), QString::fromLatin1(kNmIface), QStringLiteral("Enable"));
    msg << enabled;
    return callAndCheck(std::move(msg), errorMessage);
}

bool NmActions::setWirelessEnabled(bool enabled, QString *errorMessage)
{
    QDBusMessage setMsg = QDBusMessage::createMethodCall(
        QString::fromLatin1(kNmService), QString::fromLatin1(kNmPath), QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("Set"));
    setMsg << QString::fromLatin1(kNmIface) << QStringLiteral("WirelessEnabled") << QVariant::fromValue(QDBusVariant(enabled));
    return callAndCheck(std::move(setMsg), errorMessage);
}

} // namespace nm
