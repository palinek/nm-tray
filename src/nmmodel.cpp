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
#include "nmmodel.h"
#include "nmmodel_p.h"
#include "icons.h"
#include "log.h"

#include <NetworkManagerQt/GenericTypes>
#include <NetworkManagerQt/VpnConnection>
#include <NetworkManagerQt/WirelessDevice>
#include <NetworkManagerQt/AdslDevice>
#include <NetworkManagerQt/WiredDevice>
#include <NetworkManagerQt/WimaxDevice>
#include <NetworkManagerQt/VlanDevice>
#include <NetworkManagerQt/BondDevice>
#include <NetworkManagerQt/BridgeDevice>
#include <NetworkManagerQt/GenericDevice>
#include <NetworkManagerQt/InfinibandDevice>
#include <NetworkManagerQt/BluetoothDevice>
#include <NetworkManagerQt/OlpcMeshDevice>
#include <NetworkManagerQt/TeamDevice>
#include <NetworkManagerQt/WirelessSetting>
#include <NetworkManagerQt/WirelessSecuritySetting>
#include <NetworkManagerQt/Utils>
#include <NetworkManagerQt/ConnectionSettings>
#include <QDBusPendingCallWatcher>
#include <QInputDialog>

namespace
{
    NetworkManager::ConnectionSettings::Ptr assembleWpaXPskSettings(const NetworkManager::AccessPoint::Ptr accessPoint)
    {
        //TODO: enhance getting the password from user
        bool ok;
        QString password = QInputDialog::getText(nullptr, NmModel::tr("nm-tray - wireless password")
                , NmModel::tr("Password is needed for connection to '%1':").arg(accessPoint->ssid())
                , QLineEdit::Password, QString(), &ok);
        if (!ok)
            return NetworkManager::ConnectionSettings::Ptr{nullptr};

        NetworkManager::ConnectionSettings::Ptr settings{new NetworkManager::ConnectionSettings{NetworkManager::ConnectionSettings::Wireless}};
        settings->setId(accessPoint->ssid());
        settings->setUuid(NetworkManager::ConnectionSettings::createNewUuid());
        settings->setAutoconnect(true);
        //Note: workaround for wrongly (randomly) initialized gateway-ping-timeout
        settings->setGatewayPingTimeout(0);

        NetworkManager::WirelessSetting::Ptr wifi_sett
            = settings->setting(NetworkManager::Setting::Wireless).dynamicCast<NetworkManager::WirelessSetting>();
        wifi_sett->setInitialized(true);
        wifi_sett->setSsid(accessPoint->ssid().toUtf8());
        wifi_sett->setSecurity("802-11-wireless-security");

        NetworkManager::WirelessSecuritySetting::Ptr security_sett
            = settings->setting(NetworkManager::Setting::WirelessSecurity).dynamicCast<NetworkManager::WirelessSecuritySetting>();
        security_sett->setInitialized(true);
        if (NetworkManager::AccessPoint::Adhoc == accessPoint->mode())
        {
            wifi_sett->setMode(NetworkManager::WirelessSetting::Adhoc);
            security_sett->setKeyMgmt(NetworkManager::WirelessSecuritySetting::WpaNone);
        } else
        {
            security_sett->setKeyMgmt(NetworkManager::WirelessSecuritySetting::WpaPsk);
        }
        security_sett->setPsk(password);
        return settings;
    }
}

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
    
