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
#include <QGraphicsView>
#include <QPoint>
#include <QPointF>
#endif

class QGraphicsPathItem;
class QGraphicsScene;

namespace FoundationOgham
{
    class OghamAliasPinItem;
    class OghamNodeItem;

    // =========================================================================
    // OghamGraphView
    //
    // QGraphicsView subclass providing the node graph canvas.
    //
    //   Pan  — left-drag on empty canvas, or middle-drag anywhere
    //   Zoom — mouse wheel (clamped 10% – 500%)
    //   Fit  — "F" key: fit selection (or all items if nothing selected)
    //   Grid — subtle dot grid drawn in scene coordinates
    // =========================================================================
    class OghamGraphView : public QGraphicsView
    {
        Q_OBJECT

    public:
        explicit OghamGraphView(QWidget* parent = nullptr);

        QGraphicsScene* graphScene() const { return m_scene; }

        void fitAll();
        void fitSelected();

    signals:
        void pinDroppedOnNode(OghamNodeItem* src, int optionIdx, OghamNodeItem* dst);
        void pinDroppedOnAlias(OghamNodeItem* src, int optionIdx, OghamAliasPinItem* dst);
        void pinDroppedOnCanvas(OghamNodeItem* src, int optionIdx, QPointF scenePos);
        /// Emitted when Delete is pressed while a node is selected (RF-K-1).
        void deleteNodeRequested(int fileIdx, int entryIdx);
        /// Emitted when Delete is pressed on a selected alias pin.
        void deleteAliasPinRequested(int fileIdx, int entryIdx, int pinId);
        /// Emitted when the user starts dragging an output pin (before drop).
        void pinDragStarted();
        /// Emitted just before the drop signal fires, so interacting state can clear.
        void pinDragEnded();
        /// Emitted when the user right-clicks on empty canvas.
        void createNodeRequested(QPointF scenePos);

    protected:
        void drawBackground(QPainter* painter, const QRectF& rect) override;
        void mousePressEvent(QMouseEvent* event)       override;
        void mouseMoveEvent(QMouseEvent* event)        override;
        void mouseReleaseEvent(QMouseEvent* event)     override;
        void wheelEvent(QWheelEvent* event)            override;
        void keyPressEvent(QKeyEvent* event)           override;
        void contextMenuEvent(QContextMenuEvent* event) override;

    private:
        QGraphicsScene*    m_scene       = nullptr;
        bool               m_panning     = false;
        QPoint             m_lastPanPos;
        bool               m_pinDragging = false;
        OghamNodeItem*     m_pinDragNode = nullptr;
        int                m_pinDragOpt  = -1;
        QGraphicsPathItem* m_pinDragLine = nullptr;
    };

} // namespace FoundationOgham
