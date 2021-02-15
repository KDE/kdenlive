/***************************************************************************
 *   Copyright (C) 2021 by Jean-Baptiste Mardelle                          *
 *   This file is part of Kdenlive. See www.kdenlive.org.                  *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) version 3 or any later version accepted by the       *
 *   membership of KDE e.V. (or its successor approved  by the membership  *
 *   of KDE e.V.), which shall act as a proxy defined in Section 14 of     *
 *   version 3 of the license.                                             *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>. *
 ***************************************************************************/

#ifndef TEXTBASEDEDIT_H
#define TEXTBASEDEDIT_H

#include "ui_textbasededit_ui.h"
#include "definitions.h"

#include <QProcess>
#include <QAction>
#include <QTextEdit>
#include <QMouseEvent>

/**
 * @class VideoTextEdit: Video speech text editor
 * @brief A dialog for editing markers and guides.
 * @author Jean-Baptiste Mardelle
 */

class VideoTextEdit : public QTextEdit
{
    Q_OBJECT

public:
    explicit VideoTextEdit(QWidget *parent = nullptr);
    void lineNumberAreaPaintEvent(QPaintEvent *event);
    int lineNumberAreaWidth();
    void repaintLines();
    void cleanup();
    void checkHoverBlock(int yPos);
    void blockClicked(Qt::KeyboardModifiers modifiers, bool play = false);
    QVector<QPoint> processedZones(QVector<QPoint> sourceZones);
    QVector<QPoint> getInsertZones(double offset);
    QVector< QPair<double, double> > m_zones;
    QVector <QPoint> cutZones;
    
protected:
    void mouseMoveEvent(QMouseEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    //void mouseReleaseEvent(QMouseEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;

private slots:
    void updateLineNumberArea(const QRect &rect, int dy);
    
private:
    QWidget *lineNumberArea;
    int m_hoveredBlock;
    int m_lastClickedBlock;
    QVector <int> m_selectedBlocks;
    int getFirstVisibleBlockId();
};

class LineNumberArea : public QWidget
{
public:
    LineNumberArea(VideoTextEdit *editor) : QWidget(editor), codeEditor(editor)
    {
        setMouseTracking(true);
    }

    QSize sizeHint() const override
    {
        return QSize(codeEditor->lineNumberAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        codeEditor->lineNumberAreaPaintEvent(event);
    }
    void mouseMoveEvent(QMouseEvent *e) override
    {
        codeEditor->checkHoverBlock(e->pos().y());
        QWidget::mouseMoveEvent(e);
    }
    void mousePressEvent(QMouseEvent *e) override
    {
        codeEditor->blockClicked(e->modifiers());
        QWidget::mousePressEvent(e);
        
    }
    void mouseDoubleClickEvent(QMouseEvent *e) override
    {
        codeEditor->blockClicked(e->modifiers(), true);
        QWidget::mouseDoubleClickEvent(e);
    }
    void leaveEvent(QEvent *e) override
    {
        codeEditor->checkHoverBlock(-1);
        QWidget::leaveEvent(e);
    }

private:
    VideoTextEdit *codeEditor;
};

/**
 * @class TextBasedEdit: Subtitle edit widget
 * @brief A dialog for editing markers and guides.
 * @author Jean-Baptiste Mardelle
 */

class TextBasedEdit : public QWidget, public Ui::TextBasedEdit_UI
{
    Q_OBJECT

public:
    explicit TextBasedEdit(QWidget *parent = nullptr);

public slots:
    void deleteItem();

private slots:
    void startRecognition();
    void slotProcessSpeech();
    void slotProcessSpeechError();
    void parseVoskDictionaries();
    void slotProcessSpeechStatus(int, QProcess::ExitStatus status);
    void updateAvailability();
    /** @brief insert currently selected zones to timeline */
    void insertToTimeline();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    std::unique_ptr<QProcess> m_speechJob;
    QString m_binId;
    QString m_sourceUrl;
    double m_clipDuration;
    double m_offset;
    int m_lastPosition;
    QString m_errorString;
    QAction *m_logAction;
    VideoTextEdit *m_visualEditor;
    QTextDocument m_document;
};

#endif
