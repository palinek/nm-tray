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
#include "nmlist.h"
#include "ui_nmlist.h"
#include "nmmodel.h"
#include "nmproxy.h"

template <typename T>
void installDblClick(QAbstractItemView * view, QAbstractItemModel * m)
{
    T * net_model = qobject_cast<T *>(m);
    if (net_model)
    {
        QObject::connect(view, &QAbstractItemView::doubleClicked, [net_model] (QModelIndex const & i) {
            switch (static_cast<NmModel::ItemType>(net_model->data(i, NmModel::ItemTypeRole).toInt()))
            {
                case NmModel::ActiveConnectionType:
                    net_model->deactivateConnection(i);
                    break;
                case NmModel::WifiNetworkType:
                case NmModel::ConnectionType:
                    net_model->activateConnection(i);
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
    , ui(new Ui::NmList)
{
    ui->setupUi(this);
    setWindowTitle(title);

    QScopedPointer<QAbstractItemModel> old{ui->treeView->model()};
    ui->treeView->setModel(m);
    //XXX just testing
    Q_ASSERT(qobject_cast<NmModel*>(m));
    installDblClick<NmModel>(ui->treeView, m);

    NmProxy * proxy = new NmProxy(this);
    proxy->setNmModel(qobject_cast<NmModel*>(m), NmModel::ActiveConnectionType);
    ui->listActive->setModel(proxy);
    installDblClick<NmProxy>(ui->listActive, proxy);

    proxy = new NmProxy(this);
    proxy->setNmModel(qobject_cast<NmModel*>(m), NmModel::WifiNetworkType);
    ui->listWifi->setModel(proxy);
    installDblClick<NmProxy>(ui->listWifi, proxy);
}

NmList::~NmList()
{
    delete ui;
}
