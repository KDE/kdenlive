/*
    SPDX-FileCopyrightText: 2020 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "subtitleedit.h"
#include "bin/model/subtitlemodel.hpp"
#include "doc/kdenlivedoc.h"
#include "monitor/monitor.h"

#include "core.h"
#include "dialogs/managesubtitles.h"
#include "dialogs/subtitlestyleedit.h"
#include "kdenlivesettings.h"
#include "widgets/subtitletextedit.h"
#include "widgets/timecodedisplay.h"

#include "QTextEdit"
#include "klocalizedstring.h"

#include <QCompleter>
#include <QEvent>
#include <QFontDialog>
#include <QInputDialog>
#include <QKeyEvent>
#include <QMenu>
#include <QToolButton>

const static QRegularExpression tagBlockRegex("(?<!\\\\){[^}]*}");
const static QRegularExpression escapeRegex("\\\\[{}N]");

SimpleEditorEventFilter::SimpleEditorEventFilter(QObject *parent)
    : QObject(parent)
{
}

bool SimpleEditorEventFilter::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->modifiers()) {
            switch (keyEvent->modifiers()) {
            case Qt::ShiftModifier:
                if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                    Q_EMIT triggerUpdate();
                    return true;
                } else if (!keyEvent->text().isEmpty()) {
                    Q_EMIT singleKeyPress(keyEvent);
                }
                break;
            case Qt::KeypadModifier:
                Q_EMIT singleKeyPress(keyEvent);
                break;
            default:
                Q_EMIT shortCut(keyEvent);
                return true;
                break;
            }
        } else {
            Q_EMIT singleKeyPress(keyEvent);
        }
    } else if (event->type() == QEvent::InputMethod) {
        QInputMethodEvent *inputMethodEvent = static_cast<QInputMethodEvent *>(event);
        if (!inputMethodEvent->commitString().isEmpty()) Q_EMIT inputMethod(inputMethodEvent);
    }
    return QObject::eventFilter(obj, event);
}

SubtitleEdit::SubtitleEdit(QWidget *parent)
    : QWidget(parent)
    , m_model(nullptr)
{
    setupUi(this);
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    auto *filter = new SimpleEditorEventFilter(this);
    simpleSubText->installEventFilter(filter);
    connect(filter, &SimpleEditorEventFilter::singleKeyPress, this, [this](QKeyEvent *event) {
        // Sync the key press event
        QTextCursor cursor = subText->textCursor();
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            cursor.insertText("\\N");
        } else if (event->key() == Qt::Key_BraceLeft) {
            cursor.insertText("\\{");
        } else if (event->key() == Qt::Key_BraceRight) {
            cursor.insertText("\\}");
        } else if (event->key() == Qt::Key_Backspace) {
            if (!cursor.hasSelection()) {
                int position = cursor.position();
                cursor = subText->selectOverrideBlock(cursor);
                if (cursor.hasSelection()) {
                    cursor.setPosition(cursor.selectionStart());
                } else {
                    cursor.setPosition(position);
                }
                if (!cursor.hasSelection() && (cursor.position() > 1)) {
                    if ((cursor.block().text().at(cursor.position() - 1) == '{' || cursor.block().text().at(cursor.position() - 1) == '}' ||
                         cursor.block().text().at(cursor.position() - 1) == 'N') &&
                        cursor.block().text().at(cursor.position() - 2) == '\\') {
                        cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, 2);
                    }
                }
            }
            cursor.deletePreviousChar();
        } else if (event->key() == Qt::Key_Delete) {
            if (!cursor.hasSelection()) {
                int position = cursor.position();
                cursor = subText->selectOverrideBlock(cursor);
                if (cursor.hasSelection()) {
                    cursor.setPosition(cursor.selectionEnd());
                } else {
                    cursor.setPosition(position);
                }
                if (!cursor.hasSelection() && (cursor.position() < cursor.block().text().length() - 1)) {
                    if ((cursor.block().text().at(cursor.position()) == '\\') &&
                        ((cursor.block().text().at(cursor.position() + 1) == '{') || cursor.block().text().at(cursor.position() + 1) == '}' ||
                         cursor.block().text().at(cursor.position() + 1) == 'N')) {
                        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 2);
                    }
                }
            }
            cursor.deleteChar();
        } else if (!event->text().isEmpty()) {
            cursor.insertText(event->text());
        }
        QTimer::singleShot(10, this, [&]() { syncSimpleText(); });
    });
    connect(filter, &SimpleEditorEventFilter::inputMethod, this, [this](QInputMethodEvent *event) {
        QTextCursor cursor = subText->textCursor();
        cursor.insertText(event->commitString());
        QTimer::singleShot(10, this, [&]() { syncSimpleText(); });
    });
    connect(filter, &SimpleEditorEventFilter::shortCut, this, [this](QKeyEvent * /*event*/) {
        // TODO: The shortcut hasn't been implemented yet
        QTimer::singleShot(10, this, [&]() { syncSimpleText(); });
    });

    connect(filter, &SimpleEditorEventFilter::triggerUpdate, this, &SubtitleEdit::updateSubtitle);
    connect(subText, &SubtitleTextEdit::triggerUpdate, this, &SubtitleEdit::updateSubtitle);
    connect(subText, &SubtitleTextEdit::textChanged, this, [this]() {
        if (m_activeSub > -1) {
            buttonApply->setEnabled(true);
        }
        updateOffset();
        updateCharInfo();
    });
    connect(subText, &SubtitleTextEdit::cursorPositionChanged, this, [this]() {
        if (m_activeSub > -1) {
            buttonCut->setEnabled(true);
        }
        updateCharInfo();
    });
    connect(simpleSubText, &KTextEdit::cursorPositionChanged, this, [this]() {
        QSignalBlocker bk(subText);
        // sync the cursor position
        auto convertPosition = [this](int position) {
            int newPosition = position;
            QList<int> usedIndex;
            while (true) {
                bool moved = false;
                for (uint i = 0; i < m_offsets.size(); i++) {
                    if (m_offsets[i].first < newPosition && !usedIndex.contains(i)) {
                        newPosition -= m_offsets[i].second.second;
                        newPosition += m_offsets[i].second.first;
                        usedIndex.append(i);
                        moved = true;
                    }
                }
                if (!moved) break;
            }
            return newPosition;
        };

        QTextCursor cursor = simpleSubText->textCursor();
        if (!cursor.hasSelection()) {
            int position = simpleSubText->textCursor().position();
            position = convertPosition(position);
            QTextCursor cursor = subText->textCursor();
            cursor.setPosition(position);
            subText->setTextCursor(cursor);
        } else {
            int anchor = simpleSubText->textCursor().anchor();
            int position = simpleSubText->textCursor().position();
            anchor = convertPosition(anchor);
            position = convertPosition(position);
            QTextCursor cursor = subText->textCursor();
            cursor.setPosition(anchor);
            cursor.setPosition(position, QTextCursor::KeepAnchor);
            subText->setTextCursor(cursor);
        }
        updateCharInfo();
    });
    connect(subLayer, &QSpinBox::valueChanged, this, [this]() {
        if (m_activeSub > -1) {
            m_model->requestSubtitleMove(m_activeSub, subLayer->value(), m_model->getSubtitlePosition(m_activeSub));
        }
    });

    connect(buttonMoreOption, &QToolButton::toggled, this, [this](bool toggle) { stackedWidget->setCurrentIndex(toggle ? 1 : 0); });

    frame_position->setEnabled(false);
    buttonDelete->setEnabled(false);

    connect(tc_position, &TimecodeDisplay::timeCodeEditingFinished, this, [this](int value) {
        updateSubtitle();
        if (buttonLock->isChecked()) {
            // Perform a move instead of a resize
            m_model->requestSubtitleMove(m_activeSub, m_layer, GenTime(value, pCore->getCurrentFps()));
        } else {
            GenTime duration = m_endPos - GenTime(value, pCore->getCurrentFps());
            m_model->requestResize(m_activeSub, duration.frames(pCore->getCurrentFps()), false);
        }
    });
    connect(tc_end, &TimecodeDisplay::timeCodeEditingFinished, this, [this](int value) {
        updateSubtitle();
        if (buttonLock->isChecked()) {
            // Perform a move instead of a resize
            m_model->requestSubtitleMove(m_activeSub, m_layer, GenTime(value, pCore->getCurrentFps()) - (m_endPos - m_startPos));
        } else {
            GenTime duration = GenTime(value, pCore->getCurrentFps()) - m_startPos;
            m_model->requestResize(m_activeSub, duration.frames(pCore->getCurrentFps()), true);
        }
    });
    connect(tc_duration, &TimecodeDisplay::timeCodeEditingFinished, this, [this](int value) {
        updateSubtitle();
        m_model->requestResize(m_activeSub, value, true);
    });

    connect(buttonEditStyle, &QToolButton::clicked, this, [this]() {
        bool ok;
        QString styleName = m_model->getStyleName(m_activeSub);
        SubtitleStyle style = SubtitleStyleEdit::getStyle(this, m_model->getSubtitleStyle(styleName), styleName, m_model, false, &ok);
        if (ok) {
            m_model->setSubtitleStyle(styleName, style);
            if (m_model->getStyleName(m_activeSub) != styleName) {
                comboStyle->addItem(styleName);
                comboStyle->setCurrentIndex(comboStyle->findText(styleName));
                // update the style of the subtitle automatically
            }
            pCore->refreshProjectMonitorOnce();
        }
    });
    buttonEditStyle->setToolTip(i18n("Edit subtitle style"));
    buttonEditStyle->setWhatsThis(xi18nc("@info:whatsthis", "Opens a dialog to edit the current subtitle."));
    connect(comboStyle, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        if (index == comboStyle->count() - 1) {
            // open the style editor
            Q_EMIT showSubtitleManager(2);
            setActiveSubtitle(m_activeSub);
            return;
        }
        if (m_activeSub > -1) {
            m_model->setStyleName(m_activeSub, comboStyle->currentText());
        }
    });
    connect(buttonAdd, &QToolButton::clicked, this, [this]() { Q_EMIT addSubtitle(subText->toPlainText()); });
    connect(buttonCut, &QToolButton::clicked, this, [this]() {
        if (m_activeSub > -1 && subText->hasFocus()) {
            int pos = subText->textCursor().position();
            updateSubtitle();
            Q_EMIT cutSubtitle(m_activeSub, pos);
            setActiveSubtitle(m_activeSub);
        }
    });
    connect(buttonApply, &QToolButton::clicked, this, &SubtitleEdit::updateSubtitle);
    connect(buttonPrev, &QToolButton::clicked, this, &SubtitleEdit::goToPrevious);
    connect(buttonNext, &QToolButton::clicked, this, &SubtitleEdit::goToNext);
    connect(buttonIn, &QToolButton::clicked, []() { pCore->triggerAction(QStringLiteral("resize_timeline_clip_start")); });
    connect(buttonOut, &QToolButton::clicked, []() { pCore->triggerAction(QStringLiteral("resize_timeline_clip_end")); });
    connect(buttonDelete, &QToolButton::clicked, []() { pCore->triggerAction(QStringLiteral("delete_timeline_clip")); });
    buttonNext->setToolTip(i18n("Go to next subtitle"));
    buttonNext->setWhatsThis(xi18nc("@info:whatsthis", "Moves the playhead in the timeline to the beginning of the subtitle to the right."));
    buttonPrev->setToolTip(i18n("Go to previous subtitle"));
    buttonPrev->setWhatsThis(xi18nc("@info:whatsthis", "Moves the playhead in the timeline to the beginning of the subtitle to the left."));
    buttonAdd->setToolTip(i18n("Add subtitle"));
    buttonAdd->setWhatsThis(xi18nc("@info:whatsthis", "Creates a new subtitle with the default length (set in <interface>Settings->Configure "
                                                      "Kdenlive…->Misc</interface>) at the current playhead position/frame."));
    buttonCut->setToolTip(i18n("Split subtitle at cursor position"));
    buttonCut->setWhatsThis(
        xi18nc("@info:whatsthis", "Cuts the subtitle text at the cursor position and creates a new subtitle to the right (like cutting a clip)."));
    buttonApply->setToolTip(i18n("Update subtitle text"));
    buttonApply->setWhatsThis(xi18nc("@info:whatsthis", "Updates the subtitle display in the timeline."));
    buttonMoreOption->setToolTip(i18n("Show more options"));
    buttonMoreOption->setWhatsThis(xi18nc("@info:whatsthis", "Toggles a list to adjust more subtitle settings."));
    buttonDelete->setToolTip(i18n("Delete subtitle"));
    buttonDelete->setWhatsThis(xi18nc("@info:whatsthis", "Deletes the currently selected subtitle (doesn’t work on multiple subtitles)."));

    connect(buttonBold, &QToolButton::clicked, this, &SubtitleEdit::slotToggleBold);
    buttonBold->setToolTip(i18n("Toggle bold"));
    buttonBold->setWhatsThis(xi18nc("@info:whatsthis", "Toggles the bold text style for either the selected text or the text following the cursor position."));

    connect(buttonItalic, &QToolButton::clicked, this, &SubtitleEdit::slotToggleItalic);
    buttonItalic->setToolTip(i18n("Toggle italic"));
    buttonItalic->setWhatsThis(
        xi18nc("@info:whatsthis", "Toggles the italic text style for either the selected text or the text following the cursor position."));

    connect(buttonUnderline, &QToolButton::clicked, this, &SubtitleEdit::slotToggleUnderline);
    buttonUnderline->setToolTip(i18n("Toggle underline"));
    buttonUnderline->setWhatsThis(
        xi18nc("@info:whatsthis", "Toggles the underline text style for either the selected text or the text following the cursor position."));

    connect(buttonStrikeout, &QToolButton::clicked, this, &SubtitleEdit::slotToggleStrikeOut);
    buttonStrikeout->setToolTip(i18n("Toggle strikeout"));
    buttonStrikeout->setWhatsThis(
        xi18nc("@info:whatsthis", "Toggles the strikeout text style for either the selected text or the text following the cursor position."));

    //\fn<name> Set the font name.
    //\fs<size> Set the size of the font.
    connect(buttonFont, &QToolButton::clicked, this, &SubtitleEdit::slotSelectFont);
    buttonFont->setToolTip(i18n("Set font"));
    buttonFont->setWhatsThis(
        xi18nc("@info:whatsthis",
               "Open a font dialog to select a font and set its properties for either the selected text or the text following the cursor position."));

    connect(buttonResetStyle, &QToolButton::clicked, this, &SubtitleEdit::slotResetStyle);
    buttonResetStyle->setToolTip(i18n("Reset style"));
    buttonResetStyle->setWhatsThis(xi18nc("@info:whatsthis", "Resets the style of either the selected text or the text following the cursor position."));

    QAction *tagPos = new QAction(QIcon::fromTheme(QStringLiteral("coordinate")), i18n("Set Position"), this);
    QMenu *moreTagsMenu = new QMenu(this);
    moreTagsMenu->addAction(tagPos);
    buttonMoreTags->setMenu(moreTagsMenu);
    buttonMoreTags->setToolTip(i18n("More tags"));
    buttonMoreTags->setWhatsThis(xi18nc("@info:whatsthis", "Opens a menu to show more tags"));

    // \pos(<X>,<Y>), X & Y are numbers, should only appear in the beginning of the text
    connect(tagPos, &QAction::triggered, this, &SubtitleEdit::slotSetPosition);

    // More options
    connect(checkComment, &QCheckBox::toggled, this, [this](bool checked) { m_model->setIsDialogue(m_activeSub, !checked); });
    connect(editName, &QLineEdit::editingFinished, this, [this]() { m_model->setName(m_activeSub, editName->text()); });
    connect(marginL, &QSpinBox::editingFinished, this, [this]() { m_model->setMarginL(m_activeSub, marginL->value()); });
    connect(marginR, &QSpinBox::editingFinished, this, [this]() { m_model->setMarginR(m_activeSub, marginR->value()); });
    connect(marginV, &QSpinBox::editingFinished, this, [this]() { m_model->setMarginV(m_activeSub, marginV->value()); });

    connect(checkScroll, &QCheckBox::toggled, this, [this](bool checked) {
        if (checked) {
            QSignalBlocker bk(editEffects);
            scrollDirection->setEnabled(true);
            scrollSpeed->setEnabled(true);
            editEffects->setEnabled(false);
            // editEffects->setText(m_model->getEffects(m_activeSub));
            updateEffects();
        } else {
            scrollDirection->setEnabled(false);
            scrollSpeed->setEnabled(false);
            scrollUpper->setEnabled(false);
            scrollLower->setEnabled(false);
            editEffects->setEnabled(true);
            editEffects->clear();
        }
    });

    connect(scrollDirection, &QComboBox::currentIndexChanged, this, [this](int /*index*/) {
        QSignalBlocker bk(editEffects);
        updateEffects();
    });

    connect(scrollSpeed, &QSpinBox::editingFinished, this, [this]() {
        QSignalBlocker bk(editEffects);
        updateEffects();
    });

    connect(scrollUpper, &QSpinBox::editingFinished, this, [this]() {
        QSignalBlocker bk(editEffects);
        updateEffects();
    });

    connect(scrollLower, &QSpinBox::editingFinished, this, [this]() {
        QSignalBlocker bk(editEffects);
        updateEffects();
    });

    connect(editEffects, &KTextEdit::textChanged, this, [this]() {
        QString effects = editEffects->toPlainText();
        if (effects.isEmpty())
            checkScroll->setEnabled(true);
        else
            checkScroll->setEnabled(false);
        updateEffects();
    });

    scrollDirection->addItem(i18n("Right To Left"));
    scrollDirection->addItem(i18n("Left To Right"));
    scrollDirection->addItem(i18n("Top To Bottom"));
    scrollDirection->addItem(i18n("Bottom To Top"));

    QAction *zoomIn = new QAction(QIcon::fromTheme(QStringLiteral("zoom-in")), i18n("Zoom In"), this);
    connect(zoomIn, &QAction::triggered, this, &SubtitleEdit::slotZoomIn);
    QAction *zoomOut = new QAction(QIcon::fromTheme(QStringLiteral("zoom-out")), i18n("Zoom Out"), this);
    connect(zoomOut, &QAction::triggered, this, &SubtitleEdit::slotZoomOut);
    QAction *clearAllTags = new QAction(QIcon::fromTheme(QStringLiteral("edit-clear")), i18n("Clear All Tags"), this);
    connect(clearAllTags, &QAction::triggered, this, [this]() {
        QTextCursor cursor = subText->textCursor();
        QString text = subText->toPlainText();
        text.remove(tagBlockRegex);
        subText->setPlainText(text);
        subText->setTextCursor(cursor);
        if (m_isSimpleEdit) {
            syncSimpleText();
        }
    });
    QAction *checkSimpleEditor = new QAction(QIcon::fromTheme(QStringLiteral("document-edit")), i18n("Simple Editor"), this);
    checkSimpleEditor->setCheckable(true);
    checkSimpleEditor->setChecked(false);
    simpleSubText->hide();
    connect(checkSimpleEditor, &QAction::triggered, this, [this](bool checked) {
        if (checked) {
            subText->hide();
            simpleSubText->show();
            m_isSimpleEdit = true;
            syncSimpleText();
            updateCharInfo();
        } else {
            simpleSubText->hide();
            subText->show();
            m_isSimpleEdit = false;
            updateCharInfo();
        }
    });
    QMenu *textMenu = new QMenu(this);
    textMenu->addAction(zoomIn);
    textMenu->addAction(zoomOut);
    textMenu->addAction(clearAllTags);
    textMenu->addAction(checkSimpleEditor);
    subMenu->setMenu(textMenu);
    applyFontSize();
}

