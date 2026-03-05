#include "voidcare/ui/persistence_model.h"

namespace voidcare::ui {

PersistenceModel::PersistenceModel(QObject* parent)
    : QAbstractListModel(parent) {}

int PersistenceModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant PersistenceModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }

    const QVariantMap row = m_rows.at(index.row()).toMap();
    if (role < Qt::UserRole) {
        return row;
    }

    const QByteArray roleName = roleNames().value(role);
    return row.value(QString::fromUtf8(roleName));
}

QHash<int, QByteArray> PersistenceModel::roleNames() const {
    return {
        {Qt::UserRole + 1, "id"},
        {Qt::UserRole + 2, "sourceType"},
        {Qt::UserRole + 3, "name"},
        {Qt::UserRole + 4, "path"},
        {Qt::UserRole + 5, "args"},
        {Qt::UserRole + 6, "publisher"},
        {Qt::UserRole + 7, "signatureStatus"},
        {Qt::UserRole + 8, "enabled"},
        {Qt::UserRole + 9, "rawReference"},
    };
}

void PersistenceModel::setRows(const QVariantList& rows) {
    beginResetModel();
    m_rows = rows;
    endResetModel();
}

}  // namespace voidcare::ui
