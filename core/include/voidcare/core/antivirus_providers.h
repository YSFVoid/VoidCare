#pragma once

#include <QObject>
#include <memory>

#include "voidcare/core/process_runner.h"
#include "voidcare/core/types.h"

namespace voidcare::core {

class IAntivirusProvider {
public:
    virtual ~IAntivirusProvider() = default;

    [[nodiscard]] virtual QString name() const = 0;
    [[nodiscard]] virtual bool isAvailable() const = 0;
    [[nodiscard]] virtual bool canScan() const = 0;
    [[nodiscard]] virtual bool canRemediate() const = 0;
    [[nodiscard]] virtual QString status() const = 0;

    virtual AntivirusActionResult scanQuick(const LogCallback& logCallback = {}) = 0;
    virtual AntivirusActionResult scanFull(const LogCallback& logCallback = {}) = 0;
    virtual AntivirusActionResult scanCustom(const QString& targetPath,
                                             const LogCallback& logCallback = {}) = 0;
    virtual AntivirusActionResult remediate(const LogCallback& logCallback = {}) = 0;
};

class DefenderProvider final : public IAntivirusProvider {
public:
    explicit DefenderProvider(ProcessRunner* runner);

    [[nodiscard]] QString name() const override;
    [[nodiscard]] bool isAvailable() const override;
    [[nodiscard]] bool canScan() const override;
    [[nodiscard]] bool canRemediate() const override;
    [[nodiscard]] QString status() const override;

    AntivirusActionResult scanQuick(const LogCallback& logCallback = {}) override;
    AntivirusActionResult scanFull(const LogCallback& logCallback = {}) override;
    AntivirusActionResult scanCustom(const QString& targetPath,
                                     const LogCallback& logCallback = {}) override;
    AntivirusActionResult remediate(const LogCallback& logCallback = {}) override;

    [[nodiscard]] QString mpCmdRunPath() const;

private:
    AntivirusActionResult runScan(const QStringList& arguments,
                                  const LogCallback& logCallback) const;

    ProcessRunner* m_runner = nullptr;
};

class ExternalProvider final : public IAntivirusProvider {
public:
    explicit ExternalProvider(ProcessRunner* runner, QString managedByName);

    void setCommandForSession(const QString& executable, const QStringList& arguments);
    [[nodiscard]] QString commandExecutable() const;
    [[nodiscard]] QStringList commandArguments() const;

    [[nodiscard]] QString name() const override;
    [[nodiscard]] bool isAvailable() const override;
    [[nodiscard]] bool canScan() const override;
    [[nodiscard]] bool canRemediate() const override;
    [[nodiscard]] QString status() const override;

    AntivirusActionResult scanQuick(const LogCallback& logCallback = {}) override;
    AntivirusActionResult scanFull(const LogCallback& logCallback = {}) override;
    AntivirusActionResult scanCustom(const QString& targetPath,
                                     const LogCallback& logCallback = {}) override;
    AntivirusActionResult remediate(const LogCallback& logCallback = {}) override;

private:
    AntivirusActionResult runExternal(const QString& extraPath, const LogCallback& logCallback) const;

    ProcessRunner* m_runner = nullptr;
    QString m_managedByName;
    QString m_executable;
    QStringList m_arguments;
};

class NoneProvider final : public IAntivirusProvider {
public:
    [[nodiscard]] QString name() const override;
    [[nodiscard]] bool isAvailable() const override;
    [[nodiscard]] bool canScan() const override;
    [[nodiscard]] bool canRemediate() const override;
    [[nodiscard]] QString status() const override;

    AntivirusActionResult scanQuick(const LogCallback& logCallback = {}) override;
    AntivirusActionResult scanFull(const LogCallback& logCallback = {}) override;
    AntivirusActionResult scanCustom(const QString& targetPath,
                                     const LogCallback& logCallback = {}) override;
    AntivirusActionResult remediate(const LogCallback& logCallback = {}) override;
};

std::unique_ptr<IAntivirusProvider> makePreferredProvider(
    const QVector<AntivirusProduct>& discoveredProducts,
    ProcessRunner* runner,
    ExternalProvider** externalProviderOut = nullptr);

}  // namespace voidcare::core
