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

#include "OghamConnectionItem.h"
#include "OghamNodeItem.h"

#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QMenu>
#include <QGraphicsSceneMouseEvent>
#include <QLineF>
#include <QPainter>
#include <QPainterPathStroker>
#include <QtMath>
#include <limits>

namespace FoundationOgham
{
    static const QColor kColLine       { 0x5a, 0x6a, 0x7a };
    static const QColor kColRedirect   { 0x7a, 0x8a, 0x9a };
    static const QColor kColRedirectHot{ 0xcc, 0xdd, 0xee };

    // ── Construction ──────────────────────────────────────────────────────────

    int OghamConnectionItem::srcFileIdx() const { return m_srcNode->fileIdx(); }

    OghamConnectionItem::OghamConnectionItem(OghamNodeItem*                   srcNode,
                                             int                              optionIdx,
                                             std::function<QPointF()>         getDstPos,
                                             const QVector<QPointF>&          redirects,
                                             int                              dstFileIdx,
                                             QGraphicsItem*                   parent)
        : QGraphicsObject(parent)
        , m_srcNode(srcNode)
        , m_optionIdx(optionIdx)
        , m_getDstPos(std::move(getDstPos))
        , m_redirects(redirects)
        , m_dstFileIdx(dstFileIdx)
    {
        setZValue(0.0);
        setFlag(QGraphicsItem::ItemIsSelectable, false);
        setAcceptHoverEvents(true);
        setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);

        // Rebuild path whenever the source node moves.
        // Dst movement is wired by the caller (type varies: node or alias pin).
        connect(m_srcNode, &OghamNodeItem::positionChanged,
                this, [this](int, int, QPointF) { refreshPath(); });

