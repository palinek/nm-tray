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
#include "tray.h"

#include <QSystemTrayIcon>
#include <QMenu>
#include <QMessageBox>
#include <QApplication>
#include <QDebug>

#include <NetworkManagerQt/Manager>
#include <NetworkManagerQt/WirelessDevice>

#include "icons.h"
#include "nmmodel.h"
#include "nmproxy.h"

#include "nmlist.h"
#include "connectioninfo.h"
#include <QPersistentModelIndex>


class TrayPrivate
{
public:
    TrayPrivate();
    void updateState(QModelIndex const & index, bool removing);
    void primaryConnectionUpdate();
    void setShown(QPersistentModelIndex const & index);
    void updateIcon();

public:
    QSystemTrayIcon mTrayIcon;
    QMenu mContextMenu;
    QTimer mStateTimer;
    QAction * mActEnableNetwork;
    QAction * mActEnableWifi;
    QAction * mActConnInfo;
    NmModel mNmModel;
    NmProxy mActiveConnections;
    QPersistentModelIndex mPrimaryConnection;
    QPersistentModelIndex mShownConnection;
    icons::Icon mIconCurrent;
    icons::Icon mIcon2Show;
    QTimer mIconTimer;
    QScopedPointer<QDialog> mConnDialog;
    QScopedPointer<QDialog> mInfoDialog;

};

TrayPrivate::TrayPrivate()
{
    mActiveConnections.setNmModel(&mNmModel, NmModel::ActiveConnectionType);
}

void TrayPrivate::updateState(QModelIndex const & index, bool removing)
{
    const auto state = static_cast<NetworkManager::ActiveConnection::State>(mActiveConnections.data(index, NmModel::ActiveConnectionStateRole).toInt());
    const bool is_primary = mPrimaryConnection == index;
//qDebug() << __FUNCTION__ << index << removing << mActiveConnections.data(index, NmModel::NameRole) << mActiveConnections.data(index, NmModel::ConnectionUuidRole).toString() << is_primary << mActiveConnections.data(index, NmModel::ConnectionTypeRole).toInt() << state;

    if (removing || NetworkManager::ActiveConnection::Deactivated == state || NetworkManager::ActiveConnection::Deactivating == state)
    {
        if (is_primary)
        {
            mPrimaryConnection = QModelIndex{};
            setShown(mPrimaryConnection);
        } else if (mShownConnection == index)
        {
            setShown(mPrimaryConnection);
        }
    } else
    {
        if (is_primary || NetworkManager::ActiveConnection::Activating == state)
        {
            setShown(index);
        } else if (mShownConnection == index)
        {
            setShown(mPrimaryConnection);
        }
    }
}

void TrayPrivate::primaryConnectionUpdate()
{
    NetworkManager::ActiveConnection::Ptr prim_conn = NetworkManager::primaryConnection();
    if (!prim_conn || !prim_conn->isValid())
    {
        mPrimaryConnection = QModelIndex{};
        setShown(mPrimaryConnection);
        return;
    }

//qDebug() << __FUNCTION__ << prim_conn->uuid();

    QModelIndexList l = mActiveConnections.match(mActiveConnections.index(0, 0, QModelIndex{}), NmModel::ActiveConnectionUuidRole, prim_conn->uuid(), -1, Qt::MatchExactly);
//qDebug() << __FUNCTION__ << l.size();
    //nothing to do if the connection not populated in model yet
    if (0 >= l.size())
        return;
    Q_ASSERT(1 == l.size());
    mPrimaryConnection = l.first();
    updateState(mPrimaryConnection, false);
}

void TrayPrivate::setShown(QPersistentModelIndex const & index)
{
    mShownConnection = index;
    mIcon2Show = mShownConnection.isValid()
        ? static_cast<icons::Icon>(mActiveConnections.data(mShownConnection, NmModel::IconTypeRole).toInt()) : icons::NETWORK_OFFLINE;
    //postpone setting the icon (for case we change the icon in till our event is finished)
    mIconTimer.start();
}

void TrayPrivate::updateIcon()
{
    if (mIconCurrent == mIcon2Show)
        return;

    mIconCurrent = mIcon2Show;
    QStringList icon_names;
    mTrayIcon.setIcon(icons::getIcon(mIconCurrent));
}


