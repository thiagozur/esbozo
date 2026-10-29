#include <QApplication>

#include "ui/EsbozoWindow.hpp"

int main (int argc, char* argv[]) {
    QApplication app (argc, argv);

    EsbozoWindow window;
    window.show();

    return app.exec();
}