void SubtitleEdit::slotZoomIn()
{
    QTextCursor cursor = subText->textCursor();
    subText->selectAll();
    qreal fontSize = QFontInfo(subText->currentFont()).pointSizeF() * 1.2;
    KdenliveSettings::setSubtitleEditFontSize(fontSize);
    subText->setFontPointSize(KdenliveSettings::subtitleEditFontSize());
    subText->setTextCursor(cursor);
}

void SubtitleEdit::slotZoomOut()
{
    QTextCursor cursor = subText->textCursor();
    subText->selectAll();
    qreal fontSize = QFontInfo(subText->currentFont()).pointSizeF() / 1.2;
    fontSize = qMax(fontSize, QFontInfo(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont)).pointSizeF());
    KdenliveSettings::setSubtitleEditFontSize(fontSize);
    subText->setFontPointSize(KdenliveSettings::subtitleEditFontSize());
    subText->setTextCursor(cursor);
}

void SubtitleEdit::applyFontSize()
{
    // check if the font size is different from the current font size to avoid redundant undo/redo history
    if (KdenliveSettings::subtitleEditFontSize() > 0 && subText->fontPointSize() != KdenliveSettings::subtitleEditFontSize()) {
        QTextCursor cursor = subText->textCursor();
        subText->selectAll();
        subText->setFontPointSize(KdenliveSettings::subtitleEditFontSize());
        subText->setTextCursor(cursor);
    }
}