Tray::Tray(QObject *parent/* = 0*/)
    : QObject{parent}
    , d{new TrayPrivate}
{
    connect(&d->mTrayIcon, &QSystemTrayIcon::activated, this, &Tray::onActivated);

    //postpone the update in case of signals flood
    connect(&d->mStateTimer, &QTimer::timeout, this, &Tray::setActionsStates);
    d->mStateTimer.setSingleShot(true);
    d->mStateTimer.setInterval(200);

    d->mIconCurrent = static_cast<icons::Icon>(-1);
    d->setShown(QModelIndex{});

    //postpone updating of the icon
    connect(&d->mIconTimer, &QTimer::timeout, [this] { d->updateIcon(); });
    d->mIconTimer.setSingleShot(true);
    d->mIconTimer.setInterval(0);

    d->mActEnableNetwork = d->mContextMenu.addAction(Tray::tr("Enable Networking"));
    d->mActEnableWifi = d->mContextMenu.addAction(Tray::tr("Enable Wi-fi"));
    d->mContextMenu.addSeparator();
    d->mActConnInfo = d->mContextMenu.addAction(QIcon::fromTheme(QStringLiteral("dialog-information")), Tray::tr("Connection information"));
    d->mContextMenu.addSeparator();
    connect(d->mContextMenu.addAction(QIcon::fromTheme(QStringLiteral("help-about")), Tray::tr("About")), &QAction::triggered
            , this, &Tray::onAboutTriggered);
    connect(d->mContextMenu.addAction(QIcon::fromTheme(QStringLiteral("application-exit")), Tray::tr("Quit")), &QAction::triggered
            , this, &Tray::onQuitTriggered);

    d->mActEnableNetwork->setCheckable(true);
    d->mActEnableWifi->setCheckable(true);
    connect(d->mActEnableNetwork, &QAction::triggered, [this] (bool checked) { NetworkManager::setNetworkingEnabled(checked); });
    connect(d->mActEnableWifi, &QAction::triggered, [this] (bool checked) { NetworkManager::setWirelessEnabled(checked); });
    connect(d->mActConnInfo, &QAction::triggered, [this] (bool ) {
        if (d->mInfoDialog.isNull())
        {
            d->mInfoDialog.reset(new ConnectionInfo{&d->mNmModel});
            connect(d->mInfoDialog.data(), &QDialog::finished, [this] {
                d->mInfoDialog.reset(nullptr);
            });
        }
        if (d->mInfoDialog->isHidden() || d->mInfoDialog->isMinimized())
        {
            d->mInfoDialog->showNormal();
            d->mInfoDialog->activateWindow();
            d->mInfoDialog->raise();
        } else
            d->mInfoDialog->close();
    });

    connect(NetworkManager::notifier(), &NetworkManager::Notifier::networkingEnabledChanged, &d->mStateTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(NetworkManager::notifier(), &NetworkManager::Notifier::wirelessEnabledChanged, &d->mStateTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(NetworkManager::notifier(), &NetworkManager::Notifier::wirelessHardwareEnabledChanged, &d->mStateTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(NetworkManager::notifier(), &NetworkManager::Notifier::primaryConnectionChanged, this, &Tray::onPrimaryConnectionChanged);
    connect(&d->mActiveConnections, &QAbstractItemModel::rowsInserted, [this] (QModelIndex const & parent, int first, int last) {
//qDebug() << "rowsInserted" << parent;
        for (int i = first; i <= last; ++i)
            d->updateState(d->mActiveConnections.index(i, 0, parent), false);
    });
    connect(&d->mActiveConnections, &QAbstractItemModel::rowsAboutToBeRemoved, [this] (QModelIndex const & parent, int first, int last) {
//qDebug() << "rowsAboutToBeRemoved";
        for (int i = first; i <= last; ++i)
            d->updateState(d->mActiveConnections.index(i, 0, parent), true);
    });
    connect(&d->mActiveConnections, &QAbstractItemModel::dataChanged, [this] (const QModelIndex & topLeft, const QModelIndex & bottomRight, const QVector<int> & /*roles*/) {
//qDebug() << "dataChanged";
        for (auto const & i : QItemSelection{topLeft, bottomRight}.indexes())
            d->updateState(i, false);
    });

    d->mTrayIcon.setContextMenu(&d->mContextMenu);
    QTimer::singleShot(0, [this] { d->mTrayIcon.show(); });
}

Tray::~Tray()
{
}

void Tray::onAboutTriggered()
{
    QMessageBox::about(nullptr, Tray::tr("%1 about").arg(QStringLiteral("nm-tray"))
                , Tray::tr("This is the about nm-tray!"));
}


void Tray::onQuitTriggered()
{
    QApplication::instance()->quit();
}


void Tray::onActivated()
{
    if (d->mConnDialog.isNull())
    {
        d->mConnDialog.reset(new NmList{Tray::tr("nm-tray info"), &d->mNmModel});
        connect(d->mConnDialog.data(), &QDialog::finished, [this] {
            d->mConnDialog.reset(nullptr);
        });
    }
    if (d->mConnDialog->isHidden() || d->mConnDialog->isMinimized())
    {
        d->mConnDialog->showNormal();
        d->mConnDialog->activateWindow();
        d->mConnDialog->raise();
    } else
        d->mConnDialog->close();
}


void Tray::setActionsStates()
{
    const bool net_enabled = NetworkManager::isNetworkingEnabled();
    d->mActEnableNetwork->setChecked(net_enabled);

    d->mActEnableWifi->setChecked(NetworkManager::isWirelessEnabled());
    d->mActEnableWifi->setEnabled(NetworkManager::isNetworkingEnabled() && NetworkManager::isWirelessHardwareEnabled());

    d->mActConnInfo->setEnabled(net_enabled);
}

void Tray::onPrimaryConnectionChanged(QString const & /*uni*/)
{
    d->primaryConnectionUpdate();
}
