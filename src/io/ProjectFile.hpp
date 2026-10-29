#pragma once

#include <QString>

#include "core/Document.hpp"

namespace ProjectFile {

bool save (const Document& doc, const QString& path, QString* error = nullptr);
bool load (Document& doc, const QString& path, QString* error = nullptr);

}