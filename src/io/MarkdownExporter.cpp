#include "io/MarkdownExporter.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPixmap>
#include <QTextStream>

#include "core/GraphBlock.hpp"
#include "core/GraphRenderer.hpp"
#include "core/TableBlock.hpp"

namespace {

QString tableToMarkdown (const TableBlock& table) {
    const QVector<QStringList>& td = table.getData();
    if (td.isEmpty())
        return QString();

    QString md;
    if (!table.getTitle().isEmpty())
        md += QString ("**%1**\n\n").arg (table.getTitle());

    int ncols = 0;
    for (const QStringList& r : td)
        ncols = qMax (ncols, static_cast<int> (r.size()));

    auto rowToMd = [ncols] (const QStringList& r) {
        QString line = "|";
        for (int c = 0; c < ncols; ++c) {
            QString cell = c < r.size() ? r[c] : QString();
            cell.replace ("|", "\\|");
            line += " " + cell + " |";
        }
        return line + "\n";
    };

    int start = 0;
    if (table.hasHeaderRow()) {
        md += rowToMd (td[0]);
        start = 1;
    } else {
        md += rowToMd (QStringList());
    }
    md += "|" + QString (" --- |").repeated (ncols) + "\n";
    for (int r = start; r < td.size(); ++r)
        md += rowToMd (td[r]);
    md += "\n";
    return md;
}

}

namespace MarkdownExporter {

bool exportTo (const Document& doc, const QString& filePath) {
    const QFileInfo fileInfo (filePath);
    QDir exportDir = fileInfo.dir();
    const QString imagesDirName = "images";
    const QDir imagesDir (exportDir.filePath (imagesDirName));
    if (!imagesDir.exists())
        exportDir.mkdir (imagesDirName);

    QString md;
    int graphCounter = 1;

    for (size_t i = 0; i < doc.getBlockCount(); ++i) {
        const Block* block = doc.getBlock (i);
        if (!block)
            continue;

        switch (block->getType()) {
            case BlockType::Title:
                md += "# " + block->getContent() + "\n\n";
                break;
            case BlockType::Subtitle:
                md += "### " + block->getContent() + "\n\n";
                break;
            case BlockType::Paragraph:
                md += block->getContent() + "\n\n";
                break;
            case BlockType::Image: {
                const QString srcPath = block->getContent();
                if (!srcPath.isEmpty() && QFile::exists (srcPath)) {
                    const QString destFileName = QFileInfo (srcPath).fileName();
                    QFile::copy (srcPath, imagesDir.filePath (destFileName));
                    md += QString ("![Imagen](%1/%2)\n").arg (imagesDirName, destFileName);
                    md += QString ("> *%1*\n\n").arg (destFileName);
                } else {
                    md += "![Imagen](imagen_no_encontrada)\n";
                    md += "> *imagen_no_encontrada*\n\n";
                }
                break;
            }
            case BlockType::Graph:
                if (const GraphBlock* g = dynamic_cast<const GraphBlock*> (block)) {
                    const QString graphFileName = QString ("grafico_%1.png").arg (graphCounter++);
                    QPixmap pixmap = GraphRenderer::render (*g, 800, 520, 2.0);
                    pixmap.save (imagesDir.filePath (graphFileName), "PNG");
                    md += QString ("![Gráfico](%1/%2)\n").arg (imagesDirName, graphFileName);
                }
                break;
            case BlockType::Equation:
                md += "$$\n" + block->getContent() + "\n$$\n\n";
                break;
            case BlockType::Table:
                if (const TableBlock* t = dynamic_cast<const TableBlock*> (block))
                    md += tableToMarkdown (*t);
                break;
        }
    }

    QFile outFile (filePath);
    if (!outFile.open (QIODevice::WriteOnly | QIODevice::Text))
        return false;
    QTextStream out (&outFile);
    out << md;
    outFile.close();
    return true;
}

}