void SubtitleEdit::setModel(std::shared_ptr<SubtitleModel> model)
{
    if (m_model) {
        disconnect(this, &SubtitleEdit::showSubtitleManager, m_model.get(), &SubtitleModel::showSubtitleManager);
    }
    m_model = model;
    m_activeSub = -1;
    buttonApply->setEnabled(false);
    buttonCut->setEnabled(false);
    if (m_model == nullptr) {
        QSignalBlocker bk(subText);
        subText->clear();
        frame_position->setEnabled(false);
    } else {
        connect(m_model.get(), &SubtitleModel::dataChanged, this, [this](const QModelIndex &start, const QModelIndex &, const QVector<int> &roles) {
            if (m_activeSub > -1 && start.row() == m_model->getRowForId(m_activeSub)) {
                if (roles.contains(SubtitleModel::SubtitleRole) || roles.contains(SubtitleModel::StartFrameRole) ||
                    roles.contains(SubtitleModel::EndFrameRole) || roles.contains(SubtitleModel::LayerRole) || roles.contains(SubtitleModel::StyleNameRole) ||
                    roles.contains(SubtitleModel::NameRole) || roles.contains(SubtitleModel::MarginLRole) || roles.contains(SubtitleModel::MarginRRole) ||
                    roles.contains(SubtitleModel::MarginVRole) || roles.contains(SubtitleModel::EffectRole) || roles.contains(SubtitleModel::IsDialogueRole)) {
                    setActiveSubtitle(m_activeSub);
                }
            }
        });
        connect(this, &SubtitleEdit::showSubtitleManager, m_model.get(), &SubtitleModel::showSubtitleManager);
        frame_position->setEnabled(true);
        stackedWidget->widget(0)->setEnabled(false);
    }
}

void SubtitleEdit::updateSubtitle()
{
    if (!buttonApply->isEnabled()) {
        return;
    }
    buttonApply->setEnabled(false);
    if (m_isSimpleEdit) {
        syncSimpleText();
    }
    if (m_activeSub > -1 && m_model) {
        QString txt = subText->toPlainText().trimmed();
        txt.replace(QLatin1String("\n\n"), QStringLiteral("\n"));
        if (subText->document()->defaultTextOption().textDirection() == Qt::RightToLeft && !txt.startsWith(QChar(0x200E))) {
            txt.prepend(QChar(0x200E));
        }
        m_model->setText(m_activeSub, txt);
    }
}

