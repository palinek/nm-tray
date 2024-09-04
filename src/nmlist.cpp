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
#include "nmlist.h"
#include "ui_nmlist.h"
#include "nmmodel.h"
#include "nmproxy.h"
#include <QSortFilterProxyModel>

template <typename T>
void installDblClick(QAbstractItemView * view, QAbstractItemModel * m)
{
    T * net_model = qobject_cast<T *>(m);
    if (net_model)
    {
        QAbstractProxyModel * proxy = qobject_cast<QAbstractProxyModel *>(view->model());
        Q_ASSERT(nullptr != proxy && proxy->sourceModel() == net_model);
        QObject::connect(view, &QAbstractItemView::doubleClicked, [net_model, proxy] (QModelIndex const & i) {
            QModelIndex source_i = proxy->mapToSource(i);
            switch (static_cast<NmModel::ItemType>(net_model->data(source_i, NmModel::ItemTypeRole).toInt()))
            {
                case NmModel::ActiveConnectionType:
                    net_model->deactivateConnection(source_i);
                    break;
                case NmModel::WifiNetworkType:
                case NmModel::ConnectionType:
                    net_model->activateConnection(source_i);
                    break;
                default:
                    //do nothing
                    break;
            }
        });
    }
}

NmList::NmList(QString const & title, QAbstractItemModel * m, QWidget *parent)
    : QDialog(parent)
    , ui{new Ui::NmList}
{
    ui->setupUi(this);
    setWindowTitle(title);

    Q_ASSERT(qobject_cast<NmModel*>(m));
    QSortFilterProxyModel * proxy_sort = new QSortFilterProxyModel{this};
    proxy_sort->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxy_sort->sort(0);
    proxy_sort->setSourceModel(m);
    ui->treeView->setModel(proxy_sort);
    installDblClick<NmModel>(ui->treeView, m);

    NmProxy * proxy = new NmProxy(this);
    proxy->setNmModel(qobject_cast<NmModel*>(m), NmModel::ActiveConnectionType);
    proxy_sort = new QSortFilterProxyModel{this};
    proxy_sort->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxy_sort->sort(0);
    proxy_sort->setSourceModel(proxy);
    ui->listActive->setModel(proxy_sort);
    installDblClick<NmProxy>(ui->listActive, proxy);

    proxy = new NmProxy(this);
    proxy->setNmModel(qobject_cast<NmModel*>(m), NmModel::WifiNetworkType);
    proxy_sort = new QSortFilterProxyModel{this};
    proxy_sort->setSortCaseSensitivity(Qt::CaseInsensitive);
    proxy_sort->sort(0);
    proxy_sort->setSourceModel(proxy);
    ui->listWifi->setModel(proxy_sort);
    installDblClick<NmProxy>(ui->listWifi, proxy);
}

NmList::~NmList()
{
}
