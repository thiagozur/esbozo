#include <QApplication>

#include "ui/Backdrop.hpp"
#include "ui/EsbozoWindow.hpp"
#include "ui/Theme.hpp"

int main (int argc, char* argv[]) {
    QApplication app (argc, argv);
    Theme::applyTo (app);
    Backdrop::currentStyle() = Backdrop::preset ("subtle");

    EsbozoWindow window;
    window.show();
    window.showStartupHint();

    return app.exec();
}
