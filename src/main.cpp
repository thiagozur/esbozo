#include <QApplication>
#include <QMainWindow>
#include <QLabel>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QMainWindow mainWindow;
    mainWindow.setWindowTitle("esbozo;; - Prototipo");
    mainWindow.resize(800, 600);

    QLabel *centralLabel = new QLabel("Bienvenido a esbozo;;!", &mainWindow);
    centralLabel->setAlignment(Qt::AlignCenter);
    mainWindow.setCentralWidget(centralLabel);

    mainWindow.show();

    return app.exec();
}