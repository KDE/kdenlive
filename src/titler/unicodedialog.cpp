/***************************************************************************
 *   Copyright (C) 2008 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "unicodedialog.h"

#include <KConfigGroup>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <KSharedConfig>
#include <klocalizedstring.h>

/// CONSTANTS

const int MAX_LENGTH_HEX = 4;
const uint MAX_UNICODE_V1 = 65535;

UnicodeDialog::UnicodeDialog(InputMethod inputMeth, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(i18n("Details"));
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto *mainLayout = new QVBoxLayout(this);
    QPushButton *okButton = buttonBox->button(QDialogButtonBox::Ok);
    okButton->setDefault(true);
    okButton->setShortcut(Qt::CTRL | Qt::Key_Return);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    m_unicodeWidget = new UnicodeWidget(inputMeth);
    connect(m_unicodeWidget, &UnicodeWidget::charSelected, this, &UnicodeDialog::charSelected);
    mainLayout->addWidget(m_unicodeWidget);
    mainLayout->addWidget(buttonBox);
    connect(okButton, &QAbstractButton::clicked, this, &UnicodeDialog::slotAccept);
}

UnicodeDialog::~UnicodeDialog() = default;

void UnicodeDialog::slotAccept()
{
    m_unicodeWidget->slotReturnPressed();
    accept();
}

/// CONSTRUCTORS/DECONSTRUCTORS

UnicodeWidget::UnicodeWidget(UnicodeDialog::InputMethod inputMeth, QWidget *parent)
    : QWidget(parent)
    , m_inputMethod(inputMeth)
    , m_lastCursorPos(0)
{
    setupUi(this);
    readChoices();
    showLastUnicode();
    connect(unicodeNumber, &QLineEdit::textChanged, this, &UnicodeWidget::slotTextChanged);
    connect(unicodeNumber, &QLineEdit::returnPressed, this, &UnicodeWidget::slotReturnPressed);
    connect(arrowUp, &QAbstractButton::clicked, this, &UnicodeWidget::slotPrevUnicode);
    connect(arrowDown, &QAbstractButton::clicked, this, &UnicodeWidget::slotNextUnicode);

    switch (m_inputMethod) {
    case UnicodeDialog::InputHex:
        unicodeNumber->setMaxLength(MAX_LENGTH_HEX);
        break;

    case UnicodeDialog::InputDec:
        break;
    }

    arrowUp->setShortcut(Qt::Key_Up);
    arrowDown->setShortcut(Qt::Key_Down);
    unicode_link->setText(i18n("Information about unicode characters: <a href=\"https://decodeunicode.org\">https://decodeunicode.org</a>"));
    arrowUp->setToolTip(i18n("Previous Unicode character (Arrow Up)"));
    arrowDown->setToolTip(i18n("Next Unicode character (Arrow Down)"));
    unicodeNumber->setToolTip(i18n("Enter your Unicode number here. Allowed characters: [0-9] and [a-f]."));
    unicodeNumber->selectAll(); // Selection will be reset by setToolTip and similar, so set it here
}

UnicodeWidget::~UnicodeWidget() = default;
/// METHODS

void UnicodeWidget::showLastUnicode()
{
    unicodeNumber->setText(m_lastUnicodeNumber);
    unicodeNumber->selectAll();
    slotTextChanged(m_lastUnicodeNumber);
}

bool UnicodeWidget::controlCharacter(const QString &text)
{
    bool isControlCharacter = false;
    QString t = text.toLower();

    switch (m_inputMethod) {
    case UnicodeDialog::InputHex:
        if (t.isEmpty() || (t.length() == 1 && !(t == QLatin1String("9") || t == QLatin1String("a") || t == QLatin1String("d"))) ||
            (t.length() == 2 && t.at(0) == QChar('1'))) {
            isControlCharacter = true;
        }
        break;

    case UnicodeDialog::InputDec:
        bool ok;
        isControlCharacter = controlCharacter(text.toUInt(&ok, 16));
        break;
    }

    return isControlCharacter;
}

bool UnicodeWidget::controlCharacter(uint value)
{
    bool isControlCharacter = false;

    if (value < 32 && !(value == 9 || value == 10 || value == 13)) {
        isControlCharacter = true;
    }
    return isControlCharacter;
}

QString UnicodeWidget::trimmedUnicodeNumber(QString text)
{
    while (!text.isEmpty() && text.at(0) == QChar('0')) {
        text = text.remove(0, 1);
    }
    return text;
}

QString UnicodeWidget::unicodeInfo(const QString &unicode)
{
    QString infoText(i18n("<small>(no character selected)</small>"));
    if (unicode.isEmpty()) {
        return infoText;
    }

    QString u = trimmedUnicodeNumber(unicode).toLower();

    if (controlCharacter(u)) {
        infoText = i18n(
            "Control character. Cannot be inserted/printed. See <a href=\"https://en.wikipedia.org/wiki/Control_character\">Wikipedia:Control_character</a>");
    } else if (u == QLatin1String("a")) {
        infoText = i18n("Line Feed (newline character, \\\\n)");
    } else if (u == QLatin1String("20")) {
        infoText = i18n("Standard space character. (Other space characters: U+00a0, U+2000&#x2013;200b, U+202f)");
    } else if (u == QLatin1String("a0")) {
        infoText = i18n("No-break space. &amp;nbsp; in HTML. See U+2009 and U+0020.");
    } else if (u == QLatin1String("ab") || u == QLatin1String("bb") || u == QLatin1String("2039") || u == QLatin1String("203a")) {
        infoText =
            i18n("<p><strong>&laquo;</strong> (u+00ab, <code>&amp;lfquo;</code> in HTML) and <strong>&raquo;</strong> (u+00bb, <code>&amp;rfquo;</code> in "
                 "HTML) are called Guillemets or angle quotes. Usage in different countries: France (with non-breaking Space 0x00a0), Switzerland, Germany, "
                 "Finland and Sweden.</p><p><strong>&lsaquo;</strong> and <strong>&rsaquo;</strong> (U+2039/203a, <code>&amp;lsaquo;/&amp;rsaquo;</code>) are "
                 "their single quote equivalents.</p><p>See <a href=\"https://en.wikipedia.org/wiki/Guillemets\">Wikipedia:Guillemets</a></p>");
    } else if (u == QLatin1String("2002")) {
        infoText = i18n("En Space (width of an n)");
    } else if (u == QLatin1String("2003")) {
        infoText = i18n("Em Space (width of an m)");
    } else if (u == QLatin1String("2004")) {
        infoText = i18n("Three-Per-Em Space. Width: 1/3 of one <em>em</em>");
    } else if (u == QLatin1String("2005")) {
        infoText = i18n("Four-Per-Em Space. Width: 1/4 of one <em>em</em>");
    } else if (u == QLatin1String("2006")) {
        infoText = i18n("Six-Per-Em Space. Width: 1/6 of one <em>em</em>");
    } else if (u == QLatin1String("2007")) {
        infoText = i18n("Figure space (non-breaking). Width of a digit if digits have fixed width in this font.");
    } else if (u == QLatin1String("2008")) {
        infoText = i18n("Punctuation Space. Width the same as between a punctuation character and the next character.");
    } else if (u == QLatin1String("2009")) {
        infoText = i18n("Thin space, in HTML also &amp;thinsp;. See U+202f and <a "
                        "href=\"https://en.wikipedia.org/wiki/Space_(punctuation)\">Wikipedia:Space_(punctuation)</a>");
    } else if (u == QLatin1String("200a")) {
        infoText = i18n("Hair Space. Thinner than U+2009.");
    } else if (u == QLatin1String("2019")) {
        infoText =
            i18n("Punctuation Apostrophe. Should be used instead of U+0027. See <a href=\"https://en.wikipedia.org/wiki/Apostrophe\">Wikipedia:Apostrophe</a>");
    } else if (u == QLatin1String("2013")) {
        infoText = i18n("<p>An en Dash (dash of the width of an n).</p><p>Usage examples: In English language for value ranges (1878&#x2013;1903), for "
                        "relationships/connections (Zurich&#x2013;Dublin). In the German language it is also used (with spaces!) for showing thoughts: "
                        "&ldquo;Es war &#x2013; wie immer in den Ferien &#x2013; ein regnerischer Tag.</p> <p>See <a "
                        "href=\"https://en.wikipedia.org/wiki/Dash\">Wikipedia:Dash</a></p>");
    } else if (u == QLatin1String("2014")) {
        infoText = i18n("<p>An em Dash (dash of the width of an m).</p><p>Usage examples: In English language to mark&#x2014;like here&#x2014;thoughts. "
                        "Traditionally without spaces. </p><p>See <a href=\"https://en.wikipedia.org/wiki/Dash\">Wikipedia:Dash</a></p>");
    } else if (u == QLatin1String("202f")) {
        infoText = i18n("<p>Narrow no-break space. Has the same width as U+2009.</p><p>Usage: For units (spaces are marked with U+2423, &#x2423;): "
                        "230&#x2423;V, &#x2212;21&#x2423;&deg;C, 50&#x2423;lb, <em>but</em> 90&deg; (no space). In German for abbreviations (like: "
                        "i.&#x202f;d.&#x202f;R. instead of i.&#xa0;d.&#xa0;R. with U+00a0).</p><p>See <a "
                        "href=\"https://de.wikipedia.org/wiki/Schmales_Leerzeichen\">Wikipedia:de:Schmales_Leerzeichen</a></p>");
    } else if (u == QLatin1String("2026")) {
        infoText = i18n("Ellipsis: If text has been left o&#x2026; See <a href=\"https://en.wikipedia.org/wiki/Ellipsis\">Wikipedia:Ellipsis</a>");
    } else if (u == QLatin1String("2212")) {
        infoText = i18n("Minus sign. For numbers: &#x2212;42");
    } else if (u == QLatin1String("2423")) {
        infoText = i18n("Open box; stands for a space.");
    } else if (u == QLatin1String("2669")) {
        infoText = i18n("Quarter note (Am.) or crochet (Brit.). See <a href=\"https://en.wikipedia.org/wiki/Quarter_note\">Wikipedia:Quarter_note</a>");
    } else if (u == QLatin1String("266a") || u == QLatin1String("266b")) {
        infoText = i18n("Eighth note (Am.) or quaver (Brit.). Half as long as a quarter note (U+2669). See <a "
                        "href=\"https://en.wikipedia.org/wiki/Eighth_note\">Wikipedia:Eighth_note</a>");
    } else if (u == QLatin1String("266c")) {
        infoText = i18n("Sixteenth note (Am.) or semiquaver (Brit.). Half as long as an eighth note (U+266a). See <a "
                        "href=\"https://en.wikipedia.org/wiki/Sixteenth_note\">Wikipedia:Sixteenth_note</a>");
    } else if (u == QLatin1String("1D162")) {
        infoText = i18n("Thirty-second note (Am.) or demisemiquaver (Brit.). Half as long as a sixteenth note (U+266b). See <a "
                        "href=\"https://en.wikipedia.org/wiki/Thirty-second_note\">Wikipedia:Thirty-second_note</a>");
    } else {
        infoText = i18n("<small>No additional information available for this character.</small>");
    }

    return infoText;
}

QString UnicodeWidget::validateText(const QString &text)
{
    QRegExp regex("([0-9]|[a-f])", Qt::CaseInsensitive, QRegExp::RegExp2);
    QString newText;
    int pos = 0;

    switch (m_inputMethod) {
    case UnicodeDialog::InputHex:
        // Remove all characters we don't want
        while ((pos = regex.indexIn(text, pos)) != -1) {
            newText += regex.cap(1);
            pos++;
        }
        break;

    case UnicodeDialog::InputDec:
        // TODO
        break;
    }

    return newText;
}

void UnicodeWidget::updateOverviewChars(uint unicode)
{
    QString left;
    QString right;
    uint i;

    for (i = 1; i <= 4; ++i) {
        if (unicode > i && !controlCharacter(unicode - i)) {
            left = QLatin1Char(' ') + left;
            left = QChar(unicode - i) + left;
        }
    }

    for (i = 1; i <= 8; ++i) {
        if (unicode + i <= MAX_UNICODE_V1 && !controlCharacter(unicode + i)) {
            right += QChar(unicode + i);
            right += ' ';
        }
    }

    leftChars->setText(left);
    rightChars->setText(right);
}

void UnicodeWidget::clearOverviewChars()
{
    leftChars->clear();
    rightChars->clear();
}

QString UnicodeWidget::nextUnicode(const QString &text, Direction direction)
{
    uint value = 0;
    QString newText;
    bool ok;

    switch (m_inputMethod) {
    case UnicodeDialog::InputHex:
        value = text.toUInt(&ok, 16);
        switch (direction) {
        case Backward:
            value--;
            break;
        default:
            value++;
            break;
        }
        // Wrapping
        if (value == (uint)-1) {
            value = MAX_UNICODE_V1;
        }
        if (value > MAX_UNICODE_V1) {
            value = 0;
        }

        newText.setNum(value, 16);
        break;

    case UnicodeDialog::InputDec:
        break;
    }

    return newText;
}

void UnicodeWidget::readChoices()
{
    // Get a pointer to a shared configuration instance, then get the TitleWidget group.
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup titleConfig(config, "TitleWidget");

    // Default is 2013 because there is also (perhaps interesting) information.
    m_lastUnicodeNumber = titleConfig.readEntry("unicode_number", QStringLiteral("2013"));
}

void UnicodeWidget::writeChoices()
{
    // Get a pointer to a shared configuration instance, then get the TitleWidget group.
    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup titleConfig(config, "TitleWidget");

    titleConfig.writeEntry("unicode_number", m_lastUnicodeNumber);
}

/// SLOTS

/**
 * \brief Validates the entered Unicode number and displays its Unicode character.
 */
