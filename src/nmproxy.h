#if !defined(NMPROXY_H)
#define NMPROXY_H

#include <QAbstractProxyModel>
#include "nmmodel.h"

class NmProxy : public QAbstractProxyModel
{
    Q_OBJECT
public:
    using QAbstractProxyModel::QAbstractProxyModel;
    virtual void setSourceModel(QAbstractItemModel * sourceModel) override;
    void setNmModel(NmModel * model, NmModel::ItemType shownType);

    virtual QModelIndex index(int row, int column, const QModelIndex & parent = QModelIndex()) const override;
    virtual QModelIndex parent(const QModelIndex &index) const override;

    virtual int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    virtual int columnCount(const QModelIndex &parent = QModelIndex()) const override;

    void activateConnection(QModelIndex const & index) const;
    void deactivateConnection(QModelIndex const & index) const;

protected:
    virtual QModelIndex mapToSource(const QModelIndex & proxyIndex) const override;
    virtual QModelIndex mapFromSource(const QModelIndex & sourceIndex) const override;

private Q_SLOTS:
    void onSourceDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles = QVector<int>());
    void onSourceHeaderDataChanged(Qt::Orientation orientation, int first, int last);

    void onSourceRowsAboutToBeInserted(const QModelIndex &parent, int first, int last);
    void onSourceRowsInserted(const QModelIndex &parent, int first, int last);

    void onSourceRowsAboutToBeRemoved(const QModelIndex &parent, int first, int last);
    void onSourceRowsRemoved(const QModelIndex &parent, int first, int last);

    void onSourceColumnsAboutToBeInserted(const QModelIndex &parent, int first, int last);
    void onSourceColumnsInserted(const QModelIndex &parent, int first, int last);

    void onSourceColumnsAboutToBeRemoved(const QModelIndex &parent, int first, int last);
    void onSourceColumnsRemoved(const QModelIndex &parent, int first, int last);

    void onSourceModelAboutToBeReset();
    void onSourceModelReset();

    void onSourceRowsAboutToBeMoved( const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destinationParent, int destinationRow);
    void onSourceRowsMoved( const QModelIndex &parent, int start, int end, const QModelIndex &destination, int row);

    void onSourceColumnsAboutToBeMoved( const QModelIndex &sourceParent, int sourceStart, int sourceEnd, const QModelIndex &destinationParent, int destinationColumn);
    void onSourceColumnsMoved( const QModelIndex &parent, int start, int end, const QModelIndex &destination, int column);

private:
    QModelIndex root;
};

#endif //NMPROXY_H
