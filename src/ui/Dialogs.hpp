#pragma once

#include <QString>

class QWidget;

namespace Dialogs {

QString pickFile (QWidget* parent, const QString& title, const QString& nameFilter);

}
