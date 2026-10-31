#pragma once

#include <QString>
#include <QVector>

#include "core/Document.hpp"
#include "core/GraphBlock.hpp"

class WizardHost {
public:
    virtual ~WizardHost() = default;

    virtual Document& document() = 0;
    virtual size_t activeIndex() const = 0;

    virtual void setPrompt (const QString& text) = 0;
    virtual void clearInput() = 0;
    virtual void refresh() = 0;
    virtual void discardActiveBlock() = 0;
    virtual void showMessage (const QString& text) = 0;
    virtual QString pickFile (const QString& title, const QString& nameFilter) = 0;
    virtual void exportAs (const QString& format) = 0;
};

class Wizard {
public:
    enum class State {
        Idle,
        GraphType,
        GraphSource,
        GraphXName,
        GraphXValues,
        GraphYName,
        GraphYValues,
        GraphXScale,
        GraphYScale,
        GraphDescription,
        ExportFormat,
        TableFile,
        TableHeader,
        TableRowNames,
        TableTitle
    };

    struct Progress {
        int step = 0;
        int total = 0;
    };

    explicit Wizard (WizardHost& host);

    bool active() const { return m_state != State::Idle; }
    State state() const { return m_state; }
    Progress progress() const;

    void reset();
    void startExport();
    void beginForActiveBlock();
    void submit (const QString& text);

private:
    void setState (State state, const QString& prompt);
    void pickImage();
    void pickGraphCsv();
    void pickTableCsv();
    void finishGraph (const QString& description);
    void finishTable (const QString& title);
    static QVector<double> parseCommaValues (const QString& str);
    static bool parseYesNo (const QString& text, bool& value);

    WizardHost& m_host;
    State m_state = State::Idle;

    GraphType m_graphType = GraphType::Line;
    GraphSource m_graphSource = GraphSource::Manual;
    QString m_xName, m_yName;
    QVector<double> m_xValues, m_yValues;
    QVector<DataSeries> m_ySeries;
    AxisScale m_xScale = AxisScale::Linear;
    AxisScale m_yScale = AxisScale::Linear;

    QString m_tableCsvPath;
    QVector<QStringList> m_tableData;
    bool m_tableHasHeader = true;
    bool m_tableHasRowNames = false;
};
