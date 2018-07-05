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
#if !defined(TRAY_H)
#define TRAY_H

#include <QObject>
#include <QScopedPointer>
#include <QSystemTrayIcon>

class TrayPrivate;

class Tray : public QObject
{   
Q_OBJECT

public:
    Tray(QObject *parent = nullptr);
    ~Tray();

protected:
    virtual bool eventFilter(QObject * object, QEvent * event) override;

private Q_SLOTS:
    //menu
    void onEditConnectionsTriggered();
    void onAboutTriggered();
    void onQuitTriggered();
    void onActivated(const QSystemTrayIcon::ActivationReason reason);

    //NetworkManager
    void setActionsStates();
    void onPrimaryConnectionChanged(QString const & uni);

private:
    QScopedPointer<TrayPrivate> d;
};


#endif //TRAY_H
