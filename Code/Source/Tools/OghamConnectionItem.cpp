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
#include "OghamPaintUtils.h"

#include <QFontMetrics>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QLineF>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QtMath>
#include <limits>

namespace FoundationOgham
{
    static const QColor kColLine        { 0x5a, 0x6a, 0x7a };
    static const QColor kColRedirect    { 0x7a, 0x8a, 0x9a };
    static const QColor kColRedirectHot { 0xcc, 0xdd, 0xee };
    static const QColor kColTabBg      { 0x28, 0x48, 0x68 }; // dark blue-grey for tab
    static const QColor kColTabHoverBg { 0x38, 0x68, 0x98 }; // lighter on hover
    static const QColor kColTabBorder  { 0x5a, 0x8a, 0xba }; // tab outline

    // ── Construction ──────────────────────────────────────────────────────────

    int OghamConnectionItem::srcFileIdx() const { return m_srcNode->fileIdx(); }

    OghamConnectionItem::OghamConnectionItem(OghamNodeItem*                   srcNode,
                                             int                              optionIdx,
                                             std::function<QPointF()>         getDstPos,
                                             const QVector<QPointF>&          redirects,
                                             const QString&                   targetTag,
                                             bool                             displayAsTab,
                                             int                              dstFileIdx,
                                             const QColor&                    dstHighlight,
                                             QGraphicsItem*                   parent)
        : QGraphicsObject(parent)
        , m_srcNode(srcNode)
        , m_optionIdx(optionIdx)
        , m_getDstPos(std::move(getDstPos))
        , m_redirects(redirects)
        , m_dstFileIdx(dstFileIdx)
        , m_targetTag(targetTag)
        , m_displayAsTab(displayAsTab)
        , m_dstHighlight(dstHighlight)
    {
        setZValue(0.0);
        setFlag(QGraphicsItem::ItemIsSelectable, false);
        setAcceptHoverEvents(true);
        setAcceptedMouseButtons(Qt::LeftButton | Qt::RightButton);

        connect(m_srcNode, &OghamNodeItem::positionChanged,
                this, [this](int, int, QPointF) { refreshPath(); });

        refreshPath();
    }

    // ── Path building ─────────────────────────────────────────────────────────