void SubtitleEdit::setActiveSubtitle(int id)
{
    m_activeSub = id;
    buttonApply->setEnabled(false);
    buttonCut->setEnabled(false);
    if (m_model && id > -1) {
        subText->setEnabled(true);
        QSignalBlocker bk(subText);
        stackedWidget->widget(1)->setEnabled(true);
        stackedWidget->widget(0)->setEnabled(true);
        buttonDelete->setEnabled(true);
        QSignalBlocker bk2(tc_position);
        QSignalBlocker bk3(tc_end);
        QSignalBlocker bk4(tc_duration);
        QSignalBlocker bk5(comboStyle);
        QSignalBlocker bk6(checkComment);
        QSignalBlocker bk7(editName);
        QSignalBlocker bk8(marginL);
        QSignalBlocker bk9(marginR);
        QSignalBlocker bk10(marginV);
        QSignalBlocker bk11(checkScroll);
        QSignalBlocker bk12(scrollDirection);
        QSignalBlocker bk13(scrollSpeed);
        QSignalBlocker bk14(scrollUpper);
        QSignalBlocker bk15(scrollLower);
        QSignalBlocker bk16(editEffects);
        QSignalBlocker bk17(subLayer);

        int anchor = subText->textCursor().anchor();
        int position = subText->textCursor().position();
        subText->setPlainText(m_model->getText(id));
        QTextCursor cursor = subText->textCursor();
        cursor.setPosition(anchor);
        cursor.setPosition(position, QTextCursor::KeepAnchor);
        subText->setTextCursor(cursor);

        if (!m_isSimpleEdit) {
            simpleSubText->hide();
            subText->show();
            updateCharInfo();
        } else {
            syncSimpleText();
            updateCharInfo();
        }

        m_layer = m_model->getLayerForId(id);
        m_startPos = m_model->getStartPosForId(id);
        GenTime duration = GenTime(m_model->getSubtitlePlaytime(id), pCore->getCurrentFps());
        m_endPos = m_startPos + duration;
        subLayer->setValue(m_layer);
        tc_position->setValue(m_startPos);
        tc_end->setValue(m_endPos);
        tc_duration->setValue(duration);
        tc_position->setEnabled(true);
        tc_end->setEnabled(true);
        tc_duration->setEnabled(true);
        QString styleName = m_model->getStyleName(id);
        comboStyle->clear();
        for (auto &style : m_model->getAllSubtitleStyles()) {
            comboStyle->addItem(style.first);
            if (style.first == styleName) {
                comboStyle->setCurrentIndex(comboStyle->count() - 1);
            }
        }
        comboStyle->addItem(i18n("Manage Styles…"), -1);
        checkComment->setChecked(!m_model->getIsDialogue(id));
        editName->setText(m_model->getName(id));
        marginL->setValue(m_model->getMarginL(id));
        marginR->setValue(m_model->getMarginR(id));
        marginV->setValue(m_model->getMarginV(id));

        // update the default values
        scrollUpper->setValue(0);
        scrollLower->setValue(pCore->getCurrentFrameDisplaySize().height());
        QString effects = m_model->getEffects(id);
        if (!effects.isEmpty()) {
            anchor = editEffects->textCursor().anchor();
            position = editEffects->textCursor().position();
            editEffects->setText(effects);
            cursor = editEffects->textCursor();
            cursor.setPosition(anchor);
            cursor.setPosition(position, QTextCursor::KeepAnchor);
            editEffects->setTextCursor(cursor);

            QStringList effectParts = effects.split(";");
            if (effectParts[0] == "Banner" || effectParts[0] == "Scroll up" || effectParts[0] == "Scroll down") {
                checkScroll->setChecked(true);
                checkScroll->setEnabled(true);
                scrollDirection->setEnabled(true);
                scrollSpeed->setEnabled(true);
                scrollUpper->setEnabled(true);
                scrollLower->setEnabled(true);
                editEffects->setEnabled(false);

                if (effectParts[0] == "Banner") {
                    scrollSpeed->setValue(effectParts[1].toInt());
                    if (effectParts.size() > 2) {
                        scrollDirection->setCurrentIndex(effectParts[2].toInt());
                    }
                    scrollUpper->setEnabled(false);
                    scrollLower->setEnabled(false);
                } else if (effectParts[0] == "Scroll up") {
                    scrollDirection->setCurrentIndex(3);
                    scrollUpper->setValue(effectParts[1].toInt());
                    scrollLower->setValue(effectParts[2].toInt());
                    scrollSpeed->setValue(effectParts[3].toInt());
                } else if (effectParts[0] == "Scroll down") {
                    scrollDirection->setCurrentIndex(2);
                    scrollUpper->setValue(effectParts[1].toInt());
                    scrollLower->setValue(effectParts[2].toInt());
                    scrollSpeed->setValue(effectParts[3].toInt());
                }
            } else {
                checkScroll->setChecked(false);
                checkScroll->setEnabled(false);
                scrollDirection->setEnabled(false);
                scrollSpeed->setEnabled(false);
                scrollUpper->setEnabled(false);
                scrollLower->setEnabled(false);
                editEffects->setEnabled(true);
            }
        } else {
            editEffects->clear();
            checkScroll->setChecked(false);
            checkScroll->setEnabled(true);
            scrollDirection->setEnabled(false);
            scrollSpeed->setEnabled(false);
            scrollUpper->setEnabled(false);
            scrollLower->setEnabled(false);
            editEffects->setEnabled(true);
        }
    } else {
        tc_position->setEnabled(false);
        tc_end->setEnabled(false);
        tc_duration->setEnabled(false);
        stackedWidget->widget(1)->setEnabled(false);
        stackedWidget->widget(0)->setEnabled(false);
        buttonDelete->setEnabled(false);
        QSignalBlocker bk(subText);
        subText->clear();
        simpleSubText->clear();
    }
    updateCharInfo();
    applyFontSize();
}

void SubtitleEdit::goToPrevious()
{
    if (m_model) {
        int id = -1;
        if (m_activeSub > -1) {
            id = m_model->getPreviousSub(m_activeSub);
        } else {
            // Start from timeline cursor position
            int cursorPos = pCore->getMonitorPosition();
            std::unordered_set<int> sids = m_model->getItemsInRange(m_layer, cursorPos, cursorPos);
            if (sids.empty()) {
                sids = m_model->getItemsInRange(m_layer, 0, cursorPos);
                for (int s : sids) {
                    if (id == -1 || m_model->getSubtitleEnd(s) > m_model->getSubtitleEnd(id)) {
                        id = s;
                    }
                }
            } else {
                id = m_model->getPreviousSub(*sids.begin());
            }
        }
        if (id > -1) {
            if (buttonApply->isEnabled()) {
                updateSubtitle();
            }
            GenTime prev = m_model->getStartPosForId(id);
            pCore->getMonitor(Kdenlive::ProjectMonitor)->requestSeek(prev.frames(pCore->getCurrentFps()));
            pCore->selectTimelineItem(id);
        }
    }
    updateCharInfo();
}

void SubtitleEdit::goToNext()
{
    if (m_model) {
        int id = -1;
        if (m_activeSub > -1) {
            id = m_model->getNextSub(m_activeSub);
        } else {
            // Start from timeline cursor position
            int cursorPos = pCore->getMonitorPosition();
            std::unordered_set<int> sids = m_model->getItemsInRange(m_layer, cursorPos, cursorPos);
            if (sids.empty()) {
                sids = m_model->getItemsInRange(m_layer, cursorPos, -1);
                for (int s : sids) {
                    if (id == -1 || m_model->getStartPosForId(s) < m_model->getStartPosForId(id)) {
                        id = s;
                    }
                }
            } else {
                id = m_model->getNextSub(*sids.begin());
            }
        }
        if (id > -1) {
            if (buttonApply->isEnabled()) {
                updateSubtitle();
            }
            GenTime prev = m_model->getStartPosForId(id);
            pCore->getMonitor(Kdenlive::ProjectMonitor)->requestSeek(prev.frames(pCore->getCurrentFps()));
            pCore->selectTimelineItem(id);
        }
    }
    updateCharInfo();
}

void SubtitleEdit::updateCharInfo()
{
    if (m_isSimpleEdit) {
        char_count->setText(i18n("Character: %1, total: <b>%2</b>", simpleSubText->textCursor().position(), simpleSubText->document()->characterCount()));
    } else {
        char_count->setText(i18n("Character: %1, total: <b>%2</b>", subText->textCursor().position(), subText->document()->characterCount()));
    }
}

void SubtitleEdit::updateEffects()
{
    if (m_activeSub < 0 || !m_model) return;

    if (checkScroll->isChecked()) {
        QString effects;
        switch (scrollDirection->currentIndex()) {
        case 0:
        case 1:
            // Banner;delay;direction
            effects = "Banner;" + scrollSpeed->text() + ";" + QString::number(scrollDirection->currentIndex());
            scrollUpper->setEnabled(false);
            scrollLower->setEnabled(false);
            break;
        case 2:
        case 3:
            // Scroll down/up;upper;lower;delay
            effects = ((scrollDirection->currentIndex() == 2) ? "Scroll down;" : "Scroll up;") + scrollUpper->text() + ";" + scrollLower->text() + ";" +
                      scrollSpeed->text();
            scrollUpper->setEnabled(true);
            scrollLower->setEnabled(true);
            break;
        }
        m_model->setEffects(m_activeSub, effects);
    } else {
        m_model->setEffects(m_activeSub, editEffects->toPlainText());
    }
}

