#ifndef NM_DBUS_CLIENT_H
#define NM_DBUS_CLIENT_H

#include "nm_types.h"

#include <QObject>
#include <QTimer>

namespace nm
{

class NmDbusClient : public QObject
{
    Q_OBJECT

public:
    explicit NmDbusClient(QObject *parent = nullptr);

    const Snapshot &snapshot() const;
    void refreshNow();

Q_SIGNALS:
    void snapshotChanged();
    void managerStateChanged();

private Q_SLOTS:
    void onManagerSignal();
    void onPropertiesChanged(QString interfaceName, QVariantMap changedProperties, QStringList invalidatedProperties);
    void scheduleRefresh();

private:
    void registerSignals();
    void refreshSnapshot();

    Snapshot mSnapshot;
    QTimer mRefreshDebounce;
};

} // namespace nm

#endif
