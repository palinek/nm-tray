#if !defined(NMMODEL_H)
#define NMMODEL_H

#include <QAbstractItemModel>
#include <QThread>
#include <QString>
#include <QStringList>

#include "backend/nm_cache.h"
#include "backend/nm_dbus_client.h"

class WifiPasswordDialog;

class NmModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum class OverallState
    {
        Disconnected,
        Connecting,
        Connected,
        Limited
    };

    enum class PrimaryKind
    {
        Unknown,
        Wifi,
        Wired
    };

    struct ManagerState
    {
        OverallState overallState = OverallState::Disconnected;
        PrimaryKind primaryKind = PrimaryKind::Unknown;
        QString primaryName;
        int wifiStrength = -1;
        QStringList vpnActive;
        QString lastError;
        bool networkingEnabled = false;
        bool wirelessEnabled = false;
        bool wirelessHardwareEnabled = false;
        QString primaryConnectionPath;
    };

    struct RecentConnection
    {
        QString id;
        QString connectionPath;
        qint64 lastUsedTimestamp = 0;
    };

public:
    enum ItemType
    {
        HelperType,
        ActiveConnectionType,
        ConnectionType,
        DeviceType,
        WifiNetworkType
    };

    enum ItemRole
    {
        ItemTypeRole = Qt::UserRole + 1,
        NameRole,

        IconTypeRole,
        IconRole,
        ConnectionTypeRole,
        ActiveConnectionTypeRole = ConnectionTypeRole,
        ConnectionTypeStringRole,
        ActiveConnectionTypeStringRole = ConnectionTypeStringRole,
        ConnectionUuidRole,
        ActiveConnectionUuidRole = ConnectionUuidRole,
        ConnectionPathRole,
        ActiveConnectionPathRole = ConnectionPathRole,
        ActiveConnectionInfoRole,
        ActiveConnectionStateRole,
        ActiveConnectionMasterRole,
        ActiveConnectionDevicesRole,
        IconSecurityTypeRole,
        IconSecurityRole,
        SignalRole,
        SavedConnectionPathRole,
        AutoConnectRole,
        AutoConnectSupportedRole
    };

    enum ActiveConnectionState
    {
        ActiveUnknown = 0,
        ActiveActivating = 1,
        ActiveActivated = 2,
        ActiveDeactivating = 3,
        ActiveDeactivated = 4
    };

public:
    explicit NmModel(QObject *parent = nullptr);
    ~NmModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &index) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    QModelIndex indexTypeRoot(ItemType type) const;

    ManagerState managerState() const;
    bool networkingEnabled() const;
    bool wirelessEnabled() const;
    bool wirelessHardwareEnabled() const;
    QString primaryConnectionPath() const;
    bool showLowSignalNetworks() const;
    QList<RecentConnection> recentConnections(int maxCount = 3) const;

Q_SIGNALS:
    void managerStateChanged();

public Q_SLOTS:
    void activateConnection(const QModelIndex &index);
    void deactivateConnection(const QModelIndex &index);
    void requestScan(const QModelIndex &index) const;
    void requestAllWifiScan() const;
    void setNetworkingEnabled(bool enabled);
    void setWirelessEnabled(bool enabled);
    void setConnectionAutoconnect(const QString &connectionPath, bool enabled);
    void setShowLowSignalNetworks(bool enabled);
    void disconnectPrimaryConnection();
    void activateConnectionPath(const QString &connectionPath);
    void onSnapshotChanged(const nm::Snapshot &snapshot);
    void promptAndCreateWifiConnection(const QString &ssid, const QString &devicePath, bool secure);

private:
    enum ItemId
    {
        ITEM_ROOT = 0x0,
        ITEM_ACTIVE = 0x01,
        ITEM_ACTIVE_LEAF = 0x11,
        ITEM_CONNECTION = 0x2,
        ITEM_CONNECTION_LEAF = 0x21,
        ITEM_DEVICE = 0x3,
        ITEM_DEVICE_LEAF = 0x31,
        ITEM_WIFINET = 0x4,
        ITEM_WIFINET_LEAF = 0x41
    };

    bool isValidDataIndex(const QModelIndex &index) const;
    void rebuildFromSnapshot(const nm::Snapshot &snapshot);
    QString buildActiveInfo(const nm::ActiveConnectionRecord &active) const;
    bool disconnectActiveConnection(const nm::ActiveConnectionRecord &active);

private:
    QThread mDbusThread;
    nm::NmDbusClient *mDbus = nullptr;
    nm::NmCache mCache;

    QList<nm::ActiveConnectionRecord> mActive;
    QList<nm::ConnectionViewRecord> mConnections;
    QList<nm::DeviceRecord> mDevices;
    QList<nm::WifiViewRecord> mWifi;
    ManagerState mManagerState;
    bool mShowLowSignalNetworks = false;
};

#endif // NMMODEL_H
