#include "nmlist.h"
#include "ui_nmlist.h"
#include "nmmodel.h"
#include "nmproxy.h"

template <typename T>
void installDblClick(QAbstractItemView * view, QAbstractItemModel * m)
{
    T * net_model = qobject_cast<T *>(m);
    if (net_model)
    {
        QObject::connect(view, &QAbstractItemView::doubleClicked, [net_model] (QModelIndex const & i) {
            switch (static_cast<NmModel::ItemType>(net_model->data(i, NmModel::ItemTypeRole).toInt()))
            {
                case NmModel::ActiveConnectionType:
                    net_model->deactivateConnection(i);
                    break;
                case NmModel::WifiNetworkType:
                case NmModel::ConnectionType:
                    net_model->activateConnection(i);
                    break;
                default:
                    //do nothing
                    break;
            }
        });
    }
}

NmList::NmList(QString const & title, QAbstractItemModel * m, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::NmList)
{
    ui->setupUi(this);
    setWindowTitle(title);

    QScopedPointer<QAbstractItemModel> old{ui->treeView->model()};
    ui->treeView->setModel(m);
    //XXX just testing
    Q_ASSERT(qobject_cast<NmModel*>(m));
    installDblClick<NmModel>(ui->treeView, m);

    NmProxy * proxy = new NmProxy(this);
    proxy->setNmModel(qobject_cast<NmModel*>(m), NmModel::ActiveConnectionType);
    ui->listActive->setModel(proxy);
    installDblClick<NmProxy>(ui->listActive, proxy);

    proxy = new NmProxy(this);
    proxy->setNmModel(qobject_cast<NmModel*>(m), NmModel::WifiNetworkType);
    ui->listWifi->setModel(proxy);
    installDblClick<NmProxy>(ui->listWifi, proxy);
}

NmList::~NmList()
{
    delete ui;
}
