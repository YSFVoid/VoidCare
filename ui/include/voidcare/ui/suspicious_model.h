#pragma once

#include <QAbstractListModel>

namespace voidcare::ui {

class SuspiciousModel : public QAbstractListModel {
    Q_OBJECT

public:
    explicit SuspiciousModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRows(const QVariantList& rows);

private:
    QVariantList m_rows;
};

}  // namespace voidcare::ui
