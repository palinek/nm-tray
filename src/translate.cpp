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
#include <QCoreApplication>
#include <QLocale>
#include <QTranslator>
#include <QLibraryInfo>

#if !defined(TRANSLATION_DIR)
#define TRANSLATION_DIR "/usr/share/nm-tray"
#endif

void translate()
{
    const QString locale = QLocale::system().name();
    QTranslator * qt_trans = new QTranslator{qApp};
    if (qt_trans->load(QLatin1String("qt_") + locale, QLibraryInfo::location(QLibraryInfo::TranslationsPath)))
        qApp->installTranslator(qt_trans);
    else
        delete qt_trans;

    QString trans_dir = qgetenv("NM_TRAY_TRANSLATION_DIR").constData();
    QTranslator * my_trans = new QTranslator{qApp};
    if (my_trans->load(QLatin1String("nm-tray_") + locale, trans_dir.isEmpty() ? TRANSLATION_DIR : trans_dir))
        qApp->installTranslator(my_trans);
    else
        delete my_trans;
}

Q_COREAPP_STARTUP_FUNCTION(translate)
