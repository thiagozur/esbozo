#include "ui/Wizard.hpp"

#include "core/TableBlock.hpp"
#include "core/TableRenderer.hpp"
#include "io/GraphCsv.hpp"

Wizard::Wizard (WizardHost& host)
    : m_host (host)
{
}

Wizard::Progress Wizard::progress() const {
    switch (m_state) {
        case State::GraphType: return { 1, 9 };
        case State::GraphSource: return { 2, 9 };
        case State::GraphXName: return { 3, 9 };
        case State::GraphXValues: return { 4, 9 };
        case State::GraphYName: return { 5, 9 };
        case State::GraphYValues: return { 6, 9 };
        case State::GraphXScale: return { 7, 9 };
        case State::GraphYScale: return { 8, 9 };
        case State::GraphDescription: return { 9, 9 };
        case State::TableFile: return { 1, 4 };
        case State::TableHeader: return { 2, 4 };
        case State::TableRowNames: return { 3, 4 };
        case State::TableTitle: return { 4, 4 };
        case State::ExportFormat: return { 1, 1 };
        case State::Idle: break;
    }
    return {};
}

void Wizard::reset() {
    m_state = State::Idle;
}

void Wizard::setState (State state, const QString& prompt) {
    m_state = state;
    m_host.setPrompt (prompt);
}

void Wizard::startExport() {
    setState (State::ExportFormat, "md (Markdown) o pdf (PDF)");
}

void Wizard::beginForActiveBlock() {
    Block* block = m_host.document().getBlock (m_host.activeIndex());
    if (!block)
        return;

    if (block->getType() == BlockType::Image && block->getContent().isEmpty()) {
        pickImage();
    } else if (block->getType() == BlockType::Graph && m_state == State::Idle) {
        setState (State::GraphType, "lin (línea) o sct (dispersión)");
    } else if (block->getType() == BlockType::Table && m_state == State::Idle) {
        m_state = State::TableFile;
        pickTableCsv();
    }
}

void Wizard::pickImage() {
    const QString path = m_host.pickFile ("Seleccionar Imagen", "Imágenes (*.png *.jpg *.jpeg)");
    if (path.isEmpty()) {
        m_host.discardActiveBlock();
        return;
    }
    if (Block* block = m_host.document().getBlock (m_host.activeIndex()))
        block->setContent (path);
    m_host.refresh();
}

void Wizard::pickGraphCsv() {
    const QString path = m_host.pickFile ("Seleccionar Archivo CSV", "Archivos CSV (*.csv)");
    if (path.isEmpty()) {
        m_state = State::Idle;
        m_host.discardActiveBlock();
        return;
    }

    GraphCsv::Data data;
    if (GraphCsv::load (path, data)) {
        m_xName = data.xName;
        m_xValues = data.xValues;
        m_ySeries = data.ySeries;
        setState (State::GraphXScale,
                  QString ("CSV cargado (%1 series Y). Escala eje X: 'lin' o 'log'").arg (m_ySeries.size()));
    } else {
        m_host.setPrompt ("Error al leer el CSV. Escribe 'can;;' para cancelar o reintenta.");
    }
}

void Wizard::pickTableCsv() {
    const QString path = m_host.pickFile ("Seleccionar Archivo CSV para Tabla", "Archivos CSV (*.csv)");
    if (path.isEmpty()) {
        m_state = State::Idle;
        m_host.discardActiveBlock();
        return;
    }

    bool ok = false;
    m_tableData = TableRenderer::loadCsvFile (path, &ok);
    if (ok) {
        m_tableCsvPath = path;
        setState (State::TableHeader, "columnas nombradas en la primera fila (s/n)");
    } else {
        m_state = State::Idle;
        m_host.discardActiveBlock();
        m_host.showMessage ("No se pudo leer el CSV");
    }
}

QVector<double> Wizard::parseCommaValues (const QString& str) {
    QVector<double> result;
    const QStringList parts = str.split (',');
    for (const QString& p : parts) {
        bool ok = false;
        double val = p.trimmed().toDouble (&ok);
        if (ok)
            result.append (val);
    }
    return result;
}

bool Wizard::parseYesNo (const QString& text, bool& value) {
    if (text == "s" || text == "sí" || text == "si") {
        value = true;
        return true;
    }
    if (text == "n" || text == "no") {
        value = false;
        return true;
    }
    return false;
}

