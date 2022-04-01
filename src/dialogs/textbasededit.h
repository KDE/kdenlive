/*
    SPDX-FileCopyrightText: 2021 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

#include "ui_textbasededit_ui.h"
#include "definitions.h"
#include "pythoninterfaces/speechtotext.h"

#include <QProcess>
#include <QAction>
#include <QTextEdit>
#include <QMouseEvent>
#include <QTimer>
#include <QTemporaryFile>

class ProjectClip;

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
    /** @brief returns the link for the first word at position start in the text.
     *    This will seek forwards until a word is found in case selection starts with a space
     * @param cursor the current text cursor
     * @param start the first position of the selection
     * @param max the last position to check in seek operation*/
    const QString selectionStartAnchor(QTextCursor &cursor, int start, int max);
    /** @brief returns the link for the last word at position end in the text.
     *    This will seek backwards until a word is found in case selection ends with a space
     * @param cursor the current text cursor
     * @param end the last position of the selection
     * @param min the first position to check in seek operation*/
    const QString selectionEndAnchor(QTextCursor &cursor, int end, int min);
    void checkHoverBlock(int yPos);
    void blockClicked(Qt::KeyboardModifiers modifiers, bool play = false);
    QVector<QPoint> processedZones(const QVector<QPoint> &sourceZones);
    QVector<QPoint> getInsertZones();
    /** @brief Remove all text outside loadZones
     */
    void processCutZones(const QList <QPoint> &loadZones);
    void rebuildZones();
    QVector< QPair<double, double> > speechZones;
    QVector <QPoint> cutZones;
    QAction *bookmarkAction;
    QAction *deleteAction;
    
protected:
    void mouseMoveEvent(QMouseEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void resizeEvent(QResizeEvent *e) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private slots:
    void updateLineNumberArea(const QRect &rect, int dy);
    
private:
    QWidget *lineNumberArea;
    int m_hoveredBlock{-1};
    int m_lastClickedBlock{-1};
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
    void wheelEvent(QWheelEvent *e) override
    {
        qDebug()<<"==== WHEEL OVER LINEAREA";
        e->ignore();
        //QWidget::wheelEvent(e);
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
    ~TextBasedEdit() override;
    void openClip(std::shared_ptr<ProjectClip>);

public slots:
    void deleteItem();

private slots:
    void startRecognition();
    void slotProcessSpeech();
    void slotProcessSpeechError();
    void slotProcessSpeechStatus(int, QProcess::ExitStatus status);
    /** @brief insert currently selected zones to timeline */
    void insertToTimeline();
    /** @brief Preview current edited text in the clip monitor */
    void previewPlaylist(bool createNew = true);
    /** @brief Display info message */
    void showMessage(const QString &text, KMessageWidget::MessageType type, QAction *action = nullptr);
    void addBookmark();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    std::unique_ptr<QProcess> m_speechJob;
    std::unique_ptr<QProcess> m_tCodeJob;
    /** @brief Id of the master bin clip on which speech processing is done */
    QString m_binId;
    /** @brief Id of the playlist which is processed from the master clip */
    QString m_refId;
    QString m_sourceUrl;
    double m_clipDuration{0.};
    int m_lastPosition;
    QString m_errorString;
    QAction *m_logAction;
    QAction *m_voskConfig;
    QAction *m_currentMessageAction{nullptr};
    VideoTextEdit *m_visualEditor;
    QTextDocument m_document;
    QString m_playlist;
    QTimer m_hideTimer;
    double m_clipOffset;
    QTemporaryFile m_playlistWav;
    SpeechToText *m_stt;
};
