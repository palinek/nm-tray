#include "nmlist.h"
#include "ui_nmlist.h"

NmList::NmList(QAbstractItemModel * m, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::NmList)
{
    ui->setupUi(this);

    QScopedPointer<QAbstractItemModel> old{ui->treeView->model()};
    ui->treeView->setModel(m);

}

NmList::~NmList()
{
    delete ui;
}
