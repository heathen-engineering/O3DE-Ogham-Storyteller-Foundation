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
#include <QPoint>
#include <QPointF>
#include <QStringList>
#include <QVector>
#endif

class QGraphicsSceneContextMenuEvent;
class QGraphicsSceneHoverEvent;
class QGraphicsSceneMouseEvent;

namespace FoundationOgham
{
    // =========================================================================
    // OghamNodeItem
    //
    // One dialogue entry rendered as a node on the graph canvas.
    // The body has three collapsible sections (Operations / Fields / Choices).
    // Hovering a section header reveals a [+] add button; hovering a content
    // row reveals a [x] remove button. Left-clicking a content row emits
    // fieldClicked so the host can open the appropriate inline modal.
    //
    // Data field rows are multiline and grow to show the full resolved value.
    // When multiple data fields are present, each is labelled with the last
    // segment of its lexicon key ID (e.g. "Text1:" from "Dialogue.Node.Text1").
    // Inline <a ...>text</a> link syntax is stripped to the human-readable text.
    //
    // Execution pins are drawn as right-pointing triangles (Unreal Blueprint
    // style): open outline = no connection, filled = connected.
    // The input pin sits on the left edge below the title bar; output pins
    // sit on the right edge of each Choices row.
    // =========================================================================
    class OghamNodeItem : public QGraphicsObject
    {
        Q_OBJECT

    public:
        static constexpr qreal kNodeWidth           = 330.0;
        static constexpr qreal kHeaderHeight        = 28.0;
        static constexpr qreal kSectionHeaderHeight = 20.0;
        static constexpr qreal kInputPinRowHeight   = 22.0;  ///< dedicated row for the input pin, between labels and Operations
        static constexpr qreal kPinS               = 7.0;
        static constexpr qreal kRowHeight           = 22.0;
        static constexpr qreal kBodyPadding         = 6.0;
        static constexpr qreal kFieldMinHeight      = 28.0;
        static constexpr qreal kFieldVPadding       = 4.0;

        /// Assigned label data (color + display name) resolved at construction time.
        using LabelData = QVector<QPair<QColor, QString>>;

        /// Global toggle: when true all nodes show label text; when false color-only pills.
        static bool s_labelsExpanded;

        explicit OghamNodeItem(int fileIdx, int entryIdx,
                               const QString&     tag,
                               bool               tagInRegistry,
                               const QStringList& operationLabels,
                               const QStringList& dataKeyIds,
                               const QStringList& dataValues,
                               const QStringList& optionLabels,
                               const LabelData&   assignedLabels  = {},
                               const QColor&      highlightColor  = QColor(),
                               QColor             headerColor     = QColor(0x2d, 0x5a, 0x8e),
                               QGraphicsItem*     parent          = nullptr);

        int     fileIdx()  const { return m_fileIdx;  }
        int     entryIdx() const { return m_entryIdx; }
        QString tag()      const { return m_tag;      }

        void setSnapToGrid(bool snap) { m_snapGrid = snap; }

        void setInputConnected(bool connected);
        void setOutputConnected(int optIdx, bool connected);

        QPointF inputPinScenePos()        const;
        QPointF outputPinScenePos(int i)  const;
        int     optionCount()             const { return m_optionLabels.size(); }
        int     outputPinAt(QPointF localPos) const;

        /// Y-centre of the input pin in node-local coords (centre of the dedicated pin row, below labels).
        qreal inputPinLocalY() const;

        // QGraphicsItem interface
        QRectF boundingRect() const override;
        void   paint(QPainter* painter,
                     const QStyleOptionGraphicsItem* option,
                     QWidget* widget) override;

    signals:
        void positionChanged(int fileIdx, int entryIdx, QPointF newPos);
        void entrySelected(int fileIdx, int entryIdx);
        void grabStarted(int fileIdx, int entryIdx);
        void grabEnded(int fileIdx, int entryIdx);
        /// Emitted when a section is toggled; invalidates outputPinScenePos.
        void layoutChanged(int fileIdx, int entryIdx);

        /// User tapped a content row (not a button) — open the appropriate modal.
        void fieldClicked(int fileIdx, int entryIdx, int section, int row, QPoint screenPos);
        /// User clicked the [+] button on a section header — add a new item.
        void sectionAddClicked(int fileIdx, int entryIdx, int section, QPoint screenPos);
        /// User clicked the [x] button on a content row — remove that item.
        void rowRemoveClicked(int fileIdx, int entryIdx, int section, int row);
        /// User clicked the ▲ reorder control on a content row — move it earlier.
        void rowMoveUpClicked(int fileIdx, int entryIdx, int section, int row);
        /// User clicked the ▼ reorder control on a content row — move it later.
        void rowMoveDownClicked(int fileIdx, int entryIdx, int section, int row);

