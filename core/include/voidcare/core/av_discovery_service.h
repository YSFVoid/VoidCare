#pragma once

#include <QObject>
#include <QVector>

#include "voidcare/core/types.h"

namespace voidcare::core {

class AvDiscoveryService : public QObject {
public:
    explicit AvDiscoveryService(QObject* parent = nullptr);

    QVector<AntivirusProduct> discover() const;
};

}  // namespace voidcare::core
