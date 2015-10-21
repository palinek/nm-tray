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
#include "nmmodel.h"
#include "nmmodel_p.h"
#include "icons.h"

#include <NetworkManagerQt/VpnConnection>
#include <NetworkManagerQt/WirelessDevice>
#include <NetworkManagerQt/WiredDevice>
#include <NetworkManagerQt/WimaxDevice>
#include <NetworkManagerQt/WirelessSetting>
/*
#include <NetworkManagerQt/AdslSetting>
#include <NetworkManagerQt/CdmaSetting>
#include <NetworkManagerQt/GsmSetting>
#include <NetworkManagerQt/InfinibandSetting>
#include <NetworkManagerQt/Ipv4Setting>
#include <NetworkManagerQt/Ipv6Setting>
#include <NetworkManagerQt/PppSetting>
#include <NetworkManagerQt/PppoeSetting>
#include <NetworkManagerQt/Security8021xSetting>
#include <NetworkManagerQt/SerialSetting>
#include <NetworkManagerQt/VpnSetting>
#include <NetworkManagerQt/WiredSetting>
#include <NetworkManagerQt/WirelessSecuritySetting>
#include <NetworkManagerQt/BluetoothSetting>
#include <NetworkManagerQt/OlpcMeshSetting>
#include <NetworkManagerQt/VlanSetting>
#include <NetworkManagerQt/WimaxSetting>
#include <NetworkManagerQt/BondSetting>
#include <NetworkManagerQt/BridgeSetting>
#include <NetworkManagerQt/BridgePortSetting>
#include <NetworkManagerQt/TeamSetting>
#include <NetworkManagerQt/GenericSetting>
*/
#include <QDBusPendingCallWatcher>


NmModelPrivate::NmModelPrivate()
{
    insertActiveConnections();
    insertConnections();
    insertDevices();
    insertWifiNetworks();

    //initialize NetworkManager signals
    connect(NetworkManager::notifier(), &NetworkManager::Notifier::deviceAdded, this, &NmModelPrivate::onDeviceAdded);
    connect(NetworkManager::notifier(), &NetworkManager::Notifier::deviceRemoved, this, &NmModelPrivate::onDeviceRemoved);
    connect(NetworkManager::notifier(), &NetworkManager::Notifier::activeConnectionAdded, this, &NmModelPrivate::onActiveConnectionAdded);
    connect(NetworkManager::notifier(), &NetworkManager::Notifier::activeConnectionRemoved, this, &NmModelPrivate::onActiveConnectionRemoved);
    connect(NetworkManager::settingsNotifier(), &NetworkManager::SettingsNotifier::connectionAdded, this, &NmModelPrivate::onConnectionAdded);
    connect(NetworkManager::settingsNotifier(), &NetworkManager::SettingsNotifier::connectionRemoved, this, static_cast<void (NmModelPrivate::*)(QString const &)>(&NmModelPrivate::onConnectionRemoved));

    //TODO: listening to NetworkManager::Notifier::serviceAppeared/Disapeared !?!
    
//qDebug() << mActiveConns.size() << mConnections.size() << mDevices.size();
}

NmModelPrivate::~NmModelPrivate()
{
}