//qCDebug(NM_TRAY) << mActiveConns.size() << mConnections.size() << mDevices.size();
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
    disconnect(conn.data(), &NetworkManager::ActiveConnection::dhcp4ConfigChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    disconnect(conn.data(), &NetworkManager::ActiveConnection::dhcp6ConfigChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    disconnect(conn.data(), &NetworkManager::ActiveConnection::ipV4ConfigChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    disconnect(conn.data(), &NetworkManager::ActiveConnection::ipV6ConfigChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
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
    connect(conn.data(), &NetworkManager::ActiveConnection::dhcp4ConfigChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    connect(conn.data(), &NetworkManager::ActiveConnection::dhcp6ConfigChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    connect(conn.data(), &NetworkManager::ActiveConnection::ipV4ConfigChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
    connect(conn.data(), &NetworkManager::ActiveConnection::ipV6ConfigChanged, this, &NmModelPrivate::onActiveConnectionUpdated);
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
    disconnect(device.data(), &NetworkManager::Device::physicalPortIdChanged, this, &NmModelPrivate::onDeviceUpdated);
    disconnect(device.data(), &NetworkManager::Device::mtuChanged, this, &NmModelPrivate::onDeviceUpdated);
    disconnect(device.data(), &NetworkManager::Device::nmPluginMissingChanged, this, &NmModelPrivate::onDeviceUpdated);
    disconnect(device.data(), &NetworkManager::Device::meteredChanged, this, &NmModelPrivate::onDeviceUpdated);
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
    connect(device.data(), &NetworkManager::Device::physicalPortIdChanged, this, &NmModelPrivate::onDeviceUpdated);
    connect(device.data(), &NetworkManager::Device::mtuChanged, this, &NmModelPrivate::onDeviceUpdated);
    connect(device.data(), &NetworkManager::Device::nmPluginMissingChanged, this, &NmModelPrivate::onDeviceUpdated);
    connect(device.data(), &NetworkManager::Device::meteredChanged, this, &NmModelPrivate::onDeviceUpdated);
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
    if (!dev.isNull())
    {
        Q_ASSERT(dev->isValid());
        emit deviceAdd(dev);
    }
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
    if (!conn.isNull())
    {
        Q_ASSERT(conn->isValid());
        emit activeConnectionAdd(conn);
    }
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
    if (!conn.isNull())
    {
        Q_ASSERT(conn->isValid());
        emit connectionAdd(conn);
    }
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
//qCDebug(NM_TRAY) << "connectionAdd" << conn->name();
        if (0 > d->mConnections.indexOf(conn))
        {
            const int cnt = d->mConnections.size();
            beginInsertRows(createIndex(1, 0, ITEM_CONNECTION), cnt, cnt);
            d->addConnection(conn);
            endInsertRows();
        } else
        {
            //TODO: onConnectionUpdate
        }
    });
    connect(d.data(), &NmModelPrivate::connectionUpdate, [this] (NetworkManager::Connection * conn) {
//qCDebug(NM_TRAY) << "connectionUpdate" << conn->name();
        auto i = std::find(d->mConnections.cbegin(), d->mConnections.cend(), conn);
        if (d->mConnections.cend() != i)
        {
            QModelIndex index = createIndex(i - d->mConnections.cbegin(), 0, ITEM_CONNECTION_LEAF);
            emit dataChanged(index, index);
        }
    });
    connect(d.data(), &NmModelPrivate::connectionRemove, [this] (NetworkManager::Connection * conn) {
//qCDebug(NM_TRAY) << "connectionRemove" << conn->name();
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
//qCDebug(NM_TRAY) << "activecCnnectionAdd" << conn->connection()->name();
        if (0 > d->mActiveConns.indexOf(conn))
        {
            const int cnt = d->mActiveConns.size();
            beginInsertRows(createIndex(0, 0, ITEM_ACTIVE), cnt, cnt);
            d->addActiveConnection(conn);
            endInsertRows();
        } else
        {
            //TODO: onActiveConnectionUpdate
        }
    });
    connect(d.data(), &NmModelPrivate::activeConnectionUpdate, [this] (NetworkManager::ActiveConnection * conn) {
//qCDebug(NM_TRAY) << "activecCnnectionUpdate" << conn->connection()->name();
        auto i = std::find(d->mActiveConns.cbegin(), d->mActiveConns.cend(), conn);
        if (d->mActiveConns.cend() != i)
        {
            QModelIndex index = createIndex(i - d->mActiveConns.cbegin(), 0, ITEM_ACTIVE_LEAF);
            emit dataChanged(index, index);
        }
    });
    connect(d.data(), &NmModelPrivate::activeConnectionRemove, [this] (NetworkManager::ActiveConnection * conn) {
//qCDebug(NM_TRAY) << "activecCnnectionRemove" << conn->connection()->name();
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
//qCDebug(NM_TRAY) << "activecCnnectionReset";
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
//qCDebug(NM_TRAY) << "deviceAdd" << dev->interfaceName();
        if (0 > d->mDevices.indexOf(dev))
        {
            const int cnt = d->mDevices.size();
            beginInsertRows(createIndex(2, 0, ITEM_DEVICE), cnt, cnt);
            d->addDevice(dev);
            endInsertRows();
        } else
        {
            //TODO: onDeviceUpdate
        }
    });
    connect(d.data(), &NmModelPrivate::deviceUpdate, [this] (NetworkManager::Device * dev) {
//qCDebug(NM_TRAY) << "deviceUpdate" << dev << dev->interfaceName();
        auto i = std::find(d->mDevices.cbegin(), d->mDevices.cend(), dev);
        if (d->mDevices.cend() != i)
        {
            QModelIndex index = createIndex(i - d->mDevices.cbegin(), 0, ITEM_DEVICE_LEAF);
            emit dataChanged(index, index);
        }
    });
    connect(d.data(), &NmModelPrivate::deviceRemove, [this] (NetworkManager::Device * dev) {
//qCDebug(NM_TRAY) << "deviceRemove" << dev->interfaceName();
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
//qCDebug(NM_TRAY) << "wifiNetworkAdd" << dev << dev->interfaceName() << ssid;
        NetworkManager::WirelessDevice * w_dev = qobject_cast<NetworkManager::WirelessDevice *>(dev);
        NetworkManager::WirelessNetwork::Ptr net = w_dev->findNetwork(ssid);
        if (!net.isNull())
        {
            if (0 > d->mWifiNets.indexOf(net))
            {
                const int cnt = d->mWifiNets.size();
                beginInsertRows(createIndex(3, 0, ITEM_WIFINET), cnt, cnt);
                d->addWifiNetwork(net);
                endInsertRows();
            } else
            {
                //TODO: onWifiNetworkUpdate
            }
        }
    });
    connect(d.data(), &NmModelPrivate::wifiNetworkUpdate, [this] (NetworkManager::WirelessNetwork * net) {
//qCDebug(NM_TRAY) << "wifiNetworkUpdate" << net << net->ssid();
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
//qCDebug(NM_TRAY) << "wifiNetworkRemove" << dev << dev->interfaceName() << ssid;
        NetworkManager::WirelessNetwork::Ptr net = d->findWifiNetwork(ssid, dev->uni());
        if (!net.isNull())
        {
            auto pos = d->mWifiNets.indexOf(net);
            if (0 <= pos)
            {
                beginRemoveRows(createIndex(3, 0, ITEM_WIFINET), pos, pos);
                d->removeWifiNetwork(pos);
                endRemoveRows();
            }
        }
    });
//qCDebug(NM_TRAY) << __FUNCTION__ << "finished";
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

//qCDebug(NM_TRAY) << __FUNCTION__ << parent << cnt;
    return cnt;
}

int NmModel::columnCount(const QModelIndex & /*parent = QModelIndex()*/) const
{
//qCDebug(NM_TRAY) << __FUNCTION__ << parent << 1;
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
    return QVariant{};
}

template <>
QVariant NmModel::dataRole<NmModel::NameRole>(const QModelIndex & index) const
{
    switch (static_cast<ItemId>(index.internalId()))
    {
        case ITEM_ROOT:
            return NmModel::tr("root");
        case ITEM_ACTIVE:
            return NmModel::tr("active connection(s)");
        case ITEM_CONNECTION:
            return NmModel::tr("connection(s)");
        case ITEM_DEVICE:
            return NmModel::tr("device(s)");
        case ITEM_WIFINET:
            return NmModel::tr("wifi network(s)");
        case ITEM_ACTIVE_LEAF:
            return d->mActiveConns[index.row()]->connection()->name();
        case ITEM_CONNECTION_LEAF:
            return d->mConnections[index.row()]->name();
        case ITEM_DEVICE_LEAF:
            return d->mDevices[index.row()]->interfaceName();
        case ITEM_WIFINET_LEAF:
            return d->mWifiNets[index.row()]->referenceAccessPoint()->ssid();
    }
    return QVariant{};
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
    return QVariant{};
}

template <>
QVariant NmModel::dataRole<NmModel::ConnectionTypeStringRole>(const QModelIndex & index) const
{
    const QVariant conn_type = dataRole<ConnectionTypeRole>(index);
    if (!conn_type.isValid())
    {
        return QVariant{};
    }
    return NetworkManager::ConnectionSettings::typeAsString(static_cast<NetworkManager::ConnectionSettings::ConnectionType>(conn_type.toInt()));
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
    return QVariant{};
}

template <>
QVariant NmModel::dataRole<NmModel::ActiveConnectionMasterRole>(const QModelIndex & index) const
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
            return QVariant{};
        case ITEM_ACTIVE_LEAF:
            return d->mActiveConns[index.row()]->master();
    }
    return QVariant{};
}

template <>
QVariant NmModel::dataRole<NmModel::ActiveConnectionDevicesRole>(const QModelIndex & index) const
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
            return QVariant{};
        case ITEM_ACTIVE_LEAF:
            return d->mActiveConns[index.row()]->devices();
    }
    return QVariant{};
}

