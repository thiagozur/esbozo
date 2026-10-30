#include <QApplication>

#include "ui/EsbozoWindow.hpp"
#include "ui/Theme.hpp"

int main (int argc, char* argv[]) {
    QApplication app (argc, argv);
    Theme::applyTo (app);

    EsbozoWindow window;
    window.show();

    return app.exec();
}
