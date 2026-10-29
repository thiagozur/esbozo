#pragma once

#include <QFile>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <QVector>
#include <cmath>
#include <limits>

#include "core/GraphBlock.hpp"

namespace GraphCsv {

struct Data {
    QString xName;
    QVector<double> xValues;
    QVector<DataSeries> ySeries;
};

inline bool load (const QString& path, Data& out) {
    QFile file (path);
    if (!file.open (QIODevice::ReadOnly | QIODevice::Text))
        return false;

    QTextStream in (&file);
    if (in.atEnd())
        return false;

    const QRegularExpression sep ("[,;\\t]");
    const double nan = std::numeric_limits<double>::quiet_NaN();

    QStringList headers = in.readLine().split (sep);
    if (headers.size() < 2)
        return false;
    for (QString& h : headers)
        h = h.trimmed();

    Data data;
    data.xName = headers[0];

    for (int c = 1; c < headers.size(); ++c) {
        DataSeries s;
        s.name = headers[c].isEmpty() ? QString ("Serie %1").arg (c) : headers[c];
        data.ySeries.append (s);
    }

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty())
            continue;

        QStringList parts = line.split (sep);
        bool okX = false;
        double x = parts[0].trimmed().toDouble (&okX);
        if (!okX)
            continue;

        data.xValues.append (x);
        for (int c = 0; c < data.ySeries.size(); ++c) {
            bool ok = false;
            double y = (c + 1 < parts.size()) ? parts[c + 1].trimmed().toDouble (&ok) : nan;
            data.ySeries[c].values.append (ok ? y : nan);
        }
    }
    file.close();

    for (int c = data.ySeries.size() - 1; c >= 0; --c) {
        bool any = false;
        for (double v : data.ySeries[c].values) {
            if (std::isfinite (v)) {
                any = true;
                break;
            }
        }
        if (!any)
            data.ySeries.removeAt (c);
    }

    if (data.xValues.isEmpty() || data.ySeries.isEmpty())
        return false;

    out = data;
    return true;
}

}