    QPainterPath OghamConnectionItem::buildPath(QPointF src, QPointF dst,
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
            const QPointF& p0   = pts[i];
            const QPointF& p1   = pts[i + 1];
            const qreal    ctrl = qMax(qAbs(p1.x() - p0.x()) * 0.5, 60.0);
            path.cubicTo(p0 + QPointF(ctrl, 0.0), p1 - QPointF(ctrl, 0.0), p1);
        }
        return path;
    }

    void OghamConnectionItem::refreshPath()
    {
        if (!m_srcNode || !m_srcNode->scene()) return;
        prepareGeometryChange();
        const QPointF src = m_srcNode->outputPinScenePos(m_optionIdx);
        m_isSelfRef = !m_targetTag.isEmpty() && (m_srcNode->tag() == m_targetTag);
        if (m_displayAsTab || m_isSelfRef)
        {
            m_tabPos = src;
        }
        else
        {
            const QPointF dst = m_getDstPos();
            m_path = buildPath(src, dst, m_redirects);
        }
        update();
    }

    // ── Tab geometry ──────────────────────────────────────────────────────────

    QRectF OghamConnectionItem::tabRect() const
    {
        const qreal w = m_tabHovered ? kTabHoverW : kTabDefaultW;
        return QRectF(m_tabPos.x(),
                      m_tabPos.y() - kTabHeight * 0.5,
                      w + kTabArrow,
                      kTabHeight);
    }

    // ── QGraphicsItem interface ───────────────────────────────────────────────

    QRectF OghamConnectionItem::boundingRect() const
    {
        if (m_isSelfRef)
            return QRectF(m_tabPos.x() - 1.0, m_tabPos.y() - 13.0, 28.0, 26.0);
        if (m_displayAsTab)
        {
            const qreal maxW = kTabHoverW + kTabArrow;
            return QRectF(m_tabPos.x() - 1.0,
                          m_tabPos.y() - kTabHeight * 0.5 - 1.0,
                          maxW + 2.0,
                          kTabHeight + 2.0);
        }
        return m_path.boundingRect().adjusted(-12.0, -12.0, 12.0, 12.0);
    }

    QPainterPath OghamConnectionItem::shape() const
    {
        if (m_isSelfRef)
        {
            QPainterPath p;
            p.addRect(QRectF(m_tabPos.x(), m_tabPos.y() - 12.0, 26.0, 24.0));
            return p;
        }
        if (m_displayAsTab)
        {
            QPainterPath p;
            p.addRect(tabRect());
            return p;
        }
        QPainterPathStroker stroker;
        stroker.setWidth(12.0);
        return stroker.createStroke(m_path);
    }

    void OghamConnectionItem::paint(QPainter*                       painter,
                                    const QStyleOptionGraphicsItem* /*option*/,
                                    QWidget*                        /*widget*/)
    {
        painter->setRenderHint(QPainter::Antialiasing);

        if (m_isSelfRef)
        {
            // Self-referencing option: draw a loop/refresh icon (↻) at the output pin
            QFont f = painter->font();
            f.setPointSizeF(14.0);
            painter->setFont(f);
            painter->setPen(QPen(kColLine.lighter(150), 1.5));
            painter->drawText(
                QRectF(m_tabPos.x() + 2.0, m_tabPos.y() - 12.0, 24.0, 24.0),
                Qt::AlignCenter,
                QString(QChar(0x21BB)));  // ↻ CLOCKWISE OPEN CIRCLE ARROW
            return;
        }

        if (m_displayAsTab)
        {
            paintTab(painter);
            return;
        }

        // ── Wire mode ─────────────────────────────────────────────────────────
        painter->setPen(QPen(kColLine, 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(m_path);

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

    void OghamConnectionItem::paintTab(QPainter* painter) const
    {
        const QRectF  r    = tabRect();
        const qreal   pinX = m_tabPos.x();
        const qreal   pinY = m_tabPos.y();
        const qreal   y0   = pinY - kTabHeight * 0.5;
        const qreal   y1   = pinY + kTabHeight * 0.5;
        const qreal   x1   = r.right() - kTabArrow; // right edge of rect body (before chevron)
        const qreal   tip  = r.right();              // chevron tip

        // Flag/chevron shape
        QPainterPath tabPath;
        tabPath.moveTo(pinX, y0);
        tabPath.lineTo(x1,   y0);
        tabPath.lineTo(tip,  pinY);
        tabPath.lineTo(x1,   y1);
        tabPath.lineTo(pinX, y1);
        tabPath.closeSubpath();

        QColor bgColor;
        if (m_dstHighlight.isValid())
            bgColor = m_tabHovered ? m_dstHighlight.lighter(140) : m_dstHighlight.darker(120);
        else
            bgColor = m_tabHovered ? kColTabHoverBg : kColTabBg;
        painter->fillPath(tabPath, bgColor);
        painter->setPen(QPen(kColTabBorder, 1.0));
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(tabPath);

        // Label: full tag on hover, last segment by default
        const QString label = m_tabHovered
            ? m_targetTag
            : (m_targetTag.contains(QLatin1Char('.'))
                   ? m_targetTag.section(QLatin1Char('.'), -1)
                   : m_targetTag);

        QFont f = painter->font();
        f.setPointSizeF(7.5);
        f.setBold(false);
        f.setItalic(false);
        painter->setFont(f);
        painter->setPen(contrastTextColor(bgColor));

        const QFontMetrics fm(f);
        const qreal textW  = x1 - pinX - kTabPadX * 2.0;
        const QString estr = fm.elidedText(label, Qt::ElideRight, static_cast<int>(textW));
        painter->drawText(
            QRectF(pinX + kTabPadX, y0, textW, kTabHeight),
            Qt::AlignVCenter | Qt::AlignLeft,
            estr);
    }

    // ── Hover ─────────────────────────────────────────────────────────────────

    void OghamConnectionItem::hoverMoveEvent(QGraphicsSceneHoverEvent* event)
    {
        if (m_displayAsTab)
        {
            const bool over = tabRect().contains(event->scenePos());
            if (over != m_tabHovered)
            {
                prepareGeometryChange();
                m_tabHovered = over;
                if (over)
                {
                    m_savedZValue = zValue();
                    setZValue(50.0);   // float above all nodes/connections while hovered
                }
                else
                {
                    setZValue(m_savedZValue);
                }
                setCursor(over ? Qt::PointingHandCursor : Qt::ArrowCursor);
                update();
            }
            QGraphicsObject::hoverMoveEvent(event);
            return;
        }

        // Wire mode: highlight hovered redirect waypoints
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
        if (m_displayAsTab)
        {
            if (m_tabHovered)
            {
                prepareGeometryChange();
                m_tabHovered = false;
                setZValue(m_savedZValue);
                unsetCursor();
                update();
            }
            QGraphicsObject::hoverLeaveEvent(event);
            return;
        }

        if (m_hoverIdx != -1)
        {
            m_hoverIdx = -1;
            unsetCursor();
            update();
        }
        QGraphicsObject::hoverLeaveEvent(event);
    }

    // ── Mouse ─────────────────────────────────────────────────────────────────

    void OghamConnectionItem::mousePressEvent(QGraphicsSceneMouseEvent* event)
    {
        if (m_displayAsTab)
        {
            if (event->button() == Qt::LeftButton && tabRect().contains(event->scenePos()))
            {
                emit tabClicked(m_targetTag);
                event->accept();
                return;
            }
            QGraphicsObject::mousePressEvent(event);
            return;
        }

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

    void OghamConnectionItem::mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)
    {
        if (m_displayAsTab)
        {
            if (event->button() == Qt::LeftButton)
                emit tabClicked(m_targetTag);
            event->accept();
            return;
        }

        if (event->button() != Qt::LeftButton)
        {
            QGraphicsObject::mouseDoubleClickEvent(event);
            return;
        }
        const QPointF pos = event->scenePos();
        for (const QPointF& r : m_redirects)
            if (QLineF(pos, r).length() <= kRedirectHitRadius) { event->accept(); return; }

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

    // ── Context menu ──────────────────────────────────────────────────────────

    void OghamConnectionItem::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
    {
        const QPointF pos = event->scenePos();

        if (m_displayAsTab)
        {
            QMenu menu;
            menu.addAction(QStringLiteral("Show as Wire"), [this]()
            {
                m_displayAsTab = false;
                refreshPath();
                emit displayModeChanged(false);
            });
            menu.exec(event->screenPos());
            event->accept();
            return;
        }

        // Wire mode: check if over a redirect waypoint first
        for (int i = 0; i < m_redirects.size(); ++i)
        {
            if (QLineF(pos, m_redirects[i]).length() <= kRedirectHitRadius)
            {
                QMenu menu;
                menu.addAction(QStringLiteral("Remove Waypoint"),
                               [this, i]() { removeRedirectAt(i); });
                menu.exec(event->screenPos());
                event->accept();
                return;
            }
        }

        // Wire mode: clicked on the wire line itself
        QMenu menu;
        menu.addAction(QStringLiteral("Show as Tab"), [this]()
        {
            m_displayAsTab = true;
            refreshPath();
            emit displayModeChanged(true);
        });
        menu.addAction(QStringLiteral("Add Reroute Pin"), [this, pos]()
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
        if (m_hoverIdx == idx)      m_hoverIdx    = -1;
        else if (m_hoverIdx > idx)  --m_hoverIdx;
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
