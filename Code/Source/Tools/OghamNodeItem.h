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

#pragma once

#if !defined(Q_MOC_RUN)
#include <QColor>
#include <QGraphicsObject>
#include <QStringList>
#endif

class QGraphicsSceneContextMenuEvent;

namespace FoundationOgham
{
    // =========================================================================
    // OghamNodeItem
    //
    // One dialogue entry rendered as a function-node on the graph canvas.
    //
    //  ┌─ header ──────────────────── ●input ─┐
    //  │  Act1.Scene1.Line1                    │
    //  ├────────────────────────────��──────────┤
    //  │  Option 0                     ○ ──────│
    //  │  Option 1                     ○ ──────│
    //  └───────────────────────────────────────┘
    //
    // Input pin  — left edge of header, vertically centred.
    // Output pins — right edge of body, one per option row.
    // =========================================================================
    class OghamNodeItem : public QGraphicsObject
    {
        Q_OBJECT

    public:
        static constexpr qreal kNodeWidth    = 220.0;
        static constexpr qreal kHeaderHeight = 28.0;
        static constexpr qreal kPinRadius    = 5.0;
        static constexpr qreal kRowHeight    = 22.0;
        static constexpr qreal kBodyPadding  = 6.0;

        explicit OghamNodeItem(int fileIdx, int entryIdx,
                               const QString& tag,
                               const QStringList& optionTags,
                               QColor headerColor = QColor(0x2d, 0x5a, 0x8e),
                               QGraphicsItem* parent = nullptr);

        int     fileIdx()  const { return m_fileIdx;  }
        int     entryIdx() const { return m_entryIdx; }
        QString tag()      const { return m_tag;      }

        /// Scene-space position of the input pin centre.
        QPointF inputPinScenePos()         const;
        /// Scene-space position of the i-th output pin centre.
        QPointF outputPinScenePos(int i)   const;

        int optionCount() const { return m_optionTags.size(); }

        /// Returns the output-pin option index hit by localPos (node-local coords), or -1.
        int outputPinAt(QPointF localPos) const;

        // QGraphicsItem interface
        QRectF boundingRect() const override;
        void   paint(QPainter* painter,
                     const QStyleOptionGraphicsItem* option,
                     QWidget* widget) override;

    signals:
        /// Emitted continuously while dragging (pos is in scene coordinates).
        void positionChanged(int fileIdx, int entryIdx, QPointF newPos);
        /// Emitted once when the node is selected by the user.
        void entrySelected(int fileIdx, int entryIdx);
        /// Emitted on left-button press (drag start or simple click).
        void grabStarted(int fileIdx, int entryIdx);
        /// Emitted on left-button release.
        void grabEnded(int fileIdx, int entryIdx);
        /// Context menu: "Create Alias Pin Here" — scenePos is the click location.
        void createAliasPinRequested(int fileIdx, int entryIdx, QPointF scenePos);
        /// Context menu: "Duplicate Node".
        void duplicateNodeRequested(int fileIdx, int entryIdx);
        /// Context menu: "Delete Node".
        void deleteNodeRequested(int fileIdx, int entryIdx);

    protected:
        QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
        void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
        void mousePressEvent(QGraphicsSceneMouseEvent* event)   override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

    private:
        qreal bodyHeight() const;

        int         m_fileIdx;
        int         m_entryIdx;
        QString     m_tag;
        QStringList m_optionTags;
        QColor      m_headerColor;
    };

} // namespace FoundationOgham
