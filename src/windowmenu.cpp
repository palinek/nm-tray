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

#include <QWidgetAction>
#include <functional>
#include <QTimer>

class WindowMenuPrivate
{
public:
    NmModel * mNmModel;

    QScopedPointer<NmProxy> mWirelessModel;
    QWidgetAction * mWirelessAction;

    QScopedPointer<NmProxy> mActiveModel;
    QWidgetAction * mActiveAction;

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

    //active proxy & widgets
    d->mActiveModel.reset(new NmProxy);
    d->mActiveModel->setNmModel(d->mNmModel, NmModel::ActiveConnectionType);
    MenuView * active_view = new MenuView{d->mActiveModel.data()};
    connect(active_view, &MenuView::activatedNoMiddleRight, [this, d] (const QModelIndex & index) {
        d->onActivated(index, d->mActiveModel.data(), std::bind(&NmProxy::deactivateConnection, d->mActiveModel.data(), std::placeholders::_1));
        close();
    });

    d->mActiveAction = new QWidgetAction{this};
    d->mActiveAction->setDefaultWidget(active_view);
    connect(d->mActiveModel.data(), &QAbstractItemModel::modelReset, [d] { d->onViewRowChange(d->mActiveAction, d->mActiveModel.data()); });
    connect(d->mActiveModel.data(), &QAbstractItemModel::rowsInserted, [d] { d->onViewRowChange(d->mActiveAction, d->mActiveModel.data()); });
    connect(d->mActiveModel.data(), &QAbstractItemModel::rowsRemoved, [d] { d->onViewRowChange(d->mActiveAction, d->mActiveModel.data()); });

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

    addAction(tr("Active connection(s)"))->setEnabled(false);
    addAction(d->mActiveAction);
    addAction(tr("Wi-Fi network(s)"))->setEnabled(false);
    addAction(d->mWirelessAction);
    addAction(tr("Known connection(s)"))->setEnabled(false);
    addAction(d->mConnectionAction);

    d->mMakeDirtyAction = new QAction{this};
    d->mDelaySizeRefreshTimer.setInterval(200);
    d->mDelaySizeRefreshTimer.setSingleShot(true);
    connect(&d->mDelaySizeRefreshTimer, &QTimer::timeout, [d] { d->forceSizeRefresh(); });

    d->onViewRowChange(d->mActiveAction, d->mActiveModel.data());
    d->onViewRowChange(d->mWirelessAction, d->mWirelessModel.data());
    d->onViewRowChange(d->mConnectionAction, d->mConnectionModel.data());
}

WindowMenu::~WindowMenu()
{
}