void NmModelPrivate::removeActiveConnection(int pos)
{
    //active connections signals
    NetworkManager::ActiveConnection::Ptr conn = mActiveConns.takeAt(pos);
    disconnect(conn.data(), &NetworkManager::ActiveConnection::connectionChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    disconnect(conn.data(), &NetworkManager::ActiveConnection::default4Changed, this, &NmModelPrivate::onActiveConnectionUpdated);
    disconnect(conn.data(), &NetworkManager::ActiveConnection::default6Changed, this, &NmModelPrivate::onActiveConnectionUpdated);
#if NM_CHECK_VERSION(0, 9, 10)
    disconnect(conn.data(), &NetworkManager::ActiveConnection::dhcp4ConfigChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    disconnect(conn.data(), &NetworkManager::ActiveConnection::dhcp6ConfigChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    disconnect(conn.data(), &NetworkManager::ActiveConnection::ipV4ConfigChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    disconnect(conn.data(), &NetworkManager::ActiveConnection::ipV6ConfigChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
#endif
    disconnect(conn.data(), &NetworkManager::ActiveConnection::idChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    disconnect(conn.data(), &NetworkManager::ActiveConnection::typeChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    disconnect(conn.data(), &NetworkManager::ActiveConnection::masterChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    disconnect(conn.data(), &NetworkManager::ActiveConnection::specificObjectChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    disconnect(conn.data(), &NetworkManager::ActiveConnection::stateChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    disconnect(conn.data(), &NetworkManager::ActiveConnection::vpnChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    disconnect(conn.data(), &NetworkManager::ActiveConnection::uuidChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    disconnect(conn.data(), &NetworkManager::ActiveConnection::devicesChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    if (conn->vpn())
    {
        disconnect(qobject_cast<NetworkManager::VpnConnection *>(conn.data()), &NetworkManager::VpnConnection::bannerChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
        disconnect(qobject_cast<NetworkManager::VpnConnection *>(conn.data()), &NetworkManager::VpnConnection::stateChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    }
}

void NmModelPrivate::clearActiveConnections()
{
    while (0 < mActiveConns.size())
        removeActiveConnection(0);
}

void NmModelPrivate::addActiveConnection(NetworkManager::ActiveConnection::Ptr conn)
{
    mActiveConns.push_back(conn);
    connect(conn.data(), &NetworkManager::ActiveConnection::connectionChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    connect(conn.data(), &NetworkManager::ActiveConnection::default4Changed, this, &NmModelPrivate::onActiveConnectionUpdated);
    connect(conn.data(), &NetworkManager::ActiveConnection::default6Changed, this, &NmModelPrivate::onActiveConnectionUpdated);
#if NM_CHECK_VERSION(0, 9, 10)
    connect(conn.data(), &NetworkManager::ActiveConnection::dhcp4ConfigChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    connect(conn.data(), &NetworkManager::ActiveConnection::dhcp6ConfigChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    connect(conn.data(), &NetworkManager::ActiveConnection::ipV4ConfigChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    connect(conn.data(), &NetworkManager::ActiveConnection::ipV6ConfigChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
#endif
    connect(conn.data(), &NetworkManager::ActiveConnection::idChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    connect(conn.data(), &NetworkManager::ActiveConnection::typeChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    connect(conn.data(), &NetworkManager::ActiveConnection::masterChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    connect(conn.data(), &NetworkManager::ActiveConnection::specificObjectChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    connect(conn.data(), &NetworkManager::ActiveConnection::stateChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    connect(conn.data(), &NetworkManager::ActiveConnection::vpnChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    connect(conn.data(), &NetworkManager::ActiveConnection::uuidChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    connect(conn.data(), &NetworkManager::ActiveConnection::devicesChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    if (conn->vpn())
    {
        connect(qobject_cast<NetworkManager::VpnConnection *>(conn.data()), &NetworkManager::VpnConnection::bannerChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
        connect(qobject_cast<NetworkManager::VpnConnection *>(conn.data()), &NetworkManager::VpnConnection::stateChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    }
}

void NmModelPrivate::insertActiveConnections()
{
    for (auto const & conn : NetworkManager::activeConnections())
        addActiveConnection(conn);
}

void NmModelPrivate::removeConnection(int pos)
{
    //connections signals
    NetworkManager::Connection::Ptr conn = mConnections.takeAt(pos);
    disconnect(conn.data(), &NetworkManager::Connection::updated, this, &NmModelPrivate::onConnectionUpdated);
    disconnect(conn.data(), &NetworkManager::Connection::removed, this, static_cast<void (NmModelPrivate::*)()>(&NmModelPrivate::onConnectionRemoved));
}

void NmModelPrivate::clearConnections()
{
    while (0 < mConnections.size())
        removeConnection(0);
}

void NmModelPrivate::addConnection(NetworkManager::Connection::Ptr conn)
{
    mConnections.push_back(conn);
    //connections signals
    connect(conn.data(), &NetworkManager::Connection::updated, this, &NmModelPrivate::onConnectionUpdated);
    connect(conn.data(), &NetworkManager::Connection::removed, this, static_cast<void (NmModelPrivate::*)()>(&NmModelPrivate::onConnectionRemoved));
}

void NmModelPrivate::insertConnections()
{
    for (auto const & conn : NetworkManager::listConnections())
        addConnection(conn);
}

void NmModelPrivate::removeDevice(int pos)
{
    //connections signals
    NetworkManager::Device::Ptr device = mDevices.takeAt(pos);
    disconnect(device.data(), &NetworkManager::Device::stateChanged, this, &NmModelPrivate::onDeviceUpdated);
    disconnect(device.data(), &NetworkManager::Device::activeConnectionChanged, this, &NmModelPrivate::onDeviceUpdated);
    disconnect(device.data(), &NetworkManager::Device::autoconnectChanged, this, &NmModelPrivate::onDeviceUpdated);
    disconnect(device.data(), &NetworkManager::Device::availableConnectionChanged, this, &NmModelPrivate::onDeviceUpdated);
    disconnect(device.data(), &NetworkManager::Device::availableConnectionAppeared, this, &NmModelPrivate::onDeviceUpdated);
    disconnect(device.data(), &NetworkManager::Device::availableConnectionDisappeared, this, &NmModelPrivate::onDeviceUpdated);
    disconnect(device.data(), &NetworkManager::Device::capabilitiesChanged, this, &NmModelPrivate::onDeviceUpdated);
    disconnect(device.data(), &NetworkManager::Device::dhcp4ConfigChanged, this, &NmModelPrivate::onDeviceUpdated);
    disconnect(device.data(), &NetworkManager::Device::dhcp6ConfigChanged, this, &NmModelPrivate::onDeviceUpdated);
    disconnect(device.data(), &NetworkManager::Device::driverChanged, this, &NmModelPrivate::onDeviceUpdated);
    disconnect(device.data(), &NetworkManager::Device::driverVersionChanged, this, &NmModelPrivate::onDeviceUpdated);
    disconnect(device.data(), &NetworkManager::Device::firmwareMissingChanged, this, &NmModelPrivate::onDeviceUpdated);
    disconnect(device.data(), &NetworkManager::Device::firmwareVersionChanged, this, &NmModelPrivate::onDeviceUpdated);
    disconnect(device.data(), &NetworkManager::Device::interfaceNameChanged, this, &NmModelPrivate::onDeviceUpdated);
    disconnect(device.data(), &NetworkManager::Device::ipV4AddressChanged, this, &NmModelPrivate::onDeviceUpdated);
    disconnect(device.data(), &NetworkManager::Device::ipV4ConfigChanged, this, &NmModelPrivate::onDeviceUpdated);
    disconnect(device.data(), &NetworkManager::Device::ipV6ConfigChanged, this, &NmModelPrivate::onDeviceUpdated);
    disconnect(device.data(), &NetworkManager::Device::ipInterfaceChanged, this, &NmModelPrivate::onDeviceUpdated);
    disconnect(device.data(), &NetworkManager::Device::managedChanged, this, &NmModelPrivate::onDeviceUpdated);
#if NM_CHECK_VERSION(0, 9, 10)
    disconnect(device.data(), &NetworkManager::Device::physicalPortIdChanged, this, &NmModelPrivate::onDeviceUpdated);
    disconnect(device.data(), &NetworkManager::Device::mtuChanged, this, &NmModelPrivate::onDeviceUpdated);
#endif
#if NM_CHECK_VERSION(1, 2, 0)
    disconnect(device.data(), &NetworkManager::Device::nmPluginMissingChanged, this, &NmModelPrivate::onDeviceUpdated);
#endif
#if NM_CHECK_VERSION(1, 0, 6)
    disconnect(device.data(), &NetworkManager::Device::meteredChanged, this, &NmModelPrivate::onDeviceUpdated);
#endif
    disconnect(device.data(), &NetworkManager::Device::connectionStateChanged, this, &NmModelPrivate::onDeviceUpdated);
    disconnect(device.data(), &NetworkManager::Device::stateReasonChanged, this, &NmModelPrivate::onDeviceUpdated);
    disconnect(device.data(), &NetworkManager::Device::udiChanged, this, &NmModelPrivate::onDeviceUpdated);
    switch (device->type())
    {
        case NetworkManager::Ethernet:
            disconnect(qobject_cast<NetworkManager::WiredDevice *>(device.data()), &NetworkManager::WiredDevice::bitRateChanged, this, &NmModelPrivate::onDeviceUpdated);
            disconnect(qobject_cast<NetworkManager::WiredDevice *>(device.data()), &NetworkManager::WiredDevice::carrierChanged, this, &NmModelPrivate::onDeviceUpdated);
            disconnect(qobject_cast<NetworkManager::WiredDevice *>(device.data()), &NetworkManager::WiredDevice::hardwareAddressChanged, this, &NmModelPrivate::onDeviceUpdated);
            disconnect(qobject_cast<NetworkManager::WiredDevice *>(device.data()), &NetworkManager::WiredDevice::permanentHardwareAddressChanged, this, &NmModelPrivate::onDeviceUpdated);
            break;
        case NetworkManager::Device::Wifi:
            disconnect(qobject_cast<NetworkManager::WirelessDevice *>(device.data()), &NetworkManager::WirelessDevice::bitRateChanged, this, &NmModelPrivate::onDeviceUpdated);
            disconnect(qobject_cast<NetworkManager::WirelessDevice *>(device.data()), &NetworkManager::WirelessDevice::activeAccessPointChanged, this, &NmModelPrivate::onDeviceUpdated);
            disconnect(qobject_cast<NetworkManager::WirelessDevice *>(device.data()), &NetworkManager::WirelessDevice::modeChanged, this, &NmModelPrivate::onDeviceUpdated);
            disconnect(qobject_cast<NetworkManager::WirelessDevice *>(device.data()), &NetworkManager::WirelessDevice::wirelessCapabilitiesChanged, this, &NmModelPrivate::onDeviceUpdated);
            disconnect(qobject_cast<NetworkManager::WirelessDevice *>(device.data()), &NetworkManager::WirelessDevice::hardwareAddressChanged, this, &NmModelPrivate::onDeviceUpdated);
            disconnect(qobject_cast<NetworkManager::WirelessDevice *>(device.data()), &NetworkManager::WirelessDevice::permanentHardwareAddressChanged, this, &NmModelPrivate::onDeviceUpdated);
            disconnect(qobject_cast<NetworkManager::WirelessDevice *>(device.data()), &NetworkManager::WirelessDevice::wirelessPropertiesChanged, this, &NmModelPrivate::onDeviceUpdated);
            disconnect(qobject_cast<NetworkManager::WirelessDevice *>(device.data()), &NetworkManager::WirelessDevice::accessPointAppeared, this, &NmModelPrivate::onDeviceUpdated);
            disconnect(qobject_cast<NetworkManager::WirelessDevice *>(device.data()), &NetworkManager::WirelessDevice::accessPointDisappeared, this, &NmModelPrivate::onDeviceUpdated);
            disconnect(qobject_cast<NetworkManager::WirelessDevice *>(device.data()), &NetworkManager::WirelessDevice::networkAppeared, this, &NmModelPrivate::onWifiNetworkAppeared);
            disconnect(qobject_cast<NetworkManager::WirelessDevice *>(device.data()), &NetworkManager::WirelessDevice::networkDisappeared, this, &NmModelPrivate::onWifiNetworkDisappeared);
            break;
        default:
            //TODO: other device types!
            break;
    }
}

void NmModelPrivate::clearDevices()
{
    while (0 < mDevices.size())
        removeDevice(0);
}

void NmModelPrivate::addDevice(NetworkManager::Device::Ptr device)
{
    mDevices.push_back(device);
    //device signals
    connect(device.data(), &NetworkManager::Device::stateChanged, this, &NmModelPrivate::onDeviceUpdated);
    connect(device.data(), &NetworkManager::Device::activeConnectionChanged, this, &NmModelPrivate::onDeviceUpdated);
    connect(device.data(), &NetworkManager::Device::autoconnectChanged, this, &NmModelPrivate::onDeviceUpdated);
    connect(device.data(), &NetworkManager::Device::availableConnectionChanged, this, &NmModelPrivate::onDeviceUpdated);
    connect(device.data(), &NetworkManager::Device::availableConnectionAppeared, this, &NmModelPrivate::onDeviceUpdated);
    connect(device.data(), &NetworkManager::Device::availableConnectionDisappeared, this, &NmModelPrivate::onDeviceUpdated);
    connect(device.data(), &NetworkManager::Device::capabilitiesChanged, this, &NmModelPrivate::onDeviceUpdated);
    connect(device.data(), &NetworkManager::Device::dhcp4ConfigChanged, this, &NmModelPrivate::onDeviceUpdated);
    connect(device.data(), &NetworkManager::Device::dhcp6ConfigChanged, this, &NmModelPrivate::onDeviceUpdated);
    connect(device.data(), &NetworkManager::Device::driverChanged, this, &NmModelPrivate::onDeviceUpdated);
    connect(device.data(), &NetworkManager::Device::driverVersionChanged, this, &NmModelPrivate::onDeviceUpdated);
    connect(device.data(), &NetworkManager::Device::firmwareMissingChanged, this, &NmModelPrivate::onDeviceUpdated);
    connect(device.data(), &NetworkManager::Device::firmwareVersionChanged, this, &NmModelPrivate::onDeviceUpdated);
    connect(device.data(), &NetworkManager::Device::interfaceNameChanged, this, &NmModelPrivate::onDeviceUpdated);
    connect(device.data(), &NetworkManager::Device::ipV4AddressChanged, this, &NmModelPrivate::onDeviceUpdated);
    connect(device.data(), &NetworkManager::Device::ipV4ConfigChanged, this, &NmModelPrivate::onDeviceUpdated);
    connect(device.data(), &NetworkManager::Device::ipV6ConfigChanged, this, &NmModelPrivate::onDeviceUpdated);
    connect(device.data(), &NetworkManager::Device::ipInterfaceChanged, this, &NmModelPrivate::onDeviceUpdated);
    connect(device.data(), &NetworkManager::Device::managedChanged, this, &NmModelPrivate::onDeviceUpdated);
#if NM_CHECK_VERSION(0, 9, 10)
    connect(device.data(), &NetworkManager::Device::physicalPortIdChanged, this, &NmModelPrivate::onDeviceUpdated);
    connect(device.data(), &NetworkManager::Device::mtuChanged, this, &NmModelPrivate::onDeviceUpdated);
#endif
#if NM_CHECK_VERSION(1, 2, 0)
    connect(device.data(), &NetworkManager::Device::nmPluginMissingChanged, this, &NmModelPrivate::onDeviceUpdated);
#endif
#if NM_CHECK_VERSION(1, 0, 6)
    connect(device.data(), &NetworkManager::Device::meteredChanged, this, &NmModelPrivate::onDeviceUpdated);
#endif
    connect(device.data(), &NetworkManager::Device::connectionStateChanged, this, &NmModelPrivate::onDeviceUpdated);
    connect(device.data(), &NetworkManager::Device::stateReasonChanged, this, &NmModelPrivate::onDeviceUpdated);
    connect(device.data(), &NetworkManager::Device::udiChanged, this, &NmModelPrivate::onDeviceUpdated);
    switch (device->type())
    {
        case NetworkManager::Ethernet:
            connect(qobject_cast<NetworkManager::WiredDevice *>(device.data()), &NetworkManager::WiredDevice::bitRateChanged, this, &NmModelPrivate::onDeviceUpdated);
            connect(qobject_cast<NetworkManager::WiredDevice *>(device.data()), &NetworkManager::WiredDevice::carrierChanged, this, &NmModelPrivate::onDeviceUpdated);
            connect(qobject_cast<NetworkManager::WiredDevice *>(device.data()), &NetworkManager::WiredDevice::hardwareAddressChanged, this, &NmModelPrivate::onDeviceUpdated);
            connect(qobject_cast<NetworkManager::WiredDevice *>(device.data()), &NetworkManager::WiredDevice::permanentHardwareAddressChanged, this, &NmModelPrivate::onDeviceUpdated);
            break;

        case NetworkManager::Device::Wifi:
            connect(qobject_cast<NetworkManager::WirelessDevice *>(device.data()), &NetworkManager::WirelessDevice::bitRateChanged, this, &NmModelPrivate::onDeviceUpdated);
            connect(qobject_cast<NetworkManager::WirelessDevice *>(device.data()), &NetworkManager::WirelessDevice::activeAccessPointChanged, this, &NmModelPrivate::onDeviceUpdated);
            connect(qobject_cast<NetworkManager::WirelessDevice *>(device.data()), &NetworkManager::WirelessDevice::modeChanged, this, &NmModelPrivate::onDeviceUpdated);
            connect(qobject_cast<NetworkManager::WirelessDevice *>(device.data()), &NetworkManager::WirelessDevice::wirelessCapabilitiesChanged, this, &NmModelPrivate::onDeviceUpdated);
            connect(qobject_cast<NetworkManager::WirelessDevice *>(device.data()), &NetworkManager::WirelessDevice::hardwareAddressChanged, this, &NmModelPrivate::onDeviceUpdated);
            connect(qobject_cast<NetworkManager::WirelessDevice *>(device.data()), &NetworkManager::WirelessDevice::permanentHardwareAddressChanged, this, &NmModelPrivate::onDeviceUpdated);
            connect(qobject_cast<NetworkManager::WirelessDevice *>(device.data()), &NetworkManager::WirelessDevice::wirelessPropertiesChanged, this, &NmModelPrivate::onDeviceUpdated);
            connect(qobject_cast<NetworkManager::WirelessDevice *>(device.data()), &NetworkManager::WirelessDevice::accessPointAppeared, this, &NmModelPrivate::onDeviceUpdated);
            connect(qobject_cast<NetworkManager::WirelessDevice *>(device.data()), &NetworkManager::WirelessDevice::accessPointDisappeared, this, &NmModelPrivate::onDeviceUpdated);
            connect(qobject_cast<NetworkManager::WirelessDevice *>(device.data()), &NetworkManager::WirelessDevice::networkAppeared, this, &NmModelPrivate::onWifiNetworkAppeared);
            connect(qobject_cast<NetworkManager::WirelessDevice *>(device.data()), &NetworkManager::WirelessDevice::networkDisappeared, this, &NmModelPrivate::onWifiNetworkDisappeared);
            break;
        default:
            //TODO: other device types!
            break;
    }
}

void NmModelPrivate::insertDevices()
{
    for (auto const & device : NetworkManager::networkInterfaces())
        addDevice(device);
}

void NmModelPrivate::removeWifiNetwork(int pos)
{
    //network signals
    NetworkManager::WirelessNetwork::Ptr net = mWifiNets.takeAt(pos);
    disconnect(net.data(), &NetworkManager::WirelessNetwork::signalStrengthChanged, this, &NmModelPrivate::onWifiNetworkUpdated);
    disconnect(net.data(), &NetworkManager::WirelessNetwork::referenceAccessPointChanged, this, &NmModelPrivate::onWifiNetworkUpdated);
    disconnect(net.data(), &NetworkManager::WirelessNetwork::disappeared, this, &NmModelPrivate::onWifiNetworkUpdated);
}

void NmModelPrivate::clearWifiNetworks()
{
    while (0 < mWifiNets.size())
        removeWifiNetwork(0);
}

void NmModelPrivate::addWifiNetwork(NetworkManager::WirelessNetwork::Ptr net)
{
    mWifiNets.push_back(net);
    //device signals
    connect(net.data(), &NetworkManager::WirelessNetwork::signalStrengthChanged, this, &NmModelPrivate::onWifiNetworkUpdated);
    connect(net.data(), &NetworkManager::WirelessNetwork::referenceAccessPointChanged, this, &NmModelPrivate::onWifiNetworkUpdated);
    connect(net.data(), &NetworkManager::WirelessNetwork::disappeared, this, &NmModelPrivate::onWifiNetworkUpdated);
}

void NmModelPrivate::insertWifiNetworks()
{
    for (auto const & device : mDevices)
    {
        if (NetworkManager::Device::Wifi == device->type())
        {
            NetworkManager::WirelessDevice::Ptr w_dev = device.objectCast<NetworkManager::WirelessDevice>();
            for (auto const & net : w_dev->networks())
            {
                if (!net.isNull())
                {
                    addWifiNetwork(net);
                }
            }
        }
    }
}

NetworkManager::ActiveConnection::Ptr NmModelPrivate::findActiveConnection(QString const & path)
{
    auto i = std::find_if(mActiveConns.cbegin(), mActiveConns.cend(), [&path] (NetworkManager::ActiveConnection::Ptr const & conn) -> bool {
        return conn->path() == path;
    });
    return mActiveConns.cend() == i ? NetworkManager::ActiveConnection::Ptr{} : *i;
}

template <typename Predicate>
NetworkManager::Device::Ptr NmModelPrivate::findDevice(Predicate const & pred)
{
    auto i = std::find_if(mDevices.cbegin(), mDevices.cend(), pred);
    return mDevices.cend() == i ? NetworkManager::Device::Ptr{} : *i;
}

NetworkManager::Device::Ptr NmModelPrivate::findDeviceUni(QString const & uni)
{
    return findDevice([&uni] (NetworkManager::Device::Ptr const & dev) { return dev->uni() == uni; });
}

NetworkManager::Device::Ptr NmModelPrivate::findDeviceInterface(QString const & interfaceName)
{
    return findDevice([&interfaceName] (NetworkManager::Device::Ptr const & dev) { return dev->interfaceName() == interfaceName; });
}

NetworkManager::WirelessNetwork::Ptr NmModelPrivate::findWifiNetwork(QString const & ssid, QString const & devUni)
{
    auto i = std::find_if(mWifiNets.cbegin(), mWifiNets.cend(), [&ssid, &devUni] (NetworkManager::WirelessNetwork::Ptr const & net) -> bool {
        return net->ssid() == ssid && net->device() == devUni;
    });
    return mWifiNets.cend() == i ? NetworkManager::WirelessNetwork::Ptr{} : *i;
}

void NmModelPrivate::onConnectionUpdated()
{
    emit connectionUpdate(qobject_cast<NetworkManager::Connection *>(sender()));
}

void NmModelPrivate::onConnectionRemoved()
{
    emit connectionRemove(qobject_cast<NetworkManager::Connection *>(sender()));
}

void NmModelPrivate::onActiveConnectionUpdated()
{
    emit activeConnectionUpdate(qobject_cast<NetworkManager::ActiveConnection *>(sender()));
}

void NmModelPrivate::onDeviceUpdated()
{
    emit deviceUpdate(qobject_cast<NetworkManager::Device *>(sender()));
}

void NmModelPrivate::onWifiNetworkAppeared(QString const & ssid)
{
    NetworkManager::Device * dev = qobject_cast<NetworkManager::Device *>(sender());
    emit wifiNetworkAdd(dev, ssid);
    emit deviceUpdate(dev);
}

void NmModelPrivate::onWifiNetworkDisappeared(QString const & ssid)
{
    NetworkManager::Device * dev = qobject_cast<NetworkManager::Device *>(sender());
    emit wifiNetworkRemove(dev, ssid);
    emit deviceUpdate(dev);
}

void NmModelPrivate::onWifiNetworkUpdated()
{
    emit wifiNetworkUpdate(qobject_cast<NetworkManager::WirelessNetwork *>(sender()));
}

void NmModelPrivate::onDeviceAdded(QString const & uni)
{
    NetworkManager::Device::Ptr dev = NetworkManager::findNetworkInterface(uni);
    Q_ASSERT(!dev.isNull() && dev->isValid());
    emit deviceAdd(dev);
}

void NmModelPrivate::onDeviceRemoved(QString const & uni)
{
    NetworkManager::Device::Ptr dev = findDeviceUni(uni);
    if (!dev.isNull())
    {
        Q_ASSERT(dev->isValid());
        emit deviceRemove(dev.data());
    }
}

void NmModelPrivate::onActiveConnectionAdded(QString const & path)
{
    NetworkManager::ActiveConnection::Ptr conn = NetworkManager::findActiveConnection(path);//XXX: const QString &uni
    Q_ASSERT(!conn.isNull() && conn->isValid());
    emit activeConnectionAdd(conn);
}

void NmModelPrivate::onActiveConnectionRemoved(QString const & path)
{
    NetworkManager::ActiveConnection::Ptr conn = findActiveConnection(path);//XXX: const QString &uni
    if (!conn.isNull())
    {
        Q_ASSERT(conn->isValid());
        emit activeConnectionRemove(conn.data());
    }
}

void NmModelPrivate::onActiveConnectionsChanged()
{
    emit activeConnectionsReset();
}

void NmModelPrivate::onConnectionAdded(QString const & path)
{
    NetworkManager::Connection::Ptr conn = NetworkManager::findConnection(path);
    Q_ASSERT(!conn.isNull() && conn->isValid());
    emit connectionAdd(conn);
}

void NmModelPrivate::onConnectionRemoved(QString const & path)
{
    NetworkManager::Connection::Ptr conn = NetworkManager::findConnection(path);
    if (!conn.isNull())
    {
        Q_ASSERT(conn->isValid());
        emit connectionRemove(conn.data());
    }
}




NmModel::NmModel(QObject * parent)
    : QAbstractItemModel(parent)
    , d{new NmModelPrivate}
{
    connect(d.data(), &NmModelPrivate::connectionAdd, [this] (NetworkManager::Connection::Ptr conn) {
//qDebug() << "connectionAdd" << conn->name();
        const int cnt = d->mConnections.size();
        Q_ASSERT(0 > d->mConnections.indexOf(conn));
        beginInsertRows(createIndex(1, 0, ITEM_CONNECTION), cnt, cnt);
        d->addConnection(conn);
        endInsertRows();
    });
    connect(d.data(), &NmModelPrivate::connectionUpdate, [this] (NetworkManager::Connection * conn) {
//qDebug() << "connectionUpdate" << conn->name();
        auto i = std::find(d->mConnections.cbegin(), d->mConnections.cend(), conn);
        if (d->mConnections.cend() != i)
        {
            QModelIndex index = createIndex(i - d->mConnections.cbegin(), 0, ITEM_CONNECTION_LEAF);
            emit dataChanged(index, index);
        }
    });
    connect(d.data(), &NmModelPrivate::connectionRemove, [this] (NetworkManager::Connection * conn) {
//qDebug() << "connectionRemove" << conn->name();
        auto i = std::find(d->mConnections.cbegin(), d->mConnections.cend(), conn);
        if (d->mConnections.cend() != i)
        {
            const int pos = i - d->mConnections.cbegin();
            beginRemoveRows(createIndex(1, 0, ITEM_CONNECTION), pos, pos);
            d->removeConnection(pos);
            endRemoveRows();
        }
    });
    connect(d.data(), &NmModelPrivate::activeConnectionAdd, [this] (NetworkManager::ActiveConnection::Ptr conn) {
//qDebug() << "activecCnnectionAdd" << conn->connection()->name();
        const int cnt = d->mActiveConns.size();
        Q_ASSERT(0 > d->mActiveConns.indexOf(conn));
        beginInsertRows(createIndex(0, 0, ITEM_ACTIVE), cnt, cnt);
        d->addActiveConnection(conn);
        endInsertRows();
    });
    connect(d.data(), &NmModelPrivate::activeConnectionUpdate, [this] (NetworkManager::ActiveConnection * conn) {
//qDebug() << "activecCnnectionUpdate" << conn->connection()->name();
        auto i = std::find(d->mActiveConns.cbegin(), d->mActiveConns.cend(), conn);
        if (d->mActiveConns.cend() != i)
        {
            QModelIndex index = createIndex(i - d->mActiveConns.cbegin(), 0, ITEM_ACTIVE_LEAF);
            emit dataChanged(index, index);
        }
    });
    connect(d.data(), &NmModelPrivate::activeConnectionRemove, [this] (NetworkManager::ActiveConnection * conn) {
//qDebug() << "activecCnnectionRemove" << conn->connection()->name();
        auto i = std::find(d->mActiveConns.cbegin(), d->mActiveConns.cend(), conn);
        if (d->mActiveConns.cend() != i)
        {
            const int pos = i - d->mActiveConns.cbegin();
            beginRemoveRows(createIndex(0, 0, ITEM_ACTIVE), pos, pos);
            d->removeActiveConnection(pos);
            endRemoveRows();
        }
    });
    connect(d.data(), &NmModelPrivate::activeConnectionsReset, [this] () {
//qDebug() << "activecCnnectionReset";
        const int cnt = d->mActiveConns.size();
        if (0 < cnt)
        {
            beginRemoveRows(createIndex(0, 0, ITEM_ACTIVE), 0, d->mActiveConns.size() - 1);
            d->clearActiveConnections();
            endRemoveRows();
        }
        const int new_cnt = NetworkManager::activeConnections().size();
        if (0 < new_cnt)
        {
            beginInsertRows(createIndex(0, 0, ITEM_ACTIVE), 0, new_cnt - 1);
            d->insertActiveConnections();
            endInsertRows();
        }
    });
    connect(d.data(), &NmModelPrivate::deviceAdd, [this] (NetworkManager::Device::Ptr dev) {
//qDebug() << "deviceAdd" << dev->interfaceName();
        const int cnt = d->mDevices.size();
        Q_ASSERT(0 > d->mDevices.indexOf(dev));
        beginInsertRows(createIndex(2, 0, ITEM_DEVICE), cnt, cnt);
        d->addDevice(dev);
        endInsertRows();
    });
    connect(d.data(), &NmModelPrivate::deviceUpdate, [this] (NetworkManager::Device * dev) {
//qDebug() << "deviceUpdate" << dev << dev->interfaceName();
        auto i = std::find(d->mDevices.cbegin(), d->mDevices.cend(), dev);
        if (d->mDevices.cend() != i)
        {
            QModelIndex index = createIndex(i - d->mDevices.cbegin(), 0, ITEM_DEVICE_LEAF);
            emit dataChanged(index, index);
        }
    });
    connect(d.data(), &NmModelPrivate::deviceRemove, [this] (NetworkManager::Device * dev) {
//qDebug() << "deviceRemove" << dev->interfaceName();
        auto i = std::find(d->mDevices.cbegin(), d->mDevices.cend(), dev);
        if (d->mDevices.cend() != i)
        {
            const int pos = i - d->mDevices.cbegin();
            beginRemoveRows(createIndex(2, 0, ITEM_DEVICE), pos, pos);
            d->removeDevice(pos);
            endRemoveRows();
        }
    });
    connect(d.data(), &NmModelPrivate::wifiNetworkAdd, [this] (NetworkManager::Device * dev, QString const & ssid) {
//qDebug() << "wifiNetworkAdd" << dev << dev->interfaceName() << ssid;
        NetworkManager::WirelessDevice * w_dev = qobject_cast<NetworkManager::WirelessDevice *>(dev);
        NetworkManager::WirelessNetwork::Ptr net = w_dev->findNetwork(ssid);
        if (!net.isNull())
        {
            Q_ASSERT(0 > d->mWifiNets.indexOf(net));
            const int cnt = d->mWifiNets.size();
            beginInsertRows(createIndex(3, 0, ITEM_WIFINET), cnt, cnt);
            d->addWifiNetwork(net);
            endInsertRows();
        }
    });
    connect(d.data(), &NmModelPrivate::wifiNetworkUpdate, [this] (NetworkManager::WirelessNetwork * net) {
//qDebug() << "wifiNetworkUpdate" << net << net->ssid();
        auto i = std::find(d->mWifiNets.cbegin(), d->mWifiNets.cend(), net);
        if (d->mWifiNets.cend() != i)
        {
            if (net->accessPoints().isEmpty())
            {
                //remove
                auto pos = i - d->mWifiNets.cbegin();
                beginRemoveRows(createIndex(3, 0, ITEM_WIFINET), pos, pos);
                d->removeWifiNetwork(pos);
                endRemoveRows();
            } else
            {
                //update
                QModelIndex index = createIndex(i - d->mWifiNets.cbegin(), 0, ITEM_WIFINET_LEAF);
                emit dataChanged(index, index);
                //XXX: active connection
                auto dev = d->findDeviceUni((*i)->device());
                if (!dev.isNull())
                {
                    auto active = dev->activeConnection();
                    if (!active.isNull())
                    {
                        QModelIndex index = createIndex(d->mActiveConns.indexOf(active), 0, ITEM_ACTIVE_LEAF);
                        emit dataChanged(index, index);
                    }
                }
            }
        }
    });
    connect(d.data(), &NmModelPrivate::wifiNetworkRemove, [this] (NetworkManager::Device * dev, QString const & ssid) {
//qDebug() << "wifiNetworkRemove" << dev << dev->interfaceName() << ssid;
        NetworkManager::WirelessNetwork::Ptr net = d->findWifiNetwork(ssid, dev->uni());
        Q_ASSERT(!net.isNull());
        auto pos = d->mWifiNets.indexOf(net);
        if (0 <= pos)
        {
            beginRemoveRows(createIndex(3, 0, ITEM_WIFINET), pos, pos);
            d->removeWifiNetwork(pos);
            endRemoveRows();
        }
    });
//qDebug() << __FUNCTION__ << "finished";
}

NmModel::~NmModel()
{
}

/* Model hierarchy
 * root
 *   - mActiveConns
 *   - mConnections
 *   - mDevices
 */
int NmModel::rowCount(const QModelIndex &parent/* = QModelIndex()*/) const
{
    int cnt = 0;
    if (!parent.isValid())
        cnt = 1;
    else
    {
        const int id = parent.internalId();
        if (ITEM_ROOT  == id)
            cnt = 4;
        else if (ITEM_ACTIVE == id)
            cnt =  d->mActiveConns.size();
        else if (ITEM_CONNECTION == id)
            cnt = d->mConnections.size();
        else if (ITEM_DEVICE == id)
            cnt = d->mDevices.size();
        else if (ITEM_WIFINET == id)
            cnt = d->mWifiNets.size();
    }

//qDebug() << __FUNCTION__ << parent << cnt;
    return cnt;
}

int NmModel::columnCount(const QModelIndex & /*parent = QModelIndex()*/) const
{
//qDebug() << __FUNCTION__ << parent << 1;
    //XXX: more columns for wifi connections (for name && icons...)??
    return 1;
}

bool NmModel::isValidDataIndex(const QModelIndex & index) const
{
    switch (static_cast<ItemId>(index.internalId()))
    {
        case ITEM_ROOT:
        case ITEM_ACTIVE:
        case ITEM_CONNECTION:
        case ITEM_DEVICE:
        case ITEM_WIFINET:
            return true;
        case ITEM_ACTIVE_LEAF:
            return d->mActiveConns.size() > index.row() && 0 == index.column();
        case ITEM_CONNECTION_LEAF:
            return d->mConnections.size() > index.row() && 0 == index.column();
        case ITEM_DEVICE_LEAF:
            return d->mDevices.size() > index.row() && 0 == index.column();
        case ITEM_WIFINET_LEAF:
            return d->mWifiNets.size() > index.row() && 0 == index.column();
    }
    return false;
}

template <>
QVariant NmModel::dataRole<NmModel::ItemTypeRole>(const QModelIndex & index) const
{
    switch (static_cast<ItemId>(index.internalId()))
    {
        case ITEM_ROOT:
        case ITEM_ACTIVE:
        case ITEM_CONNECTION:
        case ITEM_WIFINET:
        case ITEM_DEVICE:
            return HelperType;
        case ITEM_ACTIVE_LEAF:
            return ActiveConnectionType;
        case ITEM_CONNECTION_LEAF:
            return ConnectionType;
        case ITEM_DEVICE_LEAF:
            return DeviceType;
        case ITEM_WIFINET_LEAF:
            return WifiNetworkType;
    }
}

template <>
QVariant NmModel::dataRole<NmModel::NameRole>(const QModelIndex & index) const
{
    switch (static_cast<ItemId>(index.internalId()))
    {
        case ITEM_ROOT:
            return tr("root");
        case ITEM_ACTIVE:
            return tr("active connection(s)");
        case ITEM_CONNECTION:
            return tr("connection(s)");
        case ITEM_DEVICE:
            return tr("device(s)");
        case ITEM_WIFINET:
            return tr("wifi network(s)");
        case ITEM_ACTIVE_LEAF:
            return d->mActiveConns[index.row()]->connection()->name();
        case ITEM_CONNECTION_LEAF:
            return d->mConnections[index.row()]->name();
        case ITEM_DEVICE_LEAF:
            return d->mDevices[index.row()]->interfaceName();
        case ITEM_WIFINET_LEAF:
            return d->mWifiNets[index.row()]->referenceAccessPoint()->ssid();
    }
}

template <>
QVariant NmModel::dataRole<NmModel::ConnectionTypeRole>(const QModelIndex & index) const
{
    switch (static_cast<ItemId>(index.internalId()))
    {
        case ITEM_ROOT:
        case ITEM_ACTIVE:
        case ITEM_CONNECTION:
        case ITEM_DEVICE:
        case ITEM_WIFINET:
            return QVariant{};
        case ITEM_DEVICE_LEAF:
            //TODO: other types
            switch (d->mDevices[index.row()]->type())
            {
                case NetworkManager::Device::Wifi:
                    return NetworkManager::ConnectionSettings::Wireless;
                case NetworkManager::Device::Wimax:
                    return NetworkManager::ConnectionSettings::Wimax;
                default:
                    return NetworkManager::ConnectionSettings::Wired;
            }
        case ITEM_ACTIVE_LEAF:
            return d->mActiveConns[index.row()]->connection()->settings()->connectionType();
        case ITEM_CONNECTION_LEAF:
            return d->mConnections[index.row()]->settings()->connectionType();
        case ITEM_WIFINET_LEAF:
            return NetworkManager::ConnectionSettings::Wireless;
    }
}

template <>
QVariant NmModel::dataRole<NmModel::ActiveConnectionStateRole>(const QModelIndex & index) const
{
    switch (static_cast<ItemId>(index.internalId()))
    {
        case ITEM_ROOT:
        case ITEM_ACTIVE:
        case ITEM_CONNECTION:
        case ITEM_DEVICE:
        case ITEM_CONNECTION_LEAF:
        case ITEM_DEVICE_LEAF:
        case ITEM_WIFINET:
        case ITEM_WIFINET_LEAF:
            return -1;
        case ITEM_ACTIVE_LEAF:
            return d->mActiveConns[index.row()]->state();
    }
}

template <>
QVariant NmModel::dataRole<NmModel::SignalRole>(const QModelIndex & index) const
{
    switch (static_cast<ItemId>(index.internalId()))
    {
        case ITEM_ROOT:
        case ITEM_ACTIVE:
        case ITEM_CONNECTION:
        case ITEM_DEVICE:
        case ITEM_DEVICE_LEAF:
        case ITEM_WIFINET:
            return -1;
        case ITEM_ACTIVE_LEAF:
            {
                NetworkManager::ConnectionSettings::Ptr sett = d->mActiveConns[index.row()]->connection()->settings();
                //TODO: other type with signals!?!
                if (NetworkManager::ConnectionSettings::Wireless == sett->connectionType())
                {
                    NetworkManager::WirelessSetting::Ptr w_sett = sett->setting(NetworkManager::Setting::Wireless).staticCast<NetworkManager::WirelessSetting>();
                    Q_ASSERT(!w_sett.isNull());
                    for (auto const & dev_uni : d->mActiveConns[index.row()]->devices())
                    {
                        auto dev = NetworkManager::findNetworkInterface(dev_uni);
                        if (!dev.isNull() && dev->isValid())
                        {
                            auto w_dev = dev.objectCast<NetworkManager::WirelessDevice>();
                            if (!w_dev.isNull())
                            {
                                auto wifi_net = w_dev->findNetwork(w_sett->ssid());
                                if (!wifi_net.isNull())
                                    return wifi_net->signalStrength();
                            }
                        }
                    }
                }
                return -1;
            }
        case ITEM_CONNECTION_LEAF:
            //TODO: implement
            return -1;
        case ITEM_WIFINET_LEAF:
            return d->mWifiNets[index.row()]->signalStrength();
    }
}

template <>
QVariant NmModel::dataRole<NmModel::ConnectionUuidRole>(const QModelIndex & index) const
{
    switch (static_cast<ItemId>(index.internalId()))
    {
        case ITEM_ROOT:
        case ITEM_ACTIVE:
        case ITEM_CONNECTION:
        case ITEM_DEVICE:
        case ITEM_DEVICE_LEAF:
        case ITEM_WIFINET:
        case ITEM_WIFINET_LEAF:
            return QVariant{};
        case ITEM_ACTIVE_LEAF:
            return d->mActiveConns[index.row()]->uuid();
        case ITEM_CONNECTION_LEAF:
            return d->mConnections[index.row()]->uuid();
    }
}

template <>
QVariant NmModel::dataRole<NmModel::ActiveConnectionInfoRole>(const QModelIndex & index) const
{
    NetworkManager::Device::Ptr dev;
    switch (static_cast<ItemId>(index.internalId()))
    {
        case ITEM_ROOT:
        case ITEM_ACTIVE:
        case ITEM_CONNECTION:
        case ITEM_DEVICE:
        case ITEM_DEVICE_LEAF:
        case ITEM_WIFINET:
        case ITEM_WIFINET_LEAF:
        case ITEM_CONNECTION_LEAF:
            return QVariant{};
            break;
        case ITEM_ACTIVE_LEAF:
            {
                auto dev_list = d->mActiveConns[index.row()]->devices();
                //TODO: work with all devices?!?
                if (!dev_list.isEmpty())
                    dev = d->findDeviceUni(dev_list.first());
            }
            break;
    }
    //TODO: we probably shouldn't assemble a string information (with styling) here and leave it to the consumer
    QString info;
    QDebug str{&info};
    str.noquote();
    str.nospace();
    if (!dev.isNull())
    {
        auto m_enum = NetworkManager::Device::staticMetaObject.enumerator(NetworkManager::Device::staticMetaObject.indexOfEnumerator("Type"));

        QString hw_address = tr("unknown", "hardware address");
        QString security = tr("none", "security");
        int bit_rate = -1;
        switch (dev->type())
        {
            case NetworkManager::Device::Ethernet:
                {
                    auto spec_dev = dev->as<NetworkManager::WiredDevice>();
                    Q_ASSERT(nullptr != spec_dev);
                    hw_address = spec_dev->hardwareAddress();
                    bit_rate = spec_dev->bitRate();
                }
                break;
            case NetworkManager::Device::Wifi:
                {
                    auto spec_dev = dev->as<NetworkManager::WirelessDevice>();
                    Q_ASSERT(nullptr != spec_dev);
                    hw_address = spec_dev->hardwareAddress();
                    bit_rate = spec_dev->bitRate();
                    //TODO: we probably should get it from Connection object
                    security = "TODO:";
                }
                break;
            case NetworkManager::Device::Wimax:
                {
                    auto spec_dev = dev->as<NetworkManager::WimaxDevice>();
                    Q_ASSERT(nullptr != spec_dev);
                    hw_address = spec_dev->hardwareAddress();
                    //bit_rate = spec_dev->bitRate();
                }
                break;
            default:
                break;
        }
        str << QStringLiteral("<big><strong>") << tr("General", "Active connection information") << QStringLiteral("</strong></big><br/>")
            << QStringLiteral("<strong>") << tr("Interface", "Active connection information") << QStringLiteral("</strong>: ")
                << m_enum.valueToKey(dev->type()) << QStringLiteral(" (") << dev->interfaceName() << QStringLiteral(")<br/>")
            << QStringLiteral("<strong>") << tr("Hardware Address", "Active connection information") << QStringLiteral("</strong>: ")
                << hw_address << QStringLiteral("<br/>")
            << QStringLiteral("<strong>") << tr("Driver", "Active connection information") << QStringLiteral("</strong>: ")
                << dev->driver() << QStringLiteral("<br/>")
            << QStringLiteral("<strong>") << tr("Speed", "Active connection information") << QStringLiteral("</strong>: ");
        if (0 <= bit_rate)
            str << bit_rate << tr(" Kb/s");
        else
            str << tr("unknown", "Speed");
        str
                << QStringLiteral("<br/>")
            << QStringLiteral("<strong>") << tr("Security", "Active connection information") << QStringLiteral("</strong>: ")
                << security << QStringLiteral("<br/>")
            ;
    }
#if 0
    str << *conn->settings();
    for (auto sett : conn->settings()->settings())
    {
        //TODO: correct info assembly
        //or return the setting and leave the presentation to the end user
        switch (sett->type())
        {
            case NetworkManager::Setting::Adsl:
                str << dynamic_cast<NetworkManager::AdslSetting&>(*sett);
                break;
            case NetworkManager::Setting::Cdma:
                str << dynamic_cast<NetworkManager::CdmaSetting&>(*sett);
                break;
            case NetworkManager::Setting::Gsm:
                str << dynamic_cast<NetworkManager::GsmSetting&>(*sett);
                break;
            case NetworkManager::Setting::Infiniband:
                str << dynamic_cast<NetworkManager::InfinibandSetting&>(*sett);
                break;
            case NetworkManager::Setting::Ipv4:
                str << dynamic_cast<NetworkManager::Ipv4Setting&>(*sett);
                break;
            case NetworkManager::Setting::Ipv6:
                str << dynamic_cast<NetworkManager::Ipv6Setting&>(*sett);
                break;
            case NetworkManager::Setting::Ppp:
                str << dynamic_cast<NetworkManager::PppSetting&>(*sett);
                break;
            case NetworkManager::Setting::Pppoe:
                str << dynamic_cast<NetworkManager::PppoeSetting&>(*sett);
                break;
            case NetworkManager::Setting::Security8021x:
                str << dynamic_cast<NetworkManager::Security8021xSetting&>(*sett);
                break;
            case NetworkManager::Setting::Serial:
                str << dynamic_cast<NetworkManager::SerialSetting&>(*sett);
                break;
            case NetworkManager::Setting::Vpn:
                str << dynamic_cast<NetworkManager::VpnSetting&>(*sett);
                break;
            case NetworkManager::Setting::Wired:
                str << dynamic_cast<NetworkManager::WiredSetting&>(*sett);
                break;
            case NetworkManager::Setting::Wireless:
                str << dynamic_cast<NetworkManager::WirelessSetting&>(*sett);
                break;
            case NetworkManager::Setting::WirelessSecurity:
                str << dynamic_cast<NetworkManager::WirelessSecuritySetting&>(*sett);
                break;
            case NetworkManager::Setting::Bluetooth:
                str << dynamic_cast<NetworkManager::BluetoothSetting&>(*sett);
                break;
            case NetworkManager::Setting::OlpcMesh:
                str << dynamic_cast<NetworkManager::OlpcMeshSetting&>(*sett);
                break;
            case NetworkManager::Setting::Vlan:
                str << dynamic_cast<NetworkManager::VlanSetting&>(*sett);
                break;
            case NetworkManager::Setting::Wimax:
                str << dynamic_cast<NetworkManager::WimaxSetting&>(*sett);
                break;
            case NetworkManager::Setting::Bond:
                str << dynamic_cast<NetworkManager::BondSetting&>(*sett);
                break;
            case NetworkManager::Setting::Bridge:
                str << dynamic_cast<NetworkManager::BridgeSetting&>(*sett);
                break;
            case NetworkManager::Setting::BridgePort:
                str << dynamic_cast<NetworkManager::BridgePortSetting&>(*sett);
                break;
            case NetworkManager::Setting::Team:
                str << dynamic_cast<NetworkManager::TeamSetting&>(*sett);
                break;
            case NetworkManager::Setting::Generic:
                str << dynamic_cast<NetworkManager::GenericSetting&>(*sett);
                break;
        }
        str << *sett;
        /*
qDebug() << QStringLiteral("<big><strong>") << sett->name() << QStringLiteral("</strong></big>\n");
        str << QStringLiteral("<big><strong>") << sett->name() << QStringLiteral("</strong></big>\n");
        for (auto i = sett->toMap().cbegin(), i_e = sett->toMap().cend(); i != i_e; ++i)
        {
qDebug() << QStringLiteral("<strong>") << i.key() << QStringLiteral("</strong>: ") << i.value().toString() << QLatin1Char('\n');
            str << QStringLiteral("<strong>") << i.key() << QStringLiteral("</strong>: ") << i.value().toString() << QLatin1Char('\n');
        }
        */
    }
#endif
    return info;
}

template <>
QVariant NmModel::dataRole<NmModel::IconTypeRole>(const QModelIndex & index) const
{
    switch (static_cast<ItemId>(index.internalId()))
    {
        case ITEM_ROOT:
        case ITEM_ACTIVE:
        case ITEM_CONNECTION:
        case ITEM_DEVICE:
        case ITEM_WIFINET:
            return -1;
        case ITEM_ACTIVE_LEAF:
        case ITEM_CONNECTION_LEAF:
        case ITEM_DEVICE_LEAF:
            {
                auto type = static_cast<NetworkManager::ConnectionSettings::ConnectionType>(dataRole<ConnectionTypeRole>(index).toInt());
                auto state = static_cast<NetworkManager::ActiveConnection::State>(dataRole<ActiveConnectionStateRole>(index).toInt());
                //TODO other types?
                switch (type)
                {
                    case NetworkManager::ConnectionSettings::Wireless:
                    case NetworkManager::ConnectionSettings::Wimax:
                        if (0 > state)
                            return icons::NETWORK_WIFI_DISCONNECTED;
                        switch (state)
                        {
                            case NetworkManager::ActiveConnection::Activating:
                                return icons::NETWORK_WIFI_ACQUIRING;
                            case NetworkManager::ActiveConnection::Activated:
                                return icons::wifiSignalIcon(dataRole<SignalRole>(index).toInt());
                            case NetworkManager::ActiveConnection::Unknown:
                            case NetworkManager::ActiveConnection::Deactivating:
                            case NetworkManager::ActiveConnection::Deactivated:
                                return icons::NETWORK_WIFI_DISCONNECTED;
                        }
                        break;
                    default:
                        return NetworkManager::ActiveConnection::Activated == state ? icons::NETWORK_WIRED : icons::NETWORK_WIRED_DISCONNECTED;
                }
            }
        case ITEM_WIFINET_LEAF:
            return icons::wifiSignalIcon(d->mWifiNets[index.row()]->signalStrength());
    }
}

template <>
QVariant NmModel::dataRole<NmModel::IconRole>(const QModelIndex & index) const
{
    return icons::getIcon(static_cast<icons::Icon>(dataRole<IconTypeRole>(index).toInt()));
}

QVariant NmModel::data(const QModelIndex &index, int role) const
{
//qDebug() << __FUNCTION__ << index << role;
    Q_ASSERT(isValidDataIndex(index));
    QVariant ret;
    if (index.isValid())
        switch (role)
        {
            case ItemTypeRole:
                ret = dataRole<ItemTypeRole>(index);
                break;
            case Qt::DisplayRole:
            case NameRole:
                ret = dataRole<NameRole>(index);
                break;
            case ConnectionTypeRole:
                ret = dataRole<ConnectionTypeRole>(index);
                break;
            case ActiveConnectionStateRole:
                ret = dataRole<ActiveConnectionStateRole>(index);
                break;
            case SignalRole:
                ret = dataRole<SignalRole>(index);
                break;
            case ConnectionUuidRole:
                ret = dataRole<ConnectionUuidRole>(index);
                break;
            case ActiveConnectionInfoRole:
                ret = dataRole<ActiveConnectionInfoRole>(index);
                break;
            case IconTypeRole:
                ret = dataRole<IconTypeRole>(index);
                break;
            case Qt::DecorationRole:
            case IconRole:
                ret = dataRole<IconRole>(index);
                break;
            default:
                ret = QVariant{};
                break;
        }
//qDebug() << __FUNCTION__ << "ret" << index << role << ret;
    return ret;
}

QModelIndex NmModel::index(int row, int column, const QModelIndex &parent/* = QModelIndex()*/) const
{
//qDebug() << __FUNCTION__ << row << column << parent;
    if (!hasIndex(row, column, parent))
        return QModelIndex{};
    const int id = parent.internalId();
    ItemId int_id;
    if (!parent.isValid())
    {
        Q_ASSERT(0 == row && 0 == column);
        int_id = ITEM_ROOT;
    } else if (ITEM_ROOT == id)
    {
        Q_ASSERT(4 > row && 0 == column);
        switch (row)
        {
            case 0: int_id = ITEM_ACTIVE; break;
            case 1: int_id = ITEM_CONNECTION; break;
            case 2: int_id = ITEM_DEVICE; break;
            case 3: int_id = ITEM_WIFINET; break;
        }
    } else if (ITEM_ACTIVE == id)
    {
        Q_ASSERT(d->mActiveConns.size() > row);
        int_id = ITEM_ACTIVE_LEAF;
    }
    else if (ITEM_CONNECTION == id)
    {
        Q_ASSERT(d->mConnections.size() > row);
        int_id = ITEM_CONNECTION_LEAF;
    }
    else if (ITEM_DEVICE == id)
    {
        Q_ASSERT(d->mDevices.size() > row);
        int_id = ITEM_DEVICE_LEAF;
    }
    else if (ITEM_WIFINET == id)
    {
        Q_ASSERT(d->mWifiNets.size() > row);
        int_id = ITEM_WIFINET_LEAF;
    }
    else
    {
        Q_ASSERT(false); //should never get here
        return QModelIndex{};
    }

//qDebug() << __FUNCTION__ << "ret: " << row << column << int_id;
    return createIndex(row, column, int_id);
}

QModelIndex NmModel::parent(const QModelIndex &index) const
{
    QModelIndex parent_i;
    if (index.isValid())
    {
        switch (static_cast<ItemId>(index.internalId()))
        {
            case ITEM_ROOT:
                break;
            case ITEM_ACTIVE:
            case ITEM_CONNECTION:
            case ITEM_DEVICE:
            case ITEM_WIFINET:
                parent_i = createIndex(0, 0, ITEM_ROOT);
                break;
            case ITEM_ACTIVE_LEAF:
                parent_i = createIndex(0, 0, ITEM_ACTIVE);
                break;
            case ITEM_CONNECTION_LEAF:
                parent_i = createIndex(1, 0, ITEM_CONNECTION);
                break;
            case ITEM_DEVICE_LEAF:
                parent_i = createIndex(2, 0, ITEM_DEVICE);
                break;
            case ITEM_WIFINET_LEAF:
                parent_i = createIndex(3, 0, ITEM_WIFINET);
                break;
        }
    }

//qDebug() << __FUNCTION__ << index << parent_i;
    return parent_i;
}

QModelIndex NmModel::indexTypeRoot(ItemType type) const
{
    QModelIndex i;
    switch (type)
    {
        case HelperType:
            i = createIndex(0, 0, ITEM_ROOT);
            break;
        case ActiveConnectionType:
            i = createIndex(0, 0, ITEM_ACTIVE);
            break;
        case ConnectionType:
            i = createIndex(1, 0, ITEM_CONNECTION);
            break;
        case DeviceType:
            i = createIndex(2, 0, ITEM_DEVICE);
            break;
        case WifiNetworkType:
            i = createIndex(3, 0, ITEM_WIFINET);
            break;
    }
    return i;
}

void NmModel::activateConnection(QModelIndex const & index)
{
    ItemId id = static_cast<ItemId>(index.internalId());
    if (!isValidDataIndex(index) || (ITEM_CONNECTION_LEAF != id && ITEM_WIFINET_LEAF != id))
    {
        //TODO: in what form should we output the warning messages
        qWarning() << "got invalid index for connection activation" << index;
        return;
    }
    QString conn_uni, dev_uni;
    QString conn_name, dev_name;
    QString spec_object;
    switch (id)
    {
        case ITEM_CONNECTION_LEAF:
            {
                auto const & conn = d->mConnections[index.row()];
                conn_uni = conn->path();
                conn_name = conn->name();
                dev_name = conn->settings()->interfaceName();
                for (auto const & dev : d->mDevices)
                    for (auto const & dev_conn : dev->availableConnections())
                        if (dev_conn == conn)
                        {
                            dev_uni = dev->uni();
                            dev_name = dev->interfaceName();
                        }
                if (dev_uni.isEmpty() && !dev_name.isEmpty())
                {
                    auto dev = d->findDeviceInterface(dev_name);
                    if (!dev.isNull())
                        dev_uni = dev->uni();
                }
                if (dev_uni.isEmpty())
                {
                    //TODO: in what form should we output the warning messages
                    qWarning() << QStringLiteral("can't find device '%1' to activate connection '%2' on").arg(dev_name).arg(conn->name());
                    return;
                }
            }
            break;
        case ITEM_WIFINET_LEAF:
            {
                auto const & net = d->mWifiNets[index.row()];
                conn_uni = net->referenceAccessPoint()->uni();
                conn_name = net->referenceAccessPoint()->ssid();
                dev_uni = net->device();
                //find the device name
                auto const & dev = d->findDeviceUni(dev_uni);
                Q_ASSERT(!dev.isNull());
                NetworkManager::Connection::Ptr conn;
                dev_name = dev->interfaceName();
                for (auto const & dev_conn : dev->availableConnections())
                    if (dev_conn->settings()->id() == conn_name)
                    {
                        conn = dev_conn;
                    }
                if (conn.isNull())
                {
                    //TODO: in what form should we output the warning messages
                    qWarning() << QStringLiteral("can't find connection for '%1' on device '%2'").arg(conn_name).arg(dev_name);
                    return;
                }
                conn_uni = conn->path();

                //spec_object = net->ssid();
            }
            break;
        default:
            Q_ASSERT(false);
    }
qDebug() << __FUNCTION__ << conn_uni << dev_uni << conn_name << dev_name;
    //TODO: check vpn type etc..
    QDBusPendingReply<QDBusObjectPath> reply = NetworkManager::activateConnection(conn_uni, dev_uni, spec_object);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, [conn_name, dev_name] (QDBusPendingCallWatcher * watcher) {
        if (watcher->isError() || !watcher->isValid())
        {
            //TODO: in what form should we output the warning messages
            qWarning() << QStringLiteral("activation of connection '%1' on interface '%2' failed: %3").arg(conn_name)
                    .arg(dev_name).arg(watcher->error().message());
         }
         watcher->deleteLater();
    });
}

void NmModel::deactivateConnection(QModelIndex const & index)
{
    ItemId id = static_cast<ItemId>(index.internalId());
    if (!isValidDataIndex(index) || ITEM_ACTIVE_LEAF != id)
    {
        //TODO: in what form should we output the warning messages
        qWarning() << "got invalid index for connection deactivation" << index;
        return;
    }

    auto const & active = d->mActiveConns[index.row()];
qDebug() << __FUNCTION__ << active->path();
    QDBusPendingReply<> reply = NetworkManager::deactivateConnection(active->path());
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, [active] (QDBusPendingCallWatcher * watcher) {
        if (watcher->isError() || !watcher->isValid())
        {
            //TODO: in what form should we output the warning messages
            qWarning() << QStringLiteral("deactivation of connection '%1' failed: %3").arg(active->connection()->name())
                    .arg(watcher->error().message());
         }
         watcher->deleteLater();
    });
}