template <>
QVariant NmModel::dataRole<NmModel::IconSecurityTypeRole>(const QModelIndex & index) const
{
    auto const internal_id = static_cast<ItemId>(index.internalId());
    switch (internal_id)
    {
        case ITEM_ROOT:
        case ITEM_ACTIVE:
        case ITEM_CONNECTION:
        case ITEM_DEVICE:
        case ITEM_WIFINET:
        case ITEM_DEVICE_LEAF:
            return -1;
        case ITEM_CONNECTION_LEAF:
        case ITEM_ACTIVE_LEAF:
            {
                NetworkManager::ConnectionSettings::Ptr settings = ITEM_CONNECTION_LEAF == internal_id
                    ? d->mConnections[index.row()]->settings()
                    : d->mActiveConns[index.row()]->connection()->settings();
                if (NetworkManager::ConnectionSettings::Wireless == settings->connectionType())
                {
                    NetworkManager::WirelessSecuritySetting::Ptr w_sett = settings->setting(NetworkManager::Setting::WirelessSecurity).staticCast<NetworkManager::WirelessSecuritySetting>();
                    if (w_sett.isNull())
                        return icons::SECURITY_LOW;
                    else if (NetworkManager::WirelessSecuritySetting::WpaNone != w_sett->keyMgmt())
                        return icons::SECURITY_HIGH;
                    else
                        return icons::SECURITY_LOW;
                }
                return -1;
            }
        case ITEM_WIFINET_LEAF:
            return d->mWifiNets[index.row()]->referenceAccessPoint()->capabilities().testFlag(NetworkManager::AccessPoint::Privacy)
                ? icons::SECURITY_HIGH
                : icons::SECURITY_LOW;
    }
    return QVariant{};
}

