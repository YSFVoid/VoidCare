#include "voidcare/ui/suspicious_model.h"

namespace voidcare::ui {

SuspiciousModel::SuspiciousModel(QObject* parent)
    : QAbstractListModel(parent) {}

int SuspiciousModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant SuspiciousModel::data(const QModelIndex& index, int role) const {
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

QHash<int, QByteArray> SuspiciousModel::roleNames() const {
    return {
        {Qt::UserRole + 1, "path"},
        {Qt::UserRole + 2, "size"},
        {Qt::UserRole + 3, "created"},
        {Qt::UserRole + 4, "modified"},
        {Qt::UserRole + 5, "sha256"},
        {Qt::UserRole + 6, "signatureStatus"},
        {Qt::UserRole + 7, "score"},
        {Qt::UserRole + 8, "reasons"},
        {Qt::UserRole + 9, "persistenceRef"},
    };
}

void SuspiciousModel::setRows(const QVariantList& rows) {
    beginResetModel();
    m_rows = rows;
    endResetModel();
}

}  // namespace voidcare::ui
