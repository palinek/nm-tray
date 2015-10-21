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
#include "connectioninfo.h"
#include "ui_connectioninfo.h"
#include "nmproxy.h"
#include <QScrollArea>
#include <QLabel>
#include <QItemSelection>
#include <QSortFilterProxyModel>

ConnectionInfo::ConnectionInfo(NmModel * model, QWidget *parent)
    : QDialog{parent}
    , ui{new Ui::ConnectionInfo}
    , mModel{model}
    , mActive{new NmProxy}
    , mSorted{new QSortFilterProxyModel}

{
    setAttribute(Qt::WA_DeleteOnClose);
    ui->setupUi(this);

    mActive->setNmModel(mModel, NmModel::ActiveConnectionType);
    mSorted->setSortCaseSensitivity(Qt::CaseInsensitive);
    mSorted->sort(0);
    mSorted->setSourceModel(mActive.data());
    for (int i = 0, row_cnt = mSorted->rowCount(); i < row_cnt; ++i)
    {
        addTab(mSorted->index(i, 0));
    }

    connect(mSorted.data(), &QAbstractItemModel::rowsInserted, [this] (QModelIndex const & parent, int first, int last) {
        ui->tabWidget->setUpdatesEnabled(false);
        for (int i = first; i <= last; ++i)
            addTab(mSorted->index(i, 0, parent));
        ui->tabWidget->setUpdatesEnabled(true);
    });
    connect(mSorted.data(), &QAbstractItemModel::rowsAboutToBeRemoved, [this] (QModelIndex const & parent, int first, int last) {
        ui->tabWidget->setUpdatesEnabled(false);
        for (int i = first; i <= last; ++i)
            removeTab(mSorted->index(i, 0, parent));
        ui->tabWidget->setUpdatesEnabled(true);
    });
    connect(mSorted.data(), &QAbstractItemModel::dataChanged, [this] (const QModelIndex & topLeft, const QModelIndex & bottomRight, const QVector<int> & /*roles*/) {
        ui->tabWidget->setUpdatesEnabled(false);
        for (auto const & i : QItemSelection{topLeft, bottomRight}.indexes())
            changeTab(i);
        ui->tabWidget->setUpdatesEnabled(true);
    });
}

ConnectionInfo::~ConnectionInfo()
{
}

void ConnectionInfo::addTab(QModelIndex const & index)
{
    QScrollArea * content = new QScrollArea;
    QLabel * l = new QLabel;
    l->setText(mSorted->data(index, NmModel::ActiveConnectionInfoRole).toString());
    content->setWidget(l);
    //QTabWidget takes ownership of the page (if we will not take it back)
    ui->tabWidget->insertTab(index.row(), content, mSorted->data(index, NmModel::IconRole).value<QIcon>(), mSorted->data(index, NmModel::NameRole).toString());
}

void ConnectionInfo::removeTab(QModelIndex const & index)
{
    const int i = index.row();
    QWidget * w = ui->tabWidget->widget(i);
    ui->tabWidget->removeTab(i);
    w->deleteLater();
}

void ConnectionInfo::changeTab(QModelIndex const & index)
{
    const int i = index.row();
    QScrollArea * content = qobject_cast<QScrollArea *>(ui->tabWidget->widget(i));
    Q_ASSERT(nullptr != content);
    QLabel * l = qobject_cast<QLabel *>(content->widget());
    Q_ASSERT(nullptr != l);
    l->setText(mSorted->data(index, NmModel::ActiveConnectionInfoRole).toString());
    ui->tabWidget->tabBar()->setTabText(i, mSorted->data(index, NmModel::NameRole).toString());
    ui->tabWidget->tabBar()->setTabIcon(i,  mSorted->data(index, NmModel::IconRole).value<QIcon>());
}