template <>
QVariant NmModel::dataRole<NmModel::IconSecurityRole>(const QModelIndex & index) const
{
    const auto type = dataRole<IconSecurityTypeRole>(index).toInt();
    if (0 <= type)
        return icons::getIcon(static_cast<icons::Icon>(type));
    else
        return QVariant{};
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
    return QVariant{};
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
    return QVariant{};
}

template <>
QVariant NmModel::dataRole<NmModel::ConnectionPathRole>(const QModelIndex & index) const
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
            return d->mActiveConns[index.row()]->path();
        case ITEM_CONNECTION_LEAF:
            return d->mConnections[index.row()]->path();
    }
    return QVariant{};
}

template <>
QVariant NmModel::dataRole<NmModel::ActiveConnectionInfoRole>(const QModelIndex & index) const
{
    NetworkManager::Device::Ptr dev;
    NetworkManager::ConnectionSettings::Ptr settings;
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
                auto const a_conn = d->mActiveConns[index.row()];
                auto const dev_list = a_conn->devices();
                //TODO: work with all devices?!?
                if (!dev_list.isEmpty())
                    dev = d->findDeviceUni(dev_list.first());
                Q_ASSERT(!a_conn->connection().isNull());
                settings = a_conn->connection()->settings();
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

        QString hw_address = NmModel::tr("unknown", "hardware address");
        QString security;
        int bit_rate = -1;
        switch (dev->type())
        {
            case NetworkManager::Device::Adsl:
                break;
            case NetworkManager::Device::Bond:
                {
                    auto spec_dev = dev->as<NetworkManager::BondDevice>();
                    hw_address = spec_dev->hwAddress();
                }
                break;
            case NetworkManager::Device::Bridge:
                {
                    auto spec_dev = dev->as<NetworkManager::BridgeDevice>();
                    hw_address = spec_dev->hwAddress();
                }
                break;
            case NetworkManager::Device::Gre:
                break;
            case NetworkManager::Device::Generic:
                {
                    auto spec_dev = dev->as<NetworkManager::GenericDevice>();
                    hw_address = spec_dev->hardwareAddress();
                }
                break;
            case NetworkManager::Device::InfiniBand:
                {
                    auto spec_dev = dev->as<NetworkManager::InfinibandDevice>();
                    hw_address = spec_dev->hwAddress();
                }
                break;
            case NetworkManager::Device::MacVlan:
            case NetworkManager::Device::Modem:
                break;
            case NetworkManager::Device::Bluetooth:
                {
                    auto spec_dev = dev->as<NetworkManager::BluetoothDevice>();
                    hw_address = spec_dev->hardwareAddress();
                }
                break;
            case NetworkManager::Device::OlpcMesh:
                {
                    auto spec_dev = dev->as<NetworkManager::OlpcMeshDevice>();
                    hw_address = spec_dev->hardwareAddress();
                }
                break;
            case NetworkManager::Device::Team:
                {
                    auto spec_dev = dev->as<NetworkManager::TeamDevice>();
                    hw_address = spec_dev->hwAddress();
                }
                break;
            case NetworkManager::Device::Tun:
            case NetworkManager::Device::Veth:
                break;
            case NetworkManager::Device::Vlan:
                {
                    auto spec_dev = dev->as<NetworkManager::VlanDevice>();
                    hw_address = spec_dev->hwAddress();
                }
                break;
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
                    NetworkManager::Setting::Ptr setting = settings->setting(NetworkManager::Setting::WirelessSecurity);
                    if (!setting.isNull())
                    {
                        QVariantMap const map = setting->toMap();
                        if (map.contains(QLatin1String(NM_SETTING_WIRELESS_SECURITY_KEY_MGMT)))
                            security = map.value(QLatin1String(NM_SETTING_WIRELESS_SECURITY_KEY_MGMT)).toString();
                    }
                }
                break;
            case NetworkManager::Device::Wimax:
                //Wimax support was dropped in network manager 1.2.0
                //we should never get here in runtime with nm >= 1.2.0
                {
                    auto spec_dev = dev->as<NetworkManager::WimaxDevice>();
                    Q_ASSERT(nullptr != spec_dev);
                    hw_address = spec_dev->hardwareAddress();
                    //bit_rate = spec_dev->bitRate();
                }
                break;
            case NetworkManager::Device::UnknownType:
            case NetworkManager::Device::Unused1:
            case NetworkManager::Device::Unused2:
                break;

        }
        str << QStringLiteral("<table>")
            << QStringLiteral("<tr><td colspan='2'><big><strong>") << NmModel::tr("General", "Active connection information") << QStringLiteral("</strong></big></td></tr>")
            << QStringLiteral("<tr><td><strong>") << NmModel::tr("Interface", "Active connection information") << QStringLiteral("</strong>: </td><td>")
                << m_enum.valueToKey(dev->type()) << QStringLiteral(" (") << dev->interfaceName() << QStringLiteral(")</td></tr>")
            << QStringLiteral("<tr><td><strong>") << NmModel::tr("Hardware Address", "Active connection information") << QStringLiteral("</strong>: </td><td>")
                << hw_address << QStringLiteral("</td></tr>")
            << QStringLiteral("<tr><td><strong>") << NmModel::tr("Driver", "Active connection information") << QStringLiteral("</strong>: </td><td>")
                << dev->driver() << QStringLiteral("</td></tr>")
            << QStringLiteral("<tr><td><strong>") << NmModel::tr("Speed", "Active connection information") << QStringLiteral("</strong>: </td><td>");
        if (0 <= bit_rate)
            str << bit_rate << NmModel::tr(" Kb/s");
        else
            str << NmModel::tr("unknown", "Speed");
        str << QStringLiteral("</td></tr>");
        if (!security.isEmpty())
        {
            str << QStringLiteral("<tr><td><strong>") << NmModel::tr("Security", "Active connection information") << QStringLiteral("</strong>: </td><td>")
                << security << QStringLiteral("</td></tr>");
        }
        //IP4/6
        QString const ip4 = NmModel::tr("IPv4", "Active connection information");
        QString const ip6 = NmModel::tr("IPv6", "Active connection information");
        for (auto const & ip : { std::make_tuple(ip4, dev->ipV4Config()) , std::make_tuple(ip6, dev->ipV6Config()) })
        {
            NetworkManager::IpConfig ip_config = std::get<1>(ip);
            if (ip_config.isValid())
            {
                str << QStringLiteral("<tr/><tr><td colspan='2'><big><strong>") << std::get<0>(ip) << QStringLiteral("</strong></big></td></tr>");
                int i = 1;
                for (QNetworkAddressEntry const & address : ip_config.addresses())
                {
                    QString suffix = (i > 1 ? QString{"(%1)"}.arg(i) : QString{});
                    str << QStringLiteral("<tr><td><strong>") << NmModel::tr("IP Address", "Active connection information") << suffix << QStringLiteral("</strong>: </td><td>")
                        << address.ip().toString() << QStringLiteral("</td></tr>")
                        << QStringLiteral("<tr><td><strong>") << NmModel::tr("Subnet Mask", "Active connection information") << suffix << QStringLiteral("</strong>: </td><td>")
                        << address.netmask().toString() << QStringLiteral("</td></tr>")
                        ;
                    ++i;
                }

                QString const gtw = ip_config.gateway();
                if (!gtw.isEmpty())
                {
                    str << QStringLiteral("<tr><td><strong>") << NmModel::tr("Default route", "Active connection information") << QStringLiteral("</strong>: </td><td>")
                        << ip_config.gateway() << QStringLiteral("</td></tr>");
                }
                i = 0;
                for (auto const & nameserver : ip_config.nameservers())
                {
                    str << QStringLiteral("<tr><td><strong>") << NmModel::tr("DNS(%1)", "Active connection information").arg(++i) << QStringLiteral("</strong>: </td><td>")
                        << nameserver.toString() << QStringLiteral("</td></tr>");
                }
            }
        }
        str << QStringLiteral("</table>");
    }
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
                    case NetworkManager::ConnectionSettings::Vpn:
                        return icons::NETWORK_VPN;
                    default:
                        return NetworkManager::ActiveConnection::Activated == state ? icons::NETWORK_WIRED : icons::NETWORK_WIRED_DISCONNECTED;
                }
            }
        case ITEM_WIFINET_LEAF:
            return icons::wifiSignalIcon(d->mWifiNets[index.row()]->signalStrength());
    }
    return QVariant{};
}

