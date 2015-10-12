#if !defined(TRAY_H)
#define TRAY_H

#include <QObject>
#include <QScopedPointer>

class TrayPrivate;

class Tray : public QObject
{   
Q_OBJECT

public:
    Tray(QObject *parent = 0);
    ~Tray();

private Q_SLOTS:
    //menu
    void onAboutTriggered();
    void onQuitTriggered();
    void onActivated();

    //NetworkManager
    void setActionsStates();
    void onPrimaryConnectionChanged(QString const & uni);

private:
    QScopedPointer<TrayPrivate> d;
};


#endif //TRAY_H
