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
#ifndef NMLIST_H
#define NMLIST_H

#include <memory>
#include <QDialog>

namespace Ui {
class NmList;
}

class QAbstractItemModel;

class NmList : public QDialog
{
    Q_OBJECT

public:
    explicit NmList(QString const & title, QAbstractItemModel * m, QWidget *parent = nullptr);
    ~NmList();

private:
    std::unique_ptr<Ui::NmList> ui;
};

#endif // NMLIST_H