template <>
QVariant NmModel::dataRole<NmModel::IconRole>(const QModelIndex & index) const
{
    return icons::getIcon(static_cast<icons::Icon>(dataRole<IconTypeRole>(index).toInt()));
}

QVariant NmModel::data(const QModelIndex &index, int role) const
{
//qCDebug(NM_TRAY) << __FUNCTION__ << index << role;
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
            case ConnectionTypeStringRole:
                ret = dataRole<ConnectionTypeStringRole>(index);
                break;
            case ActiveConnectionStateRole:
                ret = dataRole<ActiveConnectionStateRole>(index);
                break;
            case ActiveConnectionMasterRole:
                ret = dataRole<ActiveConnectionMasterRole>(index);
                break;
            case ActiveConnectionDevicesRole:
                ret = dataRole<ActiveConnectionDevicesRole>(index);
                break;
            case SignalRole:
                ret = dataRole<SignalRole>(index);
                break;
            case ConnectionUuidRole:
                ret = dataRole<ConnectionUuidRole>(index);
                break;
            case ConnectionPathRole:
                ret = dataRole<ConnectionPathRole>(index);
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
            case IconSecurityTypeRole:
                ret = dataRole<IconSecurityTypeRole>(index);
                break;
            case IconSecurityRole:
                ret = dataRole<IconSecurityRole>(index);
                break;
            default:
                ret = QVariant{};
                break;
        }
