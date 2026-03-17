#ifndef NM_ACTIONS_H
#define NM_ACTIONS_H

#include <expected>
#include <QString>

namespace nm
{

class NmActions
{
public:
    using Result = std::expected<void, QString>;

    static Result activateConnection(const QString &connectionPath, const QString &devicePath, const QString &specificObject);
    static Result deactivateConnection(const QString &activeConnectionPath);
    static Result disconnectDevice(const QString &devicePath);
    static Result requestScan(const QString &devicePath);
    static Result setNetworkingEnabled(bool enabled);
    static Result setWirelessEnabled(bool enabled);
    static Result setConnectionAutoconnect(const QString &connectionPath, bool enabled);
    static Result addWifiConnection(const QString &ssid, const QString &password, const QString &devicePath, bool secure = false);
};

} // namespace nm

#endif