void Wizard::finishGraph (const QString& description) {
    if (auto* g = dynamic_cast<GraphBlock*> (m_host.document().getBlock (m_host.activeIndex()))) {
        g->setGraphType (m_graphType);
        g->setGraphSource (m_graphSource);
        g->setSeriesX (m_xName, m_xValues);
        g->setSeriesYList (m_ySeries);
        g->setScales (m_xScale, m_yScale);
        g->setDescription (description);
    }
    m_state = State::Idle;
    m_host.refresh();
}

void Wizard::finishTable (const QString& title) {
    if (auto* t = dynamic_cast<TableBlock*> (m_host.document().getBlock (m_host.activeIndex()))) {
        t->setCsvPath (m_tableCsvPath);
        t->setHasHeaderRow (m_tableHasHeader);
        t->setHasRowNames (m_tableHasRowNames);
        t->setTitle (title);
        t->setContent (title);
        t->setData (m_tableData);
    }
    m_state = State::Idle;
    m_host.refresh();
}

void Wizard::submit (const QString& text) {
    if (m_state == State::Idle)
        return;

    const QString input = text.trimmed();

    if (input == "can;;") {
        const bool wasExport = (m_state == State::ExportFormat);
        m_state = State::Idle;
        if (wasExport)
            m_host.refresh();
        else
            m_host.discardActiveBlock();
        return;
    }

    if (m_state == State::ExportFormat) {
        if (input == "md" || input == "pdf") {
            m_state = State::Idle;
            m_host.refresh();
            m_host.exportAs (input);
        } else {
            m_host.clearInput();
        }
        return;
    }

    switch (m_state) {
        case State::GraphType:
            if (input == "lin") m_graphType = GraphType::Line;
            else if (input == "sct") m_graphType = GraphType::Scatter;
            else { m_host.clearInput(); return; }
            setState (State::GraphSource, "csv (archivo) o man (entrada manual)");
            break;

        case State::GraphSource:
            if (input == "csv") {
                m_graphSource = GraphSource::Csv;
                pickGraphCsv();
            } else if (input == "man") {
                m_graphSource = GraphSource::Manual;
                setState (State::GraphXName, "Nombre de la serie X");
            } else {
                m_host.clearInput();
            }
            break;

        case State::GraphXName:
            if (input.isEmpty()) { m_host.clearInput(); return; }
            m_xName = input;
            setState (State::GraphXValues, "Valores de X separados por comas");
            break;

        case State::GraphXValues:
            m_xValues = parseCommaValues (input);
            if (m_xValues.isEmpty()) { m_host.clearInput(); return; }
            setState (State::GraphYName, "Nombre de la serie Y");
            break;

        case State::GraphYName:
            if (input.isEmpty()) { m_host.clearInput(); return; }
            m_yName = input;
            setState (State::GraphYValues, "Valores de Y separados por comas");
            break;

        case State::GraphYValues:
            m_yValues = parseCommaValues (input);
            if (m_yValues.isEmpty() || m_yValues.size() != m_xValues.size()) { m_host.clearInput(); return; }
            m_ySeries = { DataSeries { m_yName, m_yValues } };
            setState (State::GraphXScale, "lin (eje x lineal) o log (eje x logarítmico)");
            break;

        case State::GraphXScale:
            if (input == "lin") m_xScale = AxisScale::Linear;
            else if (input == "log") m_xScale = AxisScale::Log;
            else { m_host.clearInput(); return; }
            setState (State::GraphYScale, "lin (eje y lineal) o log (eje y logarítmico)");
            break;

        case State::GraphYScale:
            if (input == "lin") m_yScale = AxisScale::Linear;
            else if (input == "log") m_yScale = AxisScale::Log;
            else { m_host.clearInput(); return; }
            setState (State::GraphDescription, "descripción de la figura");
            break;

        case State::GraphDescription:
            finishGraph (input);
            break;

        case State::TableHeader:
            if (!parseYesNo (input, m_tableHasHeader)) { m_host.clearInput(); return; }
            setState (State::TableRowNames, "filas nombradas en la primera columna (s/n)");
            break;

        case State::TableRowNames:
            if (!parseYesNo (input, m_tableHasRowNames)) { m_host.clearInput(); return; }
            setState (State::TableTitle, "título de la tabla");
            break;

        case State::TableTitle:
            finishTable (input);
            break;

        default:
            break;
    }
}
