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
#include "OghamConnectionItem.h"
#include "OghamGraphView.h"
#include "OghamNodeItem.h"

#include <QContextMenuEvent>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>
#include <QtMath>

namespace FoundationOgham
{
    static constexpr qreal kGridMinor  = 20.0;
    static constexpr qreal kGridMajor  = 100.0;
    static constexpr qreal kZoomMin    = 0.1;
    static constexpr qreal kZoomMax    = 5.0;
    static constexpr qreal kZoomFactor = 1.15;

    OghamGraphView::OghamGraphView(QWidget* parent)
        : QGraphicsView(parent)
    {
        m_scene = new QGraphicsScene(this);
        m_scene->setSceneRect(-100000, -100000, 200000, 200000);
        setScene(m_scene);

        setRenderHint(QPainter::Antialiasing);
        setDragMode(QGraphicsView::RubberBandDrag);
        setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        setResizeAnchor(QGraphicsView::AnchorViewCenter);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setFrameShape(QFrame::NoFrame);
        setBackgroundBrush(QColor(0x1a, 0x1a, 0x1a));
        setMinimumWidth(200);
        setFocusPolicy(Qt::StrongFocus);
    }

    // -------------------------------------------------------------------------
    // Grid background
    // -------------------------------------------------------------------------

    void OghamGraphView::drawBackground(QPainter* painter, const QRectF& rect)
    {
        QGraphicsView::drawBackground(painter, rect);

        const qreal left  = qFloor(rect.left()   / kGridMinor) * kGridMinor;
        const qreal top   = qFloor(rect.top()    / kGridMinor) * kGridMinor;
        const qreal right = qCeil(rect.right()   / kGridMinor) * kGridMinor;
        const qreal bot   = qCeil(rect.bottom()  / kGridMinor) * kGridMinor;

        // Major grid lines (subtle)
        painter->setPen(QPen(QColor(0x2e, 0x2e, 0x2e), 1.0));
        for (qreal x = qFloor(rect.left() / kGridMajor) * kGridMajor; x <= rect.right(); x += kGridMajor)
            painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
        for (qreal y = qFloor(rect.top() / kGridMajor) * kGridMajor; y <= rect.bottom(); y += kGridMajor)
            painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));

        // Minor grid dots
        painter->setPen(QPen(QColor(0x2a, 0x2a, 0x2a), 1.0));
        for (qreal x = left; x <= right; x += kGridMinor)
            for (qreal y = top; y <= bot; y += kGridMinor)
                painter->drawPoint(QPointF(x, y));
    }

    // -------------------------------------------------------------------------
    // Pan
    // -------------------------------------------------------------------------

    void OghamGraphView::mousePressEvent(QMouseEvent* event)
    {
        const bool isMiddle = (event->button() == Qt::MiddleButton);
        const bool isLeft   = (event->button() == Qt::LeftButton);

        if (isLeft)
        {
            if (auto* node = dynamic_cast<OghamNodeItem*>(itemAt(event->pos())))
            {
                const int opt = node->outputPinAt(node->mapFromScene(mapToScene(event->pos())));
                if (opt >= 0)
                {
                    m_pinDragging = true;
                    m_pinDragNode = node;
                    m_pinDragOpt  = opt;
                    m_pinDragLine = new QGraphicsPathItem();
                    m_pinDragLine->setPen(QPen(QColor(0x88, 0xaa, 0xcc), 2.0, Qt::DashLine));
                    m_pinDragLine->setZValue(100.0);
                    m_scene->addItem(m_pinDragLine);
                    emit pinDragStarted();
                    event->accept();
                    return;
                }
            }
        }

        if (isMiddle)
        {
            m_panning    = true;
            m_lastPanPos = event->pos();
            setCursor(Qt::ClosedHandCursor);
            event->accept();
            return;
        }

        QGraphicsView::mousePressEvent(event);
    }

    void OghamGraphView::mouseMoveEvent(QMouseEvent* event)
    {
        if (m_pinDragging && m_pinDragLine)
        {
            if (!m_pinDragNode || !m_pinDragNode->scene()) { m_pinDragging = false; return; }
            const QPointF src  = m_pinDragNode->outputPinScenePos(m_pinDragOpt);
            const QPointF dst  = mapToScene(event->pos());
            const qreal   ctrl = qMax(qAbs(dst.x() - src.x()) * 0.5, 60.0);
            QPainterPath path;
            path.moveTo(src);
            path.cubicTo(src + QPointF(ctrl, 0.0), dst - QPointF(ctrl, 0.0), dst);
            m_pinDragLine->setPath(path);
            event->accept();
            return;
        }

        if (m_panning)
        {
            const QPoint delta = event->pos() - m_lastPanPos;
            m_lastPanPos = event->pos();
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
            verticalScrollBar()->setValue(verticalScrollBar()->value()     - delta.y());
            event->accept();
            return;
        }

        QGraphicsView::mouseMoveEvent(event);
    }

    void OghamGraphView::mouseReleaseEvent(QMouseEvent* event)
    {
        if (m_pinDragging && event->button() == Qt::LeftButton)
        {
            m_pinDragging = false;
            if (m_pinDragLine)
            {
                m_scene->removeItem(m_pinDragLine);
                delete m_pinDragLine;
                m_pinDragLine = nullptr;
            }

            const QPointF       scenePos = mapToScene(event->pos());
            OghamNodeItem*      dstNode  = nullptr;
            OghamAliasPinItem*  dstAlias = nullptr;
            for (QGraphicsItem* item : m_scene->items(scenePos,
                     Qt::IntersectsItemShape, Qt::DescendingOrder))
            {
                if (auto* n = dynamic_cast<OghamNodeItem*>(item))         { dstNode  = n; break; }
                if (auto* a = dynamic_cast<OghamAliasPinItem*>(item))     { dstAlias = a; break; }
            }

            OghamNodeItem* src = m_pinDragNode;
            const int      opt = m_pinDragOpt;
            m_pinDragNode = nullptr;
            m_pinDragOpt  = -1;

            emit pinDragEnded();

            if (dstNode && dstNode != src)
                emit pinDroppedOnNode(src, opt, dstNode);
            else if (dstAlias)
                emit pinDroppedOnAlias(src, opt, dstAlias);
            else
                emit pinDroppedOnCanvas(src, opt, scenePos);

            event->accept();
            return;
        }

        if (m_panning && event->button() == Qt::MiddleButton)
        {
            m_panning = false;
            setCursor(Qt::ArrowCursor);
            event->accept();
            return;
        }

        QGraphicsView::mouseReleaseEvent(event);
    }

    // -------------------------------------------------------------------------
    // Zoom
    // -------------------------------------------------------------------------

    void OghamGraphView::wheelEvent(QWheelEvent* event)
    {
        const qreal factor    = (event->angleDelta().y() > 0) ? kZoomFactor : (1.0 / kZoomFactor);
        const qreal current   = transform().m11();
        const qreal projected = current * factor;

        if (projected < kZoomMin || projected > kZoomMax)
        {
            event->accept();
            return;
        }

        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
        scale(factor, factor);
        setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        event->accept();
    }

    // -------------------------------------------------------------------------
    // Keyboard — "F" to fit
    // -------------------------------------------------------------------------

    void OghamGraphView::keyPressEvent(QKeyEvent* event)
    {
        if (event->key() == Qt::Key_F)
        {
            const QList<QGraphicsItem*> sel = m_scene->selectedItems();
            sel.isEmpty() ? fitAll() : fitSelected();
            event->accept();
            return;
        }

        if (event->key() == Qt::Key_Delete)
        {
            bool handled = false;

            // RF-K-1: delete selected nodes and alias pins
            for (QGraphicsItem* item : m_scene->selectedItems())
            {
                if (auto* node = dynamic_cast<OghamNodeItem*>(item))
                {
                    emit deleteNodeRequested(node->fileIdx(), node->entryIdx());
                    handled = true;
                }
                else if (auto* alias = dynamic_cast<OghamAliasPinItem*>(item))
                {
                    emit deleteAliasPinRequested(alias->fileIdx(), alias->entryIdx(), alias->pinId());
                    handled = true;
                }
            }

            // RF-K-2: remove hovered redirect on any connection (only if no node was deleted)
            if (!handled)
            {
                for (QGraphicsItem* item : m_scene->items())
                {
                    if (auto* conn = dynamic_cast<OghamConnectionItem*>(item))
                    {
                        const int idx = conn->hoveredRedirectIdx();
                        if (idx >= 0)
                        {
                            conn->removeRedirectAt(idx);
                            handled = true;
                            break;
                        }
                    }
                }
            }

            if (handled) { event->accept(); return; }
        }

        QGraphicsView::keyPressEvent(event);
    }

    // -------------------------------------------------------------------------
    // Fit helpers
    // -------------------------------------------------------------------------

    void OghamGraphView::fitAll()
    {
        const QRectF bounds = m_scene->itemsBoundingRect();
        if (bounds.isNull())
        {
            resetTransform();
            centerOn(0.0, 0.0);
            return;
        }
        fitInView(bounds.adjusted(-60, -60, 60, 60), Qt::KeepAspectRatio);
    }

    void OghamGraphView::fitSelected()
    {
        QRectF bounds;
        for (const QGraphicsItem* item : m_scene->selectedItems())
            bounds = bounds.united(item->mapToScene(item->boundingRect()).boundingRect());
        if (bounds.isNull()) { fitAll(); return; }
        fitInView(bounds.adjusted(-60, -60, 60, 60), Qt::KeepAspectRatio);
    }

    // -------------------------------------------------------------------------
    // Context menu — right-click on empty canvas
    // -------------------------------------------------------------------------

    void OghamGraphView::contextMenuEvent(QContextMenuEvent* event)
    {
        const QPointF scenePos = mapToScene(event->pos());
        if (m_scene->itemAt(scenePos, QTransform()) == nullptr)
        {
            QMenu menu(this);
            menu.addAction(QStringLiteral("Create Node Here"), [this, scenePos]()
            {
                emit createNodeRequested(scenePos);
            });
            menu.exec(event->globalPos());
            event->accept();
            return;
        }
        QGraphicsView::contextMenuEvent(event);
    }

} // namespace FoundationOgham
