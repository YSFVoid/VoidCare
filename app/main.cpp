#include <QApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QMessageBox>

#include "voidcare/platform/windows/admin_utils.h"
#include "voidcare/ui/app_controller.h"

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("VoidCare"));
    application.setOrganizationName(QStringLiteral("VoidTools"));

    if (!voidcare::platform::windows::isRunningAsAdmin()) {
        QMessageBox::critical(nullptr,
                              QStringLiteral("VoidCare"),
                              QStringLiteral("VoidCare must be run as Administrator."));
        return 1;
    }

    QQmlApplicationEngine engine;
    voidcare::ui::AppController controller;
    engine.rootContext()->setContextProperty(QStringLiteral("appController"), &controller);

    const QUrl url(QStringLiteral("qrc:/ui/qml/Main.qml"));
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreated,
        &application,
        [url](QObject* object, const QUrl& objectUrl) {
            if (object == nullptr && url == objectUrl) {
                QCoreApplication::exit(-1);
            }
        },
        Qt::QueuedConnection);

    engine.load(url);
    return application.exec();
}