void UnicodeWidget::slotTextChanged(const QString &text)
{
    unicodeNumber->blockSignals(true);

    QString newText = validateText(text);
    if (newText.isEmpty()) {
        unicodeChar->clear();
        unicodeNumber->clear();
        clearOverviewChars();
        m_lastCursorPos = 0;
        m_lastUnicodeNumber = QString();
        labelInfoText->setText(unicodeInfo(QString()));

    } else {

        int cursorPos = unicodeNumber->cursorPosition();

        unicodeNumber->setText(newText);
        unicodeNumber->setCursorPosition(cursorPos);

        // Get the decimal number as uint to create the QChar from
        bool ok;
        uint value = 0;
        switch (m_inputMethod) {
        case UnicodeDialog::InputHex:
            value = newText.toUInt(&ok, 16);
            break;
        case UnicodeDialog::InputDec:
            value = newText.toUInt(&ok, 10);
            break;
        }
        updateOverviewChars(value);

        if (!ok) {
            // Impossible! validateText never fails!
        }

        // If an invalid character has been entered:
        // Reset the cursor position because the entered char has been deleted.
        if (text != newText && newText == m_lastUnicodeNumber) {
            unicodeNumber->setCursorPosition(m_lastCursorPos);
        }

        m_lastCursorPos = unicodeNumber->cursorPosition();
        m_lastUnicodeNumber = newText;

        labelInfoText->setText(unicodeInfo(newText));
        unicodeChar->setText(QChar(value));
    }

    unicodeNumber->blockSignals(false);
}

/**
 * When return pressed, we return the selected unicode character
 * if it was not a control character.
 */
void UnicodeWidget::slotReturnPressed()
{
    unicodeNumber->setFocus();
    const QString text = trimmedUnicodeNumber(unicodeNumber->text());
    if (!controlCharacter(text)) {
        emit charSelected(unicodeChar->text());
        writeChoices();
    }
}

void UnicodeWidget::slotNextUnicode()
{
    const QString text = unicodeNumber->text();
    unicodeNumber->setText(nextUnicode(text, Forward));
}

void UnicodeWidget::slotPrevUnicode()
{
    const QString text = unicodeNumber->text();
    unicodeNumber->setText(nextUnicode(text, Backward));
}

void UnicodeWidget::wheelEvent(QWheelEvent *event)
{
    if (frame->underMouse()) {
        if (event->angleDelta().y() > 0) {
            slotNextUnicode();
        } else {
            slotPrevUnicode();
        }
    }
}
