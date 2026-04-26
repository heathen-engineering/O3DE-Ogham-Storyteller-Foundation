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

#include <QFontMetrics>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneMouseEvent>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionGraphicsItem>

namespace FoundationOgham
{
    // ── Colours ────────────────────────────────────────────────────���──────────
    static const QColor kColBodyBg    { 0x25, 0x25, 0x25 };
    static const QColor kColBorderNorm{ 0x40, 0x40, 0x40 };
    static const QColor kNodeColBorderSel { 0x55, 0x99, 0xdd };
    static const QColor kColHeaderText   { 0xe8, 0xe8, 0xe8 };
    static const QColor kColOptionText   { 0xa0, 0xa0, 0xa0 };
    static const QColor kColPinIn        { 0x6a, 0xb0, 0xd0 };
    static const QColor kColPinOut       { 0xd0, 0x90, 0x4a };

    static constexpr qreal kNodeCornerRadius = 6.0;

    // ── Construction ──────────────────────────────────────────────────────────

    OghamNodeItem::OghamNodeItem(int fileIdx, int entryIdx,
                                 const QString& tag,
                                 const QStringList& optionTags,
                                 QColor headerColor,
                                 QGraphicsItem* parent)
        : QGraphicsObject(parent)
        , m_fileIdx(fileIdx)
        , m_entryIdx(entryIdx)
        , m_tag(tag)
        , m_optionTags(optionTags)
        , m_headerColor(headerColor)
    {
        setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
        setAcceptHoverEvents(false);
        setCacheMode(DeviceCoordinateCache);
        setZValue(1.0);
    }

    // ── Geometry ──────────────────────────────────────────────────────────────

    qreal OghamNodeItem::bodyHeight() const
    {
        const int rows = qMax(1, m_optionTags.size());
        return kBodyPadding + rows * kRowHeight + kBodyPadding;
    }

    QRectF OghamNodeItem::boundingRect() const
    {
        // Extra kPinRadius margin on left/right so pin circles aren't clipped.
        const qreal totalH = kHeaderHeight + bodyHeight();
        return QRectF(-kPinRadius - 1.0, -1.0,
                      kNodeWidth + kPinRadius * 2.0 + 2.0,
                      totalH + 2.0);
    }

    QPointF OghamNodeItem::inputPinScenePos() const
    {
        return mapToScene(QPointF(0.0, kHeaderHeight * 0.5));
    }

    QPointF OghamNodeItem::outputPinScenePos(int i) const
    {
        const qreal y = kHeaderHeight + kBodyPadding
                        + i * kRowHeight + kRowHeight * 0.5;
        return mapToScene(QPointF(kNodeWidth, y));
    }

    // ── Paint ─────────────────────────────────────────────────────────────────

