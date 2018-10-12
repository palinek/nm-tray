/*COPYRIGHT_HEADER

This file is a part of nm-tray.

Copyright (c)
    2015~now Palo Kisa <palo.kisa@gmail.com>

nm-tray is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

COPYRIGHT_HEADER*/
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
            , ConnectionTypeStringRole
            , ActiveConnectionTypeStringRole = ConnectionTypeStringRole
            , ConnectionUuidRole
            , ActiveConnectionUuidRole = ConnectionUuidRole
            , ConnectionPathRole
            , ActiveConnectionPathRole = ConnectionPathRole
            , ActiveConnectionInfoRole
            , ActiveConnectionStateRole
            , ActiveConnectionMasterRole
            , ActiveConnectionDevicesRole
            , IconSecurityTypeRole
            , IconSecurityRole

            , SignalRole
    };

public:
    explicit NmModel(QObject * parent = nullptr);
    ~NmModel();

    //QAbstractItemModel methods
    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    virtual QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex &index) const override;

    virtual QVariant data(const QModelIndex &index, int role) const override;

    QModelIndex indexTypeRoot(ItemType type) const;

public Q_SLOTS:
    //NetworkManager management methods
    void activateConnection(QModelIndex const & index);
    void deactivateConnection(QModelIndex const & index);
    void requestScan(QModelIndex const & index) const;
    void requestAllWifiScan() const;

private:
    bool isValidDataIndex(const QModelIndex & index) const;
    template <int role>
    QVariant dataRole(const QModelIndex & index) const;

private:
    QScopedPointer<NmModelPrivate> d;
};


#endif //NMMODEL_H
