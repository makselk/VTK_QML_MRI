#include "Viewers/QVTKPlaneViewer.h"
#include "Viewers/QVTKModelViewer.h"
#include "Viewers/MriDataProvider.h"
#include <QQmlApplicationEngine>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQuickView>
#include <QList>



int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);

    qmlRegisterType<QVTKPlaneViewerItem>("VTK", 8, 2, "VtkPlaneViewer");
    qmlRegisterType<QVTKModelViewerItem>("VTK", 8, 2, "VtkModelViewer");

    QQmlApplicationEngine engine;

    QQmlContext* context = engine.rootContext();
    context->setContextProperty("mri_data_provider", &MriDataProvider::getInstance());

    const QUrl url(QStringLiteral("qrc:/main.qml"));

    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    QObject* topLevel = engine.rootObjects().value(0);
    QQuickWindow* window = qobject_cast<QQuickWindow*>(topLevel);
    window->show();

    return app.exec();
}
