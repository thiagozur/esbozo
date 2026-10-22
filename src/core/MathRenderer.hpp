#pragma once

#include <QString>
#include <QPixmap>
#include <QProcess>
#include <QTemporaryDir>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QCryptographicHash>
#include <QDir>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QPainter>
#include <QColor>
#include <QRect>
#include <QFont>
#include <QFontMetrics>

namespace math_renderer_detail {

struct ToolPaths {
    QString latex;
    QString dvipng;
    bool ok() const {
        return !latex.isEmpty() && !dvipng.isEmpty();
    }
};

inline const ToolPaths& tools() {
    static const ToolPaths t = []{
        ToolPaths r;
        r.latex  = QStandardPaths::findExecutable ("latex");
        r.dvipng = QStandardPaths::findExecutable ("dvipng");
        return r;
    }();
    return t;
}

inline QHash<QString, QPixmap>& cache() {
    static QHash<QString, QPixmap> c;
    return c;
}

inline QMutex& cacheMutex() {
    static QMutex m;
    return m;
}

inline QString cacheDirectory() {
    static QString dir = []{
        QString path = QStandardPaths::writableLocation (QStandardPaths::CacheLocation) + "/equations";
        QDir().mkpath (path);
        return path;
    }();
    return dir;
}

inline QPixmap renderFallback (const QString& text, const QString& reason) {
    QFont font ("Courier New", 10);
    font.setStyleHint (QFont::Monospace);
    QFontMetrics fm (font);

    const QString display = text.trimmed().isEmpty() ? QStringLiteral ("(ecuación vacía)") : text;
    QRect br = fm.boundingRect (QRect (0, 0, 500, 2000), Qt::TextWordWrap, display);

    QSize sz (qMax (120, br.width() + 24), qMax (40, br.height() + 24));
    QPixmap pm (sz);
    pm.fill (Qt::transparent);

    const QRect textRect (0, 0, sz.width(), sz.height());

    QPainter p (&pm);
    p.setRenderHint (QPainter::Antialiasing);
    p.setFont (font);
    p.setPen (QColor (180, 40, 40));
    p.drawText (textRect.adjusted (12, 12, -12, -12), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, display);

    if (!reason.isEmpty()) {
        QFont small = font;
        small.setPointSize (8);
        p.setFont (small);
        p.setPen (QColor (150, 150, 150));
        p.drawText (QRect (12, sz.height() - 22, sz.width() - 24, 18), Qt::AlignRight | Qt::AlignVCenter, reason);
    }
    p.end();
    return pm;
}

}

class MathRenderer {
public:
    static QPixmap render (const QString& latex, qreal dpr = 1.0);

    static bool isAvailable() {
        return math_renderer_detail::tools().ok();
    }

    static void clearCache() {
        QMutexLocker lk (&math_renderer_detail::cacheMutex());
        math_renderer_detail::cache().clear();
        
        QDir dir(math_renderer_detail::cacheDirectory());
        dir.removeRecursively();
    }
};

inline QPixmap MathRenderer::render (const QString& latex, qreal dpr) {
    using namespace math_renderer_detail;

    if (latex.trimmed().isEmpty()) {
        return renderFallback (QString(), QString());
    }

    const QString key = latex + QStringLiteral ("|") + QString::number (dpr, 'f', 2);

    {
        QMutexLocker lk (&cacheMutex());
        auto it = cache().find (key);
        if (it != cache().end())
            return it.value();
    }

    const QByteArray hash = QCryptographicHash::hash (key.toUtf8(), QCryptographicHash::Sha256).toHex();
    const QString diskPath = cacheDirectory() + "/" + QString::fromLatin1 (hash) + ".png";

    if (QFile::exists (diskPath)) {
        QPixmap pm;
        if (pm.load (diskPath, "PNG")) {
            pm.setDevicePixelRatio (dpr);
            QMutexLocker lk (&cacheMutex());
            cache().insert (key, pm);
            return pm;
        }
    }

    QPixmap result;

    if (!tools().ok()) {
        result = renderFallback (latex, QStringLiteral ("LaTeX no disponible"));
    } else {
        QTemporaryDir tmp;
        if (tmp.isValid()) {
            const QString texPath = tmp.filePath (QStringLiteral ("formula.tex"));
            QFile texFile (texPath);
            if (texFile.open (QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream ts (&texFile);
                ts << "\\documentclass[12pt]{article}\n"
                   << "\\usepackage{amsmath,amssymb}\n"
                   << "\\pagestyle{empty}\n"
                   << "\\begin{document}\n"
                   << "\\[\n" << latex << "\n\\]\n"
                   << "\\end{document}\n";
                texFile.close();

                QProcess latexProc;
                latexProc.setWorkingDirectory (tmp.path());
                latexProc.start (tools().latex,
                                 { "-interaction=nonstopmode", "-halt-on-error", "formula.tex" });

                if (latexProc.waitForFinished (10000) && latexProc.exitCode() == 0) {
                    const int dpi = qRound (96.0 * dpr);

                    QProcess dvipngProc;
                    dvipngProc.setWorkingDirectory (tmp.path());
                    dvipngProc.start (tools().dvipng,
                                      { "-T", "tight",
                                        "-D", QString::number (dpi),
                                        "-bg", "Transparent",
                                        "-o", "formula.png",
                                        "formula.dvi" });

                    if (dvipngProc.waitForFinished (10000) && dvipngProc.exitCode() == 0) {
                        const QString pngPath = tmp.filePath (QStringLiteral ("formula.png"));
                        if (QFile::exists (pngPath)) {
                            QPixmap pm;
                            if (pm.load (pngPath, "PNG")) {
                                pm.setDevicePixelRatio (dpr);
                                result = pm;
                                pm.save (diskPath, "PNG");
                            }
                        }
                    }
                }

                if (result.isNull())
                    result = renderFallback (latex, QStringLiteral ("Error al compilar LaTeX"));
            }
        }
    }

    if (result.isNull())
        result = renderFallback (latex, QString());

    {
        QMutexLocker lk (&cacheMutex());
        if (cache().size() > 100) {
            auto it = cache().begin();
            for (int i = 0; i < 50 && it != cache().end(); ++i)
                it = cache().erase (it);
        }
        cache().insert (key, result);
    }
    return result;
}