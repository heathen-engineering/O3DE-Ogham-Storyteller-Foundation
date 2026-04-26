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
#include <QGraphicsObject>
#include <QString>
#endif

namespace FoundationOgham
{
    // =========================================================================
    // OghamAliasPinItem
    //
    // A small tag-shaped element that acts as a named terminus for connections.
    // Each alias pin belongs to a specific dialogue entry (fileIdx/entryIdx)
    // and is identified by a pinId.
    //
    // Behaviour:
    //   Drag          — repositions the pin; updates the data model.
    //   Double-click  — emits nodeRequested() to focus and select the
    //                   entry node this pin belongs to.
    //
    // Visual: narrow rounded rect (lighter style than a full node), tag
    // label, and a single input pin on the left edge.
    // =========================================================================
    class OghamAliasPinItem : public QGraphicsObject
    {
        Q_OBJECT

    public:
        static constexpr qreal kWidth     = 140.0;
        static constexpr qreal kHeight    = 24.0;
        static constexpr qreal kPinRadius = 4.0;

        explicit OghamAliasPinItem(int fileIdx, int entryIdx, int pinId,
                                   const QString& tag,
                                   QGraphicsItem* parent = nullptr);

        int     fileIdx()  const { return m_fileIdx;  }
        int     entryIdx() const { return m_entryIdx; }
        int     pinId()    const { return m_pinId;    }
        QString tag()      const { return m_tag;      }

        /// Scene-space centre of the input pin.
        QPointF inputPinScenePos() const;

        // QGraphicsItem interface
        QRectF boundingRect() const override;
        void   paint(QPainter* painter,
                     const QStyleOptionGraphicsItem* option,
                     QWidget* widget) override;

    signals:
        /// Emitted continuously while dragging.
        void positionChanged(QPointF newPos);
        /// Emitted when drag ends — caller should persist to data model.
        void positionInDataChanged(int fileIdx, int entryIdx, int pinId, QPointF newPos);
        /// Emitted on double-click — caller focuses the owning node.
        void nodeRequested(int fileIdx, int entryIdx);

    protected:
        QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
        void mousePressEvent(QGraphicsSceneMouseEvent* event)   override;
        void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;

    private:
        int     m_fileIdx;
        int     m_entryIdx;
        int     m_pinId;
        QString m_tag;
        bool    m_dragged = false;
    };

} // namespace FoundationOgham
