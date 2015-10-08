#if !defined(NMMODEL_H)
#define NMMODEL_H

#include <QAbstractItemModel>

class NmModelPrivate;

class NmModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum ItemType
    {
        HelperType
            , ActiveConnectionType
            , ConnectionType
            , DeviceType
            , WifiNetworkType
    };
    enum ItemRole
    {
        ItemTypeRole = Qt::UserRole + 1
            , NameRole

            , IconTypeRole
            , IconRole
            , ConnectionTypeRole
            , ActiveConnectionTypeRole = ConnectionTypeRole
            , ConnectionUuidRole
            , ActiveConnectionUuidRole = ConnectionUuidRole
            , ActiveConnectionStateRole

            , SignalRole
    };

public:
    explicit NmModel(QObject * parent = nullptr);
    ~NmModel();

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex &index) const override;

    virtual QVariant data(const QModelIndex &index, int role) const override;

    QModelIndex indexTypeFirst(ItemType type);


private:
    bool isValidDataIndex(const QModelIndex & index) const;
    template <int role>
    QVariant dataRole(const QModelIndex & index) const;

private:
    QScopedPointer<NmModelPrivate> d;
};


#endif //NMMODEL_H
