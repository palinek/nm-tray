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

#if !defined(MENU_VIEW_H)
#define MENU_VIEW_H

#include <QListView>

class QAbstractItemModel;
class QSortFilterProxyModel;

class MenuView : public QListView
{
    Q_OBJECT
public:
    MenuView(QAbstractItemModel * model, QWidget * parent = nullptr);

    /*! \brief Sets the filter for entries to be presented
     */
    void setFilter(QString const & filter);
    /*! \brief Set the maximum number of items/results to show
     */
    void setMaxItemsToShow(int max);
    /*! \brief Set the maximum width of item to show
     */
    void setMaxItemWidth(int max);

public Q_SLOTS:
    /*! \brief Trigger action on currently active item
     */
    void activateCurrent();

protected:
    virtual QSize viewportSizeHint() const override;
    virtual QSize minimumSizeHint() const override;

private:
    QSortFilterProxyModel * mProxy;
    int mMaxItemsToShow;
};

#endif //MENU_VIEW_H
