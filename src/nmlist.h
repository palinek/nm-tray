#ifndef NMLIST_H
#define NMLIST_H

#include <QDialog>

namespace Ui {
class NmList;
}

class QAbstractItemModel;

class NmList : public QDialog
{
    Q_OBJECT

public:
    explicit NmList(QAbstractItemModel * m, QWidget *parent = 0);
    ~NmList();

private:
    Ui::NmList *ui;
};

#endif // NMLIST_H