void SubtitleEdit::syncSimpleText()
{

    QSignalBlocker bk(simpleSubText);

    // save the cursor position
    int anchor = simpleSubText->textCursor().anchor();
    int pos = simpleSubText->textCursor().position();

    // update the simple editor
    simpleSubText->setCurrentCharFormat(QTextCharFormat());
    simpleSubText->setPlainText(subText->toPlainText());

    // get the tags
    QString text = subText->toPlainText();
    QRegularExpressionMatch tagBlock = tagBlockRegex.match(text);
    while (tagBlock.hasMatch()) {
        QString tags = tagBlock.captured(0);
        int index = tagBlock.capturedStart(0);
        int length = tagBlock.capturedLength(0);

        // set format
        QTextCharFormat format;
        const static QRegularExpression fontWeightRegex("\\\\b\\d+");
        const static QRegularExpression fontItalicRegex("\\\\i\\d+");
        const static QRegularExpression fontUnderlineRegex("\\\\u\\d+");
        const static QRegularExpression fontStrikeOutRegex("\\\\s\\d+");
        QRegularExpressionMatch weightMatch = fontWeightRegex.match(tags);
        QRegularExpressionMatch italicMatch = fontItalicRegex.match(tags);
        QRegularExpressionMatch underlineMatch = fontUnderlineRegex.match(tags);
        QRegularExpressionMatch strikeOutMatch = fontStrikeOutRegex.match(tags);

        if (weightMatch.hasMatch()) {
            switch (weightMatch.captured(0).section("\\b", 1, 1).toInt()) {
            case 0:
                format.setFontWeight(QFont::Normal);
                break;
            case 1:
                format.setFontWeight(QFont::Bold);
                break;
            default:
                break;
            }
        }

        if (italicMatch.hasMatch()) {
            switch (italicMatch.captured(0).section("\\i", 1, 1).toInt()) {
            case 0:
                format.setFontItalic(false);
                break;
            case 1:
                format.setFontItalic(true);
                break;
            default:
                break;
            }
        }

        if (underlineMatch.hasMatch()) {
            switch (underlineMatch.captured(0).section("\\u", 1, 1).toInt()) {
            case 0:
                format.setFontUnderline(false);
                break;
            case 1:
                format.setFontUnderline(true);
                break;
            default:
                break;
            }
        }

        if (strikeOutMatch.hasMatch()) {
            switch (strikeOutMatch.captured(0).section("\\s", 1, 1).toInt()) {
            case 0:
                format.setFontStrikeOut(false);
                break;
            case 1:
                format.setFontStrikeOut(true);
                break;
            default:
                break;
            }
        }

        QTextCursor cursor = simpleSubText->textCursor();

        // remove the tag block from the text
        text.remove(index, length);

        // remove the tag block from the simpleSubText
        cursor.setPosition(index);
        cursor.setPosition(index + length, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();

        // set the format
        cursor.setPosition(index);
        cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor);
        cursor.mergeCharFormat(format);

        tagBlock = tagBlockRegex.match(text);
    }

    // convert the escaped "\{" and "\}" to "{" and "}", convert the '\N' to newline
    QTextCursor cursor = simpleSubText->textCursor();
    while (!cursor.atBlockEnd()) {
        QChar ch = cursor.block().text().at(cursor.positionInBlock());
        if (ch == '\\' && (cursor.positionInBlock() + 1) < cursor.block().length()) {
            if (cursor.block().text().at(cursor.positionInBlock() + 1) == '{' || cursor.block().text().at(cursor.positionInBlock() + 1) == '}') {
                cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 1);
                cursor.removeSelectedText();
                cursor.clearSelection();
                continue;
            } else if (cursor.block().text().at(cursor.positionInBlock() + 1) == 'N') {
                cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, 2);
                cursor.insertText("\n");
                cursor.clearSelection();
                continue;
            }
        }
        cursor.movePosition(QTextCursor::NextCharacter);
    }

    // restore the cursor position
    cursor.setPosition(anchor);
    cursor.setPosition(pos, QTextCursor::KeepAnchor);
    simpleSubText->setTextCursor(cursor);
}

void SubtitleEdit::slotToggleBold()
{
    static const QRegularExpression fontWeightRegex("\\\\b\\d+");
    QTextCursor cursor = subText->textCursor();
    auto checkBold = [this](QTextCursor cursor) {
        QTextCursor searchCursor = cursor;
        searchCursor.clearSelection();
        // check override block
        while (searchCursor.movePosition(QTextCursor::Left)) {
            searchCursor = subText->selectOverrideBlock(searchCursor);
            if (searchCursor.hasSelection()) {
                QRegularExpressionMatch match = fontWeightRegex.match(searchCursor.selectedText());
                if (match.hasMatch()) {
                    if (match.captured(0) == "\\b0") {
                        return false;
                    } else {
                        return true;
                    }
                }
                searchCursor.setPosition(searchCursor.selectionStart());
            }
        }
        // check the style
        if (m_model->getSubtitleStyle(m_model->getStyleName(m_activeSub)).bold()) {
            return true;
        } else {
            return false;
        }
    };
    if (!cursor.hasSelection()) {
        cursor = subText->selectOverrideBlock(cursor);
        if (cursor.hasSelection()) {
            // nearby '{' and '}' found
            Q_ASSERT(cursor.selectedText().startsWith('{') && cursor.selectedText().endsWith('}'));
            // check if there's already a bold tag
            QString text = cursor.selectedText();
            if (text.contains("\\b0")) {
                text.replace("\\b0", "\\b1");
            } else if (fontWeightRegex.match(text).hasMatch()) {
                // \b1 or \b<weight>
                text.replace(fontWeightRegex.match(text).captured(0), "\\b0");
            } else {
                if (checkBold(cursor)) {
                    text.insert(text.length() - 1, "\\b0");
                } else {
                    text.insert(text.length() - 1, "\\b1");
                }
            }
            cursor.insertText(text);
        } else {
            if (checkBold(cursor)) {
                cursor.insertText("{\\b0}");
            } else {
                cursor.insertText("{\\b1}");
            }
        }
    } else {
        // if text selected
        QTextCursor leftTagsBlock = cursor;
        leftTagsBlock.setPosition(cursor.selectionStart());
        leftTagsBlock = subText->selectOverrideBlock(leftTagsBlock);

        QTextCursor rightTagsBlock = cursor;
        rightTagsBlock.setPosition(cursor.selectionEnd());
        rightTagsBlock = subText->selectOverrideBlock(rightTagsBlock);

        if (leftTagsBlock.hasSelection()) {
            QString leftText = leftTagsBlock.selectedText();
            if (leftText.contains("\\b0")) {
                leftText.replace("\\b0", "\\b1");
            } else if (fontWeightRegex.match(leftText).hasMatch()) {
                // /b1 or /b<weight>
                leftText.replace(fontWeightRegex.match(leftText).captured(0), "\\b0");
            } else {
                if (checkBold(leftTagsBlock)) {
                    leftText.insert(leftText.length() - 1, "\\b0");
                } else {
                    leftText.insert(leftText.length() - 1, "\\b1");
                }
            }
            leftTagsBlock.insertText(leftText);
        } else {
            if (checkBold(leftTagsBlock)) {
                leftTagsBlock.insertText("{\\b0}");
            } else {
                leftTagsBlock.insertText("{\\b1}");
            }
        }

        // ensure leftTagsBlock and rightTagsBlock are not the same
        if (leftTagsBlock != rightTagsBlock) {
            if (rightTagsBlock.hasSelection()) {
                QString rightText = rightTagsBlock.selectedText();
                // restrict the range of effect
                if (!fontWeightRegex.match(rightText).hasMatch()) {
                    if (checkBold(rightTagsBlock)) {
                        rightText.insert(rightText.length() - 1, "\\b0");
                    } else {
                        rightText.insert(rightText.length() - 1, "\\b1");
                    }
                }
                rightTagsBlock.insertText(rightText);
            } else {
                if (checkBold(rightTagsBlock)) {
                    rightTagsBlock.insertText("{\\b0}");
                } else {
                    rightTagsBlock.insertText("{\\b1}");
                }
            }
        }

        // restore the cursor position
        leftTagsBlock = subText->selectOverrideBlock(leftTagsBlock);
        rightTagsBlock = subText->selectOverrideBlock(rightTagsBlock);
        cursor.setPosition(leftTagsBlock.selectionEnd());
        cursor.setPosition(rightTagsBlock.selectionStart(), QTextCursor::KeepAnchor);
        subText->setTextCursor(cursor);
    }
    if (buttonApply->isEnabled()) updateSubtitle();
    pCore->refreshProjectMonitorOnce();
}

