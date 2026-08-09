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

#include "OghamAliasPinItem.h"

#include <QFontMetrics>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionGraphicsItem>

namespace FoundationOgham
{
    static const QColor kColBg        { 0x1e, 0x2a, 0x38 };
    static const QColor kColBorder    { 0x3a, 0x6a, 0x9a };
    static const QColor kColBorderSel { 0x55, 0x99, 0xdd };
    static const QColor kColText      { 0x9a, 0xc0, 0xd8 };
    static const QColor kColPin       { 0x6a, 0xb0, 0xd0 };

    static constexpr qreal kCornerRadius = 4.0;

    // ── Construction ──────────────────────────────────────────────────────────

    OghamAliasPinItem::OghamAliasPinItem(int fileIdx, int entryIdx, int pinId,
                                         const QString& tag,
                                         QGraphicsItem* parent)
        : QGraphicsObject(parent)
        , m_fileIdx(fileIdx)
        , m_entryIdx(entryIdx)
        , m_pinId(pinId)
        , m_tag(tag)
    {
        setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
        setZValue(1.0);
    }

    // ── Geometry ──────────────────────────────────────────────────────────────

    QPointF OghamAliasPinItem::inputPinScenePos() const
    {
        return mapToScene(QPointF(0.0, kHeight * 0.5));
    }

    QRectF OghamAliasPinItem::boundingRect() const
    {
        return QRectF(-kPinRadius - 1.0, -1.0,
                      kWidth + kPinRadius + 2.0, kHeight + 2.0);
    }

    // ── Paint ─────────────────────────────────────────────────────────────────

    void OghamAliasPinItem::paint(QPainter*                       painter,
                                  const QStyleOptionGraphicsItem* /*option*/,
                                  QWidget*                        /*widget*/)
    {
        painter->setRenderHint(QPainter::Antialiasing);

        const QRectF body(0.0, 0.0, kWidth, kHeight);

        QPainterPath clipPath;
        clipPath.addRoundedRect(body, kCornerRadius, kCornerRadius);

        painter->save();
        painter->setClipPath(clipPath);
        painter->fillRect(body, kColBg);
        painter->restore();

        const QColor& borderCol = isSelected() ? kColBorderSel : kColBorder;
        const qreal   borderW   = isSelected() ? 2.0 : 1.0;
        painter->setPen(QPen(borderCol, borderW));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(clipPath);

        // Tag label (elided to fit)
        painter->setPen(kColText);
        QFont f = painter->font();
        f.setPointSizeF(7.5);
        painter->setFont(f);
        const QFontMetrics fm(f);
        const qreal textX  = kPinRadius * 2.0 + 4.0;
        const qreal textW  = kWidth - textX - 4.0;
        const QString text = fm.elidedText(m_tag, Qt::ElideMiddle, static_cast<int>(textW));
        painter->drawText(QRectF(textX, 0.0, textW, kHeight),
                          Qt::AlignVCenter | Qt::AlignLeft, text);

        // Input pin
        painter->setBrush(kColPin);
        painter->setPen(QPen(kColBorder, 1.0));
        painter->drawEllipse(QPointF(0.0, kHeight * 0.5), kPinRadius, kPinRadius);
    }

    // ── itemChange ────────────────────────────────────────────────────────────

    QVariant OghamAliasPinItem::itemChange(GraphicsItemChange change,
                                           const QVariant&    value)
    {
        if (change == ItemPositionHasChanged)
        {
            m_dragged = true;
            emit positionChanged(value.toPointF());
        }
        return QGraphicsObject::itemChange(change, value);
    }

    void OghamAliasPinItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
    {
        if (event->button() == Qt::LeftButton)
        {
            if (!(event->modifiers() & Qt::ShiftModifier))
                scene()->clearSelection();
            setSelected(true);
        }
        QGraphicsObject::mousePressEvent(event);
    }

    void OghamAliasPinItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
    {
        QGraphicsObject::mouseReleaseEvent(event);
        if (m_dragged)
        {
            m_dragged = false;
            emit positionInDataChanged(m_fileIdx, m_entryIdx, m_pinId, pos());
        }
    }

    void OghamAliasPinItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
    {
        QGraphicsObject::mouseDoubleClickEvent(event);
        emit nodeRequested(m_fileIdx, m_entryIdx);
    }

} // namespace FoundationOgham
