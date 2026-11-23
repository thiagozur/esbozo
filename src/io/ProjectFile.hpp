#pragma once

#include <QByteArray>
#include <QString>

#include "core/Document.hpp"

namespace ProjectFile {

QByteArray serialize (const Document& doc);
bool save (const Document& doc, const QString& path, QString* error = nullptr);
bool load (Document& doc, const QString& path, QString* error = nullptr);

}
