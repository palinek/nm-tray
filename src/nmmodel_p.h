/*COPYRIGHT_HEADER

This file is a part of nm-tray.

Copyright (c)
    2015 Palo Kisa <palo.kisa@gmail.com>

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
#if !defined(NMMODEL_P_H)
#define NMMODEL_P_H

#include <NetworkManagerQt/Manager>
#include <NetworkManagerQt/Settings>
#include <NetworkManagerQt/WirelessDevice>

class NmModelPrivate : public QObject
{
    Q_OBJECT
public:
    NmModelPrivate();
    ~NmModelPrivate();

    void removeActiveConnection(int pos);
    void clearActiveConnections();
    void insertActiveConnections();
    void addActiveConnection(NetworkManager::ActiveConnection::Ptr conn);

    void removeConnection(int pos);
    void clearConnections();
    void insertConnections();
    void addConnection(NetworkManager::Connection::Ptr conn);

    void removeDevice(int pos);
    void clearDevices();
    void insertDevices();
    void addDevice(NetworkManager::Device::Ptr conn);

    void removeWifiNetwork(int pos);
    void clearWifiNetworks();
    void insertWifiNetworks();
    void addWifiNetwork(NetworkManager::WirelessNetwork::Ptr net);

    NetworkManager::ActiveConnection::Ptr findActiveConnection(QString const & path);
    template <typename Predicate>
    NetworkManager::Device::Ptr findDevice(Predicate const & pred);
    NetworkManager::Device::Ptr findDeviceUni(QString const & uni);
    NetworkManager::Device::Ptr findDeviceInterface(QString const & interfaceName);
    NetworkManager::WirelessNetwork::Ptr findWifiNetwork(QString const & ssid, QString const & devUni);

Q_SIGNALS:
    void connectionAdd(NetworkManager::Connection::Ptr conn);
    void connectionUpdate(NetworkManager::Connection * conn);
    void connectionRemove(NetworkManager::Connection * conn);
    void activeConnectionAdd(NetworkManager::ActiveConnection::Ptr conn);
    void activeConnectionUpdate(NetworkManager::ActiveConnection * conn);
    void activeConnectionRemove(NetworkManager::ActiveConnection * conn);
    void activeConnectionsReset();
    void deviceAdd(NetworkManager::Device::Ptr dev);
    void deviceUpdate(NetworkManager::Device * dev);
    void deviceRemove(NetworkManager::Device * dev);
    void wifiNetworkAdd(NetworkManager::Device * dev, QString const & ssid);
    void wifiNetworkUpdate(NetworkManager::WirelessNetwork * net);
    void wifiNetworkRemove(NetworkManager::Device * dev, QString const & ssid);


private Q_SLOT:
    //connection
    void onConnectionUpdated();
    void onConnectionRemoved();

    //active connection
    void onActiveConnectionUpdated();

    //device
    void onDeviceUpdated();
    void onWifiNetworkAppeared(QString const & ssid);
    void onWifiNetworkDisappeared(QString const & ssid);

    //wifi network
    void onWifiNetworkUpdated();

    //notifier
    void onDeviceAdded(QString const & uni);
    void onDeviceRemoved(QString const & uni);
    void onActiveConnectionAdded(QString const & path);
    void onActiveConnectionRemoved(QString const & path);
    void onActiveConnectionsChanged();

    //settings notifier
    void onConnectionAdded(QString const & path);
    void onConnectionRemoved(QString const & path);

public:
    NetworkManager::ActiveConnection::List mActiveConns;
    NetworkManager::Connection::List mConnections;
    NetworkManager::Device::List mDevices;
    NetworkManager::WirelessNetwork::List mWifiNets;

};


enum ItemId
{
    ITEM_ROOT = 0x0
        , ITEM_ACTIVE = 0x01
        , ITEM_ACTIVE_LEAF = 0x11
        , ITEM_CONNECTION = 0x2
        , ITEM_CONNECTION_LEAF = 0x21
        , ITEM_DEVICE = 0x3
        , ITEM_DEVICE_LEAF = 0x31
        , ITEM_WIFINET = 0x4
        , ITEM_WIFINET_LEAF = 0x41
};

#endif //NMMODEL_P_H
