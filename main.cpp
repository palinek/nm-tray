#include <QApplication>

#include "tray.h"

#include "nmmodel.h"
#include "nmlist.h"

int main(int argc, char * argv[])
{
    QApplication app{argc, argv};
    app.setQuitOnLastWindowClosed(false);

    Tray tray;
    
    return app.exec();
}
