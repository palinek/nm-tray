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

#include "menuview.h"
#include "nmmodel.h"

#include <QSortFilterProxyModel>
#include <QScrollBar>
#include <QProxyStyle>
#include <QStyledItemDelegate>
#include <QApplication>
#include <QPainter>
#include <QDebug>

namespace
{
    class SingleActivateStyle : public QProxyStyle
    {
    public:
        using QProxyStyle::QProxyStyle;
        virtual int styleHint(StyleHint hint, const QStyleOption * option = 0, const QWidget * widget = 0, QStyleHintReturn * returnData = 0) const override
        {
            if(hint == QStyle::SH_ItemView_ActivateItemOnSingleClick)
                return 1;
            return QProxyStyle::styleHint(hint, option, widget, returnData);

        }
    };

    class MultiIconDelegate : public QStyledItemDelegate
    {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;
        void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
        {
            Q_ASSERT(index.isValid());

            QStyleOptionViewItem opt = option;
            initStyleOption(&opt, index);

            //add the security overlay icon
            QIcon sec_icon = qvariant_cast<QIcon>(index.data(NmModel::IconSecurityRole));
            if (!sec_icon.isNull())
            {
                QPixmap res_pixmap = opt.icon.pixmap(opt.decorationSize);
                QPainter p{&res_pixmap};
                QSize sec_size = res_pixmap.size();
                sec_size /= 2;
                QPoint sec_pos = res_pixmap.rect().bottomRight();
                sec_pos -= QPoint{sec_size.width(), sec_size.height()};
                p.drawPixmap(sec_pos, sec_icon.pixmap(sec_size));

                opt.icon = QIcon{res_pixmap};

            }
            QStyle *style = option.widget ? option.widget->style() : QApplication::style();
            style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, option.widget);
        }
    };

}

MenuView::MenuView(QAbstractItemModel * model, QWidget * parent /*= nullptr*/)
    : QListView(parent)
    , mProxy{new QSortFilterProxyModel{this}}
    , mMaxItemsToShow(10)
{
    setEditTriggers(NoEditTriggers);
    setSizeAdjustPolicy(AdjustToContents);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setSelectionBehavior(SelectRows);
    setSelectionMode(NoSelection);
    setFrameShape(QFrame::HLine);
    setFrameShadow(QFrame::Plain);

    setStyle(new SingleActivateStyle);
    mProxy->setSourceModel(model);
    mProxy->setDynamicSortFilter(true);
    mProxy->setFilterRole(Qt::DisplayRole);
    mProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    mProxy->setSortRole(Qt::DisplayRole);
    mProxy->setSortCaseSensitivity(Qt::CaseInsensitive);
    mProxy->sort(0);
    {
        QScopedPointer<QItemSelectionModel> guard{selectionModel()};
        setModel(mProxy);
    }
    {
        QScopedPointer<QAbstractItemDelegate> guard{itemDelegate()};
        setItemDelegate(new MultiIconDelegate{this});
    }
    //XXX: leave this to styling!?!
    setIconSize({32, 32});
}

void MenuView::setFilter(QString const & filter)
{
    mProxy->setFilterFixedString(filter);
    const int count = mProxy->rowCount();
    if (0 < count)
    {
        if (count > mMaxItemsToShow)
        {
            setCurrentIndex(mProxy->index(mMaxItemsToShow - 1, 0));
            verticalScrollBar()->triggerAction(QScrollBar::SliderToMinimum);
        } else
        {
            setCurrentIndex(mProxy->index(count - 1, 0));
        }
    }
}

void MenuView::setMaxItemsToShow(int max)
{
    mMaxItemsToShow = max;
}

void MenuView::activateCurrent()
{
    QModelIndex const index = currentIndex();
    if (index.isValid())
        emit activated(index);
}

QSize MenuView::viewportSizeHint() const
{
    const int count = mProxy->rowCount();
    QSize s{0, 0};
    if (0 < count)
    {
        const bool scrollable = mMaxItemsToShow < count;
        s.setWidth(sizeHintForColumn(0) + (scrollable ? verticalScrollBar()->sizeHint().width() : 0));
        s.setHeight(sizeHintForRow(0) * (scrollable ? mMaxItemsToShow  : count));
    }
    return s;
}

QSize MenuView::minimumSizeHint() const
{
    return QSize{0, 0};
}
