#ifndef NM_DBUS_CLIENT_H
#define NM_DBUS_CLIENT_H

#include "nm_types.h"

#include <QObject>
#include <QSet>
#include <QTimer>

namespace nm
{

class NmDbusClient : public QObject
{
    Q_OBJECT

public:
    explicit NmDbusClient(QObject *parent = nullptr);

Q_SIGNALS:
    void snapshotChanged(const Snapshot &snapshot);
    void managerStateChanged();

public Q_SLOTS:
    void start();
    void refreshNow();

private Q_SLOTS:
    void onManagerSignal();
    void onPropertiesChanged(QString interfaceName, QVariantMap changedProperties, QStringList invalidatedProperties);
    void scheduleRefresh();

private:
    void registerSignals();
    void refreshSnapshot();
    void updateDynamicPropertySubscriptions(const Snapshot &snapshot);

    Snapshot mSnapshot;
    QTimer mRefreshDebounce;
    bool mStarted = false;
    bool mRefreshInProgress = false;
    bool mRefreshQueued = false;
    QSet<QString> mDynamicPropertyPaths;
};

} // namespace nm

#endif
