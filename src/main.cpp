#include <QApplication>
#include <QFileInfo>

#include "ui/Backdrop.hpp"
#include "ui/EsbozoWindow.hpp"
#include "ui/Theme.hpp"

int main (int argc, char* argv[]) {
    QApplication app (argc, argv);
    QCoreApplication::setOrganizationName ("Esbozo");
    QCoreApplication::setApplicationName ("Esbozo");
    Theme::applyTo (app);
    Backdrop::currentStyle() = Backdrop::preset ("subtle");

    EsbozoWindow window;
    window.show();

    const QStringList args = QCoreApplication::arguments();
    if (args.size() > 1 && QFileInfo::exists (args.at (1)))
        window.loadProject (args.at (1));
    else
        window.showStartupHint();

    return app.exec();
}
