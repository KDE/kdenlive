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
            emit triggerUpdate();
            return true;
        }
    }
    if (event->type() == QEvent::FocusOut) {
        emit triggerUpdate();
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
    buttonApply->setIcon(QIcon::fromTheme(QStringLiteral("dialog-ok-apply")));
    buttonAdd->setIcon(QIcon::fromTheme(QStringLiteral("list-add")));
    buttonCut->setIcon(QIcon::fromTheme(QStringLiteral("edit-cut")));
    buttonIn->setIcon(QIcon::fromTheme(QStringLiteral("zone-in")));
    buttonOut->setIcon(QIcon::fromTheme(QStringLiteral("zone-out")));
    buttonDelete->setIcon(QIcon::fromTheme(QStringLiteral("edit-delete")));
    buttonStyle->setIcon(QIcon::fromTheme(QStringLiteral("format-text-color")));
    buttonLock->setIcon(QIcon::fromTheme(QStringLiteral("kdenlive-lock")));
    auto *keyFilter = new ShiftEnterFilter(this);
    subText->installEventFilter(keyFilter);
    connect(keyFilter, &ShiftEnterFilter::triggerUpdate, this, &SubtitleEdit::updateSubtitle);
    connect(subText, &KTextEdit::textChanged, this, [this]() {
        if (m_activeSub > -1) {
            buttonApply->setEnabled(true);
        }
    });
    connect(subText, &KTextEdit::cursorPositionChanged, this, [this]() {
        if (m_activeSub > -1) {
            buttonCut->setEnabled(true);
        }
    });

    connect(buttonStyle, &QToolButton::toggled, this, [this](bool toggle) { stackedWidget->setCurrentIndex(toggle ? 1 : 0); });

    m_position = new TimecodeDisplay(true, this);
    m_endPosition = new TimecodeDisplay(true, this);
    m_duration = new TimecodeDisplay(true, this);
    frame_position->setEnabled(false);
    buttonDelete->setEnabled(false);

    position_box->addWidget(m_position);
    auto *spacer = new QSpacerItem(1, 1, QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    position_box->addSpacerItem(spacer);
    spacer = new QSpacerItem(1, 1, QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    end_box->addWidget(m_endPosition);
    end_box->addSpacerItem(spacer);
    duration_box->addWidget(m_duration);
    spacer = new QSpacerItem(1, 1, QSizePolicy::MinimumExpanding, QSizePolicy::Maximum);
    duration_box->addSpacerItem(spacer);
    connect(m_position, &TimecodeDisplay::timeCodeEditingFinished, this, [this](int value) {
        updateSubtitle();
        if (buttonLock->isChecked()) {
            // Perform a move instead of a resize
            m_model->requestSubtitleMove(m_activeSub, GenTime(value, pCore->getCurrentFps()));
        } else {
            GenTime duration = m_endPos - GenTime(value, pCore->getCurrentFps());
            m_model->requestResize(m_activeSub, duration.frames(pCore->getCurrentFps()), false);
        }
    });
    connect(m_endPosition, &TimecodeDisplay::timeCodeEditingFinished, this, [this](int value) {
        updateSubtitle();
        if (buttonLock->isChecked()) {
            // Perform a move instead of a resize
            m_model->requestSubtitleMove(m_activeSub, GenTime(value, pCore->getCurrentFps()) - (m_endPos - m_startPos));
        } else {
            GenTime duration = GenTime(value, pCore->getCurrentFps()) - m_startPos;
            m_model->requestResize(m_activeSub, duration.frames(pCore->getCurrentFps()), true);
        }
    });
    connect(m_duration, &TimecodeDisplay::timeCodeEditingFinished, this, [this](int value) {
        updateSubtitle();
        m_model->requestResize(m_activeSub, value, true);
    });
    connect(buttonAdd, &QToolButton::clicked, this, [this]() { emit addSubtitle(subText->toPlainText()); });
    connect(buttonCut, &QToolButton::clicked, this, [this]() {
        if (m_activeSub > -1 && subText->hasFocus()) {
            int pos = subText->textCursor().position();
            updateSubtitle();
            emit cutSubtitle(m_activeSub, pos);
        }
    });
    connect(buttonApply, &QToolButton::clicked, this, &SubtitleEdit::updateSubtitle);
    connect(buttonPrev, &QToolButton::clicked, this, &SubtitleEdit::goToPrevious);
    connect(buttonNext, &QToolButton::clicked, this, &SubtitleEdit::goToNext);
    connect(buttonIn, &QToolButton::clicked, []() { pCore->triggerAction(QStringLiteral("resize_timeline_clip_start")); });
    connect(buttonOut, &QToolButton::clicked, []() { pCore->triggerAction(QStringLiteral("resize_timeline_clip_end")); });
    connect(buttonDelete, &QToolButton::clicked, []() { pCore->triggerAction(QStringLiteral("delete_timeline_clip")); });
    buttonNext->setToolTip(i18n("Go to next subtitle"));
    buttonPrev->setToolTip(i18n("Go to previous subtitle"));
    buttonAdd->setToolTip(i18n("Add subtitle"));
    buttonCut->setToolTip(i18n("Split subtitle at cursor position"));
    buttonApply->setToolTip(i18n("Update subtitle text"));
    buttonStyle->setToolTip(i18n("Show style options"));
    buttonDelete->setToolTip(i18n("Delete subtitle"));

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
}

void SubtitleEdit::updateStyle()
{
    QString styleString;
    if (fontFamily->isEnabled()) {
        styleString.append(QStringLiteral("Fontname=%1,").arg(fontFamily->currentFont().family()));
    }
    if (fontSize->isEnabled()) {
        styleString.append(QStringLiteral("Fontsize=%1,").arg(fontSize->value()));
    }
    if (fontColor->isEnabled()) {
        QColor color = fontColor->color();
        QColor destColor(color.blue(), color.green(), color.red(), 255 - color.alpha());
        // Strip # character
        QString colorName = destColor.name(QColor::HexArgb);
        colorName.remove(0, 1);
        styleString.append(QStringLiteral("PrimaryColour=&H%1,").arg(colorName));
    }
    if (outlineSize->isEnabled()) {
        styleString.append(QStringLiteral("Outline=%1,").arg(outlineSize->value()));
    }
    if (shadowSize->isEnabled()) {
        styleString.append(QStringLiteral("Shadow=%1,").arg(shadowSize->value()));
    }
    if (outlineColor->isEnabled()) {
        // Qt AARRGGBB must be converted to AABBGGRR where AA is 255-AA
        QColor color = outlineColor->color();
        QColor destColor(color.blue(), color.green(), color.red(), 255 - color.alpha());
        // Strip # character
        QString colorName = destColor.name(QColor::HexArgb);
        colorName.remove(0, 1);
        styleString.append(QStringLiteral("OutlineColour=&H%1,").arg(colorName));
    }
    if (checkOpaque->isChecked()) {
        styleString.append(QStringLiteral("BorderStyle=3,"));
    }
    if (alignment->isEnabled()) {
        styleString.append(QStringLiteral("Alignment=%1,").arg(alignment->currentData().toInt()));
    }
    m_model->setStyle(styleString);
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
        QSignalBlocker bk2(m_position);
        QSignalBlocker bk3(m_endPosition);
        QSignalBlocker bk4(m_duration);
        subText->setPlainText(m_model->getText(id));
        m_startPos = m_model->getStartPosForId(id);
        GenTime duration = GenTime(m_model->getSubtitlePlaytime(id), pCore->getCurrentFps());
        m_endPos = m_startPos + duration;
        m_position->setValue(m_startPos);
        m_endPosition->setValue(m_endPos);
        m_duration->setValue(duration);
        m_position->setEnabled(true);
        m_endPosition->setEnabled(true);
        m_duration->setEnabled(true);
    } else {
        m_position->setEnabled(false);
        m_endPosition->setEnabled(false);
        m_duration->setEnabled(false);
        stackedWidget->widget(0)->setEnabled(false);
        buttonDelete->setEnabled(false);
        QSignalBlocker bk(subText);
        subText->clear();
    }
}

void SubtitleEdit::goToPrevious()
{
    if (m_model) {
        int id = -1;
        if (m_activeSub > -1) {
            id = m_model->getPreviousSub(m_activeSub);
        } else {
            // Start from timeline cursor position
            int cursorPos = pCore->getTimelinePosition();
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
}

void SubtitleEdit::goToNext()
{
    if (m_model) {
        int id = -1;
        if (m_activeSub > -1) {
            id = m_model->getNextSub(m_activeSub);
        } else {
            // Start from timeline cursor position
            int cursorPos = pCore->getTimelinePosition();
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
}
