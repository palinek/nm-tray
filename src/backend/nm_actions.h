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
    static Result requestScan(const QString &devicePath);
    static Result setNetworkingEnabled(bool enabled);
    static Result setWirelessEnabled(bool enabled);
};

} // namespace nm

#endif