void SubtitleEdit::slotToggleItalic()
{
    // use regex to fix wrong italic tags automatically
    static const QRegularExpression fontItalicRegex("\\\\i\\d+");
    auto checkItalic = [this](QTextCursor cursor) {
        QTextCursor searchCursor = cursor;
        searchCursor.clearSelection();
        // check override block
        while (searchCursor.movePosition(QTextCursor::Left)) {
            searchCursor = subText->selectOverrideBlock(searchCursor);
            if (searchCursor.hasSelection()) {
                QRegularExpressionMatch match = fontItalicRegex.match(searchCursor.selectedText());
                if (match.hasMatch()) {
                    if (match.captured(0) == "\\i0") {
                        return false;
                    } else {
                        return true;
                    }
                }
                searchCursor.setPosition(searchCursor.selectionStart());
            }
        }
        // check the style
        if (m_model->getSubtitleStyle(m_model->getStyleName(m_activeSub)).italic()) {
            return true;
        } else {
            return false;
        }
    };
    QTextCursor cursor = subText->textCursor();
    if (!cursor.hasSelection()) {
        cursor = subText->selectOverrideBlock(cursor);
        if (cursor.hasSelection()) {
            // nearby '{' and '}' found
            Q_ASSERT(cursor.selectedText().startsWith('{') && cursor.selectedText().endsWith('}'));
            // check if there's already a italic tag
            QString text = cursor.selectedText();
            if (text.contains("\\i0")) {
                text.replace("\\i0", "\\i1");
            } else if (fontItalicRegex.match(text).hasMatch()) {
                // \i1 or \i<wrong arg>, we can fix the wrong arg automatically
                text.replace(fontItalicRegex.match(text).captured(0), "\\i0");
            } else {
                if (checkItalic(cursor)) {
                    text.insert(text.length() - 1, "\\i0");
                } else {
                    text.insert(text.length() - 1, "\\i1");
                }
            }
            cursor.insertText(text);
        } else {
            if (checkItalic(cursor)) {
                cursor.insertText("{\\i0}");
            } else {
                cursor.insertText("{\\i1}");
            }
        }
    } else {
        // if text selected
        QTextCursor leftTagsBlock = cursor;
        leftTagsBlock.setPosition(cursor.selectionStart());
        leftTagsBlock = subText->selectOverrideBlock(leftTagsBlock);

        QTextCursor rightTagsBlock = cursor;
        rightTagsBlock.setPosition(cursor.selectionEnd());
        rightTagsBlock = subText->selectOverrideBlock(rightTagsBlock);

        if (leftTagsBlock.hasSelection()) {
            QString leftText = leftTagsBlock.selectedText();
            if (leftText.contains("\\i0")) {
                leftText.replace("\\i0", "\\i1");
            } else if (leftTagsBlock.selectedText().contains("\\i1")) {
                leftText.replace("\\i1", "\\i0");
            } else {
                if (checkItalic(leftTagsBlock)) {
                    leftText.insert(leftText.length() - 1, "\\i0");
                } else {
                    leftText.insert(leftText.length() - 1, "\\i1");
                }
            }
            leftTagsBlock.insertText(leftText);
        } else {
            if (checkItalic(leftTagsBlock)) {
                leftTagsBlock.insertText("{\\i0}");
            } else {
                leftTagsBlock.insertText("{\\i1}");
            }
        }

        if (leftTagsBlock != rightTagsBlock) {
            if (rightTagsBlock.hasSelection()) {
                QString rightText = rightTagsBlock.selectedText();
                // only add tag if there's no italic tag to restrict the range of effect
                if (!fontItalicRegex.match(rightText).hasMatch()) {
                    if (checkItalic(rightTagsBlock)) {
                        rightText.insert(rightText.length() - 1, "\\i0");
                    } else {
                        rightText.insert(rightText.length() - 1, "\\i1");
                    }
                }
                rightTagsBlock.insertText(rightText);
            } else {
                if (checkItalic(rightTagsBlock)) {
                    rightTagsBlock.insertText("{\\i0}");
                } else {
                    rightTagsBlock.insertText("{\\i1}");
                }
            }
        }

        // restore the cursor position
        leftTagsBlock = subText->selectOverrideBlock(leftTagsBlock);
        rightTagsBlock = subText->selectOverrideBlock(rightTagsBlock);
        cursor.setPosition(leftTagsBlock.selectionEnd());
        cursor.setPosition(rightTagsBlock.selectionStart(), QTextCursor::KeepAnchor);
        subText->setTextCursor(cursor);
    }
    if (buttonApply->isEnabled()) updateSubtitle();
    pCore->refreshProjectMonitorOnce();
}

void SubtitleEdit::slotToggleUnderline()
{
    static const QRegularExpression fontUnderlineRegex("\\\\u\\d+");
    auto checkUnderline = [this](QTextCursor cursor) {
        QTextCursor searchCursor = cursor;
        searchCursor.clearSelection();
        // check override block
        while (searchCursor.movePosition(QTextCursor::Left)) {
            searchCursor = subText->selectOverrideBlock(searchCursor);
            if (searchCursor.hasSelection()) {
                QRegularExpressionMatch match = fontUnderlineRegex.match(searchCursor.selectedText());
                if (match.hasMatch()) {
                    if (match.captured(0) == "\\u0") {
                        return false;
                    } else {
                        return true;
                    }
                }
                searchCursor.setPosition(searchCursor.selectionStart());
            }
        }
        // check the style
        if (m_model->getSubtitleStyle(m_model->getStyleName(m_activeSub)).underline()) {
            return true;
        } else {
            return false;
        }
    };
    QTextCursor cursor = subText->textCursor();
    if (!cursor.hasSelection()) {
        cursor = subText->selectOverrideBlock(cursor);
        if (cursor.hasSelection()) {
            // nearby '{' and '}' found
            Q_ASSERT(cursor.selectedText().startsWith('{') && cursor.selectedText().endsWith('}'));
            // check if there's already a underline tag
            QString text = cursor.selectedText();
            if (text.contains("\\u0")) {
                text.replace("\\u0", "\\u1");
            } else if (text.contains("\\u1")) {
                text.replace("\\u1", "\\u0");
            } else {
                if (checkUnderline(cursor)) {
                    text.insert(text.length() - 1, "\\u0");
                } else {
                    text.insert(text.length() - 1, "\\u1");
                }
            }
            cursor.insertText(text);
        } else {
            if (checkUnderline(cursor)) {
                cursor.insertText("{\\u0}");
            } else {
                cursor.insertText("{\\u1}");
            }
        }
    } else {
        // if text selected
        QTextCursor leftTagsBlock = cursor;
        leftTagsBlock.setPosition(cursor.selectionStart());
        leftTagsBlock = subText->selectOverrideBlock(leftTagsBlock);

        QTextCursor rightTagsBlock = cursor;
        rightTagsBlock.setPosition(cursor.selectionEnd());
        rightTagsBlock = subText->selectOverrideBlock(rightTagsBlock);

        if (leftTagsBlock.hasSelection()) {
            QString leftText = leftTagsBlock.selectedText();
            if (leftText.contains("\\u0")) {
                leftText.replace("\\u0", "\\u1");
            } else if (leftTagsBlock.selectedText().contains("\\u1")) {
                leftText.replace("\\u1", "\\u0");
            } else {
                if (checkUnderline(leftTagsBlock)) {
                    leftText.insert(leftText.length() - 1, "\\u0");
                } else {
                    leftText.insert(leftText.length() - 1, "\\u1");
                }
            }
            leftTagsBlock.insertText(leftText);
        } else {
            if (checkUnderline(leftTagsBlock)) {
                leftTagsBlock.insertText("{\\u0}");
            } else {
                leftTagsBlock.insertText("{\\u1}");
            }
        }

        if (leftTagsBlock != rightTagsBlock) {
            if (rightTagsBlock.hasSelection()) {
                QString rightText = rightTagsBlock.selectedText();
                if (!fontUnderlineRegex.match(rightText).hasMatch()) {
                    if (checkUnderline(rightTagsBlock)) {
                        rightText.insert(rightText.length() - 1, "\\u0");
                    } else {
                        rightText.insert(rightText.length() - 1, "\\u1");
                    }
                }
                rightTagsBlock.insertText(rightText);
            } else {
                if (checkUnderline(rightTagsBlock)) {
                    rightTagsBlock.insertText("{\\u0}");
                } else {
                    rightTagsBlock.insertText("{\\u1}");
                }
            }
        }

        // restore the cursor position
        leftTagsBlock = subText->selectOverrideBlock(leftTagsBlock);
        rightTagsBlock = subText->selectOverrideBlock(rightTagsBlock);
        cursor.setPosition(leftTagsBlock.selectionEnd());
        cursor.setPosition(rightTagsBlock.selectionStart(), QTextCursor::KeepAnchor);
        subText->setTextCursor(cursor);
    }
    if (buttonApply->isEnabled()) updateSubtitle();
    pCore->refreshProjectMonitorOnce();
}

