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
#include <QRectF>
#include <QString>
#include <QVector>
#include <functional>
#endif

namespace FoundationOgham
{
    class OghamNodeItem;

    // =========================================================================
    // OghamConnectionItem
    //
    // A directed edge from one node's output pin to any input pin.
    // Renders in one of two modes:
    //
    //  Wire mode  — cubic bezier from output pin to destination input pin,
    //               with optional redirect waypoints.
    //
    //  Tab mode   — a flag-shaped label anchored at the source output pin,
    //               showing the destination tag. The bezier wire is hidden.
    //               Right-click "Show as Tab" / "Show as Wire" toggles modes.
    //               Left-click on the tab selects the target node.
    //               Hover expands the label to show the full target tag.
    //
    // The item lives at scene origin so scene coords == local coords.
    // =========================================================================
    class OghamConnectionItem : public QGraphicsObject
    {
        Q_OBJECT

    public:
        explicit OghamConnectionItem(OghamNodeItem*                   srcNode,
                                     int                              optionIdx,
                                     std::function<QPointF()>         getDstPos,
                                     const QVector<QPointF>&          redirects,
                                     const QString&                   targetTag,
                                     bool                             displayAsTab    = false,
                                     int                              dstFileIdx      = -1,
                                     const QColor&                    dstHighlight    = QColor(),
                                     QGraphicsItem*                   parent          = nullptr);

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
        /// Emitted when the user toggles wire ↔ tab mode via the context menu.
        void displayModeChanged(bool displayAsTab);
        /// Emitted when the user left-clicks the tab; caller should select the target node.
        void tabClicked(const QString& targetTag);

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

        void    paintTab(QPainter* painter) const;
        QRectF  tabRect()                   const;

        OghamNodeItem*           m_srcNode;
        int                      m_optionIdx;
        std::function<QPointF()> m_getDstPos;
        QVector<QPointF>         m_redirects;
        QPainterPath             m_path;
        int                      m_dstFileIdx;
        int                      m_draggingIdx = -1;
        int                      m_hoverIdx    = -1;

        // Tab mode state
        QString m_targetTag;
        bool    m_displayAsTab = false;
        bool    m_tabHovered   = false;
        qreal   m_savedZValue  = 0.0;
        QPointF m_tabPos;
        QColor  m_dstHighlight;   ///< destination node's highlight color; invalid = use default tab color

        static constexpr qreal kRedirectRadius    = 4.0;
        static constexpr qreal kRedirectHitRadius = 8.0;
        static constexpr qreal kTabHeight         = 18.0;
        static constexpr qreal kTabArrow          = 7.0;  ///< horizontal width of the chevron tip
        static constexpr qreal kTabPadX           = 5.0;  ///< horizontal text padding inside tab
        static constexpr qreal kTabDefaultW       = 27.0;   // 1/3 of original 80
        static constexpr qreal kTabHoverW         = 60.0;   // 1/3 of original 180
    };

} // namespace FoundationOgham