//qCDebug(NM_TRAY) << __FUNCTION__ << "ret" << index << role << ret;
    return ret;
}

QModelIndex NmModel::index(int row, int column, const QModelIndex &parent/* = QModelIndex()*/) const
{
//qCDebug(NM_TRAY) << __FUNCTION__ << row << column << parent;
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

//qCDebug(NM_TRAY) << __FUNCTION__ << "ret: " << row << column << int_id;
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

//qCDebug(NM_TRAY) << __FUNCTION__ << index << parent_i;
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
        qCWarning(NM_TRAY).noquote() << "got invalid index for connection activation" << index;
        return;
    }
    QString conn_uni, dev_uni;
    QString conn_name, dev_name;
    QString spec_object;
    NMVariantMapMap map_settings;
    bool add_connection = false;
    switch (id)
    {
        case ITEM_CONNECTION_LEAF:
            {
                auto const & conn = d->mConnections[index.row()];
                conn_uni = conn->path();
                conn_name = conn->name();
                if (NetworkManager::ConnectionSettings::Vpn == conn->settings()->connectionType())
                {
                    spec_object = dev_uni = QStringLiteral("/");
                    /*
                    // find first non-vpn active connection
                    const auto act_i = std::find_if(d->mActiveConns.cbegin(), d->mActiveConns.cend(), [] (NetworkManager::ActiveConnection::Ptr const & conn) -> bool
                    {
                    return nullptr != dynamic_cast<NetworkManager::VpnConnection const *>(conn.data());
                    });
                    if (act_i != d->mActiveConns.cend() && !(*act_i)->devices().empty())
                    {
                    dev_uni = (*act_i)->devices().front();
                    spec_object = (*act_i)->path();
                    }
                    */

                } else
                {
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
                }
                if (dev_uni.isEmpty())
                {
                    //TODO: in what form should we output the warning messages
                    qCWarning(NM_TRAY).noquote() << QStringLiteral("can't find device '%1' to activate connection '%2' on").arg(dev_name).arg(conn->name());
                    return;
                }
            }
            break;
        case ITEM_WIFINET_LEAF:
            {
                auto const & net = d->mWifiNets[index.row()];
                auto access_point = net->referenceAccessPoint();
                Q_ASSERT(!access_point.isNull());
                dev_uni = net->device();
                auto dev = d->findDeviceUni(dev_uni);
                Q_ASSERT(!dev.isNull());
                auto spec_dev = dev->as<NetworkManager::WirelessDevice>();
                Q_ASSERT(nullptr != spec_dev);
                conn_uni = access_point->uni();
                conn_name = access_point->ssid();
                //find the device name
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
                    qCWarning(NM_TRAY).noquote() << QStringLiteral("can't find connection for '%1' on device '%2', will create new...").arg(conn_name).arg(dev_name);
                    add_connection = true;
                    spec_object = conn_uni;
                    NetworkManager::WirelessSecurityType sec_type = NetworkManager::findBestWirelessSecurity(spec_dev->wirelessCapabilities()
                            , true, (spec_dev->mode() == NetworkManager::WirelessDevice::Adhoc)
                            , access_point->capabilities(), access_point->wpaFlags(), access_point->rsnFlags());
                    switch (sec_type)
                    {
                        case NetworkManager::UnknownSecurity:
                            qCWarning(NM_TRAY).noquote() << QStringLiteral("unknown security to use for '%1'").arg(conn_name);
                        case NetworkManager::NoneSecurity:
                            //nothing to do
                            break;
                        case NetworkManager::WpaPsk:
                        case NetworkManager::Wpa2Psk:
                            if (NetworkManager::ConnectionSettings::Ptr settings = assembleWpaXPskSettings(access_point))
                            {
                                map_settings = settings->toMap();
                            } else
                            {
                                qCWarning(NM_TRAY).noquote() << QStringLiteral("connection settings assembly for '%1' failed, abandoning activation...")
                                    .arg(conn_name);
                                return;
                            }
                            break;
                            //TODO: other types...
                    }
                } else
                {
                    conn_uni = conn->path();
                }
            }
            break;
        default:
            Q_ASSERT(false);
    }