    void OghamNodeItem::paint(QPainter* painter,
                              const QStyleOptionGraphicsItem* /*option*/,
                              QWidget* /*widget*/)
    {
        painter->setRenderHint(QPainter::Antialiasing);

        const qreal totalH = kHeaderHeight + bodyHeight();
        const QRectF nodeRect(0.0, 0.0, kNodeWidth, totalH);

        // ── Clipped body fill ─────────────────────────────────────────────────
        QPainterPath clipPath;
        clipPath.addRoundedRect(nodeRect, kNodeCornerRadius, kNodeCornerRadius);

        painter->save();
        painter->setClipPath(clipPath);

        // Body background
        painter->fillRect(nodeRect, kColBodyBg);

        // Header background
        painter->fillRect(QRectF(0.0, 0.0, kNodeWidth, kHeaderHeight), m_headerColor);

        painter->restore();

        // ── Outline ───────────────────────────────────────────────────────────
        const QColor& borderCol = isSelected() ? kNodeColBorderSel : kColBorderNorm;
        const qreal   borderW   = isSelected() ? 2.0 : 1.0;
        painter->setPen(QPen(borderCol, borderW));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(clipPath);

        // Divider between header and body
        painter->setPen(QPen(kColBorderNorm, 1.0));
        painter->drawLine(QPointF(0.0, kHeaderHeight),
                          QPointF(kNodeWidth, kHeaderHeight));

        // ── Header text (tag name, elided) ────────────────────────────────────
        painter->setPen(kColHeaderText);
        QFont headerFont = painter->font();
        headerFont.setBold(true);
        headerFont.setPointSizeF(8.5);
        painter->setFont(headerFont);

        const QFontMetrics fm(headerFont);
        const qreal textX    = kPinRadius * 2.0 + 6.0;  // indent past input pin
        const qreal textW    = kNodeWidth - textX - 6.0;
        const QString elided = fm.elidedText(m_tag, Qt::ElideMiddle, static_cast<int>(textW));
        painter->drawText(
            QRectF(textX, 0.0, textW, kHeaderHeight),
            Qt::AlignVCenter | Qt::AlignLeft,
            elided);

        // ── Option labels + output pins ───────────────────────────────────────
        QFont optFont = painter->font();
        optFont.setBold(false);
        optFont.setPointSizeF(7.5);
        painter->setFont(optFont);

        const QFontMetrics optFm(optFont);
        for (int i = 0; i < m_optionTags.size(); ++i)
        {
            const qreal pinY = kHeaderHeight + kBodyPadding + i * kRowHeight + kRowHeight * 0.5;

            // Option text
            painter->setPen(kColOptionText);
            const QString optLabel = optFm.elidedText(
                m_optionTags[i], Qt::ElideMiddle,
                static_cast<int>(kNodeWidth - kPinRadius * 2.0 - 12.0));
            painter->drawText(
                QRectF(8.0, pinY - kRowHeight * 0.5,
                       kNodeWidth - kPinRadius * 2.0 - 12.0, kRowHeight),
                Qt::AlignVCenter | Qt::AlignLeft,
                optLabel);

            // Output pin circle
            painter->setBrush(kColPinOut);
            painter->setPen(QPen(kColBorderNorm, 1.0));
            painter->drawEllipse(
                QPointF(kNodeWidth, pinY),
                kPinRadius, kPinRadius);
        }

        // ── Input pin (left edge of header) ──────────────────────────────────
        painter->setBrush(kColPinIn);
        painter->setPen(QPen(kColBorderNorm, 1.0));
        painter->drawEllipse(
            QPointF(0.0, kHeaderHeight * 0.5),
            kPinRadius, kPinRadius);
    }

    // ── outputPinAt ───────────────────────────────────────────────────────────

    int OghamNodeItem::outputPinAt(QPointF localPos) const
    {
        constexpr qreal kHit = kPinRadius + 3.0;
        for (int i = 0; i < m_optionTags.size(); ++i)
        {
            const QPointF pin(kNodeWidth,
                kHeaderHeight + kBodyPadding + i * kRowHeight + kRowHeight * 0.5);
            const qreal dx = localPos.x() - pin.x();
            const qreal dy = localPos.y() - pin.y();
            if (dx * dx + dy * dy <= kHit * kHit)
                return i;
        }
        return -1;
    }

    // ── itemChange ────────────────────────────────────────────────────────────

    QVariant OghamNodeItem::itemChange(GraphicsItemChange change, const QVariant& value)
    {
        if (change == ItemPositionHasChanged)
            emit positionChanged(m_fileIdx, m_entryIdx, value.toPointF());

        if (change == ItemSelectedHasChanged && value.toBool())
            emit entrySelected(m_fileIdx, m_entryIdx);

        return QGraphicsObject::itemChange(change, value);
    }

    void OghamNodeItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
    {
        const QPointF scenePos = event->scenePos();
        const int fi = m_fileIdx;
        const int ei = m_entryIdx;

        QMenu menu;
        menu.addAction("Create Alias Pin Here", [this, fi, ei, scenePos]()
            { emit createAliasPinRequested(fi, ei, scenePos); });
        menu.addAction("Duplicate Node", [this, fi, ei]()
            { emit duplicateNodeRequested(fi, ei); });
        menu.addSeparator();
        auto* delAct = menu.addAction("Delete Node", [this, fi, ei]()
            { emit deleteNodeRequested(fi, ei); });
        delAct->setIcon(QIcon::fromTheme("edit-delete"));

        menu.exec(event->screenPos());
        event->accept();
    }

    void OghamNodeItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
    {
        if (event->button() == Qt::LeftButton)
            emit grabStarted(m_fileIdx, m_entryIdx);
        QGraphicsObject::mousePressEvent(event);
    }

    void OghamNodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
    {
        QGraphicsObject::mouseReleaseEvent(event);
        if (event->button() == Qt::LeftButton)
            emit grabEnded(m_fileIdx, m_entryIdx);
    }

} // namespace FoundationOgham