void SubtitleEdit::slotToggleStrikeOut()
{

    static const QRegularExpression fontStrikeOutRegex("\\\\s\\d+");
    auto checkStrikeout = [this](QTextCursor cursor) {
        QTextCursor searchCursor = cursor;
        searchCursor.clearSelection();
        // check override block
        while (searchCursor.movePosition(QTextCursor::Left)) {
            searchCursor = subText->selectOverrideBlock(searchCursor);
            if (searchCursor.hasSelection()) {
                QRegularExpressionMatch match = fontStrikeOutRegex.match(searchCursor.selectedText());
                if (match.hasMatch()) {
                    if (match.captured(0) == "\\s0") {
                        return false;
                    } else {
                        return true;
                    }
                }
                searchCursor.setPosition(searchCursor.selectionStart());
            }
        }
        // check the style
        if (m_model->getSubtitleStyle(m_model->getStyleName(m_activeSub)).strikeOut()) {
            return true;
        } else {
            return false;
        }
    };
    QTextCursor cursor = subText->textCursor();
    if (!cursor.hasSelection()) {
        cursor = subText->selectOverrideBlock(cursor);
        if (cursor.hasSelection()) {
            // nearby '{' and '}' found
            Q_ASSERT(cursor.selectedText().startsWith('{') && cursor.selectedText().endsWith('}'));
            // check if there's already a strike tag
            QString text = cursor.selectedText();
            if (text.contains("\\s0")) {
                text.replace("\\s0", "\\s1");
            } else if (text.contains("\\s1")) {
                text.replace("\\s1", "\\s0");
            } else {
                if (checkStrikeout(cursor)) {
                    text.insert(text.length() - 1, "\\s0");
                } else {
                    text.insert(text.length() - 1, "\\s1");
                }
            }
            cursor.insertText(text);
        } else {
            if (checkStrikeout(cursor)) {
                cursor.insertText("{\\s0}");
            } else {
                cursor.insertText("{\\s1}");
            }
        }
    } else {
        // if text selected
        QTextCursor leftTagsBlock = cursor;
        leftTagsBlock.setPosition(cursor.selectionStart());
        leftTagsBlock = subText->selectOverrideBlock(leftTagsBlock);

        QTextCursor rightTagsBlock = cursor;
        rightTagsBlock.setPosition(cursor.selectionEnd());
        rightTagsBlock = subText->selectOverrideBlock(rightTagsBlock);

        if (leftTagsBlock.hasSelection()) {
            QString leftText = leftTagsBlock.selectedText();
            if (leftText.contains("\\s0")) {
                leftText.replace("\\s0", "\\s1");
            } else if (leftTagsBlock.selectedText().contains("\\s1")) {
                leftText.replace("\\s1", "\\s0");
            } else {
                if (checkStrikeout(leftTagsBlock)) {
                    leftText.insert(leftText.length() - 1, "\\s0");
                } else {
                    leftText.insert(leftText.length() - 1, "\\s1");
                }
            }
            leftTagsBlock.insertText(leftText);
        } else {
            if (checkStrikeout(leftTagsBlock)) {
                leftTagsBlock.insertText("{\\s0}");
            } else {
                leftTagsBlock.insertText("{\\s1}");
            }
        }

        if (leftTagsBlock != rightTagsBlock) {
            if (rightTagsBlock.hasSelection()) {
                QString rightText = rightTagsBlock.selectedText();
                if (!fontStrikeOutRegex.match(rightText).hasMatch()) {
                    if (checkStrikeout(rightTagsBlock)) {
                        rightText.insert(rightText.length() - 1, "\\s0");
                    } else {
                        rightText.insert(rightText.length() - 1, "\\s1");
                    }
                }
                rightTagsBlock.insertText(rightText);
            } else {
                if (checkStrikeout(rightTagsBlock)) {
                    rightTagsBlock.insertText("{\\s0}");
                } else {
                    rightTagsBlock.insertText("{\\s1}");
                }
            }
        }

        // restore the cursor position
        leftTagsBlock = subText->selectOverrideBlock(leftTagsBlock);
        rightTagsBlock = subText->selectOverrideBlock(rightTagsBlock);
        cursor.setPosition(leftTagsBlock.selectionEnd());
        cursor.setPosition(rightTagsBlock.selectionStart(), QTextCursor::KeepAnchor);
        subText->setTextCursor(cursor);
    }
    if (buttonApply->isEnabled()) updateSubtitle();
    pCore->refreshProjectMonitorOnce();
}

void SubtitleEdit::slotSelectFont()
{
    QTextCursor cursor = subText->textCursor();
    int cursorPos = cursor.selectionStart();
    cursor.clearSelection();
    cursor.setPosition(cursorPos);
    cursor = subText->selectOverrideBlock(cursor);
    QString text = cursor.selectedText();
    QFont oldFont;
    static const QRegularExpression sizeRegex("\\\\fs\\d+");
    static const QRegularExpression fontNameRegex("\\\\fn[^\\\\}]+");
    static const QRegularExpression fontWeightRegex("\\\\b\\d+");

    // load existing font style of the tag block
    if (cursor.hasSelection()) {
        bool italic = text.contains("\\i1");
        bool underline = text.contains("\\u1");
        bool strikeout = text.contains("\\s1");
        double fontSize = sizeRegex.match(text).hasMatch() ? sizeRegex.match(text).captured(0).section("\\fs", 1, 1).toDouble() : 60;
        QString fontName = fontNameRegex.match(text).hasMatch() ? fontNameRegex.match(text).captured(0).section("\\fn", 1, 1) : "Arial";
        int fontWeight = fontWeightRegex.match(text).hasMatch() ? fontWeightRegex.match(text).captured(0).section("\\b", 1, 1).toInt() : 0;
        if (fontWeight == 1) {
            oldFont.setBold(true);
        } else {
            oldFont.setBold(false);
        }
        oldFont.setFamily(fontName);
        oldFont.setItalic(italic);
        oldFont.setUnderline(underline);
        oldFont.setStrikeOut(strikeout);
        if (fontWeight != 0 && fontWeight != 1) {
            oldFont.setWeight(QFont::Weight(fontWeight));
        }
        oldFont.setPointSizeF(fontSize);
    }
    bool ok;
    QFont font = QFontDialog::getFont(&ok, oldFont, this);
    // open dialog leads to position changes of the cursor, so we need to reset the cursor position
    cursor.setPosition(cursorPos);
    cursor = subText->selectOverrideBlock(cursor);

    if (ok) {
        QString fontName = font.family();
        double fontSize = font.pointSizeF();
        int fontWeight = font.weight();
        bool italic = font.italic();
        bool underline = font.underline();
        bool strikeout = font.strikeOut();
        bool bold = font.bold();
        if (cursor.hasSelection()) {
            QString newText = text;

            if (newText.contains(fontNameRegex)) {
                newText.replace(fontNameRegex, QString("\\fn%1").arg(fontName));
            } else {
                newText.insert(newText.length() - 1, QString("\\fn%1").arg(fontName));
            }

            if (newText.contains(sizeRegex)) {
                newText.replace(sizeRegex, QString("\\fs%1").arg(fontSize));
            } else {
                newText.insert(newText.length() - 1, QString("\\fs%1").arg(fontSize));
            }

            if (!bold) {
                if (newText.contains(fontWeightRegex)) {
                    newText.replace(fontWeightRegex, QString("\\b%1").arg(fontWeight));
                } else {
                    newText.insert(newText.length() - 1, QString("\\b%1").arg(fontWeight));
                }
            }

            if (bold) {
                if (newText.contains(fontWeightRegex)) newText.replace(fontWeightRegex, "\\b1");
            }

            if (italic) {
                if (newText.contains("\\i0"))
                    newText.replace("\\i0", "\\i1");
                else if (!newText.contains("\\i1"))
                    newText.insert(newText.length() - 1, "\\i1");
            } else {
                if (newText.contains("\\i1")) newText.replace("\\i1", "\\i0");
            }

            if (underline) {
                if (newText.contains("\\u0"))
                    newText.replace("\\u0", "\\u1");
                else if (!newText.contains("\\u1"))
                    newText.insert(newText.length() - 1, "\\u1");
            } else {
                if (newText.contains("\\u1")) newText.replace("\\u1", "\\u0");
            }

            if (strikeout) {
                if (newText.contains("\\s0"))
                    newText.replace("\\s0", "\\s1");
                else if (!newText.contains("\\s1"))
                    newText.insert(newText.length() - 1, "\\s1");
            } else {
                if (newText.contains("\\s1")) newText.replace("\\s1", "\\s0");
            }

            cursor.insertText(newText);
        } else {
            cursor.insertText(QString("{\\fn%1\\fs%2\\b%3\\i%4\\u%5\\s%6}")
                                  .arg(fontName)
                                  .arg(fontSize)
                                  .arg(fontWeight)
                                  .arg(italic ? 1 : 0)
                                  .arg(underline ? 1 : 0)
                                  .arg(strikeout ? 1 : 0));
        }
        if (buttonApply->isEnabled()) updateSubtitle();
        // restore cursor position
        cursor.setPosition(cursorPos);
        subText->setTextCursor(cursor);
        pCore->refreshProjectMonitorOnce();
    }
}

