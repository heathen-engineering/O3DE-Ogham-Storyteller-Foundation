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
#include <QPainterPath>
#include <QPointF>
#include <QVector>
#include <functional>
#endif

namespace FoundationOgham
{
    class OghamNodeItem;

    // =========================================================================
    // OghamConnectionItem
    //
    // A directed bezier edge from one node's output pin to any input pin
    // (either another node's body pin, or an alias pin).
    //
    // The source is always an OghamNodeItem output pin.
    // The destination is resolved via getDstPos — a callable that returns
    // the current scene position of the destination pin.
    //
    // The item lives at scene origin so scene coords == local coords.
    // The constructor wires src movement internally; the caller is responsible
    // for connecting the dst movement signal to refreshPath().
    // =========================================================================
    class OghamConnectionItem : public QGraphicsObject
    {
        Q_OBJECT

    public:
        explicit OghamConnectionItem(OghamNodeItem*                   srcNode,
                                     int                              optionIdx,
                                     std::function<QPointF()>         getDstPos,
                                     const QVector<QPointF>&          redirects,
                                     int                              dstFileIdx = -1,
                                     QGraphicsItem*                   parent = nullptr);

        int srcFileIdx() const;
        int dstFileIdx() const { return m_dstFileIdx; }

        /// Index of the redirect waypoint currently under the cursor, or -1.
        int hoveredRedirectIdx() const { return m_hoverIdx; }
        /// Remove the waypoint at idx and emit redirectsChanged.
        void removeRedirectAt(int idx);

        // QGraphicsItem interface
        QRectF       boundingRect() const override;
        QPainterPath shape()        const override;
        void         paint(QPainter* painter,
                           const QStyleOptionGraphicsItem* option,
                           QWidget* widget) override;

    signals:
        /// Emitted after a waypoint is added, moved, or removed.
        void redirectsChanged(QVector<QPointF> redirects);

    public slots:
        void refreshPath();

    protected:
        void hoverMoveEvent(QGraphicsSceneHoverEvent*        event) override;
        void hoverLeaveEvent(QGraphicsSceneHoverEvent*       event) override;
        void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event) override;
        void mousePressEvent(QGraphicsSceneMouseEvent*       event) override;
        void mouseMoveEvent(QGraphicsSceneMouseEvent*        event) override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent*     event) override;
        void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;

    private:
        static QPainterPath buildPath(QPointF src, QPointF dst,
                                      const QVector<QPointF>& redirects);
        static int          redirectInsertIndex(const QVector<QPointF>& pts, QPointF click);

        OghamNodeItem*           m_srcNode;
        int                      m_optionIdx;
        std::function<QPointF()> m_getDstPos;
        QVector<QPointF>         m_redirects;
        QPainterPath             m_path;
        int                      m_dstFileIdx;
        int                      m_draggingIdx = -1; ///< redirect index being dragged, -1 = none
        int                      m_hoverIdx    = -1; ///< redirect index under cursor, -1 = none

        static constexpr qreal kRedirectRadius    = 4.0;
        static constexpr qreal kRedirectHitRadius = 8.0; ///< grab radius for waypoints
    };

} // namespace FoundationOgham
