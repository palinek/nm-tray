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
#ifndef CONNECTIONINFO_H
#define CONNECTIONINFO_H

#include <QDialog>

namespace Ui {
class ConnectionInfo;
}

class NmModel;
class NmProxy;
class QSortFilterProxyModel;

class ConnectionInfo : public QDialog
{
    Q_OBJECT

public:
    explicit ConnectionInfo(NmModel * model, QWidget *parent = 0);
    ~ConnectionInfo();

private:
    void addTab(QModelIndex const & index);
    void removeTab(QModelIndex const & index);
    void changeTab(QModelIndex const & index);

private:
    QScopedPointer<Ui::ConnectionInfo> ui;
    NmModel * mModel;
    QScopedPointer<NmProxy> mActive;
    QScopedPointer<QSortFilterProxyModel> mSorted;
};

#endif // CONNECTIONINFO_H
