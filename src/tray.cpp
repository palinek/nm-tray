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
#include "tray.h"

#include <QMenu>
#include <QMessageBox>
#include <QApplication>
#include <QPersistentModelIndex>

#include <NetworkManagerQt/Manager>
#include <NetworkManagerQt/WirelessDevice>

#include "icons.h"
#include "nmmodel.h"
#include "nmproxy.h"
#include "log.h"
#include "dbus/org.freedesktop.Notifications.h"

#include "nmlist.h"
#include "connectioninfo.h"
#include "windowmenu.h"

// config keys
static const QString ENABLE_NOTIFICATIONS = QStringLiteral("enableNotifications");
static const QString CONNECTIONS_EDITOR = QStringLiteral("connectionsEditor");

class TrayPrivate
{
public:
    TrayPrivate();
    void updateState(QModelIndex const & index, bool removing);
    void primaryConnectionUpdate();
    void setShown(QPersistentModelIndex const & index);
    void updateIcon();
    void refreshIcon();
    void openCloseDialog(QDialog * dialog);
    void notify(QModelIndex const & index, bool removing);

public:
    QSystemTrayIcon mTrayIcon;
    QMenu mContextMenu;
    QTimer mStateTimer;
    QAction * mActEnableNetwork;
    QAction * mActEnableWifi;
    QAction * mActConnInfo;
    QAction * mActDebugInfo;
    NmModel mNmModel;
    NmProxy mActiveConnections;
    QPersistentModelIndex mPrimaryConnection;
    QPersistentModelIndex mShownConnection;
    QList<QPersistentModelIndex> mConnectionsToNotify; //!< just "created" connections to which notification wasn't sent yet
    icons::Icon mIconCurrent;
    icons::Icon mIcon2Show;
    QTimer mIconTimer;
    QScopedPointer<QDialog> mConnDialog;
    QScopedPointer<QDialog> mInfoDialog;

    org::freedesktop::Notifications mNotification;


    // configuration
    bool mEnableNotifications; //!< should info about connection establishment etc. be sent by org.freedesktop.Notifications
};

TrayPrivate::TrayPrivate()
    : mNotification{QStringLiteral("org.freedesktop.Notifications"), QStringLiteral("/org/freedesktop/Notifications"), QDBusConnection::sessionBus()}
{
    mActiveConnections.setNmModel(&mNmModel, NmModel::ActiveConnectionType);
}

