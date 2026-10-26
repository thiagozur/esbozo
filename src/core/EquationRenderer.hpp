#pragma once

#include <QString>
#include <QStringList>
#include <QPixmap>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QFont>
#include <QFontMetricsF>
#include <QColor>
#include <QHash>
#include <QSet>
#include <functional>
#include <vector>
#include <algorithm>
#include <cmath>

namespace EquationRenderer {

namespace detail {

using PaintFn = std::function<void (QPainter&, double, double)>;

struct Box {
    double w = 0.0, asc = 0.0, desc = 0.0;
    PaintFn paint;
    double h() const { return asc + desc; }
};

enum class Kind { Ord, Bin, Rel, Open, Close, Punct };
enum class FontKind { Italic, Roman, Bold };

struct Atom {
    Box box;
    Kind kind = Kind::Ord;
    bool limits = false;
};

inline QColor ink() { return QColor (17, 17, 17); }
inline QString U (char32_t cp) { return QString::fromUcs4 (&cp, 1); }

inline QFont makeFont (double px, FontKind kind) {
    QFont f;
    f.setFamilies (QStringList { "Times New Roman", "Cambria Math", "STIX Two Text", "Liberation Serif", "Times", "serif" });
    f.setPixelSize (qMax (6, qRound (px)));
    f.setItalic (kind == FontKind::Italic);
    f.setBold (kind == FontKind::Bold);
    return f;
}

inline Box textBox (const QString& s, double px, FontKind kind, QColor color = QColor (17, 17, 17)) {
    const QFont f = makeFont (px, kind);
    const QFontMetricsF fm (f);
    const QRectF r = fm.tightBoundingRect (s);
    Box b;
    b.w = qMax (fm.horizontalAdvance (s), r.right());
    b.asc = qMax (0.0, -r.top());
    b.desc = qMax (0.0, r.bottom());
    b.paint = [f, s, color] (QPainter& p, double x, double y) {
        p.setFont (f);
        p.setPen (color);
        p.drawText (QPointF (x, y), s);
    };
    return b;
}

inline Box spaceBox (double w) {
    Box b;
    b.w = w;
    return b;
}

inline Box padBox (const Box& b, double left, double right) {
    Box o;
    o.w = b.w + left + right;
    o.asc = b.asc;
    o.desc = b.desc;
    o.paint = [b, left] (QPainter& p, double x, double y) {
        if (b.paint) b.paint (p, x + left, y);
    };
    return o;
}

inline Box hbox (const std::vector<Box>& items) {
    Box o;
    for (const Box& b : items) {
        o.w += b.w;
        o.asc = qMax (o.asc, b.asc);
        o.desc = qMax (o.desc, b.desc);
    }
    o.paint = [items] (QPainter& p, double x, double y) {
        double cx = x;
        for (const Box& b : items) {
            if (b.paint) b.paint (p, cx, y);
            cx += b.w;
        }
    };
    return o;
}

inline Box bigGlyph (const QString& g, double sz, double scale, bool italic) {
    const QFont f = makeFont (sz * scale, italic ? FontKind::Italic : FontKind::Roman);
    const QFontMetricsF fm (f);
    const QRectF r = fm.tightBoundingRect (g);
    const double axis = 0.27 * sz;
    const double centerRel = (r.top() + r.bottom()) / 2.0;
    const double dy = -axis - centerRel;
    Box b;
    b.w = qMax (fm.horizontalAdvance (g), r.right());
    b.asc = qMax (0.0, axis + r.height() / 2.0);
    b.desc = qMax (0.0, r.height() / 2.0 - axis);
    b.paint = [f, g, dy] (QPainter& p, double x, double y) {
        p.setFont (f);
        p.setPen (ink());
        p.drawText (QPointF (x, y + dy), g);
    };
    return b;
}

inline Box fracBox (const Box& num, const Box& den, double sz, bool rule) {
    const double axis = 0.27 * sz;
    const double thick = qMax (1.0, sz * 0.055);
    const double gap = 0.16 * sz;
    const double padX = 0.12 * sz;
    Box o;
    o.w = qMax (num.w, den.w) + 2 * padX;
    o.asc = axis + thick / 2 + gap + num.h();
    o.desc = qMax (0.0, -axis + thick / 2 + gap + den.h());
    const double W = o.w;
    o.paint = [=] (QPainter& p, double x, double y) {
        const double lineY = y - axis;
        if (num.paint) num.paint (p, x + (W - num.w) / 2, lineY - thick / 2 - gap - num.desc);
        if (den.paint) den.paint (p, x + (W - den.w) / 2, lineY + thick / 2 + gap + den.asc);
        if (rule) {
            p.setPen (QPen (ink(), thick, Qt::SolidLine, Qt::FlatCap));
            p.drawLine (QPointF (x + padX * 0.4, lineY), QPointF (x + W - padX * 0.4, lineY));
        }
    };
    return o;
}

inline Box sqrtBox (const Box& rad, const Box& idx, bool hasIdx, double sz) {
    const double thick = qMax (1.0, sz * 0.055);
    const double gap = 0.12 * sz;
    const double signW = 0.65 * sz;
    const double top = rad.asc + gap + thick / 2;
    const double bottom = rad.desc;
    const double h = top + bottom;
    const double extra = hasIdx ? qMax (0.0, idx.w - signW * 0.45) : 0.0;

    Box o;
    o.w = extra + signW + rad.w + 0.15 * sz;
    o.asc = top + thick / 2;
    o.desc = bottom;
    const double idxOffset = bottom - 0.55 * h - idx.desc;
    if (hasIdx) o.asc = qMax (o.asc, -(idxOffset - idx.asc));

    o.paint = [=] (QPainter& p, double x, double y) {
        const double sx = x + extra;
        QPainterPath path;
        path.moveTo (sx, y + bottom - 0.38 * h);
        path.lineTo (sx + 0.18 * signW, y + bottom - 0.46 * h);
        path.lineTo (sx + 0.45 * signW, y + bottom);
        path.lineTo (sx + signW, y - top);
        path.lineTo (sx + signW + rad.w + 0.1 * sz, y - top);
        p.setPen (QPen (ink(), thick, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
        p.setBrush (Qt::NoBrush);
        p.drawPath (path);
        if (rad.paint) rad.paint (p, sx + signW + 0.03 * sz, y);
        if (hasIdx && idx.paint) idx.paint (p, x + (extra + signW * 0.45 - idx.w), y + idxOffset);
    };
    return o;
}

inline Box scriptsBox (const Box& base, const Box& sup, bool hasSup, const Box& sub, bool hasSub, double sz) {
    const double kern = 0.03 * sz;
    double supShift = 0.0, subShift = 0.0;
    if (hasSup) supShift = std::max ({ 0.42 * sz, base.asc - 0.28 * sz, sup.desc + 0.12 * sz });
    if (hasSub) subShift = qMax (0.2 * sz, base.desc + 0.08 * sz);
    if (hasSup && hasSub) {
        const double deficit = 0.15 * sz - ((supShift - sup.desc) - (sub.asc - subShift));
        if (deficit > 0) subShift += deficit;
    }

    Box o;
    o.w = base.w + kern + qMax (hasSup ? sup.w : 0.0, hasSub ? sub.w : 0.0);
    o.asc = qMax (base.asc, hasSup ? supShift + sup.asc : 0.0);
    o.desc = qMax (base.desc, hasSub ? subShift + sub.desc : 0.0);
    o.paint = [=] (QPainter& p, double x, double y) {
        if (base.paint) base.paint (p, x, y);
        const double sx = x + base.w + kern;
        if (hasSup && sup.paint) sup.paint (p, sx, y - supShift);
        if (hasSub && sub.paint) sub.paint (p, sx, y + subShift);
    };
    return o;
}

inline Box limitsBox (const Box& base, const Box& top, bool hasTop, const Box& bot, bool hasBot, double sz) {
    const double gap = 0.1 * sz;
    Box o;
    o.w = std::max ({ base.w, hasTop ? top.w : 0.0, hasBot ? bot.w : 0.0 });
    o.asc = base.asc + (hasTop ? gap + top.h() : 0.0);
    o.desc = base.desc + (hasBot ? gap + bot.h() : 0.0);
    const double W = o.w;
    o.paint = [=] (QPainter& p, double x, double y) {
        if (base.paint) base.paint (p, x + (W - base.w) / 2, y);
        if (hasTop && top.paint) top.paint (p, x + (W - top.w) / 2, y - base.asc - gap - top.desc);
        if (hasBot && bot.paint) bot.paint (p, x + (W - bot.w) / 2, y + base.desc + gap + bot.asc);
    };
    return o;
}

inline Box accentBox (const QString& kind, const Box& b, double sz) {
    const double thick = qMax (1.0, sz * 0.055);
    Box o;
    o.w = b.w;
    o.asc = b.asc;
    o.desc = b.desc;

    if (kind == "underline") {
        const double gap = 0.12 * sz;
        o.desc = b.desc + gap + thick;
        o.paint = [=] (QPainter& p, double x, double y) {
            if (b.paint) b.paint (p, x, y);
            p.setPen (QPen (ink(), thick, Qt::SolidLine, Qt::FlatCap));
            p.drawLine (QPointF (x, y + b.desc + gap), QPointF (x + b.w, y + b.desc + gap));
        };
    } else if (kind == "overline" || kind == "bar") {
        const double gap = 0.14 * sz;
        o.asc = b.asc + gap + thick;
        o.paint = [=] (QPainter& p, double x, double y) {
            if (b.paint) b.paint (p, x, y);
            p.setPen (QPen (ink(), thick, Qt::SolidLine, Qt::FlatCap));
            p.drawLine (QPointF (x, y - b.asc - gap), QPointF (x + b.w, y - b.asc - gap));
        };
    } else {
        const double lift = 0.12 * sz;
        o.asc = b.asc + lift + 0.22 * sz;
        o.paint = [=] (QPainter& p, double x, double y) {
            if (b.paint) b.paint (p, x, y);
            const double cx = x + b.w / 2.0;
            const double base = y - b.asc - lift;
            p.setPen (QPen (ink(), thick, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            p.setBrush (Qt::NoBrush);
            QPainterPath path;
            if (kind == "hat" || kind == "widehat") {
                const double hw = qMax (0.2 * sz, b.w * 0.4);
                path.moveTo (cx - hw, base);
                path.lineTo (cx, base - 0.17 * sz);
                path.lineTo (cx + hw, base);
                p.drawPath (path);
            } else if (kind == "vec" || kind == "overrightarrow") {
                const double hw = qMax (0.22 * sz, b.w * 0.45);
                const double ly = base - 0.08 * sz;
                path.moveTo (cx - hw, ly);
                path.lineTo (cx + hw, ly);
                path.moveTo (cx + hw - 0.1 * sz, ly - 0.06 * sz);
                path.lineTo (cx + hw, ly);
                path.lineTo (cx + hw - 0.1 * sz, ly + 0.06 * sz);
                p.drawPath (path);
            } else if (kind == "dot" || kind == "ddot") {
                p.setPen (Qt::NoPen);
                p.setBrush (ink());
                const double r = thick * 1.1;
                if (kind == "dot") {
                    p.drawEllipse (QPointF (cx, base - r), r, r);
                } else {
                    p.drawEllipse (QPointF (cx - 0.1 * sz, base - r), r, r);
                    p.drawEllipse (QPointF (cx + 0.1 * sz, base - r), r, r);
                }
            } else {
                const double hw = qMax (0.2 * sz, b.w * 0.4);
                const double ly = base - 0.08 * sz;
                path.moveTo (cx - hw, ly + 0.03 * sz);
                path.cubicTo (cx - hw * 0.4, ly - 0.12 * sz, cx + hw * 0.4, ly + 0.12 * sz, cx + hw, ly - 0.03 * sz);
                p.drawPath (path);
            }
        };
    }
    return o;
}

inline Box delimBox (const QString& d, double cAsc, double cDesc, double sz) {
    Box o;
    if (d.isEmpty() || d == QLatin1String (".")) return o;

    int code = -1;
    bool mirror = false;
    if (d == "(") code = 0;
    else if (d == ")") { code = 0; mirror = true; }
    else if (d == "[") code = 1;
    else if (d == "]") { code = 1; mirror = true; }
    else if (d == "{") code = 2;
    else if (d == "}") { code = 2; mirror = true; }
    else if (d == U (0x27E8) || d == "<") code = 3;
    else if (d == U (0x27E9) || d == ">") { code = 3; mirror = true; }
    else if (d == "|") code = 4;
    else if (d == U (0x2016)) code = 5;

    if (code < 0) {
        Box g = textBox (d, sz, FontKind::Roman);
        return g;
    }

    const double h = cAsc + cDesc;
    const double thick = qMax (1.0, sz * 0.06);
    const double pad = 0.04 * sz;
    double w = 0.3 * sz;
    if (code == 0) w = 0.28 * sz + qMin (0.012 * h, 0.2 * sz);
    else if (code == 2) w = 0.4 * sz;
    else if (code == 4) w = 0.2 * sz;
    else if (code == 5) w = 0.3 * sz;

    o.w = w;
    o.asc = cAsc;
    o.desc = cDesc;
    o.paint = [=] (QPainter& p, double x, double y) {
        const double top = y - cAsc, bot = y + cDesc, mid = (top + bot) / 2.0;
        const double a = mirror ? x + pad : x + w - pad;
        const double b = mirror ? x + w - pad : x + pad;
        const double xm = (a + b) / 2.0;
        QPainterPath path;
        switch (code) {
            case 0: {
                const double cx = b + (b - a) * 0.35;
                path.moveTo (a, top);
                path.cubicTo (cx, top + 0.3 * h, cx, bot - 0.3 * h, a, bot);
                break;
            }
            case 1:
                path.moveTo (a, top);
                path.lineTo (b, top);
                path.lineTo (b, bot);
                path.lineTo (a, bot);
                break;
            case 2: {
                const double cr = qMin (h * 0.12, sz * 0.5);
                path.moveTo (a, top);
                path.quadTo (xm, top, xm, top + cr);
                path.lineTo (xm, mid - cr);
                path.quadTo (xm, mid, b, mid);
                path.quadTo (xm, mid, xm, mid + cr);
                path.lineTo (xm, bot - cr);
                path.quadTo (xm, bot, a, bot);
                break;
            }
            case 3:
                path.moveTo (a, top);
                path.lineTo (b, mid);
                path.lineTo (a, bot);
                break;
            case 4:
                path.moveTo (xm, top);
                path.lineTo (xm, bot);
                break;
            case 5:
                path.moveTo (xm - 0.06 * sz, top);
                path.lineTo (xm - 0.06 * sz, bot);
                path.moveTo (xm + 0.06 * sz, top);
                path.lineTo (xm + 0.06 * sz, bot);
                break;
        }
        p.setPen (QPen (ink(), thick, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
        p.setBrush (Qt::NoBrush);
        p.drawPath (path);
    };
    return o;
}

inline Box wrapDelims (const QString& d1, const Box& inner, const QString& d2, double sz) {
    const double axis = 0.27 * sz;
    const double half = qMax (inner.asc - axis, inner.desc + axis);
    const double cAsc = half + axis;
    const double cDesc = qMax (0.0, half - axis);
    return hbox ({ delimBox (d1, cAsc, cDesc, sz), inner, delimBox (d2, cAsc, cDesc, sz) });
}

enum class GridAlign { Center, Left, AlignedRL };

inline Box gridBox (const std::vector<std::vector<Box>>& rows, double sz, GridAlign mode) {
    const size_t nrows = rows.size();
    size_t ncols = 0;
    for (const auto& r : rows) ncols = qMax (ncols, r.size());

    std::vector<double> colW (ncols, 0.0), rowAsc (nrows, 0.7 * sz), rowDesc (nrows, 0.2 * sz);
    for (size_t r = 0; r < nrows; ++r) {
        for (size_t c = 0; c < rows[r].size(); ++c) {
            colW[c] = qMax (colW[c], rows[r][c].w);
            rowAsc[r] = qMax (rowAsc[r], rows[r][c].asc);
            rowDesc[r] = qMax (rowDesc[r], rows[r][c].desc);
        }
    }

    const double colGap = (mode == GridAlign::AlignedRL) ? 0.0 : 1.0 * sz;
    const double rowGap = 0.3 * sz;
    const double axis = 0.27 * sz;

    double totalW = colGap * (ncols > 0 ? ncols - 1 : 0);
    for (double w : colW) totalW += w;
    double totalH = rowGap * (nrows > 0 ? nrows - 1 : 0);
    for (size_t r = 0; r < nrows; ++r) totalH += rowAsc[r] + rowDesc[r];

    Box o;
    o.w = totalW;
    o.asc = totalH / 2 + axis;
    o.desc = qMax (0.0, totalH / 2 - axis);
    const double oAsc = o.asc;
    o.paint = [=] (QPainter& p, double x, double y) {
        double ry = y - oAsc;
        for (size_t r = 0; r < nrows; ++r) {
            const double baseline = ry + rowAsc[r];
            double cx = x;
            for (size_t c = 0; c < rows[r].size(); ++c) {
                const Box& cell = rows[r][c];
                double off = 0.0;
                if (mode == GridAlign::Center) off = (colW[c] - cell.w) / 2;
                else if (mode == GridAlign::AlignedRL && c % 2 == 0) off = colW[c] - cell.w;
                if (cell.paint) cell.paint (p, cx + off, baseline);
                cx += colW[c] + colGap;
            }
            ry += rowAsc[r] + rowDesc[r] + rowGap;
        }
    };
    return o;
}

inline Box stackLines (const std::vector<Box>& lines, double sz) {
    if (lines.size() == 1) return lines[0];
    const double gap = 0.35 * sz;
    double total = 0.0, maxW = 0.0;
    for (const Box& b : lines) {
        total += b.h();
        maxW = qMax (maxW, b.w);
    }
    total += gap * (lines.size() - 1);

    Box o;
    o.w = maxW;
    o.asc = lines.empty() ? 0.0 : lines[0].asc;
    o.desc = total - o.asc;
    const double W = maxW, oAsc = o.asc;
    o.paint = [=] (QPainter& p, double x, double y) {
        double top = y - oAsc;
        for (const Box& b : lines) {
            if (b.paint) b.paint (p, x + (W - b.w) / 2, top + b.asc);
            top += b.h() + gap;
        }
    };
    return o;
}

struct Sym {
    QString glyph;
    Kind kind;
    bool italic;
};

inline const QHash<QString, Sym>& symbols() {
    static const QHash<QString, Sym> table = [] {
        QHash<QString, Sym> m;
        auto add = [&m] (const char* name, char32_t cp, Kind k, bool italic = false) {
            m.insert (QString::fromLatin1 (name), Sym { U (cp), k, italic });
        };
        add ("alpha", 0x3B1, Kind::Ord, true);
        add ("beta", 0x3B2, Kind::Ord, true);
        add ("gamma", 0x3B3, Kind::Ord, true);
        add ("delta", 0x3B4, Kind::Ord, true);
        add ("epsilon", 0x3B5, Kind::Ord, true);
        add ("varepsilon", 0x3B5, Kind::Ord, true);
        add ("zeta", 0x3B6, Kind::Ord, true);
        add ("eta", 0x3B7, Kind::Ord, true);
        add ("theta", 0x3B8, Kind::Ord, true);
        add ("vartheta", 0x3D1, Kind::Ord, true);
        add ("iota", 0x3B9, Kind::Ord, true);
        add ("kappa", 0x3BA, Kind::Ord, true);
        add ("lambda", 0x3BB, Kind::Ord, true);
        add ("mu", 0x3BC, Kind::Ord, true);
        add ("nu", 0x3BD, Kind::Ord, true);
        add ("xi", 0x3BE, Kind::Ord, true);
        add ("pi", 0x3C0, Kind::Ord, true);
        add ("rho", 0x3C1, Kind::Ord, true);
        add ("sigma", 0x3C3, Kind::Ord, true);
        add ("tau", 0x3C4, Kind::Ord, true);
        add ("upsilon", 0x3C5, Kind::Ord, true);
        add ("phi", 0x3C6, Kind::Ord, true);
        add ("varphi", 0x3D5, Kind::Ord, true);
        add ("chi", 0x3C7, Kind::Ord, true);
        add ("psi", 0x3C8, Kind::Ord, true);
        add ("omega", 0x3C9, Kind::Ord, true);
        add ("Gamma", 0x393, Kind::Ord);
        add ("Delta", 0x394, Kind::Ord);
        add ("Theta", 0x398, Kind::Ord);
        add ("Lambda", 0x39B, Kind::Ord);
        add ("Xi", 0x39E, Kind::Ord);
        add ("Pi", 0x3A0, Kind::Ord);
        add ("Sigma", 0x3A3, Kind::Ord);
        add ("Upsilon", 0x3A5, Kind::Ord);
        add ("Phi", 0x3A6, Kind::Ord);
        add ("Psi", 0x3A8, Kind::Ord);
        add ("Omega", 0x3A9, Kind::Ord);
        add ("infty", 0x221E, Kind::Ord);
        add ("partial", 0x2202, Kind::Ord);
        add ("nabla", 0x2207, Kind::Ord);
        add ("hbar", 0x210F, Kind::Ord);
        add ("ell", 0x2113, Kind::Ord);
        add ("forall", 0x2200, Kind::Ord);
        add ("exists", 0x2203, Kind::Ord);
        add ("emptyset", 0x2205, Kind::Ord);
        add ("ldots", 0x2026, Kind::Ord);
        add ("dots", 0x2026, Kind::Ord);
        add ("cdots", 0x22EF, Kind::Ord);
        add ("vdots", 0x22EE, Kind::Ord);
        add ("ddots", 0x22F1, Kind::Ord);
        add ("angle", 0x2220, Kind::Ord);
        add ("prime", 0x2032, Kind::Ord);
        add ("degree", 0xB0, Kind::Ord);
        add ("neg", 0xAC, Kind::Ord);
        add ("aleph", 0x2135, Kind::Ord);
        add ("pm", 0xB1, Kind::Bin);
        add ("mp", 0x2213, Kind::Bin);
        add ("times", 0xD7, Kind::Bin);
        add ("cdot", 0xB7, Kind::Bin);
        add ("div", 0xF7, Kind::Bin);
        add ("ast", 0x2217, Kind::Bin);
        add ("circ", 0x2218, Kind::Bin);
        add ("oplus", 0x2295, Kind::Bin);
        add ("otimes", 0x2297, Kind::Bin);
        add ("cup", 0x222A, Kind::Bin);
        add ("cap", 0x2229, Kind::Bin);
        add ("wedge", 0x2227, Kind::Bin);
        add ("vee", 0x2228, Kind::Bin);
        add ("setminus", 0x2216, Kind::Bin);
        add ("star", 0x22C6, Kind::Bin);
        add ("bullet", 0x2219, Kind::Bin);
        add ("leq", 0x2264, Kind::Rel);
        add ("le", 0x2264, Kind::Rel);
        add ("geq", 0x2265, Kind::Rel);
        add ("ge", 0x2265, Kind::Rel);
        add ("neq", 0x2260, Kind::Rel);
        add ("ne", 0x2260, Kind::Rel);
        add ("approx", 0x2248, Kind::Rel);
        add ("equiv", 0x2261, Kind::Rel);
        add ("sim", 0x223C, Kind::Rel);
        add ("simeq", 0x2243, Kind::Rel);
        add ("propto", 0x221D, Kind::Rel);
        add ("ll", 0x226A, Kind::Rel);
        add ("gg", 0x226B, Kind::Rel);
        add ("in", 0x2208, Kind::Rel);
        add ("notin", 0x2209, Kind::Rel);
        add ("ni", 0x220B, Kind::Rel);
        add ("subset", 0x2282, Kind::Rel);
        add ("supset", 0x2283, Kind::Rel);
        add ("subseteq", 0x2286, Kind::Rel);
        add ("supseteq", 0x2287, Kind::Rel);
        add ("perp", 0x22A5, Kind::Rel);
        add ("parallel", 0x2225, Kind::Rel);
        add ("cong", 0x2245, Kind::Rel);
        add ("to", 0x2192, Kind::Rel);
        add ("rightarrow", 0x2192, Kind::Rel);
        add ("leftarrow", 0x2190, Kind::Rel);
        add ("leftrightarrow", 0x2194, Kind::Rel);
        add ("Rightarrow", 0x21D2, Kind::Rel);
        add ("Leftarrow", 0x21D0, Kind::Rel);
        add ("Leftrightarrow", 0x21D4, Kind::Rel);
        add ("implies", 0x21D2, Kind::Rel);
        add ("iff", 0x21D4, Kind::Rel);
        add ("mapsto", 0x21A6, Kind::Rel);
        add ("uparrow", 0x2191, Kind::Ord);
        add ("downarrow", 0x2193, Kind::Ord);
        return m;
    }();
    return table;
}

inline const QSet<QString>& functionNames() {
    static const QSet<QString> s = {
        "sin", "cos", "tan", "cot", "sec", "csc", "arcsin", "arccos", "arctan",
        "sinh", "cosh", "tanh", "coth", "log", "ln", "lg", "exp", "det", "dim",
        "ker", "deg", "gcd", "arg", "Pr", "hom", "mod"
    };
    return s;
}

inline const QSet<QString>& limitFunctions() {
    static const QSet<QString> s = { "lim", "max", "min", "sup", "inf", "limsup", "liminf" };
    return s;
}

class Parser {
public:
    explicit Parser (const QString& src) : m_s (src), m_n (src.size()) {}

    Box parseAll (double sz) {
        std::vector<Box> lines;
        while (true) {
            lines.push_back (parseRow (sz));
            if (atEnd()) break;
            if (startsWith ("\\\\")) m_pos += 2;
            else ++m_pos;
        }
        return stackLines (lines, sz);
    }

private:
    QString m_s;
    int m_n;
    int m_pos = 0;
    int m_groupDepth = 0, m_leftDepth = 0, m_envDepth = 0;

    static double scriptSize (double sz) { return qMax (8.0, sz * 0.72); }

    bool atEnd() const { return m_pos >= m_n; }
    QChar peek() const { return m_s[m_pos]; }
    void skipSpaces() { while (!atEnd() && peek().isSpace()) ++m_pos; }
    bool startsWith (const QString& t) const { return m_s.mid (m_pos, t.size()) == t; }
    static bool isAsciiLetter (QChar c) { return c.unicode() < 128 && c.isLetter(); }

    bool isCommandAt (const QString& name) const {
        if (atEnd() || peek() != u'\\') return false;
        if (m_s.mid (m_pos + 1, name.size()) != name) return false;
        const int e = m_pos + 1 + name.size();
        return e >= m_n || !isAsciiLetter (m_s[e]);
    }

    bool atStop() const {
        if (atEnd()) return true;
        const QChar c = peek();
        if (c == u'}' && m_groupDepth > 0) return true;
        if (c == u'&' && m_envDepth > 0) return true;
        if (c == u'\\') {
            if (m_pos + 1 < m_n && m_s[m_pos + 1] == u'\\') return true;
            if (m_leftDepth > 0 && isCommandAt ("right")) return true;
            if (m_envDepth > 0 && isCommandAt ("end")) return true;
        }
        return false;
    }

    Box parseRow (double sz) {
        std::vector<Box> items;
        Kind prev = Kind::Open;
        while (!atStop()) {
            if (peek().isSpace()) {
                ++m_pos;
                continue;
            }
            Atom a = parseAtom (sz);
            Box b = std::move (a.box);
            switch (a.kind) {
                case Kind::Rel:
                    b = padBox (b, 0.28 * sz, 0.28 * sz);
                    break;
                case Kind::Bin:
                    if (prev == Kind::Ord || prev == Kind::Close)
                        b = padBox (b, 0.22 * sz, 0.22 * sz);
                    break;
                case Kind::Punct:
                    b = padBox (b, 0.0, 0.17 * sz);
                    break;
                default:
                    break;
            }
            items.push_back (std::move (b));
            prev = a.kind;
        }
        return hbox (items);
    }

    Box parseGroupBody (double sz) {
        ++m_groupDepth;
        Box b = parseRow (sz);
        --m_groupDepth;
        if (!atEnd() && peek() == u'}') ++m_pos;
        return b;
    }

    Box parseArg (double sz) {
        skipSpaces();
        if (atEnd()) return Box();
        if (peek() == u'{') {
            ++m_pos;
            return parseGroupBody (sz);
        }
        if (peek() == u'}') return Box();
        return parsePrimary (sz).box;
    }

    QString readRawGroup() {
        skipSpaces();
        if (atEnd()) return QString();
        if (peek() != u'{') return QString (m_s[m_pos++]);
        ++m_pos;
        int depth = 1;
        QString out;
        while (!atEnd()) {
            const QChar c = m_s[m_pos++];
            if (c == u'{') ++depth;
            else if (c == u'}' && --depth == 0) break;
            out += c;
        }
        return out;
    }

    QString readDelimiter() {
        skipSpaces();
        if (atEnd()) return QStringLiteral (".");
        const QChar c = m_s[m_pos++];
        if (c != u'\\') return QString (c);
        if (atEnd()) return QStringLiteral (".");
        QString name;
        if (isAsciiLetter (peek())) {
            while (!atEnd() && isAsciiLetter (peek())) name += m_s[m_pos++];
        } else {
            name = QString (m_s[m_pos++]);
        }
        if (name == "{" || name == "lbrace") return "{";
        if (name == "}" || name == "rbrace") return "}";
        if (name == "|") return U (0x2016);
        if (name == "langle") return U (0x27E8);
        if (name == "rangle") return U (0x27E9);
        if (name == "lvert" || name == "rvert") return "|";
        return QStringLiteral (".");
    }

    Atom parseAtom (double sz) { return parseScripts (parsePrimary (sz), sz); }

    Atom parseScripts (Atom base, double sz) {
        const double ssz = scriptSize (sz);
        bool hasSup = false, hasSub = false;
        Box sup, sub;
        while (true) {
            skipSpaces();
            if (atEnd()) break;
            const QChar c = peek();
            if (c == u'^' && !hasSup) {
                ++m_pos;
                sup = parseArg (ssz);
                hasSup = true;
            } else if (c == u'_' && !hasSub) {
                ++m_pos;
                sub = parseArg (ssz);
                hasSub = true;
            } else {
                break;
            }
        }
        if (!hasSup && !hasSub) return base;

        if (base.limits) base.box = limitsBox (base.box, sup, hasSup, sub, hasSub, sz);
        else base.box = scriptsBox (base.box, sup, hasSup, sub, hasSub, sz);
        base.limits = false;
        return base;
    }

    Atom make (Box b, Kind k = Kind::Ord) {
        Atom a;
        a.box = std::move (b);
        a.kind = k;
        return a;
    }

    Atom parsePrimary (double sz) {
        if (atEnd()) return Atom();
        const QChar c = peek();

        if (c == u'{') {
            ++m_pos;
            return make (parseGroupBody (sz));
        }
        if (c == u'\\') {
            ++m_pos;
            return parseCommand (sz);
        }
        if (c == u'^' || c == u'_') return Atom();
        if (c == u'}' || c == u'&') {
            ++m_pos;
            return Atom();
        }

        ++m_pos;
        if (c.isLetter()) return make (textBox (QString (c), sz, FontKind::Italic));
        if (c.isDigit()) return make (textBox (QString (c), sz, FontKind::Roman));

        switch (c.unicode()) {
            case u'+': return make (textBox ("+", sz, FontKind::Roman), Kind::Bin);
            case u'-': return make (textBox (U (0x2212), sz, FontKind::Roman), Kind::Bin);
            case u'*': return make (textBox (U (0x2217), sz, FontKind::Roman), Kind::Bin);
            case u'=': return make (textBox ("=", sz, FontKind::Roman), Kind::Rel);
            case u'<': return make (textBox ("<", sz, FontKind::Roman), Kind::Rel);
            case u'>': return make (textBox (">", sz, FontKind::Roman), Kind::Rel);
            case u'(': case u'[': return make (textBox (QString (c), sz, FontKind::Roman), Kind::Open);
            case u')': case u']': return make (textBox (QString (c), sz, FontKind::Roman), Kind::Close);
            case u',': case u';': return make (textBox (QString (c), sz, FontKind::Roman), Kind::Punct);
            case u'\'': return make (textBox (U (0x2032), sz, FontKind::Roman));
            case u'~': return make (spaceBox (0.3 * sz));
            default: return make (textBox (QString (c), sz, FontKind::Roman));
        }
    }

    Atom parseCommand (double sz) {
        if (atEnd()) return Atom();
        QString name;
        if (isAsciiLetter (peek())) {
            while (!atEnd() && isAsciiLetter (peek())) name += m_s[m_pos++];
        } else {
            name = QString (m_s[m_pos++]);
        }

        if (name == ",") return make (spaceBox (0.17 * sz));
        if (name == ":") return make (spaceBox (0.22 * sz));
        if (name == ";" || name == " ") return make (spaceBox (0.28 * sz));
        if (name == "!") return Atom();
        if (name == "quad") return make (spaceBox (1.0 * sz));
        if (name == "qquad") return make (spaceBox (2.0 * sz));
        if (name == "\\") return Atom();

        if (name == "{" || name == "}" || name == "%" || name == "$" || name == "#" || name == "&" || name == "_")
            return make (textBox (name, sz, FontKind::Roman));
        if (name == "|") return make (textBox (U (0x2016), sz, FontKind::Roman));

        if (name == "frac" || name == "dfrac" || name == "tfrac" || name == "binom") {
            const double fsz = qMax (9.0, sz * (name == "tfrac" ? 0.7 : 0.88));
            Box num = parseArg (fsz);
            Box den = parseArg (fsz);
            if (name == "binom")
                return make (wrapDelims ("(", fracBox (num, den, sz, false), ")", sz));
            return make (fracBox (num, den, sz, true));
        }
        if (name == "sqrt") {
            skipSpaces();
            Box idx;
            bool hasIdx = false;
            if (!atEnd() && peek() == u'[') {
                const int close = m_s.indexOf (u']', m_pos);
                if (close > 0) {
                    Parser sub (m_s.mid (m_pos + 1, close - m_pos - 1));
                    idx = sub.parseAll (qMax (8.0, sz * 0.55));
                    hasIdx = true;
                    m_pos = close + 1;
                }
            }
            Box rad = parseArg (sz);
            return make (sqrtBox (rad, idx, hasIdx, sz));
        }
        if (name == "left") {
            const QString d1 = readDelimiter();
            ++m_leftDepth;
            Box inner = parseRow (sz);
            --m_leftDepth;
            QString d2 = ".";
            if (isCommandAt ("right")) {
                m_pos += 6;
                d2 = readDelimiter();
            }
            return make (wrapDelims (d1, inner, d2, sz));
        }
        if (name == "right") {
            readDelimiter();
            return Atom();
        }
        if (name == "begin") return parseEnvironment (sz);
        if (name == "end") {
            readRawGroup();
            return Atom();
        }

        if (name == "text" || name == "mathrm" || name == "textrm" || name == "operatorname")
            return make (textBox (readRawGroup(), sz, FontKind::Roman));
        if (name == "mathbf" || name == "textbf" || name == "boldsymbol" || name == "bm")
            return make (textBox (readRawGroup(), sz, FontKind::Bold));
        if (name == "mathit" || name == "textit" || name == "mathcal")
            return make (textBox (readRawGroup(), sz, FontKind::Italic));
        if (name == "mathbb") {
            const QString raw = readRawGroup();
            QString out;
            for (QChar ch : raw) {
                switch (ch.unicode()) {
                    case u'R': out += U (0x211D); break;
                    case u'N': out += U (0x2115); break;
                    case u'Z': out += U (0x2124); break;
                    case u'Q': out += U (0x211A); break;
                    case u'C': out += U (0x2102); break;
                    case u'P': out += U (0x2119); break;
                    default: out += ch;
                }
            }
            return make (textBox (out, sz, FontKind::Roman));
        }

        if (name == "overline" || name == "bar" || name == "underline" || name == "hat" || name == "widehat" ||
            name == "vec" || name == "overrightarrow" || name == "dot" || name == "ddot" ||
            name == "tilde" || name == "widetilde")
            return make (accentBox (name, parseArg (sz), sz));

        if (name == "int" || name == "iint" || name == "iiint" || name == "oint") {
            const char32_t cp = name == "int" ? 0x222B : name == "iint" ? 0x222C : name == "iiint" ? 0x222D : 0x222E;
            return make (padBox (bigGlyph (U (cp), sz, 1.7, false), 0.0, 0.1 * sz));
        }
        if (name == "sum" || name == "prod" || name == "coprod" || name == "bigcup" || name == "bigcap") {
            const char32_t cp = name == "sum" ? 0x2211 : name == "prod" ? 0x220F : name == "coprod" ? 0x2210
                              : name == "bigcup" ? 0x22C3 : 0x22C2;
            Atom a = make (bigGlyph (U (cp), sz, 1.35, false));
            a.limits = true;
            return a;
        }

        if (limitFunctions().contains (name)) {
            Atom a = make (textBox (name, sz, FontKind::Roman));
            a.limits = true;
            return a;
        }
        if (functionNames().contains (name))
            return make (padBox (textBox (name, sz, FontKind::Roman), 0.0, 0.17 * sz));

        if (name == "displaystyle" || name == "textstyle" || name == "scriptstyle" || name == "limits" ||
            name == "nolimits" || name == "not")
            return Atom();

        auto it = symbols().constFind (name);
        if (it != symbols().constEnd())
            return make (textBox (it->glyph, sz, it->italic ? FontKind::Italic : FontKind::Roman), it->kind);

        return make (textBox ("\\" + name, sz, FontKind::Roman, QColor (200, 30, 30)));
    }

    Atom parseEnvironment (double sz) {
        const QString env = readRawGroup();
        ++m_envDepth;
        std::vector<std::vector<Box>> rows;
        rows.emplace_back();
        while (!atEnd()) {
            rows.back().push_back (parseRow (sz));
            if (atEnd()) break;
            if (peek() == u'&') {
                ++m_pos;
            } else if (startsWith ("\\\\")) {
                m_pos += 2;
                rows.emplace_back();
            } else if (isCommandAt ("end")) {
                m_pos += 4;
                readRawGroup();
                break;
            } else {
                ++m_pos;
            }
        }
        --m_envDepth;

        while (rows.size() > 1) {
            bool empty = true;
            for (const Box& b : rows.back()) if (b.w > 0) empty = false;
            if (!empty) break;
            rows.pop_back();
        }

        GridAlign align = GridAlign::Center;
        if (env == "cases") align = GridAlign::Left;
        else if (env == "aligned" || env == "align" || env == "align*" || env == "split") align = GridAlign::AlignedRL;

        Box grid = gridBox (rows, sz, align);
        if (env == "pmatrix") return make (wrapDelims ("(", grid, ")", sz));
        if (env == "bmatrix") return make (wrapDelims ("[", grid, "]", sz));
        if (env == "Bmatrix") return make (wrapDelims ("{", grid, "}", sz));
        if (env == "vmatrix") return make (wrapDelims ("|", grid, "|", sz));
        if (env == "Vmatrix") return make (wrapDelims (U (0x2016), grid, U (0x2016), sz));
        if (env == "cases") return make (wrapDelims ("{", grid, ".", sz));
        return make (grid);
    }
};

struct Layout {
    Box box;
    double scale = 1.0;
};

constexpr double kBaseSize = 22.0;

inline double numberAreaWidth (int equationNumber) {
    if (equationNumber <= 0) return 0.0;
    QFont f ("Segoe UI");
    f.setPixelSize (15);
    return QFontMetricsF (f).horizontalAdvance (QString ("(%1)").arg (equationNumber)) + 24.0;
}

inline Layout layout (const QString& latex, int width, int equationNumber) {
    Layout L;
    Parser parser (latex);
    L.box = parser.parseAll (kBaseSize);
    const double avail = width - 2 * qMax (numberAreaWidth (equationNumber), 16.0);
    if (L.box.w > avail && L.box.w > 0)
        L.scale = qBound (0.4, avail / L.box.w, 1.0);
    return L;
}

}

inline int preferredHeight (const QString& latex, int width, int equationNumber = 0) {
    if (latex.trimmed().isEmpty()) return 50;
    const detail::Layout L = detail::layout (latex, qMax (100, width), equationNumber);
    return qMax (50, static_cast<int> (std::ceil (L.box.h() * L.scale)) + 28);
}

inline QPixmap render (const QString& latex, int width, int height, qreal dpr = 1.0, int equationNumber = 0) {
    const int w = qMax (100, width);
    const int h = qMax (50, height);
    QImage img (qRound (w * dpr), qRound (h * dpr), QImage::Format_ARGB32_Premultiplied);
    img.setDevicePixelRatio (dpr);
    img.fill (Qt::transparent);

    QPainter painter (&img);
    painter.setRenderHint (QPainter::Antialiasing, true);
    painter.setRenderHint (QPainter::TextAntialiasing, true);

    if (latex.trimmed().isEmpty()) {
        QFont f = detail::makeFont (16, detail::FontKind::Italic);
        painter.setFont (f);
        painter.setPen (QColor (150, 150, 150));
        painter.drawText (QRectF (0, 0, w, h), Qt::AlignCenter, "(ecuación vacía)");
    } else {
        const detail::Layout L = detail::layout (latex, w, equationNumber);
        const double bw = L.box.w * L.scale;
        const double bh = L.box.h() * L.scale;
        painter.save();
        painter.translate ((w - bw) / 2.0, (h - bh) / 2.0 + L.box.asc * L.scale);
        painter.scale (L.scale, L.scale);
        if (L.box.paint) L.box.paint (painter, 0.0, 0.0);
        painter.restore();
    }

    if (equationNumber > 0) {
        QFont numFont ("Segoe UI");
        numFont.setPixelSize (15);
        painter.setFont (numFont);
        painter.setPen (QColor (100, 100, 100));
        const QString numStr = QString ("(%1)").arg (equationNumber);
        const QFontMetricsF fm (numFont);
        painter.drawText (QPointF (w - fm.horizontalAdvance (numStr) - 16, (h + fm.ascent() - fm.descent()) / 2.0), numStr);
    }

    painter.end();
    return QPixmap::fromImage (img);
}

namespace detail {

constexpr double kVectorDesign = kBaseSize * 8.0;

struct VectorLayout {
    Box box;
    double k = 1.0;
};

inline VectorLayout vectorLayout (const QString& latex, double fontSize, double maxWidth) {
    VectorLayout L;
    Parser parser (latex);
    L.box = parser.parseAll (kVectorDesign);
    L.k = fontSize / kVectorDesign;
    if (L.box.w > 0 && L.box.w * L.k > maxWidth)
        L.k = qMax (L.k * 0.4, maxWidth / L.box.w);
    return L;
}

}

inline QSizeF measureScaled (const QString& latex, double fontSize, double maxWidth) {
    const detail::VectorLayout L = detail::vectorLayout (latex, fontSize, maxWidth);
    return QSizeF (L.box.w * L.k, L.box.h() * L.k);
}

inline void paintScaled (QPainter& p, const QString& latex, const QRectF& area, double fontSize) {
    const detail::VectorLayout L = detail::vectorLayout (latex, fontSize, area.width());
    const double bw = L.box.w * L.k;
    const double bh = L.box.h() * L.k;
    p.save();
    p.translate (area.x() + (area.width() - bw) / 2.0, area.y() + (area.height() - bh) / 2.0 + L.box.asc * L.k);
    p.scale (L.k, L.k);
    if (L.box.paint) L.box.paint (p, 0.0, 0.0);
    p.restore();
}

}