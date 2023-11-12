/*
    SPDX-FileCopyrightText: 2020 Jean-Baptiste Mardelle
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#include "subtitleedit.h"
#include "bin/model/subtitlemodel.hpp"
#include "monitor/monitor.h"

#include "core.h"
#include "kdenlivesettings.h"
#include "widgets/timecodedisplay.h"

#include "QTextEdit"
#include "klocalizedstring.h"

#include <QEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QToolButton>

ShiftEnterFilter::ShiftEnterFilter(QObject *parent)
    : QObject(parent)
{
}

bool ShiftEnterFilter::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if ((keyEvent->modifiers() & Qt::ShiftModifier) && ((keyEvent->key() == Qt::Key_Enter) || (keyEvent->key() == Qt::Key_Return))) {
            Q_EMIT triggerUpdate();
            return true;
        }
    }
    if (event->type() == QEvent::FocusOut) {
        Q_EMIT triggerUpdate();
        return true;
    }
    return QObject::eventFilter(obj, event);
}

SubtitleEdit::SubtitleEdit(QWidget *parent)
    : QWidget(parent)
    , m_model(nullptr)
{
    setFont(QFontDatabase::systemFont(QFontDatabase::SmallestReadableFont));
    setupUi(this);
    auto *keyFilter = new ShiftEnterFilter(this);
    subText->installEventFilter(keyFilter);
    connect(keyFilter, &ShiftEnterFilter::triggerUpdate, this, &SubtitleEdit::updateSubtitle);
    connect(subText, &KTextEdit::textChanged, this, [this]() {
        if (m_activeSub > -1) {
            buttonApply->setEnabled(true);
        }
        updateCharInfo();
    });
    connect(subText, &KTextEdit::cursorPositionChanged, this, [this]() {
        if (m_activeSub > -1) {
            buttonCut->setEnabled(true);
        }
        updateCharInfo();
    });

    connect(buttonStyle, &QToolButton::toggled, this, [this](bool toggle) { stackedWidget->setCurrentIndex(toggle ? 1 : 0); });

    frame_position->setEnabled(false);
    buttonDelete->setEnabled(false);

    connect(tc_position, &TimecodeDisplay::timeCodeEditingFinished, this, [this](int value) {
        updateSubtitle();
        if (buttonLock->isChecked()) {
            // Perform a move instead of a resize
            m_model->requestSubtitleMove(m_activeSub, GenTime(value, pCore->getCurrentFps()));
        } else {
            GenTime duration = m_endPos - GenTime(value, pCore->getCurrentFps());
            m_model->requestResize(m_activeSub, duration.frames(pCore->getCurrentFps()), false);
        }
    });
    connect(tc_end, &TimecodeDisplay::timeCodeEditingFinished, this, [this](int value) {
        updateSubtitle();
        if (buttonLock->isChecked()) {
            // Perform a move instead of a resize
            m_model->requestSubtitleMove(m_activeSub, GenTime(value, pCore->getCurrentFps()) - (m_endPos - m_startPos));
        } else {
            GenTime duration = GenTime(value, pCore->getCurrentFps()) - m_startPos;
            m_model->requestResize(m_activeSub, duration.frames(pCore->getCurrentFps()), true);
        }
    });
    connect(tc_duration, &TimecodeDisplay::timeCodeEditingFinished, this, [this](int value) {
        updateSubtitle();
        m_model->requestResize(m_activeSub, value, true);
    });
    connect(buttonAdd, &QToolButton::clicked, this, [this]() { Q_EMIT addSubtitle(subText->toPlainText()); });
    connect(buttonCut, &QToolButton::clicked, this, [this]() {
        if (m_activeSub > -1 && subText->hasFocus()) {
            int pos = subText->textCursor().position();
            updateSubtitle();
            Q_EMIT cutSubtitle(m_activeSub, pos);
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
    buttonStyle->setToolTip(i18n("Show style options"));
    buttonStyle->setWhatsThis(xi18nc("@info:whatsthis", "Toggles a list to adjust font, size, color, outline style, shadow, position and background."));
    buttonDelete->setToolTip(i18n("Delete subtitle"));
    buttonDelete->setWhatsThis(xi18nc("@info:whatsthis", "Deletes the currently selected subtitle (doesn’t work on multiple subtitles)."));

    // Styling dialog
    connect(fontSize, QOverload<int>::of(&QSpinBox::valueChanged), this, &SubtitleEdit::updateStyle);
    connect(outlineSize, QOverload<int>::of(&QSpinBox::valueChanged), this, &SubtitleEdit::updateStyle);
    connect(shadowSize, QOverload<int>::of(&QSpinBox::valueChanged), this, &SubtitleEdit::updateStyle);
    connect(fontFamily, &QFontComboBox::currentFontChanged, this, &SubtitleEdit::updateStyle);
    connect(fontColor, &KColorButton::changed, this, &SubtitleEdit::updateStyle);
    connect(outlineColor, &KColorButton::changed, this, &SubtitleEdit::updateStyle);
    connect(checkFont, &QCheckBox::toggled, this, &SubtitleEdit::updateStyle);
    connect(checkFontSize, &QCheckBox::toggled, this, &SubtitleEdit::updateStyle);
    connect(checkFontColor, &QCheckBox::toggled, this, &SubtitleEdit::updateStyle);
    connect(checkOutlineColor, &QCheckBox::toggled, this, &SubtitleEdit::updateStyle);
    connect(checkOutlineSize, &QCheckBox::toggled, this, &SubtitleEdit::updateStyle);
    connect(checkPosition, &QCheckBox::toggled, this, &SubtitleEdit::updateStyle);
    connect(checkShadowSize, &QCheckBox::toggled, this, &SubtitleEdit::updateStyle);
    connect(checkOpaque, &QCheckBox::toggled, this, &SubtitleEdit::updateStyle);
    alignment->addItem(i18n("Bottom Center"), 2);
    alignment->addItem(i18n("Bottom Left"), 1);
    alignment->addItem(i18n("Bottom Right"), 3);
    alignment->addItem(i18n("Center Left"), 9);
    alignment->addItem(i18n("Center"), 10);
    alignment->addItem(i18n("Center Right"), 11);
    alignment->addItem(i18n("Top Left"), 4);
    alignment->addItem(i18n("Top Center"), 6);
    alignment->addItem(i18n("Top Right"), 7);
    connect(alignment, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SubtitleEdit::updateStyle);
    QAction *zoomIn = new QAction(QIcon::fromTheme(QStringLiteral("zoom-in")), i18n("Zoom In"), this);
    connect(zoomIn, &QAction::triggered, this, &SubtitleEdit::slotZoomIn);
    QAction *zoomOut = new QAction(QIcon::fromTheme(QStringLiteral("zoom-out")), i18n("Zoom Out"), this);
    connect(zoomOut, &QAction::triggered, this, &SubtitleEdit::slotZoomOut);
    QMenu *menu = new QMenu(this);
    menu->addAction(zoomIn);
    menu->addAction(zoomOut);
    subMenu->setMenu(menu);
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
    if (KdenliveSettings::subtitleEditFontSize() > 0) {
        QTextCursor cursor = subText->textCursor();
        subText->selectAll();
        subText->setFontPointSize(KdenliveSettings::subtitleEditFontSize());
        subText->setTextCursor(cursor);
    }
}

void SubtitleEdit::updateStyle()
{
    QStringList styleString;
    if (fontFamily->isEnabled()) {
        styleString << QStringLiteral("Fontname=%1").arg(fontFamily->currentFont().family());
    }
    if (fontSize->isEnabled()) {
        styleString << QStringLiteral("Fontsize=%1").arg(fontSize->value());
    }
    if (fontColor->isEnabled()) {
        QColor color = fontColor->color();
        QColor destColor(color.blue(), color.green(), color.red(), 255 - color.alpha());
        // Strip # character
        QString colorName = destColor.name(QColor::HexArgb);
        colorName.remove(0, 1);
        styleString << QStringLiteral("PrimaryColour=&H%1").arg(colorName);
    }
    if (outlineSize->isEnabled()) {
        styleString << QStringLiteral("Outline=%1").arg(outlineSize->value());
    }
    if (shadowSize->isEnabled()) {
        styleString << QStringLiteral("Shadow=%1").arg(shadowSize->value());
    }
    if (outlineColor->isEnabled()) {
        // Qt AARRGGBB must be converted to AABBGGRR where AA is 255-AA
        QColor color = outlineColor->color();
        QColor destColor(color.blue(), color.green(), color.red(), 255 - color.alpha());
        // Strip # character
        QString colorName = destColor.name(QColor::HexArgb);
        colorName.remove(0, 1);
        styleString << QStringLiteral("OutlineColour=&H%1").arg(colorName);
    }
    if (checkOpaque->isChecked()) {
        QColor color = outlineColor->color();
        if (color.alpha() < 255) {
            // To avoid alpha overlay with multi lines, draw only one box
            QColor destColor(color.blue(), color.green(), color.red(), 255 - color.alpha());
            // Strip # character
            QString colorName = destColor.name(QColor::HexArgb);
            colorName.remove(0, 1);
            styleString << QStringLiteral("BorderStyle=4") << QStringLiteral("BackColour=&H%1").arg(colorName);
        } else {
            styleString << QStringLiteral("BorderStyle=3");
        }
    }
    if (alignment->isEnabled()) {
        styleString << QStringLiteral("Alignment=%1").arg(alignment->currentData().toInt());
    }
    m_model->setStyle(styleString.join(QLatin1Char(',')));
}

void SubtitleEdit::setModel(std::shared_ptr<SubtitleModel> model)
{
    m_model = model;
    m_activeSub = -1;
    buttonApply->setEnabled(false);
    buttonCut->setEnabled(false);
    if (m_model == nullptr) {
        QSignalBlocker bk(subText);
        subText->clear();
        loadStyle(QString());
        frame_position->setEnabled(false);
    } else {
        connect(m_model.get(), &SubtitleModel::updateSubtitleStyle, this, &SubtitleEdit::loadStyle);
        connect(m_model.get(), &SubtitleModel::dataChanged, this, [this](const QModelIndex &start, const QModelIndex &, const QVector<int> &roles) {
            if (m_activeSub > -1 && start.row() == m_model->getRowForId(m_activeSub)) {
                if (roles.contains(SubtitleModel::SubtitleRole) || roles.contains(SubtitleModel::StartFrameRole) ||
                    roles.contains(SubtitleModel::EndFrameRole)) {
                    setActiveSubtitle(m_activeSub);
                }
            }
        });
        frame_position->setEnabled(true);
        stackedWidget->widget(0)->setEnabled(false);
    }
}

void SubtitleEdit::loadStyle(const QString &style)
{
    QStringList params = style.split(QLatin1Char(','));
    // Read style params
    QSignalBlocker bk1(checkFont);
    QSignalBlocker bk2(checkFontSize);
    QSignalBlocker bk3(checkFontColor);
    QSignalBlocker bk4(checkOutlineColor);
    QSignalBlocker bk5(checkOutlineSize);
    QSignalBlocker bk6(checkShadowSize);
    QSignalBlocker bk7(checkPosition);
    QSignalBlocker bk8(checkOpaque);

    checkFont->setChecked(false);
    checkFontSize->setChecked(false);
    checkFontColor->setChecked(false);
    checkOutlineColor->setChecked(false);
    checkOutlineSize->setChecked(false);
    checkShadowSize->setChecked(false);
    checkPosition->setChecked(false);
    checkOpaque->setChecked(false);

    fontFamily->setEnabled(false);
    fontSize->setEnabled(false);
    fontColor->setEnabled(false);
    outlineColor->setEnabled(false);
    outlineSize->setEnabled(false);
    shadowSize->setEnabled(false);
    alignment->setEnabled(false);

    for (const QString &p : params) {
        const QString pName = p.section(QLatin1Char('='), 0, 0);
        QString pValue = p.section(QLatin1Char('='), 1);
        if (pName == QLatin1String("Fontname")) {
            checkFont->setChecked(true);
            QFont font(pValue);
            QSignalBlocker bk(fontFamily);
            fontFamily->setEnabled(true);
            fontFamily->setCurrentFont(font);
        } else if (pName == QLatin1String("Fontsize")) {
            checkFontSize->setChecked(true);
            QSignalBlocker bk(fontSize);
            fontSize->setEnabled(true);
            fontSize->setValue(pValue.toInt());
        } else if (pName == QLatin1String("OutlineColour")) {
            checkOutlineColor->setChecked(true);
            pValue.replace(QLatin1String("&H"), QLatin1String("#"));
            QColor col(pValue);
            QColor result(col.blue(), col.green(), col.red(), 255 - col.alpha());
            QSignalBlocker bk(outlineColor);
            outlineColor->setEnabled(true);
            outlineColor->setColor(result);
        } else if (pName == QLatin1String("Outline")) {
            checkOutlineSize->setChecked(true);
            QSignalBlocker bk(outlineSize);
            outlineSize->setEnabled(true);
            outlineSize->setValue(pValue.toInt());
        } else if (pName == QLatin1String("Shadow")) {
            checkShadowSize->setChecked(true);
            QSignalBlocker bk(shadowSize);
            shadowSize->setEnabled(true);
            shadowSize->setValue(pValue.toInt());
        } else if (pName == QLatin1String("BorderStyle")) {
            checkOpaque->setChecked(true);
        } else if (pName == QLatin1String("Alignment")) {
            checkPosition->setChecked(true);
            QSignalBlocker bk(alignment);
            alignment->setEnabled(true);
            int ix = alignment->findData(pValue.toInt());
            if (ix > -1) {
                alignment->setCurrentIndex(ix);
            }
        } else if (pName == QLatin1String("PrimaryColour")) {
            checkFontColor->setChecked(true);
            pValue.replace(QLatin1String("&H"), QLatin1String("#"));
            QColor col(pValue);
            QColor result(col.blue(), col.green(), col.red(), 255 - col.alpha());
            QSignalBlocker bk(fontColor);
            fontColor->setEnabled(true);
            fontColor->setColor(result);
        }
    }
}

void SubtitleEdit::updateSubtitle()
{
    if (!buttonApply->isEnabled()) {
        return;
    }
    buttonApply->setEnabled(false);
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
        stackedWidget->widget(0)->setEnabled(true);
        buttonDelete->setEnabled(true);
        QSignalBlocker bk2(tc_position);
        QSignalBlocker bk3(tc_end);
        QSignalBlocker bk4(tc_duration);
        subText->setPlainText(m_model->getText(id));
        m_startPos = m_model->getStartPosForId(id);
        GenTime duration = GenTime(m_model->getSubtitlePlaytime(id), pCore->getCurrentFps());
        m_endPos = m_startPos + duration;
        tc_position->setValue(m_startPos);
        tc_end->setValue(m_endPos);
        tc_duration->setValue(duration);
        tc_position->setEnabled(true);
        tc_end->setEnabled(true);
        tc_duration->setEnabled(true);
    } else {
        tc_position->setEnabled(false);
        tc_end->setEnabled(false);
        tc_duration->setEnabled(false);
        stackedWidget->widget(0)->setEnabled(false);
        buttonDelete->setEnabled(false);
        QSignalBlocker bk(subText);
        subText->clear();
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
            std::unordered_set<int> sids = m_model->getItemsInRange(cursorPos, cursorPos);
            if (sids.empty()) {
                sids = m_model->getItemsInRange(0, cursorPos);
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
            std::unordered_set<int> sids = m_model->getItemsInRange(cursorPos, cursorPos);
            if (sids.empty()) {
                sids = m_model->getItemsInRange(cursorPos, -1);
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
    char_count->setText(i18n("Character: %1, total: <b>%2</b>", subText->textCursor().position(), subText->document()->characterCount()));
}
