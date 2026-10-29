#pragma once

#include <QString>

#include "core/Document.hpp"

namespace MarkdownExporter {

bool exportTo (const Document& doc, const QString& filePath);

}