qCDebug(NM_TRAY) << __FUNCTION__ << conn_uni << dev_uni << conn_name << dev_name << spec_object;
    //TODO: check vpn type etc..
    QDBusPendingCallWatcher * watcher;
    if (add_connection)
        watcher = new QDBusPendingCallWatcher{NetworkManager::addAndActivateConnection(map_settings, dev_uni, spec_object), this};
    else
        watcher = new QDBusPendingCallWatcher{NetworkManager::activateConnection(conn_uni, dev_uni, spec_object), this};
    connect(watcher, &QDBusPendingCallWatcher::finished, [conn_name, dev_name] (QDBusPendingCallWatcher * watcher) {
        if (watcher->isError() || !watcher->isValid())
        {
            //TODO: in what form should we output the warning messages
            qCWarning(NM_TRAY).noquote() << QStringLiteral("activation of connection '%1' on interface '%2' failed: %3").arg(conn_name)
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
        qCWarning(NM_TRAY).noquote() << "got invalid index for connection deactivation" << index;
        return;
    }

    auto const & active = d->mActiveConns[index.row()];
qCDebug(NM_TRAY) << __FUNCTION__ << active->path();
    QDBusPendingReply<> reply = NetworkManager::deactivateConnection(active->path());
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
    connect(watcher, &QDBusPendingCallWatcher::finished, [active] (QDBusPendingCallWatcher * watcher) {
        if (watcher->isError() || !watcher->isValid())
        {
            //TODO: in what form should we output the warning messages
            qCWarning(NM_TRAY).noquote() << QStringLiteral("deactivation of connection '%1' failed: %3").arg(active->connection()->name())
                    .arg(watcher->error().message());
         }
         watcher->deleteLater();
    });
}