void TrayPrivate::updateState(QModelIndex const & index, bool removing)
{
    notify(index, removing);

    const auto state = static_cast<NetworkManager::ActiveConnection::State>(mActiveConnections.data(index, NmModel::ActiveConnectionStateRole).toInt());
    const bool is_primary = mPrimaryConnection == index;
//qCDebug(NM_TRAY) << __FUNCTION__ << index << removing << mActiveConnections.data(index, NmModel::NameRole) << mActiveConnections.data(index, NmModel::ConnectionUuidRole).toString() << is_primary << mActiveConnections.data(index, NmModel::ConnectionTypeRole).toInt() << state;

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
    //TODO: optimize this text assembly (to not do it every time)?
    if (mPrimaryConnection.isValid())
    {
        mTrayIcon.setToolTip(Tray::tr("<pre>Connection <strong>%1</strong>(%2) active</pre>")
                .arg(mPrimaryConnection.data(NmModel::NameRole).toString())
                .arg(mPrimaryConnection.data(NmModel::ActiveConnectionTypeStringRole).toString())
                );
    } else
    {
        mTrayIcon.setToolTip(Tray::tr("<pre>No active connection</pre>"));
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

//qCDebug(NM_TRAY) << __FUNCTION__ << prim_conn->uuid();

    QModelIndexList l = mActiveConnections.match(mActiveConnections.index(0, 0, QModelIndex{}), NmModel::ActiveConnectionUuidRole, prim_conn->uuid(), -1, Qt::MatchExactly);
//qCDebug(NM_TRAY) << __FUNCTION__ << l.size();
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
    refreshIcon();
}

void TrayPrivate::refreshIcon()
{
    //Note: the icons::getIcon chooses the right icon from list of possible candidates
    // -> we need to refresh the icon in case of icon theme change
    mTrayIcon.setIcon(icons::getIcon(mIconCurrent));
}

void TrayPrivate::openCloseDialog(QDialog * dialog)
{
    if (dialog->isHidden() || dialog->isMinimized())
    {
        dialog->showNormal();
        dialog->activateWindow();
        dialog->raise();
    } else
        dialog->close();
}

void TrayPrivate::notify(QModelIndex const & index, bool removing)
{
    if (!mEnableNotifications)
    {
        return;
    }

    QString summary, body;
    if (removing)
    {
        mConnectionsToNotify.removeOne(index);
        summary = Tray::tr("Connection lost");
        body = Tray::tr("No longer connected to %1 '%2'.");
    } else
    {
        const int notif_i = mConnectionsToNotify.indexOf(index);
        // do nothing if not just added or the connection is not activated yet
        if (-1 == notif_i
                || NetworkManager::ActiveConnection::Activated != static_cast<NetworkManager::ActiveConnection::State>(mActiveConnections.data(index, NmModel::ActiveConnectionStateRole).toInt())
           )
        {
            return;
        }
        mConnectionsToNotify.removeAt(notif_i); // fire the notification only once
        summary = Tray::tr("Connection established");
        body = Tray::tr("Now connected to %1 '%2'.");
    }

    // TODO: do somehow check the result?
    mNotification.Notify(Tray::tr("NetworkManager(nm-tray)")
            , 0
            , icons::getIcon(static_cast<icons::Icon>(mActiveConnections.data(index, NmModel::IconTypeRole).toInt())).name()
            , summary
            , body.arg(mActiveConnections.data(index, NmModel::ConnectionTypeStringRole).toString()).arg(mActiveConnections.data(index, NmModel::NameRole).toString())
            , {}
            , {}
            , -1);
}


Tray::Tray(QObject *parent/* = nullptr*/)
    : QObject{parent}
    , d{new TrayPrivate}
{
    d->mEnableNotifications = QSettings{}.value(ENABLE_NOTIFICATIONS, true).toBool();

    connect(&d->mTrayIcon, &QSystemTrayIcon::activated, this, &Tray::onActivated);

    //postpone the update in case of signals flood
    connect(&d->mStateTimer, &QTimer::timeout, this, &Tray::setActionsStates);
    d->mStateTimer.setSingleShot(true);
    d->mStateTimer.setInterval(200);

    d->mIconCurrent = static_cast<icons::Icon>(-1);
    d->setShown(QModelIndex{});
    d->refreshIcon(); //force setting the icon instantly

    //postpone updating of the icon
    connect(&d->mIconTimer, &QTimer::timeout, [this] { d->updateIcon(); });
    d->mIconTimer.setSingleShot(true);
    d->mIconTimer.setInterval(0);

    d->mActEnableNetwork = d->mContextMenu.addAction(Tray::tr("Enable Networking"));
    d->mActEnableWifi = d->mContextMenu.addAction(Tray::tr("Enable Wi-Fi"));
    d->mContextMenu.addSeparator();
    QAction * enable_notifications = d->mContextMenu.addAction(Tray::tr("Enable notifications"));
    d->mContextMenu.addSeparator();
    d->mActConnInfo = d->mContextMenu.addAction(QIcon::fromTheme(QStringLiteral("dialog-information")), Tray::tr("Connection information"));
    d->mActDebugInfo = d->mContextMenu.addAction(QIcon::fromTheme(QStringLiteral("dialog-information")), Tray::tr("Debug information"));
    connect(d->mContextMenu.addAction(QIcon::fromTheme(QStringLiteral("document-edit")), Tray::tr("Edit connections...")), &QAction::triggered
            , this, &Tray::onEditConnectionsTriggered);
    d->mContextMenu.addSeparator();
    connect(d->mContextMenu.addAction(QIcon::fromTheme(QStringLiteral("help-about")), Tray::tr("About")), &QAction::triggered
            , this, &Tray::onAboutTriggered);
    connect(d->mContextMenu.addAction(QIcon::fromTheme(QStringLiteral("application-exit")), Tray::tr("Quit")), &QAction::triggered
            , this, &Tray::onQuitTriggered);
    //for listening on the QEvent::ThemeChange (is delivered only to QWidget objects)
    d->mContextMenu.installEventFilter(this);

    d->mActEnableNetwork->setCheckable(true);
    d->mActEnableWifi->setCheckable(true);
    enable_notifications->setCheckable(true);
    enable_notifications->setChecked(d->mEnableNotifications);
    connect(d->mActEnableNetwork, &QAction::triggered, [] (bool checked) { NetworkManager::setNetworkingEnabled(checked); });
    connect(d->mActEnableWifi, &QAction::triggered, [] (bool checked) { NetworkManager::setWirelessEnabled(checked); });
    connect(enable_notifications, &QAction::triggered, this, [this] (bool checked) { d->mEnableNotifications = checked; QSettings{}.setValue(ENABLE_NOTIFICATIONS, checked); });
    connect(d->mActConnInfo, &QAction::triggered, this, [this] (bool ) {
        if (d->mInfoDialog.isNull())
        {
            d->mInfoDialog.reset(new ConnectionInfo{&d->mNmModel});
            connect(d->mInfoDialog.data(), &QDialog::finished, [this] {
                d->mInfoDialog.reset(nullptr);
            });
        }
        d->openCloseDialog(d->mInfoDialog.data());
    });
    connect(d->mActDebugInfo, &QAction::triggered, this, [this] (bool ) {
        if (d->mConnDialog.isNull())
        {
            d->mConnDialog.reset(new NmList{Tray::tr("nm-tray info"), &d->mNmModel});
            connect(d->mConnDialog.data(), &QDialog::finished, [this] {
                d->mConnDialog.reset(nullptr);
            });
        }
        d->openCloseDialog(d->mConnDialog.data());
    });

    // Note: Force all the updates as the NetworkManager::Notifier signals aren't
    // emitted at application startup.
    d->primaryConnectionUpdate();
    setActionsStates();

    connect(NetworkManager::notifier(), &NetworkManager::Notifier::networkingEnabledChanged, &d->mStateTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(NetworkManager::notifier(), &NetworkManager::Notifier::wirelessEnabledChanged, &d->mStateTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(NetworkManager::notifier(), &NetworkManager::Notifier::wirelessHardwareEnabledChanged, &d->mStateTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(NetworkManager::notifier(), &NetworkManager::Notifier::primaryConnectionChanged, this, &Tray::onPrimaryConnectionChanged);

    connect(&d->mActiveConnections, &QAbstractItemModel::rowsInserted, [this] (QModelIndex const & parent, int first, int last) {
        for (int i = first; i <= last; ++i)
        {
            const QModelIndex index = d->mActiveConnections.index(i, 0, parent);
//qCDebug(NM_TRAY) << "rowsInserted" << index;
            if (d->mEnableNotifications)
            {
                d->mConnectionsToNotify.append(index);
            }
            d->updateState(index, false);
        }
    });
    connect(&d->mActiveConnections, &QAbstractItemModel::rowsAboutToBeRemoved, [this] (QModelIndex const & parent, int first, int last) {
//qCDebug(NM_TRAY) << "rowsAboutToBeRemoved";
        for (int i = first; i <= last; ++i)
            d->updateState(d->mActiveConnections.index(i, 0, parent), true);
    });
    connect(&d->mActiveConnections, &QAbstractItemModel::dataChanged, [this] (const QModelIndex & topLeft, const QModelIndex & bottomRight, const QVector<int> & /*roles*/) {
//qCDebug(NM_TRAY) << "dataChanged";
        for (auto const & i : QItemSelection{topLeft, bottomRight}.indexes())
            d->updateState(i, false);
    });

    d->mTrayIcon.setContextMenu(&d->mContextMenu);
    QTimer::singleShot(0, [this] { d->mTrayIcon.show(); });
}

Tray::~Tray()
{
}

bool Tray::eventFilter(QObject * object, QEvent * event)
{
    Q_ASSERT(&d->mContextMenu == object);
    if (QEvent::ThemeChange == event->type())
        d->refreshIcon();
    return false;
}

void Tray::onEditConnectionsTriggered()
{
    const QStringList connections_editor = QSettings{}.value(CONNECTIONS_EDITOR, QStringList{{"xterm", "-e", "nmtui-edit"}}).toStringList();
    if (connections_editor.empty() || connections_editor.front().isEmpty())
    {
        qCCritical(NM_TRAY) << "Can't start connection editor, because of misconfiguration. Value of"
            << CONNECTIONS_EDITOR << "invalid key," << connections_editor;
        return;
    }

    // Note: let this object dangle, if the process isn't finished until our application is closed
    QProcess * editor = new QProcess;
    editor->setProcessChannelMode(QProcess::ForwardedChannels);
    connect(editor, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished)
            , [connections_editor, editor] (int exitCode, QProcess::ExitStatus exitStatus) {
            qCInfo(NM_TRAY) << "connection editor " << connections_editor << " finished, exitCode=" << exitCode << ", exitStatus=" << exitStatus;
            editor->deleteLater();
    });
    connect(editor, &QProcess::errorOccurred
            , [connections_editor, editor] (QProcess::ProcessError error) {
            qCInfo(NM_TRAY) << "connection editor " << connections_editor << " failed, error=" << error;
            editor->deleteLater();
    });

    qCInfo(NM_TRAY) << "starting connection editor " << connections_editor;

    QString program = connections_editor.front();
    QStringList args;
    std::copy(connections_editor.cbegin() + 1, connections_editor.cend(), std::back_inserter(args));
    editor->start(program, args);
    editor->closeWriteChannel();
}

void Tray::onAboutTriggered()
{
    QMessageBox::about(nullptr, Tray::tr("%1 about").arg(QStringLiteral("nm-tray"))
                , Tray::tr("<strong><a href=\"https://github.com/palinek/nm-tray\">nm-tray</a></strong> is a simple Qt based"
                    " frontend for <a href=\"https://wiki.gnome.org/Projects/NetworkManager\">NetworkManager</a>.<br/><br/>"
                    "Version: %1").arg(NM_TRAY_VERSION));
}


void Tray::onQuitTriggered()
{
    QApplication::instance()->quit();
}


void Tray::onActivated(const QSystemTrayIcon::ActivationReason reason)
{
    switch (reason)
    {
        case QSystemTrayIcon::Trigger:
        case QSystemTrayIcon::DoubleClick:
            {
                QMenu * menu = new WindowMenu(&d->mNmModel);
                menu->setAttribute(Qt::WA_DeleteOnClose);
                menu->popup(QCursor::pos());
            }
            break;
        default:
            break;
    }
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
