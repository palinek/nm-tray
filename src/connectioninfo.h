#ifndef CONNECTIONINFO_H
#define CONNECTIONINFO_H

#include <QDialog>

namespace Ui {
class ConnectionInfo;
}

class NmModel;
class NmProxy;

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
};

#endif // CONNECTIONINFO_H