        void createAliasPinRequested(int fileIdx, int entryIdx, QPointF scenePos);
        void cascadeFromNodeRequested(int fileIdx, int entryIdx, QPoint screenPos);
        void duplicateNodeRequested(int fileIdx, int entryIdx);
        void deleteNodeRequested(int fileIdx, int entryIdx);

        void tagStatusClicked(int fileIdx, int entryIdx, QPoint screenPos);
        void tagRenameRequested(int fileIdx, int entryIdx, QPoint screenPos);

        /// User clicked "Add Label…" in the node context menu.
        void addLabelRequested(int fileIdx, int entryIdx, QPoint screenPos);
        /// User clicked "Set Highlight Color…" in the node context menu.
        void setHighlightColorRequested(int fileIdx, int entryIdx);
        /// User clicked "Clear Highlight Color" in the node context menu.
        void clearHighlightColorRequested(int fileIdx, int entryIdx);
        /// User clicked a label pill — host should toggle s_labelsExpanded and repaint all.
        void labelPillClicked();

    protected:
        QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
        void contextMenuEvent(QGraphicsSceneContextMenuEvent* event)  override;
        void hoverMoveEvent(QGraphicsSceneHoverEvent*          event)  override;
        void hoverLeaveEvent(QGraphicsSceneHoverEvent*         event)  override;
        void mousePressEvent(QGraphicsSceneMouseEvent*   event)        override;
        void mouseReleaseEvent(QGraphicsSceneMouseEvent* event)        override;
        void mouseDoubleClickEvent(QGraphicsSceneMouseEvent* event)    override;

    private:
        // ── Layout helpers (node-local Y) ──────────────────────────────────────
        qreal labelSectionHeight()  const;  ///< 0 if no labels; else height of label row area
        qreal bodyHeight()          const;
        qreal opContentTopY()       const;
        qreal fieldsHdrTopY()       const;
        qreal fieldsContentTopY()   const;
        qreal choicesHdrTopY()      const;
        qreal choicesContentTopY()  const;

        /// Height of the i-th data field row (word-wrapped to fit value text).
        qreal fieldHeightAt(int i)   const;
        /// Total height consumed by all data field rows (expanded).
        qreal totalFieldsHeight()    const;
        /// Node-local Y of the TOP of the i-th data field content row.
        qreal fieldTopY(int i)       const;

        /// Build the full display string for a data field, including optional
        /// label prefix and with <a …>text</a> link syntax stripped.
        QString buildFieldDisplayText(int i) const;
        /// Strip all <a …>…</a> anchor tags, keeping only the inner text.
        static QString stripHtmlLinks(const QString& raw);

        /// Returns section index (0/1/2) if localPos is in that header strip, else -1.
        int sectionHeaderAt(QPointF localPos) const;
        void toggleSection(int sectionIdx);

        /// Returns {section, row} for a content row tap, or {-1,-1} for anything else.
        /// Excludes hits that land on the [x] remove button rect.
        QPair<int,int> rowContentAt(QPointF localPos) const;

        // Button rects in node-local coords
        QRectF addBtnRect(int section)             const; ///< [+] in section header
        QRectF removeBtnRect(int section, int row) const; ///< [x] on content row
        QRectF reorderBtnRect(int section, int row) const; ///< ▲▼ reorder; left of [x], shown when section has >1 items
        QRectF tagStatusRect()                     const; ///< status dot in header

        // Helper: draw a right-pointing triangle execution pin
        void drawPinTriangle(QPainter* painter, QPointF centre,
                             bool filled, const QColor& col) const;

        // ── Data ──────────────────────────────────────────────────────────────
        int         m_fileIdx;
        int         m_entryIdx;
        QString     m_tag;
        bool        m_tagInRegistry;
        QStringList m_operationLabels;
        QStringList m_dataKeyIds;
        QStringList m_dataKeys;
        QStringList m_optionLabels;
        LabelData   m_assignedLabels;   ///< resolved {color, name} pairs for label pills
        QColor      m_highlightColor;   ///< node border highlight; invalid = none
        QColor      m_headerColor;
        QPointF     m_pressPos;

        // ── Pin connection state ───────────────────────────────────────────────
        bool          m_inputConnected = false;
        QVector<bool> m_outputConnected;

        // ── Section collapse state ─────────────────────────────────────────────
        bool m_operationsExpanded = false;
        bool m_fieldsExpanded     = true;
        bool m_choicesExpanded    = true;

        bool m_snapGrid = false;

        // ── Hover state ───────────────────────────────────────────────────────
        int m_hoveredSection = -1;
        int m_hoveredRow     = -1;
    };

} // namespace FoundationOgham
