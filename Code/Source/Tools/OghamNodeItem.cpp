/*
 * Copyright (c) 2026 Heathen Engineering Limited
 * Irish Registered Company #556277
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "OghamNodeItem.h"
#include "OghamPaintUtils.h"

#include <QCursor>
#include <QFont>
#include <QFontMetrics>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QLinearGradient>
#include <QMenu>
#include <QtMath>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QStyleOptionGraphicsItem>

namespace FoundationOgham
{
    bool OghamNodeItem::s_labelsExpanded = false;

    // ── Colours ────────────────────────────────────────────────────────────────
    static const QColor kColBodyBg        { 0x25, 0x25, 0x25 };
    static const QColor kColSectionHdrBg  { 0x1e, 0x1e, 0x1e };
    static const QColor kColBorderNorm    { 0x40, 0x40, 0x40 };
    static const QColor kColBorderSelTop  { 0xFF, 0x9C, 0x00 };
    static const QColor kColBorderSelBot  { 0xE6, 0x51, 0x00 };
    static const QColor kColSectionText   { 0x80, 0x80, 0x80 };
    static const QColor kColFieldBg       { 0x33, 0x33, 0x33 };
    static const QColor kColFieldText     { 0xe0, 0xe0, 0xe0 };
    static const QColor kColFieldLabel    { 0x88, 0xbb, 0xdd };  // tint for "Label:" prefix
    static const QColor kColOperationText { 0x68, 0x68, 0x68 };
    static const QColor kColOptionText    { 0xa0, 0xa0, 0xa0 };
    static const QColor kColPinIn         { 0x6a, 0xb0, 0xd0 };
    static const QColor kColPinOut        { 0xd0, 0x90, 0x4a };
    static const QColor kColBtnAdd        { 0x28, 0x68, 0x28 };
    static const QColor kColBtnRemove     { 0x78, 0x28, 0x28 };
    static const QColor kColBtnText       { 0xe8, 0xe8, 0xe8 };

    static constexpr qreal kNodeCornerRadius = 6.0;
    static constexpr qreal kBtnSize         = 14.0;
    static constexpr qreal kDividerInset    = 2.0;  ///< horizontal dividers stop short of the border

    // ── Construction ──────────────────────────────────────────────────────────

    OghamNodeItem::OghamNodeItem(int fileIdx, int entryIdx,
                                 const QString&     tag,
                                 bool               tagInRegistry,
                                 const QStringList& operationLabels,
                                 const QStringList& dataKeyIds,
                                 const QStringList& dataValues,
                                 const QStringList& optionLabels,
                                 const LabelData&   assignedLabels,
                                 const QColor&      highlightColor,
                                 QColor             headerColor,
                                 QGraphicsItem*     parent)
        : QGraphicsObject(parent)
        , m_fileIdx(fileIdx)
        , m_entryIdx(entryIdx)
        , m_tag(tag)
        , m_tagInRegistry(tagInRegistry)
        , m_operationLabels(operationLabels)
        , m_dataKeyIds(dataKeyIds)
        , m_dataKeys(dataValues)
        , m_optionLabels(optionLabels)
        , m_assignedLabels(assignedLabels)
        , m_highlightColor(highlightColor)
        , m_headerColor(headerColor)
        , m_outputConnected(optionLabels.size(), false)
    {
        setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
        setAcceptHoverEvents(true);
        setCacheMode(DeviceCoordinateCache);
        setZValue(1.0);
    }

    // ── Connection state ──────────────────────────────────────────────────────

    void OghamNodeItem::setInputConnected(bool connected)
    {
        if (m_inputConnected != connected)
        {
            m_inputConnected = connected;
            update();
        }
    }

    void OghamNodeItem::setOutputConnected(int optIdx, bool connected)
    {
        if (optIdx < 0 || optIdx >= m_outputConnected.size()) return;
        if (m_outputConnected[optIdx] != connected)
        {
            m_outputConnected[optIdx] = connected;
            update();
        }
    }

    // ── Field text helpers ────────────────────────────────────────────────────

    QString OghamNodeItem::stripHtmlLinks(const QString& raw)
    {
        static const QRegularExpression rx(
            QStringLiteral("<a\\b[^>]*>(.*?)</a>"),
            QRegularExpression::DotMatchesEverythingOption);
        QString r = raw;
        r.replace(rx, QStringLiteral("\\1"));
        return r;
    }

    QString OghamNodeItem::buildFieldDisplayText(int i) const
    {
        const QString val = stripHtmlLinks(m_dataKeys.value(i));

        if (m_dataKeys.size() > 1 && i < m_dataKeyIds.size())
        {
            const QString& keyId = m_dataKeyIds.value(i);
            const int dot = keyId.lastIndexOf(QLatin1Char('.'));
            const QString label = (dot >= 0) ? keyId.mid(dot + 1) : keyId;
            if (!label.isEmpty())
                return label + QStringLiteral(":\n") + val;
        }
        return val;
    }

    qreal OghamNodeItem::fieldHeightAt(int i) const
    {
        QFont f;
        f.setPointSizeF(7.5);
        QFontMetrics fm(f);

        const int availW = static_cast<int>(kNodeWidth - kBodyPadding * 2.0 - 12.0);
        const QString text = buildFieldDisplayText(i);

        const QRect r = fm.boundingRect(
            QRect(0, 0, availW, 10000),
            Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
            text);

        return qMax(kFieldMinHeight, static_cast<qreal>(r.height()) + kFieldVPadding * 2.0 + 4.0);
    }

    qreal OghamNodeItem::totalFieldsHeight() const
    {
        qreal h = 0.0;
        for (int i = 0; i < m_dataKeys.size(); ++i)
            h += fieldHeightAt(i);
        return h;
    }

    qreal OghamNodeItem::fieldTopY(int i) const
    {
        qreal y = fieldsContentTopY();
        for (int j = 0; j < i; ++j)
            y += fieldHeightAt(j);
        return y;
    }

    // ── Label layout ──────────────────────────────────────────────────────────

    qreal OghamNodeItem::labelSectionHeight() const
    {
        if (m_assignedLabels.isEmpty()) return 0.0;

        // Compute pill dimensions
        const qreal pillH  = s_labelsExpanded ? 18.0 : 10.0;
        const qreal pillGX = 3.0;
        const qreal pillGY = 3.0;
        const qreal maxW   = kNodeWidth - kBodyPadding * 2.0;

        QFont f;
        f.setPointSizeF(7.0);
        QFontMetrics fm(f);

        qreal x = 0.0;
        int   rows = 1;
        for (const auto& lbl : m_assignedLabels)
        {
            const qreal pw = s_labelsExpanded
                ? qMax(30.0, static_cast<qreal>(fm.horizontalAdvance(lbl.second)) + 14.0)
                : 20.0;
            if (x > 0.0 && x + pw > maxW) { ++rows; x = 0.0; }
            x += pw + pillGX;
        }
        return kBodyPadding
             + static_cast<qreal>(rows) * pillH
             + static_cast<qreal>(rows - 1) * pillGY
             + kBodyPadding;
    }

    qreal OghamNodeItem::inputPinLocalY() const
    {
        return kHeaderHeight + labelSectionHeight() + kInputPinRowHeight * 0.5;
    }

    // ── Layout helpers ─────────────────────────────────────────────────────────

    qreal OghamNodeItem::opContentTopY() const
    {
        return kHeaderHeight + labelSectionHeight() + kInputPinRowHeight + kSectionHeaderHeight;
    }

    qreal OghamNodeItem::fieldsHdrTopY() const
    {
        return opContentTopY()
             + (m_operationsExpanded ? m_operationLabels.size() * kRowHeight : 0.0);
    }

    qreal OghamNodeItem::fieldsContentTopY() const
    {
        return fieldsHdrTopY() + kSectionHeaderHeight;
    }

    qreal OghamNodeItem::choicesHdrTopY() const
    {
        return fieldsContentTopY()
             + (m_fieldsExpanded ? totalFieldsHeight() : 0.0);
    }

    qreal OghamNodeItem::choicesContentTopY() const
    {
        return choicesHdrTopY() + kSectionHeaderHeight;
    }

    qreal OghamNodeItem::bodyHeight() const
    {
        const qreal opH      = m_operationsExpanded ? m_operationLabels.size() * kRowHeight : 0.0;
        const qreal fieldsH  = m_fieldsExpanded     ? totalFieldsHeight()                   : 0.0;
        const qreal choicesH = m_choicesExpanded    ? m_optionLabels.size()    * kRowHeight : 0.0;
        return labelSectionHeight() + kInputPinRowHeight + kSectionHeaderHeight * 3.0 + opH + fieldsH + choicesH;
    }

    // ── Button rects ──────────────────────────────────────────────────────────

    QRectF OghamNodeItem::addBtnRect(int section) const
    {
        qreal hdrY = (section == 0) ? opContentTopY() - kSectionHeaderHeight
                   : (section == 1) ? fieldsHdrTopY()
                   : choicesHdrTopY();
        return QRectF(kNodeWidth - kBtnSize - 4.0,
                      hdrY + (kSectionHeaderHeight - kBtnSize) * 0.5,
                      kBtnSize, kBtnSize);
    }

    QRectF OghamNodeItem::removeBtnRect(int section, int row) const
    {
        qreal rowTopY;
        qreal rowH;
        if (section == 0)
        {
            rowTopY = opContentTopY() + row * kRowHeight;
            rowH    = kRowHeight;
        }
        else if (section == 1)
        {
            rowTopY = fieldTopY(row);
            rowH    = fieldHeightAt(row);
        }
        else
        {
            rowTopY = choicesContentTopY() + row * kRowHeight;
            rowH    = kRowHeight;
        }

        // Choices rows have the output triangle at kNodeWidth; put button further left
        const qreal rightX = (section == 2)
            ? kNodeWidth - kPinS * 2.0 - kBtnSize - 6.0
            : kNodeWidth - kBtnSize - 4.0;

        return QRectF(rightX, rowTopY + (rowH - kBtnSize) * 0.5, kBtnSize, kBtnSize);
    }

    QRectF OghamNodeItem::reorderBtnRect(int section, int row) const
    {
        const QRectF rem = removeBtnRect(section, row);
        return QRectF(rem.left() - kBtnSize - 2.0, rem.top(), kBtnSize, kBtnSize);
    }

    // ── Geometry ──────────────────────────────────────────────────────────────

    QRectF OghamNodeItem::boundingRect() const
    {
        const qreal totalH = kHeaderHeight + bodyHeight();
        return QRectF(-kPinS - 1.0, -1.0,
                      kNodeWidth + kPinS * 2.0 + 2.0,
                      totalH + 2.0);
    }

    QPointF OghamNodeItem::inputPinScenePos() const
    {
        return mapToScene(QPointF(0.0, inputPinLocalY()));
    }

    QPointF OghamNodeItem::outputPinScenePos(int i) const
    {
        qreal y;
        if (m_choicesExpanded)
            y = choicesContentTopY() + i * kRowHeight + kRowHeight * 0.5;
        else
            y = choicesHdrTopY() + kSectionHeaderHeight * 0.5;
        // Return the tip of the output triangle (extends beyond node right edge)
        return mapToScene(QPointF(kNodeWidth + kPinS, y));
    }

    int OghamNodeItem::outputPinAt(QPointF localPos) const
    {
        if (!m_choicesExpanded) return -1;
        constexpr qreal kHit = kPinS + 4.0;
        const qreal contentY = choicesContentTopY();
        for (int i = 0; i < m_optionLabels.size(); ++i)
        {
            const QPointF pin(kNodeWidth, contentY + i * kRowHeight + kRowHeight * 0.5);
            const qreal dx = localPos.x() - pin.x();
            const qreal dy = localPos.y() - pin.y();
            if (dx * dx + dy * dy <= kHit * kHit)
                return i;
        }
        return -1;
    }

    // ── Section toggle ─────────────────────────────────────────────────────────

    QRectF OghamNodeItem::tagStatusRect() const
    {
        constexpr qreal kDotSz = 10.0;
        return QRectF(kNodeWidth - kDotSz - 6.0,
                      (kHeaderHeight - kDotSz) * 0.5,
                      kDotSz, kDotSz);
    }

    int OghamNodeItem::sectionHeaderAt(QPointF localPos) const
    {
        const qreal y = localPos.y();
        const qreal opHdrY = opContentTopY() - kSectionHeaderHeight;
        if (y >= opHdrY && y < opHdrY + kSectionHeaderHeight)
            return 0;
        const qreal fTop = fieldsHdrTopY();
        if (y >= fTop && y < fTop + kSectionHeaderHeight)
            return 1;
        const qreal cTop = choicesHdrTopY();
        if (y >= cTop && y < cTop + kSectionHeaderHeight)
            return 2;
        return -1;
    }

    void OghamNodeItem::toggleSection(int sectionIdx)
    {
        prepareGeometryChange();
        switch (sectionIdx)
        {
        case 0: m_operationsExpanded = !m_operationsExpanded; break;
        case 1: m_fieldsExpanded     = !m_fieldsExpanded;     break;
        case 2: m_choicesExpanded    = !m_choicesExpanded;    break;
        default: return;
        }
        update();
        emit layoutChanged(m_fileIdx, m_entryIdx);
    }

    QPair<int,int> OghamNodeItem::rowContentAt(QPointF localPos) const
    {
        // Operations rows (fixed height)
        if (m_operationsExpanded)
        {
            const qreal cTop = opContentTopY();
            const qreal cBot = cTop + m_operationLabels.size() * kRowHeight;
            if (localPos.y() >= cTop && localPos.y() < cBot)
            {
                const int row = static_cast<int>((localPos.y() - cTop) / kRowHeight);
                if (!removeBtnRect(0, row).contains(localPos)
                    && !(m_operationLabels.size() > 1 && reorderBtnRect(0, row).contains(localPos)))
                    return {0, row};
            }
        }

        // Fields rows (variable height)
        if (m_fieldsExpanded)
        {
            qreal y = fieldsContentTopY();
            for (int i = 0; i < m_dataKeys.size(); ++i)
            {
                const qreal fh = fieldHeightAt(i);
                if (localPos.y() >= y && localPos.y() < y + fh)
                {
                    if (!removeBtnRect(1, i).contains(localPos)
                        && !(m_dataKeys.size() > 1 && reorderBtnRect(1, i).contains(localPos)))
                        return {1, i};
                    break;
                }
                y += fh;
            }
        }

        // Choices rows (fixed height)
        if (m_choicesExpanded)
        {
            const qreal cTop = choicesContentTopY();
            const qreal cBot = cTop + m_optionLabels.size() * kRowHeight;
            if (localPos.y() >= cTop && localPos.y() < cBot)
            {
                const int row = static_cast<int>((localPos.y() - cTop) / kRowHeight);
                if (!removeBtnRect(2, row).contains(localPos)
                    && !(m_optionLabels.size() > 1 && reorderBtnRect(2, row).contains(localPos)))
                    return {2, row};
            }
        }

        return {-1, -1};
    }

    // ── Hover ─────────────────────────────────────────────────────────────────

    void OghamNodeItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
    {
        const QPointF pos = event->pos();
        int newSection = -1;
        int newRow     = -1;

        const int hdr = sectionHeaderAt(pos);
        if (hdr >= 0)
        {
            newSection = hdr;
            newRow     = -1;
        }
        else if (m_operationsExpanded)
        {
            const qreal cTop = opContentTopY();
            const qreal cBot = cTop + m_operationLabels.size() * kRowHeight;
            if (pos.y() >= cTop && pos.y() < cBot)
            {
                newSection = 0;
                newRow     = static_cast<int>((pos.y() - cTop) / kRowHeight);
            }
        }

        if (newSection < 0 && m_fieldsExpanded)
        {
            qreal y = fieldsContentTopY();
            for (int i = 0; i < m_dataKeys.size(); ++i)
            {
                const qreal fh = fieldHeightAt(i);
                if (pos.y() >= y && pos.y() < y + fh)
                {
                    newSection = 1;
                    newRow     = i;
                    break;
                }
                y += fh;
            }
        }

        if (newSection < 0 && m_choicesExpanded)
        {
            const qreal cTop = choicesContentTopY();
            const qreal cBot = cTop + m_optionLabels.size() * kRowHeight;
            if (pos.y() >= cTop && pos.y() < cBot)
            {
                newSection = 2;
                newRow     = static_cast<int>((pos.y() - cTop) / kRowHeight);
            }
        }

        if (newSection != m_hoveredSection || newRow != m_hoveredRow)
        {
            m_hoveredSection = newSection;
            m_hoveredRow     = newRow;
            update();
        }

        QGraphicsObject::hoverMoveEvent(event);
    }

    void OghamNodeItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
    {
        if (m_hoveredSection != -1 || m_hoveredRow != -1)
        {
            m_hoveredSection = -1;
            m_hoveredRow     = -1;
            update();
        }
        QGraphicsObject::hoverLeaveEvent(event);
    }

    // ── Pin triangle helper ───────────────────────────────────────────────────

    void OghamNodeItem::drawPinTriangle(QPainter* painter, QPointF centre,
                                        bool filled, const QColor& col) const
    {
        // Right-pointing triangle centred at `centre`
        QPainterPath tri;
        tri.moveTo(centre.x() + kPinS,          centre.y());
        tri.lineTo(centre.x() - kPinS * 0.6,    centre.y() - kPinS * 0.85);
        tri.lineTo(centre.x() - kPinS * 0.6,    centre.y() + kPinS * 0.85);
        tri.closeSubpath();

        if (filled)
        {
            painter->fillPath(tri, col);
            painter->setPen(QPen(col.lighter(130), 1.0));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(tri);
        }
        else
        {
            painter->setPen(QPen(col, 1.5));
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(tri);
        }
    }

    // ── Paint ─────────────────────────────────────────────────────────────────

    void OghamNodeItem::paint(QPainter* painter,
                              const QStyleOptionGraphicsItem* /*option*/,
                              QWidget* /*widget*/)
    {
        painter->setRenderHint(QPainter::Antialiasing);

        const qreal totalH  = kHeaderHeight + bodyHeight();
        const QRectF nodeRect(0.0, 0.0, kNodeWidth, totalH);

        QPainterPath clipPath;
        clipPath.addRoundedRect(nodeRect, kNodeCornerRadius, kNodeCornerRadius);

        // ── Phase 1: fills (clipped to rounded boundary) ──────────────────────
        painter->save();
        painter->setClipPath(clipPath);

        painter->fillRect(nodeRect, kColBodyBg);
        painter->fillRect(QRectF(0.0, 0.0, kNodeWidth, kHeaderHeight), m_headerColor);

        auto fillSecHdr = [&](qreal y)
        {
            painter->fillRect(QRectF(0.0, y, kNodeWidth, kSectionHeaderHeight), kColSectionHdrBg);
        };
        fillSecHdr(opContentTopY() - kSectionHeaderHeight);
        fillSecHdr(fieldsHdrTopY());
        fillSecHdr(choicesHdrTopY());

        if (m_fieldsExpanded)
        {
            for (int i = 0; i < m_dataKeys.size(); ++i)
            {
                const qreal rowY = fieldTopY(i);
                const qreal fh   = fieldHeightAt(i);
                painter->fillRect(
                    QRectF(kBodyPadding, rowY + kFieldVPadding,
                           kNodeWidth - kBodyPadding * 2.0, fh - kFieldVPadding * 2.0),
                    kColFieldBg);
            }
        }

        painter->restore();

        // ── Phase 2: outline and divider ──────────────────────────────────────
        QPen  borderPen;
        if (isSelected())
        {
            QLinearGradient grad(0.0, 0.0, 0.0, totalH);
            grad.setColorAt(0.0, kColBorderSelTop);
            grad.setColorAt(1.0, kColBorderSelBot);
            borderPen = QPen(QBrush(grad), 2.0);
        }
        else if (m_highlightColor.isValid())
        {
            borderPen = QPen(m_highlightColor, 3.0);
        }
        else
        {
            borderPen = QPen(kColBorderNorm, 1.0);
        }
        painter->setPen(borderPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(clipPath);

        painter->setPen(QPen(kColBorderNorm, 1.0));
        painter->drawLine(QPointF(kDividerInset, kHeaderHeight),
                          QPointF(kNodeWidth - kDividerInset, kHeaderHeight));
        if (!m_assignedLabels.isEmpty())
            painter->drawLine(QPointF(kDividerInset, kHeaderHeight + labelSectionHeight()),
                              QPointF(kNodeWidth - kDividerInset, kHeaderHeight + labelSectionHeight()));

        // ── Phase 3: header text ───────────────────────────────────────────────
        painter->setPen(contrastTextColor(m_headerColor));
        QFont headerFont = painter->font();
        headerFont.setBold(true);
        headerFont.setItalic(false);
        headerFont.setPointSizeF(8.5);
        painter->setFont(headerFont);
        const QFontMetrics hFm(headerFont);
        const qreal textX    = kPinS * 2.0 + 6.0;
        const qreal textW    = tagStatusRect().left() - textX - 4.0;
        const QString elided = hFm.elidedText(m_tag, Qt::ElideMiddle, static_cast<int>(textW));
        painter->drawText(QRectF(textX, 0.0, textW, kHeaderHeight),
                          Qt::AlignVCenter | Qt::AlignLeft, elided);

        // Status dot: green = in registry, orange = not in registry
        if (!m_tag.isEmpty())
        {
            const QRectF sr = tagStatusRect();
            painter->setPen(Qt::NoPen);
            painter->setBrush(m_tagInRegistry
                ? QColor(0x22, 0x99, 0x44)
                : QColor(0xcc, 0x77, 0x00));
            painter->drawEllipse(sr);
        }

        // ── Phase 4: section header text + buttons ────────────────────────────
        QFont secFont = painter->font();
        secFont.setBold(false);
        secFont.setItalic(false);
        secFont.setPointSizeF(7.5);
        painter->setFont(secFont);

        auto paintSecHdr = [&](qreal y, const QString& name, int count, bool expanded, int secIdx)
        {
            painter->setPen(QPen(kColBorderNorm, 1.0));
            painter->drawLine(QPointF(kDividerInset, y), QPointF(kNodeWidth - kDividerInset, y));

            painter->setPen(kColSectionText);
            const QString chevron = expanded ? QStringLiteral("▼ ") : QStringLiteral("▶ ");
            const QString label   = chevron + name
                                  + QStringLiteral(" (") + QString::number(count) + QStringLiteral(")");
            const qreal labelW = (m_hoveredSection == secIdx && m_hoveredRow == -1)
                                 ? kNodeWidth - kBtnSize - 12.0
                                 : kNodeWidth - 16.0;
            painter->drawText(QRectF(8.0, y, labelW, kSectionHeaderHeight),
                              Qt::AlignVCenter | Qt::AlignLeft, label);

            if (m_hoveredSection == secIdx && m_hoveredRow == -1)
            {
                const QRectF btn = addBtnRect(secIdx);
                QPainterPath btnPath;
                btnPath.addRoundedRect(btn, 2.0, 2.0);
                painter->fillPath(btnPath, kColBtnAdd);
                QFont btnFont = secFont;
                btnFont.setBold(true);
                painter->setFont(btnFont);
                painter->setPen(kColBtnText);
                painter->drawText(btn, Qt::AlignCenter, QStringLiteral("+"));
                painter->setFont(secFont);
            }
        };

        paintSecHdr(opContentTopY() - kSectionHeaderHeight, QStringLiteral("Operations"), m_operationLabels.size(), m_operationsExpanded, 0);
        paintSecHdr(fieldsHdrTopY(),  QStringLiteral("Fields"),     m_dataKeys.size(),        m_fieldsExpanded,     1);
        paintSecHdr(choicesHdrTopY(), QStringLiteral("Choices"),    m_optionLabels.size(),    m_choicesExpanded,    2);

        auto maybeDrawRemoveBtn = [&](int section, int row)
        {
            if (m_hoveredSection == section && m_hoveredRow == row)
            {
                const QRectF btn = removeBtnRect(section, row);
                QPainterPath btnPath;
                btnPath.addRoundedRect(btn, 2.0, 2.0);
                painter->fillPath(btnPath, kColBtnRemove);
                QFont btnFont = secFont;
                btnFont.setBold(true);
                painter->setFont(btnFont);
                painter->setPen(kColBtnText);
                painter->drawText(btn, Qt::AlignCenter, QString(QChar(0x00D7))); // ×
                painter->setFont(secFont);
            }
        };

        auto maybeDrawReorderBtn = [&](int section, int row)
        {
            const int cnt = (section == 0) ? m_operationLabels.size()
                          : (section == 1) ? m_dataKeys.size()
                          : m_optionLabels.size();
            if (cnt <= 1 || m_hoveredSection != section || m_hoveredRow != row) return;

            const QRectF btn = reorderBtnRect(section, row);
            QPainterPath btnPath;
            btnPath.addRoundedRect(btn, 2.0, 2.0);
            painter->fillPath(btnPath, kColSectionHdrBg);

            QFont arrowFont = secFont;
            arrowFont.setPointSizeF(5.5);
            painter->setFont(arrowFont);

            const QRectF topHalf(btn.left(), btn.top(), btn.width(), btn.height() * 0.5);
            const QRectF botHalf(btn.left(), btn.top() + btn.height() * 0.5,
                                 btn.width(), btn.height() * 0.5);

            static const QColor kArrowDim(0x3a, 0x3a, 0x3a);
            painter->setPen(row == 0       ? kArrowDim : kColSectionText);
            painter->drawText(topHalf, Qt::AlignCenter, QString(QChar(0x25B2)));  // ▲
            painter->setPen(row == cnt - 1 ? kArrowDim : kColSectionText);
            painter->drawText(botHalf, Qt::AlignCenter, QString(QChar(0x25BC)));  // ▼
            painter->setFont(secFont);
        };

        // ── Phase 5: operation rows ────────────────────────────────────────────
        if (m_operationsExpanded && !m_operationLabels.isEmpty())
        {
            QFont opFont = secFont;
            opFont.setItalic(true);
            painter->setFont(opFont);
            painter->setPen(kColOperationText);
            const QFontMetrics opFm(opFont);
            const qreal cTop = opContentTopY();
            for (int i = 0; i < m_operationLabels.size(); ++i)
            {
                const qreal rowY = cTop + i * kRowHeight;
                const qreal maxW = (m_hoveredSection == 0 && m_hoveredRow == i)
                                   ? kNodeWidth - kBtnSize - 16.0
                                   : kNodeWidth - 20.0;
                const QString lbl = opFm.elidedText(m_operationLabels[i], Qt::ElideRight,
                                                    static_cast<int>(maxW));
                painter->drawText(QRectF(12.0, rowY, maxW, kRowHeight),
                                  Qt::AlignVCenter | Qt::AlignLeft, lbl);
                maybeDrawReorderBtn(0, i);
                maybeDrawRemoveBtn(0, i);
            }
            painter->setFont(secFont);
        }

        // ── Phase 6: field rows (multiline, variable height) ──────────────────
        if (m_fieldsExpanded && !m_dataKeys.isEmpty())
        {
            painter->setFont(secFont);
            for (int i = 0; i < m_dataKeys.size(); ++i)
            {
                const qreal rowY = fieldTopY(i);
                const qreal fh   = fieldHeightAt(i);
                const qreal maxW = (m_hoveredSection == 1 && m_hoveredRow == i)
                                   ? kNodeWidth - kBodyPadding * 2.0 - kBtnSize - 16.0
                                   : kNodeWidth - kBodyPadding * 2.0 - 12.0;

                const QString fullText = buildFieldDisplayText(i);

                // If there's a label prefix (e.g. "Text1:\n..."), paint label in accent colour
                const int newline = fullText.indexOf(QLatin1Char('\n'));
                if (newline > 0 && m_dataKeys.size() > 1)
                {
                    const QString labelPart = fullText.left(newline);
                    const QString bodyPart  = fullText.mid(newline + 1);

                    // Measure label height
                    QFontMetrics fm(secFont);
                    const QRect labelR = fm.boundingRect(
                        QRect(0, 0, static_cast<int>(maxW), 10000),
                        Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, labelPart);
                    const qreal labelH = static_cast<qreal>(labelR.height());

                    painter->setPen(kColFieldLabel);
                    painter->drawText(
                        QRectF(kBodyPadding + 4.0, rowY + kFieldVPadding, maxW, labelH),
                        Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, labelPart);

                    painter->setPen(kColFieldText);
                    painter->drawText(
                        QRectF(kBodyPadding + 4.0, rowY + kFieldVPadding + labelH,
                               maxW, fh - kFieldVPadding * 2.0 - labelH),
                        Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, bodyPart);
                }
                else
                {
                    painter->setPen(kColFieldText);
                    painter->drawText(
                        QRectF(kBodyPadding + 4.0, rowY + kFieldVPadding,
                               maxW, fh - kFieldVPadding * 2.0),
                        Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, fullText);
                }

                maybeDrawReorderBtn(1, i);
                maybeDrawRemoveBtn(1, i);
            }
        }

        // ── Phase 7: choice rows + output triangles ────────────────────────────
        if (m_choicesExpanded && !m_optionLabels.isEmpty())
        {
            painter->setFont(secFont);
            const QFontMetrics optFm(secFont);
            const qreal cTop = choicesContentTopY();
            for (int i = 0; i < m_optionLabels.size(); ++i)
            {
                const qreal pinY  = cTop + i * kRowHeight + kRowHeight * 0.5;
                const qreal btnW  = (m_hoveredSection == 2 && m_hoveredRow == i)
                                    ? kBtnSize + 6.0 : 0.0;
                const qreal optW  = kNodeWidth - kPinS * 2.0 - 12.0 - btnW;
                painter->setPen(kColOptionText);
                const QString lbl = optFm.elidedText(m_optionLabels[i], Qt::ElideMiddle,
                                                     static_cast<int>(optW));
                painter->drawText(QRectF(8.0, cTop + i * kRowHeight, optW, kRowHeight),
                                  Qt::AlignVCenter | Qt::AlignLeft, lbl);

                const bool outConnected = (i < m_outputConnected.size()) && m_outputConnected[i];
                drawPinTriangle(painter, QPointF(kNodeWidth, pinY), outConnected, kColPinOut);

                maybeDrawReorderBtn(2, i);
                maybeDrawRemoveBtn(2, i);
            }
        }

        // ── Phase 8: input pin (left edge, at operations section header level) ────
        drawPinTriangle(painter,
                        QPointF(0.0, inputPinLocalY()),
                        m_inputConnected,
                        kColPinIn);

        // ── Phase 9: label pills (under header, above operations) ─────────────
        if (!m_assignedLabels.isEmpty())
        {
            const qreal pillH  = s_labelsExpanded ? 18.0 : 10.0;
            const qreal pillGX = 3.0;
            const qreal pillGY = 3.0;
            const qreal maxW   = kNodeWidth - kBodyPadding * 2.0;
            const qreal startY = kHeaderHeight + kBodyPadding;

            QFont lf;
            lf.setPointSizeF(7.0);
            painter->setFont(lf);
            QFontMetrics lfm(lf);

            qreal x = kBodyPadding;
            qreal y = startY;

            for (int li = 0; li < m_assignedLabels.size(); ++li)
            {
                const QColor& col  = m_assignedLabels[li].first;
                const QString& name = m_assignedLabels[li].second;

                const qreal pw = s_labelsExpanded
                    ? qMax(30.0, static_cast<qreal>(lfm.horizontalAdvance(name)) + 14.0)
                    : 20.0;

                if (x > kBodyPadding && x + pw > maxW + kBodyPadding)
                {
                    x  = kBodyPadding;
                    y += pillH + pillGY;
                }

                const QRectF pillR(x, y, pw, pillH);

                QPainterPath pillPath;
                pillPath.addRoundedRect(pillR, pillH * 0.5, pillH * 0.5);
                painter->fillPath(pillPath, col);
                painter->setPen(QPen(col.lighter(160), 0.5));
                painter->setBrush(Qt::NoBrush);
                painter->drawPath(pillPath);

                if (s_labelsExpanded)
                {
                    painter->setPen(contrastTextColor(col));
                    painter->drawText(pillR, Qt::AlignCenter, lfm.elidedText(name, Qt::ElideRight,
                                                                              static_cast<int>(pw - 6.0)));
                }

                x += pw + pillGX;
            }
        }
    }

    // ── itemChange ────────────────────────────────────────────────────────────

    QVariant OghamNodeItem::itemChange(GraphicsItemChange change, const QVariant& value)
    {
        if (change == ItemPositionChange && m_snapGrid)
        {
            constexpr qreal kSnap = 20.0;
            QPointF p = value.toPointF();
            p.setX(qRound(p.x() / kSnap) * kSnap);
            p.setY(qRound(p.y() / kSnap) * kSnap);
            return p;
        }
        if (change == ItemPositionHasChanged)
            emit positionChanged(m_fileIdx, m_entryIdx, value.toPointF());
        if (change == ItemSelectedHasChanged && value.toBool())
            emit entrySelected(m_fileIdx, m_entryIdx);
        return QGraphicsObject::itemChange(change, value);
    }

    void OghamNodeItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
    {
        if (event->button() == Qt::LeftButton)
        {
            const QPointF pos = event->pos();
            if (pos.y() < kHeaderHeight && !tagStatusRect().contains(pos))
            {
                emit tagRenameRequested(m_fileIdx, m_entryIdx, event->screenPos());
                event->accept();
                return;
            }
        }
        QGraphicsObject::mouseDoubleClickEvent(event);
    }

    void OghamNodeItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
    {
        const QPointF scenePos = event->scenePos();
        const int fi = m_fileIdx;
        const int ei = m_entryIdx;

        QMenu menu;
        menu.addAction(QStringLiteral("Create Alias Pin Here"), [this, fi, ei, scenePos]()
            { emit createAliasPinRequested(fi, ei, scenePos); });
        menu.addAction(QStringLiteral("Create Cascade") + QChar(0x2026), [this, fi, ei]()
            { emit cascadeFromNodeRequested(fi, ei, QCursor::pos()); });
        menu.addAction(QStringLiteral("Duplicate Node"), [this, fi, ei]()
            { emit duplicateNodeRequested(fi, ei); });
        menu.addSeparator();
        menu.addAction(QStringLiteral("Add Label") + QChar(0x2026), [this, fi, ei]()
            { emit addLabelRequested(fi, ei, QCursor::pos()); });
        {
            const QString colorLabel = m_highlightColor.isValid()
                ? QStringLiteral("Change Highlight Color")
                : QStringLiteral("Set Highlight Color") + QChar(0x2026);
            menu.addAction(colorLabel, [this, fi, ei]()
                { emit setHighlightColorRequested(fi, ei); });
            if (m_highlightColor.isValid())
                menu.addAction(QStringLiteral("Clear Highlight Color"), [this, fi, ei]()
                    { emit clearHighlightColorRequested(fi, ei); });
        }
        menu.addSeparator();
        auto* delAct = menu.addAction(QStringLiteral("Delete Node"), [this, fi, ei]()
            { emit deleteNodeRequested(fi, ei); });
        delAct->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));

        menu.exec(event->screenPos());
        event->accept();
    }

    void OghamNodeItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
    {
        m_pressPos = event->pos();

        if (event->button() == Qt::LeftButton)
        {
            const QPointF pos = event->pos();

            // Label pill click: toggle global expand/compact mode
            if (!m_assignedLabels.isEmpty() && pos.y() >= kHeaderHeight
                && pos.y() < kHeaderHeight + labelSectionHeight())
            {
                emit labelPillClicked();
                event->accept();
                return;
            }

            if (!m_tag.isEmpty() && !m_tagInRegistry && tagStatusRect().contains(pos))
            {
                emit tagStatusClicked(m_fileIdx, m_entryIdx, event->screenPos());
                event->accept();
                return;
            }

            if (m_hoveredRow == -1 && m_hoveredSection >= 0
                && addBtnRect(m_hoveredSection).contains(pos))
            {
                emit sectionAddClicked(m_fileIdx, m_entryIdx, m_hoveredSection,
                                       event->screenPos());
                event->accept();
                return;
            }

            if (m_hoveredRow >= 0 && m_hoveredSection >= 0)
            {
                const int reorderCnt = (m_hoveredSection == 0) ? m_operationLabels.size()
                                     : (m_hoveredSection == 1) ? m_dataKeys.size()
                                     : m_optionLabels.size();
                const QRectF reorder = reorderBtnRect(m_hoveredSection, m_hoveredRow);
                if (reorderCnt > 1 && reorder.contains(pos))
                {
                    if (pos.y() < reorder.center().y())
                        emit rowMoveUpClicked(m_fileIdx, m_entryIdx, m_hoveredSection, m_hoveredRow);
                    else
                        emit rowMoveDownClicked(m_fileIdx, m_entryIdx, m_hoveredSection, m_hoveredRow);
                    event->accept();
                    return;
                }
            }

            if (m_hoveredRow >= 0 && m_hoveredSection >= 0
                && removeBtnRect(m_hoveredSection, m_hoveredRow).contains(pos))
            {
                emit rowRemoveClicked(m_fileIdx, m_entryIdx, m_hoveredSection, m_hoveredRow);
                event->accept();
                return;
            }

            emit grabStarted(m_fileIdx, m_entryIdx);
        }

        QGraphicsObject::mousePressEvent(event);
    }

    void OghamNodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
    {
        QGraphicsObject::mouseReleaseEvent(event);
        if (event->button() == Qt::LeftButton)
        {
            emit grabEnded(m_fileIdx, m_entryIdx);

            if ((event->pos() - m_pressPos).manhattanLength() < 5.0)
            {
                const QPointF pos = event->pos();

                const int sec = sectionHeaderAt(pos);
                if (sec >= 0)
                {
                    if (!addBtnRect(sec).contains(pos))
                        toggleSection(sec);
                    return;
                }

                const auto [clickSec, clickRow] = rowContentAt(pos);
                if (clickSec >= 0)
                    emit fieldClicked(m_fileIdx, m_entryIdx, clickSec, clickRow,
                                      event->screenPos());
            }
        }
    }

} // namespace FoundationOgham
