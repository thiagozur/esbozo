#include <QApplication>
#include <QMainWindow>
#include <QTextEdit>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QLabel>
#include <QWidget>

#include "core/Document.hpp"
#include "core/CommandProcessor.hpp"

class EsbozoWindow : public QMainWindow {
    Q_OBJECT

private:
    Document m_document;
    CommandProcessor m_processor;

    QTextEdit *m_markdownView;
    QLineEdit *m_inputBox;
    QLabel *m_statusLabel;

public:
    EsbozoWindow() : m_processor (m_document) {
        setWindowTitle ("Esbozo");
        resize (800, 600);

        QWidget *centralWidget = new QWidget (this);
        QVBoxLayout *layout = new QVBoxLayout (centralWidget);

        layout->addWidget (new QLabel ("Md Preview:", this));
        m_markdownView = new QTextEdit (this);
        m_markdownView->setReadOnly (true);
        m_markdownView->setFont (QFont ("Consolas", 11));
        layout->addWidget (m_markdownView);

        m_statusLabel = new QLabel ("Escribir un comando (o texto) y presionar Enter.", this);
        m_statusLabel->setStyleSheet ("color: gray; font-style: italic;");
        layout->addWidget (m_statusLabel);

        m_inputBox = new QLineEdit (this);
        m_inputBox->setPlaceholderText ("Escribir acá...");
        m_inputBox->setFont (QFont ("Segoe UI", 11));
        layout->addWidget (m_inputBox);

        setCentralWidget (centralWidget);

        connect (m_inputBox, &QLineEdit::returnPressed, this, &EsbozoWindow::handleInputSubmitted);

        updateView();
    }

private slots:
    void handleInputSubmitted() {
        QString text = m_inputBox->text();
        if (text.isEmpty()) return;

        CommandResult result = m_processor.processInput (text);

        m_inputBox->clear();

        switch (result) {
            case CommandResult::BlockCreated:
                m_statusLabel->setText ("Comando ejecutado: Nuevo bloque creado!");
                break;
            case CommandResult::BlockReset:
                m_statusLabel->setText ("Comando ejecutado: Bloque actual reiniciado.");
                break;
            case CommandResult::SectionFinished:
                m_statusLabel->setText ("Comando ejecutado: Pasando a la sección siguiente.");
                break;
            case CommandResult::TextUpdated:
                m_statusLabel->setText ("Texto del bloque actual actualizado.");
                break;
            default:
                m_statusLabel->setText ("Entrada procesada.");
                break;
        }

        updateView();
    }

    void updateView() {
        QString markdown = m_document.toMarkdown();
        m_markdownView->setPlainText (markdown);
    }
};

int main(int argc, char *argv[]) {
    QApplication app (argc, argv);

    EsbozoWindow window;
    window.show();

    return app.exec();
}

#include "main.moc"