        refreshPath();
    }

    // ── Path building ─────────────────────────────────────────────────────────

    QPainterPath OghamConnectionItem::buildPath(QPointF             src,
                                                QPointF             dst,
                                                const QVector<QPointF>& redirects)
    {
        QVector<QPointF> pts;
        pts.reserve(redirects.size() + 2);
        pts.append(src);
        for (const QPointF& r : redirects) pts.append(r);
        pts.append(dst);

        QPainterPath path;
        path.moveTo(pts.first());

        for (int i = 0; i + 1 < pts.size(); ++i)
        {
            const QPointF& p0 = pts[i];
            const QPointF& p1 = pts[i + 1];
            // Control point distance: proportional to horizontal span, minimum 60.
            const qreal ctrl = qMax(qAbs(p1.x() - p0.x()) * 0.5, 60.0);
            path.cubicTo(p0 + QPointF(ctrl,  0.0),
                         p1 - QPointF(ctrl,  0.0),
                         p1);
        }

        return path;
    }

    void OghamConnectionItem::refreshPath()
    {
        if (!m_srcNode || !m_srcNode->scene()) return;
        prepareGeometryChange();
        const QPointF src = m_srcNode->outputPinScenePos(m_optionIdx);
        const QPointF dst = m_getDstPos();
        m_path = buildPath(src, dst, m_redirects);
        update();
    }

    // ── QGraphicsItem interface ───────────────────────────────────────────────

    QRectF OghamConnectionItem::boundingRect() const
    {
        return m_path.boundingRect().adjusted(-12.0, -12.0, 12.0, 12.0);
    }

    QPainterPath OghamConnectionItem::shape() const
    {
        QPainterPathStroker stroker;
        stroker.setWidth(12.0);
        return stroker.createStroke(m_path);
    }

    void OghamConnectionItem::paint(QPainter*                       painter,
                                    const QStyleOptionGraphicsItem* /*option*/,
                                    QWidget*                        /*widget*/)
    {
        painter->setRenderHint(QPainter::Antialiasing);

        // Line
        painter->setPen(QPen(kColLine, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(m_path);

        // Redirect waypoint circles — highlight hovered and dragged indices
        if (!m_redirects.isEmpty())
        {
            for (int i = 0; i < m_redirects.size(); ++i)
            {
                const bool hot = (i == m_hoverIdx || i == m_draggingIdx);
                painter->setBrush(hot ? kColRedirectHot : kColRedirect);
                painter->setPen(QPen(hot ? kColLine.lighter(160) : kColLine, 1.0));
                painter->drawEllipse(m_redirects[i], kRedirectRadius, kRedirectRadius);
            }
        }
    }

    // ── Hover ─────────────────────────────────────────────────────────────────

    void OghamConnectionItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
    {
        const QPointF pos = event->scenePos();
        int newHover = -1;
        for (int i = 0; i < m_redirects.size(); ++i)
        {
            if (QLineF(pos, m_redirects[i]).length() <= kRedirectHitRadius)
            {
                newHover = i;
                break;
            }
        }
        if (newHover != m_hoverIdx)
        {
            m_hoverIdx = newHover;
            setCursor(newHover >= 0 ? Qt::SizeAllCursor : Qt::CrossCursor);
            update();
        }
        QGraphicsObject::hoverMoveEvent(event);
    }

    void OghamConnectionItem::hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
    {
        if (m_hoverIdx != -1)
        {
            m_hoverIdx = -1;
            unsetCursor();
            update();
        }
        QGraphicsObject::hoverLeaveEvent(event);
    }

    // ── Mouse — drag existing waypoint ───────────────────────────────────────

    void OghamConnectionItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
    {
        if (event->button() != Qt::LeftButton)
        {
            QGraphicsObject::mousePressEvent(event);
            return;
        }
        const QPointF pos = event->scenePos();
        for (int i = 0; i < m_redirects.size(); ++i)
        {
            if (QLineF(pos, m_redirects[i]).length() <= kRedirectHitRadius)
            {
                m_draggingIdx = i;
                event->accept();
                return;
            }
        }
        QGraphicsObject::mousePressEvent(event);
    }

    void OghamConnectionItem::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
    {
        if (m_draggingIdx >= 0)
        {
            m_redirects[m_draggingIdx] = event->scenePos();
            refreshPath();
            event->accept();
            return;
        }
        QGraphicsObject::mouseMoveEvent(event);
    }

    void OghamConnectionItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
    {
        if (m_draggingIdx >= 0 && event->button() == Qt::LeftButton)
        {
            emit redirectsChanged(m_redirects);
            m_draggingIdx = -1;
            event->accept();
            return;
        }
        QGraphicsObject::mouseReleaseEvent(event);
    }

    // ── Mouse — double-click: insert new waypoint ─────────────────────────────

    void OghamConnectionItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
    {
        if (event->button() != Qt::LeftButton)
        {
            QGraphicsObject::mouseDoubleClickEvent(event);
            return;
        }
        const QPointF pos = event->scenePos();

        // Ignore if clicking on an existing waypoint — let drag handle it
        for (const QPointF& r : m_redirects)
            if (QLineF(pos, r).length() <= kRedirectHitRadius) { event->accept(); return; }

        // Build the full ordered point list and find the correct insertion index
        if (!m_srcNode || !m_srcNode->scene()) { event->ignore(); return; }
        const QPointF src = m_srcNode->outputPinScenePos(m_optionIdx);
        const QPointF dst = m_getDstPos();
        QVector<QPointF> pts;
        pts.reserve(m_redirects.size() + 2);
        pts.append(src);
        for (const QPointF& r : m_redirects) pts.append(r);
        pts.append(dst);

        const int insertAt = redirectInsertIndex(pts, pos);
        m_redirects.insert(insertAt, pos);
        refreshPath();
        emit redirectsChanged(m_redirects);
        event->accept();
    }

    // ── Context menu — right-click: remove waypoint ───────────────────────────

    void OghamConnectionItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
    {
        const QPointF pos = event->scenePos();

        // Check if click is over a redirect waypoint
        for (int i = 0; i < m_redirects.size(); ++i)
        {
            if (QLineF(pos, m_redirects[i]).length() <= kRedirectHitRadius)
            {
                QMenu menu;
                menu.addAction("Remove Waypoint", [this, i]() { removeRedirectAt(i); });
                menu.exec(event->screenPos());
                event->accept();
                return;
            }
        }

        // Click on the line itself — offer to insert a redirect point
        QMenu menu;
        menu.addAction("Add Redirect Point", [this, pos]()
        {
            const int idx = redirectInsertIndex(m_redirects, pos);
            m_redirects.insert(idx, pos);
            refreshPath();
            emit redirectsChanged(m_redirects);
        });
        menu.exec(event->screenPos());
        event->accept();
    }

    void OghamConnectionItem::removeRedirectAt(int idx)
    {
        if (idx < 0 || idx >= m_redirects.size()) return;
        m_redirects.removeAt(idx);
        if (m_hoverIdx == idx)    m_hoverIdx    = -1;
        else if (m_hoverIdx > idx) --m_hoverIdx;
        refreshPath();
        emit redirectsChanged(m_redirects);
    }

    // ── Static helpers ────────────────────────────────────────────────────────

    int OghamConnectionItem::redirectInsertIndex(const QVector<QPointF>& pts, QPointF click)
    {
        int   best     = 0;
        qreal bestDist = std::numeric_limits<qreal>::max();
        for (int i = 0; i + 1 < pts.size(); ++i)
        {
            const QPointF d    = pts[i + 1] - pts[i];
            const qreal   len2 = d.x() * d.x() + d.y() * d.y();
            qreal t = 0.0;
            if (len2 > 0.0)
            {
                const QPointF v = click - pts[i];
                t = qBound(0.0, (v.x() * d.x() + v.y() * d.y()) / len2, 1.0);
            }
            const QPointF proj = pts[i] + t * d;
            const QPointF diff = click - proj;
            const qreal   dist = diff.x() * diff.x() + diff.y() * diff.y();
            if (dist < bestDist) { bestDist = dist; best = i; }
        }
        return best;
    }

} // namespace FoundationOgham
