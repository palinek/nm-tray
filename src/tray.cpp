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

#include "nmlist.h"


class TrayPrivate
{
public:
    void updateState(QModelIndex const & index, bool removing);
    void primaryConnectionUpdate();
    void setIcon(icons::Icon icon);
    void updateIcon();

public:
    QSystemTrayIcon mTrayIcon;
    QMenu mContextMenu;
    QTimer mStateTimer;
    QAction * mActEnableNetwork;
    QAction * mActEnableWifi;
    QAction * mActConnInfo;
    NmModel mNmModel;
    QString mPrimaryConnectionUuid;
    icons::Icon mIconCurrent;
    icons::Icon mIcon2Show;
    QTimer mIconTimer;

};

void TrayPrivate::updateState(QModelIndex const & index, bool removing)
{
    const auto item_type = static_cast<NmModel::ItemType>(mNmModel.data(index, NmModel::ItemTypeRole).toInt());
    if (NmModel::ActiveConnectionType != item_type)
        return;

    const auto state = static_cast<NetworkManager::ActiveConnection::State>(mNmModel.data(index, NmModel::ActiveConnectionStateRole).toInt());
    const QString uuid = mNmModel.data(index, NmModel::ConnectionUuidRole).toString();
    const bool is_primary = mPrimaryConnectionUuid == uuid;
//qDebug() << __FUNCTION__ << index << removing << mNmModel.data(index, NmModel::NameRole) << uuid << is_primary << item_type << mNmModel.data(index, NmModel::ConnectionTypeRole).toInt() << state;

    if (removing || NetworkManager::ActiveConnection::Deactivated == state)
    {
        if (is_primary)
        {
            mPrimaryConnectionUuid.clear();
            setIcon(icons::NETWORK_OFFLINE);
        }
    } else
    {
        if (is_primary || NetworkManager::ActiveConnection::Activating == state)
            setIcon(static_cast<icons::Icon>(mNmModel.data(index, NmModel::IconTypeRole).toInt()));
        if (!is_primary && NetworkManager::ActiveConnection::Activating != state)
            primaryConnectionUpdate();
    }
}

void TrayPrivate::primaryConnectionUpdate()
{
    NetworkManager::ActiveConnection::Ptr prim_conn = NetworkManager::primaryConnection();
    if (!prim_conn || !prim_conn->isValid())
    {
        mPrimaryConnectionUuid.clear();
        setIcon(icons::NETWORK_OFFLINE);
        return;
    }

//qDebug() << __FUNCTION__ << prim_conn->uuid();

    mPrimaryConnectionUuid = prim_conn->uuid();
    QModelIndexList l = mNmModel.match(mNmModel.indexTypeFirst(NmModel::ActiveConnectionType), NmModel::ActiveConnectionUuidRole, mPrimaryConnectionUuid, -1, Qt::MatchExactly);
//qDebug() << __FUNCTION__ << l.size();
    //nothing to do if the connection not populated in model yet
    if (0 >= l.size())
        return;
    Q_ASSERT(1 == l.size());
    updateState(l.first(), false);
}

void TrayPrivate::setIcon(icons::Icon icon)
{
    mIcon2Show = icon;
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
    //connect(this, &QSystemTrayIcon::activated, this, &Tray::onActivated);

    //postpone the update in case of signals flood
    connect(&d->mStateTimer, &QTimer::timeout, this, &Tray::setActionsStates);
    d->mStateTimer.setSingleShot(true);
    d->mStateTimer.setInterval(200);

    d->mIconCurrent = static_cast<icons::Icon>(-1);
    d->setIcon(icons::NETWORK_OFFLINE);

    //postpone updating of the icon
    connect(&d->mIconTimer, &QTimer::timeout, [this] { d->updateIcon(); });
    d->mIconTimer.setSingleShot(true);
    d->mIconTimer.setInterval(0);

    d->mActEnableNetwork = d->mContextMenu.addAction(tr("Enable Networking"));
    d->mActEnableWifi = d->mContextMenu.addAction(tr("Enable Wi-fi"));
    d->mContextMenu.addSeparator();
    d->mActConnInfo = d->mContextMenu.addAction(QIcon::fromTheme(QStringLiteral("dialog-information")), tr("Connection information"));
    d->mContextMenu.addSeparator();
    connect(d->mContextMenu.addAction(QIcon::fromTheme(QStringLiteral("help-about")), tr("About")), &QAction::triggered
            , this, &Tray::onAboutTriggered);
    connect(d->mContextMenu.addAction(QIcon::fromTheme(QStringLiteral("application-exit")), tr("Quit")), &QAction::triggered
            , this, &Tray::onQuitTriggered);

    d->mActEnableNetwork->setCheckable(true);
    d->mActEnableWifi->setCheckable(true);
    connect(d->mActEnableNetwork, &QAction::triggered, [this] (bool checked) { NetworkManager::setNetworkingEnabled(checked); });
    connect(d->mActEnableWifi, &QAction::triggered, [this] (bool checked) { NetworkManager::setWirelessEnabled(checked); });
    connect(d->mActConnInfo, &QAction::triggered, [this] (bool ) { /*TODO: information dialog*/
        NmList * dialog = new NmList{&d->mNmModel};
        connect(dialog, &QDialog::finished, dialog, &QObject::deleteLater);
        dialog->show();
    });

    connect(NetworkManager::notifier(), &NetworkManager::Notifier::networkingEnabledChanged, &d->mStateTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(NetworkManager::notifier(), &NetworkManager::Notifier::wirelessEnabledChanged, &d->mStateTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(NetworkManager::notifier(), &NetworkManager::Notifier::wirelessHardwareEnabledChanged, &d->mStateTimer, static_cast<void (QTimer::*)()>(&QTimer::start));
    connect(NetworkManager::notifier(), &NetworkManager::Notifier::primaryConnectionChanged, this, &Tray::onPrimaryConnectionChanged);
    connect(&d->mNmModel, &QAbstractItemModel::rowsInserted, [this] (QModelIndex const & parent, int first, int last) {
//qDebug() << "rowsInserted" << parent;
        for (int i = first; i <= last; ++i)
            d->updateState(d->mNmModel.index(i, 0, parent), false);
    });
    connect(&d->mNmModel, &QAbstractItemModel::rowsAboutToBeRemoved, [this] (QModelIndex const & parent, int first, int last) {
//qDebug() << "rowsAboutToBeRemoved";
        for (int i = first; i <= last; ++i)
            d->updateState(d->mNmModel.index(i, 0, parent), true);
    });
    connect(&d->mNmModel, &QAbstractItemModel::dataChanged, [this] (const QModelIndex & topLeft, const QModelIndex & bottomRight, const QVector<int> & /*roles*/) {
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
    QMessageBox * about = new QMessageBox{QMessageBox::Information, tr("%1 about").arg(QStringLiteral("nm-tray"))
        , tr("This is the about nm-tray!")};
    about->setModal(false);
    about->setAttribute(Qt::WA_DeleteOnClose);
    about->show();
}


void Tray::onQuitTriggered()
{
    QApplication::instance()->quit();
}

/*
void Tray::onActivated(QSystemTrayIcon::ActivationReason reason)
{
    qDebug() << __FUNCTION__ << reason;
}
*/

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
