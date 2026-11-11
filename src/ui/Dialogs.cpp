#include "ui/Dialogs.hpp"

#include <QFileDialog>

#include "ui/Theme.hpp"

namespace Dialogs {

QString pickFile (QWidget* parent, const QString& title, const QString& nameFilter) {
    QFileDialog dialog (parent, title);
    dialog.setOption (QFileDialog::DontUseNativeDialog, false);
    dialog.setFileMode (QFileDialog::ExistingFile);
    dialog.setNameFilter (nameFilter);
    dialog.setViewMode (QFileDialog::Detail);
    dialog.setStyleSheet (Theme::dialogStyle());

    if (dialog.exec() != QDialog::Accepted)
        return QString();
    return dialog.selectedFiles().value (0);
}

}