void SubtitleEdit::slotResetStyle()
{
    QStringList styles;
    styles.append(i18n("None"));
    for (auto &style : m_model->getAllSubtitleStyles()) {
        styles.append(style.first);
    }
    QMenu *menu = new QMenu(this);
    for (const QString &style : styles) {
        menu->addAction(style);
    }

    QString newStyle;
    QAction *action = menu->exec(QCursor::pos());
    if (action) {
        newStyle = action->toolTip();
    } else {
        return;
    }

    if (newStyle == i18n("None")) {
        newStyle = "";
    }

    QTextCursor cursor = subText->textCursor();
    static const QRegularExpression styleRegex("\\\\r[^\\\\}]*");
    QString currentStyle = "";
    if (!cursor.hasSelection()) {
        // no text selected, try to select a nearby override block
        cursor = subText->selectOverrideBlock(cursor);
        if (cursor.hasSelection()) {
            // nearby '{' and '}' found
            Q_ASSERT(cursor.selectedText().startsWith('{') && cursor.selectedText().endsWith('}'));
            QString text = cursor.selectedText();
            currentStyle = styleRegex.match(text).hasMatch() ? styleRegex.match(text).captured(0) : i18n("None");

            int left = cursor.selectionStart();
            int right = cursor.selectionEnd();

            // reselect the text
            cursor.setPosition(left, QTextCursor::MoveAnchor);
            cursor.setPosition(right, QTextCursor::KeepAnchor);

            if (styleRegex.match(text).hasMatch()) {
                text.replace(styleRegex.match(text).captured(0), QString("\\r%1").arg(newStyle));
            } else {
                text.insert(text.length() - 1, QString("\\r%1").arg(newStyle));
            }
            cursor.insertText(text);
        } else {
            int left = cursor.selectionStart();
            int right = cursor.selectionEnd();

            // reselect the text
            cursor.setPosition(left, QTextCursor::MoveAnchor);
            cursor.setPosition(right, QTextCursor::KeepAnchor);

            cursor.insertText(QString("{\\r%1}").arg(newStyle));
        }
    } else {
        // if text selected
        int left = cursor.selectionStart();
        int right = cursor.selectionEnd();

        QTextCursor leftTagsBlock = cursor;
        leftTagsBlock.setPosition(left);
        leftTagsBlock = subText->selectOverrideBlock(leftTagsBlock);
        QString leftText = leftTagsBlock.selectedText();

        QTextCursor rightTagsBlock = cursor;
        rightTagsBlock.setPosition(right);
        rightTagsBlock = subText->selectOverrideBlock(rightTagsBlock);
        QString rightText = rightTagsBlock.selectedText();

        currentStyle = styleRegex.match(leftText).hasMatch() ? styleRegex.match(leftText).captured(0) : i18n("None");

        // reselect the text
        leftTagsBlock.setPosition(left);
        leftTagsBlock = subText->selectOverrideBlock(leftTagsBlock);

        rightTagsBlock.setPosition(right);
        rightTagsBlock = subText->selectOverrideBlock(rightTagsBlock);

        if (leftTagsBlock.hasSelection()) {
            if (styleRegex.match(leftText).hasMatch()) {
                leftText.replace(styleRegex.match(leftText).captured(0), QString("\\r%1").arg(newStyle));
            } else {
                leftText.insert(leftText.length() - 1, QString("\\r%1").arg(newStyle));
            }
            leftTagsBlock.insertText(leftText);
        } else {
            leftTagsBlock.insertText(QString("{\\r%1}").arg(newStyle));
        }

        if (leftTagsBlock != rightTagsBlock) {
            if (rightTagsBlock.hasSelection()) {
                if (styleRegex.match(rightText).hasMatch()) {
                    // we should keep the original style of the right block
                    ;
                } else {
                    // insert \r to make new style only take effect between the selected text
                    rightText.insert(rightText.length() - 1, QString("\\r"));
                }
                rightTagsBlock.insertText(rightText);
            } else {
                rightTagsBlock.insertText(QString("{\\r}"));
            }
        }
    }
    if (buttonApply->isEnabled()) updateSubtitle();
    pCore->refreshProjectMonitorOnce();
}

void SubtitleEdit::slotSetPosition()
{
    QTextCursor cursor = subText->textCursor();
    cursor.setPosition(0);
    cursor = subText->selectOverrideBlock(cursor);
    std::pair<int, int> subPos = {0, 0};
    if (cursor.hasSelection()) {
        QString text = cursor.selectedText();
        static const QRegularExpression posRegex("\\\\pos\\(\\d+,\\d+\\)");
        QRegularExpressionMatch match = posRegex.match(text);
        if (match.hasMatch()) {
            // read the current position
            QString pos = match.captured(0);
            pos = pos.split('(').last().split(')').first();
            subPos.first = pos.split(',').first().toInt();
            subPos.second = pos.split(',').last().toInt();
        }

        bool ok;
        int x = QInputDialog::getInt(this, i18n("Set Position X"), i18n("X:"), subPos.first, 0, 9999, 1, &ok);
        if (ok) {
            int y = QInputDialog::getInt(this, i18n("Set Position Y"), i18n("Y:"), subPos.second, 0, 9999, 1, &ok);
            if (ok) {
                if (match.hasMatch()) {
                    text.replace(posRegex, QString("\\pos(%1,%2)").arg(x).arg(y));
                } else {
                    text.insert(text.length() - 1, QString("\\pos(%1,%2)").arg(x).arg(y));
                }
                cursor.insertText(text);
            }
        }
    } else {
        bool ok;
        int x = QInputDialog::getInt(this, i18n("Set Position X"), i18n("X:"), subPos.first, 0, 9999, 1, &ok);
        if (ok) {
            int y = QInputDialog::getInt(this, i18n("Set Position Y"), i18n("Y:"), subPos.second, 0, 9999, 1, &ok);
            if (ok) {
                cursor.insertText(QString("{\\pos(%1,%2)}").arg(x).arg(y));
            }
        }
    }
    if (buttonApply->isEnabled()) updateSubtitle();
    pCore->refreshProjectMonitorOnce();
}

void SubtitleEdit::updateOffset()
{
    m_offsets.clear();

    // the tag block
    QRegularExpressionMatchIterator allTagBlocks = tagBlockRegex.globalMatch(subText->toPlainText());
    while (allTagBlocks.hasNext()) {
        QRegularExpressionMatch match = allTagBlocks.next();
        QString tagBlock = match.captured(0);
        int pos = match.capturedStart(0);
        int length = match.capturedLength(0);
        m_offsets.push_back({pos, {length, 0}});
    }

    // the '\{' , '\}' , '\N' escape sequence
    QRegularExpressionMatchIterator allEscapes = escapeRegex.globalMatch(subText->toPlainText());
    while (allEscapes.hasNext()) {
        QRegularExpressionMatch match = allEscapes.next();
        QString escape = match.captured(0);
        int pos = match.capturedStart(0);
        m_offsets.push_back({pos, {2, 1}});
    }
}
