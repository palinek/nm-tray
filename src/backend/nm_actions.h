#ifndef NM_ACTIONS_H
#define NM_ACTIONS_H

#include <QString>

namespace nm
{

class NmActions
{
public:
    static bool activateConnection(const QString &connectionPath, const QString &devicePath, const QString &specificObject, QString *errorMessage = nullptr);
    static bool deactivateConnection(const QString &activeConnectionPath, QString *errorMessage = nullptr);
    static bool requestScan(const QString &devicePath, QString *errorMessage = nullptr);
    static bool setNetworkingEnabled(bool enabled, QString *errorMessage = nullptr);
    static bool setWirelessEnabled(bool enabled, QString *errorMessage = nullptr);
};

} // namespace nm

#endif
