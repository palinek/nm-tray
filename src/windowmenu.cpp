/*COPYRIGHT_HEADER

This file is a part of nm-tray.

Copyright (c)
    2016~now Palo Kisa <palo.kisa@gmail.com>

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

#include "windowmenu.h"
#include "nmmodel.h"
#include "nmproxy.h"
#include "menuview.h"
#include "connectioninfo.h"

#include <QWidgetAction>
#include <QPushButton>
#include <functional>
#include <QTimer>

class WindowMenuPrivate
{
public:
    NmModel * mNmModel;

    QScopedPointer<NmProxy> mWirelessModel;
    QWidgetAction * mWirelessAction;

    QScopedPointer<NmProxy> mConnectionModel;
    QWidgetAction * mConnectionAction;

    QAction * mMakeDirtyAction;
    QTimer mDelaySizeRefreshTimer;

    WindowMenuPrivate(WindowMenu * q);
    template <typename F>
    void onActivated(QModelIndex const & index
            , QAbstractItemModel const * topParent
            , F const & functor);
    void forceSizeRefresh();
    void onViewRowChange(QAction * viewAction, QAbstractItemModel const * model);
private:
    WindowMenu * q_ptr;
    Q_DECLARE_PUBLIC(WindowMenu);
};

WindowMenuPrivate::WindowMenuPrivate(WindowMenu * q)
    : q_ptr{q}
{
}

template <typename F>
void WindowMenuPrivate::onActivated(QModelIndex const & index
        , QAbstractItemModel const * topParent
        , F const & functor)
{
    QModelIndex i = index;
    for (QAbstractProxyModel const * proxy = qobject_cast<QAbstractProxyModel const *>(index.model())
            ; nullptr != proxy && topParent != proxy
            ; proxy = qobject_cast<QAbstractProxyModel const *>(proxy->sourceModel())
            )
    {
        i = proxy->mapToSource(i);
    }
    functor(i);
}

void WindowMenuPrivate::forceSizeRefresh()
{
    Q_Q(WindowMenu);
    if (!q->isVisible())
    {
        return;
    }

    const QSize old_size = q->size();
    //TODO: how to force the menu to recalculate it's size in a more elegant way?
    q->addAction(mMakeDirtyAction);
    q->removeAction(mMakeDirtyAction);
    // ensure to be visible (should the resize make it out of screen)
    if (old_size != q->size())
    {
        q->popup(q->geometry().topLeft());
    }
}

void WindowMenuPrivate::onViewRowChange(QAction * viewAction, QAbstractItemModel const * model)
{
    viewAction->setVisible(model->rowCount(QModelIndex{}) > 0);
    mDelaySizeRefreshTimer.start();
}




WindowMenu::WindowMenu(NmModel * nmModel, QWidget * parent /*= nullptr*/)
    : QMenu{parent}
    , d_ptr{new WindowMenuPrivate{this}}
{
    Q_D(WindowMenu);
    d->mNmModel = nmModel;
    d->mNmModel->requestAllWifiScan();

    const NmModel::ManagerState state = d->mNmModel->managerState();
    QString statusText = tr("No active connection");
    bool hasPrimaryConnection = false;
    if (!state.primaryName.isEmpty()) {
        hasPrimaryConnection = true;
        statusText = tr("Connected: %1").arg(state.primaryName);
        if (state.wifiStrength >= 0) {
            statusText += tr(" (%1%)").arg(state.wifiStrength);
        }
    }
    auto *statusButton = new QPushButton(statusText);
    statusButton->setFlat(true);
    statusButton->setCursor(Qt::PointingHandCursor);
    statusButton->setEnabled(hasPrimaryConnection);
    statusButton->setToolTip(hasPrimaryConnection
            ? tr("Disconnect current connection")
            : tr("No active connection to disconnect"));
    auto *statusAction = new QWidgetAction(this);
    statusAction->setDefaultWidget(statusButton);
    addAction(statusAction);
    connect(statusButton, &QPushButton::clicked, this, [this, d] {
        d->mNmModel->disconnectPrimaryConnection();
    });
    QAction *usageAction = addAction(tr("Connection details / usage..."));
    connect(usageAction, &QAction::triggered, [this, d] {
        auto *dialog = new ConnectionInfo{d->mNmModel, nullptr};
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
    });
    addSeparator();

    //wireless proxy & widgets
    d->mWirelessModel.reset(new NmProxy);
    d->mWirelessModel->setNmModel(d->mNmModel, NmModel::WifiNetworkType);
    MenuView * wifi_view = new MenuView{d->mWirelessModel.data()};
    connect(wifi_view, &MenuView::activatedNoMiddleRight, [this, d] (const QModelIndex & index) {
        d->onActivated(index, d->mWirelessModel.data(), std::bind(&NmProxy::activateConnection, d->mWirelessModel.data(), std::placeholders::_1));
        close();
    });

    d->mWirelessAction = new QWidgetAction{this};
    d->mWirelessAction->setDefaultWidget(wifi_view);
    connect(d->mWirelessModel.data(), &QAbstractItemModel::modelReset, [d] { d->onViewRowChange(d->mWirelessAction, d->mWirelessModel.data()); });
    connect(d->mWirelessModel.data(), &QAbstractItemModel::rowsInserted, [d] { d->onViewRowChange(d->mWirelessAction, d->mWirelessModel.data()); });
    connect(d->mWirelessModel.data(), &QAbstractItemModel::rowsRemoved, [d] { d->onViewRowChange(d->mWirelessAction, d->mWirelessModel.data()); });

    //connection proxy & widgets
    d->mConnectionModel.reset(new NmProxy);
    d->mConnectionModel->setNmModel(d->mNmModel, NmModel::ConnectionType);
    MenuView * connection_view = new MenuView{d->mConnectionModel.data()};
    connect(connection_view, &MenuView::activatedNoMiddleRight, [this, d] (const QModelIndex & index) {
        d->onActivated(index, d->mConnectionModel.data(), std::bind(&NmProxy::activateConnection, d->mConnectionModel.data(), std::placeholders::_1));
        close();
    });

    d->mConnectionAction = new QWidgetAction{this};
    d->mConnectionAction->setDefaultWidget(connection_view);
    connect(d->mConnectionModel.data(), &QAbstractItemModel::modelReset, [d] { d->onViewRowChange(d->mConnectionAction, d->mConnectionModel.data()); });
    connect(d->mConnectionModel.data(), &QAbstractItemModel::rowsInserted, [d] { d->onViewRowChange(d->mConnectionAction, d->mConnectionModel.data()); });
    connect(d->mConnectionModel.data(), &QAbstractItemModel::rowsRemoved, [d] { d->onViewRowChange(d->mConnectionAction, d->mConnectionModel.data()); });

    addAction(tr("Recent connection(s)"))->setEnabled(false);
    const auto recents = d->mNmModel->recentConnections(3);
    if (recents.isEmpty()) {
        QAction *none = addAction(tr("No recent connections"));
        none->setEnabled(false);
    } else {
        for (const auto &recent : recents) {
            QAction *a = addAction(recent.id);
            a->setToolTip(tr("Reconnect"));
            connect(a, &QAction::triggered, this, [this, d, path = recent.connectionPath] {
                d->mNmModel->activateConnectionPath(path);
            });
        }
    }
    addAction(tr("Wi-Fi network(s)"))->setEnabled(false);
    addAction(d->mWirelessAction);
    addAction(tr("Known connection(s)"))->setEnabled(false);
    addAction(d->mConnectionAction);
    addSeparator();
    auto *toggleButton = new QPushButton(d->mNmModel->showLowSignalNetworks()
            ? tr("Hide low signal networks")
            : tr("Show low signal networks"));
    toggleButton->setFlat(true);
    toggleButton->setCursor(Qt::PointingHandCursor);
    toggleButton->setToolTip(tr("Toggle visibility of weak Wi-Fi networks"));
    auto *toggleAction = new QWidgetAction(this);
    toggleAction->setDefaultWidget(toggleButton);
    addAction(toggleAction);
    connect(toggleButton, &QPushButton::clicked, this, [d] {
        d->mNmModel->setShowLowSignalNetworks(!d->mNmModel->showLowSignalNetworks());
    });
    connect(d->mNmModel, &QAbstractItemModel::modelReset, toggleButton, [d, toggleButton] {
        toggleButton->setText(d->mNmModel->showLowSignalNetworks()
                ? WindowMenu::tr("Hide low signal networks")
                : WindowMenu::tr("Show low signal networks"));
    });

    d->mMakeDirtyAction = new QAction{this};
    d->mDelaySizeRefreshTimer.setInterval(200);
    d->mDelaySizeRefreshTimer.setSingleShot(true);
    connect(&d->mDelaySizeRefreshTimer, &QTimer::timeout, [d] { d->forceSizeRefresh(); });

    d->onViewRowChange(d->mWirelessAction, d->mWirelessModel.data());
    d->onViewRowChange(d->mConnectionAction, d->mConnectionModel.data());
}

WindowMenu::~WindowMenu()
{
}
