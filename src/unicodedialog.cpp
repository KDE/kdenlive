/***************************************************************************
 *   Copyright (C) 2008 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "unicodedialog.h"

#include <QWheelEvent>

/// CONSTANTS

const int MAX_LENGTH_HEX = 4;
const uint MAX_UNICODE_V1 = 65535;


/// CONSTRUCTORS/DECONSTRUCTORS

UnicodeDialog::UnicodeDialog(InputMethod inputMeth) :
        inputMethod(inputMeth),
        m_lastCursorPos(0)
{
    setupUi(this);
    readChoices();
    showLastUnicode();
    connect(unicodeNumber, SIGNAL(textChanged(QString)), this, SLOT(slotTextChanged(QString)));
    connect(unicodeNumber, SIGNAL(returnPressed()), this, SLOT(slotReturnPressed()));
    connect(arrowUp, SIGNAL(clicked()), this, SLOT(slotPrevUnicode()));
    connect(arrowDown, SIGNAL(clicked()), this, SLOT(slotNextUnicode()));

    switch (inputMethod) {
    case InputHex:
        unicodeNumber->setMaxLength(MAX_LENGTH_HEX);
        break;

    case InputDec:
        break;
    }

    arrowUp->setShortcut(Qt::Key_Up);
    arrowDown->setShortcut(Qt::Key_Down);
    unicode_link->setText(i18n("Information about unicode characters: <a href=\"http://decodeunicode.org\">http://decodeunicode.org</a>"));
    arrowUp->setToolTip(i18n("Previous Unicode character (Arrow Up)"));
    arrowDown->setToolTip(i18n("Next Unicode character (Arrow Down)"));
    unicodeNumber->setToolTip(i18n("Enter your Unicode number here. Allowed characters: [0-9] and [a-f]."));
    unicodeNumber->selectAll(); // Selection will be reset by setToolTip and similar, so set it here

}

UnicodeDialog::~UnicodeDialog()
{
}


/// PUBLIC SLOTS

int UnicodeDialog::exec()
{
    unicodeNumber->setFocus();
    return QDialog::exec();
}


/// METHODS

void UnicodeDialog::showLastUnicode()
{
    unicodeNumber->setText(m_lastUnicodeNumber);
    unicodeNumber->selectAll();
    slotTextChanged(m_lastUnicodeNumber);
}

bool UnicodeDialog::controlCharacter(QString text)
{
    bool isControlCharacter = false;
    QString t = text.toLower();

    switch (inputMethod) {
    case InputHex:
        if (t.isEmpty()
                || (t.length() == 1 && !(t == "9" || t == "a" || t == "d"))
                || (t.length() == 2 && t.at(0) == QChar('1'))) {
            isControlCharacter = true;
        }
        break;

    case InputDec:
        bool ok;
        isControlCharacter = controlCharacter(text.toUInt(&ok, 16));
        break;
    }

    return isControlCharacter;
}

bool UnicodeDialog::controlCharacter(uint value)
{
    bool isControlCharacter = false;

    if (value < 32 && !(value == 9 || value == 10 || value == 13)) {
        isControlCharacter = true;
    }
    return isControlCharacter;

}

QString UnicodeDialog::trimmedUnicodeNumber(QString text)
{
    while (text.length() > 0 && text.at(0) == QChar('0')) {
        text = text.remove(0, 1);
    }
    return text;
}

QString UnicodeDialog::unicodeInfo(QString unicode)
{
    QString infoText(i18n("<small>(no character selected)</small>"));
    if (unicode.length() == 0) return infoText;

    QString u = trimmedUnicodeNumber(unicode).toLower();

    if (controlCharacter(u)) {
        infoText = i18n("Control character. Cannot be inserted/printed. See <a href=\"http://en.wikipedia.org/wiki/Control_character\">Wikipedia:Control_character</a>");
    } else if (u == "a") {
        infoText = i18n("Line Feed (newline character, \\\\n)");
    } else if (u == "20") {
        infoText = i18n("Standard space character. (Other space characters: U+00a0, U+2000&#x2013;200b, U+202f)");
    } else if (u == "a0") {
        infoText = i18n("No-break space. &amp;nbsp; in HTML. See U+2009 and U+0020.");
    } else if (u == "ab" || u == "bb" || u == "2039" || u == "203a") {
        infoText = i18n("<p><strong>&laquo;</strong> (u+00ab, <code>&amp;lfquo;</code> in HTML) and <strong>&raquo;</strong> (u+00bb, <code>&amp;rfquo;</code> in HTML) are called Guillemets or angle quotes. Usage in different countries: France (with non-breaking Space 0x00a0), Switzerland, Germany, Finland and Sweden.</p><p><strong>&lsaquo;</strong> and <strong>&rsaquo;</strong> (U+2039/203a, <code>&amp;lsaquo;/&amp;rsaquo;</code>) are their single quote equivalents.</p><p>See <a href=\"http://en.wikipedia.org/wiki/Guillemets\">Wikipedia:Guillemets</a></p>");
    } else if (u == "2002") {
        infoText = i18n("En Space (width of an n)");
    } else if (u == "2003") {
        infoText = i18n("Em Space (width of an m)");
    } else if (u == "2004") {
        infoText = i18n("Three-Per-Em Space. Width: 1/3 of one <em>em</em>");
    } else if (u == "2005") {
        infoText = i18n("Four-Per-Em Space. Width: 1/4 of one <em>em</em>");
    } else if (u == "2006") {
        infoText = i18n("Six-Per-Em Space. Width: 1/6 of one <em>em</em>");
    } else if (u == "2007") {
        infoText = i18n("Figure space (non-breaking). Width of a digit if digits have fixed width in this font.");
    } else if (u == "2008") {
        infoText = i18n("Punctuation Space. Width the same as between a punctuation character and the next character.");
    } else if (u == "2009") {
        infoText = i18n("Thin space, in HTML also &amp;thinsp;. See U+202f and <a href=\"http://en.wikipedia.org/wiki/Space_(punctuation)\">Wikipedia:Space_(punctuation)</a>");
    } else if (u == "200a") {
        infoText = i18n("Hair Space. Thinner than U+2009.");
    } else if (u == "2019") {
        infoText = i18n("Punctuation Apostrophe. Should be used instead of U+0027. See <a href=\"http://en.wikipedia.org/wiki/Apostrophe\">Wikipedia:Apostrophe</a>");
    } else if (u == "2013") {
        infoText = i18n("<p>An en Dash (dash of the width of an n).</p><p>Usage examples: In English language for value ranges (1878&#x2013;1903), for relationships/connections (Zurich&#x2013;Dublin). In the German language it is also used (with spaces!) for showing thoughts: &ldquo;Es war &#x2013; wie immer in den Ferien &#x2013; ein regnerischer Tag.</p> <p>See <a href=\"http://en.wikipedia.org/wiki/Dash\">Wikipedia:Dash</a></p>");
    } else if (u == "2014") {
        infoText = i18n("<p>An em Dash (dash of the width of an m).</p><p>Usage examples: In English language to mark&#x2014;like here&#x2014;thoughts. Traditionally without spaces. </p><p>See <a href=\"http://en.wikipedia.org/wiki/Dash\">Wikipedia:Dash</a></p>");
    } else if (u == "202f") {
        infoText = i18n("<p>Narrow no-break space. Has the same width as U+2009.</p><p>Usage: For units (spaces are marked with U+2423, &#x2423;): 230&#x2423;V, &#x2212;21&#x2423;&deg;C, 50&#x2423;lb, <em>but</em> 90&deg; (no space). In German for abbreviations (like: i.&#x202f;d.&#x202f;R. instead of i.&#xa0;d.&#xa0;R. with U+00a0).</p><p>See <a href=\"http://de.wikipedia.org/wiki/Schmales_Leerzeichen\">Wikipedia:de:Schmales_Leerzeichen</a></p>");
    } else if (u == "2026") {
        infoText = i18n("Ellipsis: If text has been left o&#x2026; See <a href=\"http://en.wikipedia.org/wiki/Ellipsis\">Wikipedia:Ellipsis</a>");
    } else if (u == "2212") {
        infoText = i18n("Minus sign. For numbers: &#x2212;42");
    } else if (u == "2423") {
        infoText = i18n("Open box; stands for a space.");
    } else if (u == "2669") {
        infoText = i18n("Quarter note (Am.) or crochet (Brit.). See <a href=\"http://en.wikipedia.org/wiki/Quarter_note\">Wikipedia:Quarter_note</a>");
    } else if (u == "266a" || u == "266b") {
        infoText = i18n("Eighth note (Am.) or quaver (Brit.). Half as long as a quarter note (U+2669). See <a href=\"http://en.wikipedia.org/wiki/Eighth_note\">Wikipedia:Eighth_note</a>");
    } else if (u == "266c") {
        infoText = i18n("Sixteenth note (Am.) or semiquaver (Brit.). Half as long as an eighth note (U+266a). See <a href=\"http://en.wikipedia.org/wiki/Sixteenth_note\">Wikipedia:Sixteenth_note</a>");
    } else if (u == "1D162") {
        infoText = i18n("Thirty-second note (Am.) or demisemiquaver (Brit.). Half as long as a sixteenth note (U+266b). See <a href=\"http://en.wikipedia.org/wiki/Thirty-second_note\">Wikipedia:Thirty-second_note</a>");
    } else {
        infoText = i18n("<small>No additional information available for this character.</small>");
    }

    return infoText;
}

QString UnicodeDialog::validateText(QString text)
{
    QRegExp regex("([0-9]|[a-f])", Qt::CaseInsensitive, QRegExp::RegExp2);
    QString newText = "";
    int pos = 0;

    switch (inputMethod) {
    case InputHex:
        // Remove all characters we don't want
        while ((pos = regex.indexIn(text, pos)) != -1) {
            newText += regex.cap(1);
            pos++;
        }
        break;

    case InputDec:
        // TODO
        break;
    }

    return newText;
}

void UnicodeDialog::updateOverviewChars(uint unicode)
{
    QString left = "";
    QString right = "";
    uint i;

    for (i = 1; i <= 4; i++) {
        if (unicode > i && !controlCharacter(unicode - i)) {
            left = ' ' + left;
            left = QChar(unicode - i) + left;
        }
    }

    for (i = 1; i <= 8; i++) {
        if (unicode + i <= MAX_UNICODE_V1 && !controlCharacter(unicode + i)) {
            right += QChar(unicode + i);
            right += ' ';
        }
    }

    leftChars->setText(left);
    rightChars->setText(right);

}

void UnicodeDialog::clearOverviewChars()
{
    leftChars->setText("");
    rightChars->setText("");
}

QString UnicodeDialog::nextUnicode(QString text, Direction direction)
{
    uint value = 0;
    QString newText = "";
    bool ok;

    switch (inputMethod) {
    case InputHex:
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
        if (value == (uint) - 1) value = MAX_UNICODE_V1;
        if (value > MAX_UNICODE_V1) value = 0;

        newText.setNum(value, 16);
        break;

    case InputDec:
        break;
    }

    return newText;
}

void UnicodeDialog::readChoices()
{
    // Get a pointer to a shared configuration instance, then get the TitleWidget group.
    KSharedConfigPtr config = KGlobal::config();
    KConfigGroup titleConfig(config, "TitleWidget");

    // Default is 2013 because there is also (perhaps interesting) information.
    m_lastUnicodeNumber = titleConfig.readEntry("unicode_number", QString("2013"));
}

void UnicodeDialog::writeChoices()
{
    // Get a pointer to a shared configuration instance, then get the TitleWidget group.
    KSharedConfigPtr config = KGlobal::config();
    KConfigGroup titleConfig(config, "TitleWidget");

    titleConfig.writeEntry("unicode_number", m_lastUnicodeNumber);
}


/// SLOTS

/**
 * \brief Validates the entered Unicode number and displays its Unicode character.
 */
void UnicodeDialog::slotTextChanged(QString text)
{
    unicodeNumber->blockSignals(true);

    QString newText = validateText(text);
    if (newText.length() == 0) {
        unicodeChar->setText("");
        unicodeNumber->setText("");
        clearOverviewChars();
        m_lastCursorPos = 0;
        m_lastUnicodeNumber = "";
        labelInfoText->setText(unicodeInfo(""));

    } else {

        int cursorPos = unicodeNumber->cursorPosition();

        unicodeNumber->setText(newText);
        unicodeNumber->setCursorPosition(cursorPos);

        // Get the decimal number as uint to create the QChar from
        bool ok;
        uint value = 0;
        switch (inputMethod) {
        case InputHex:
            value = newText.toUInt(&ok, 16);
            break;
        case InputDec:
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
void UnicodeDialog::slotReturnPressed()
{
    QString text = trimmedUnicodeNumber(unicodeNumber->text());
    if (!controlCharacter(text)) {
        emit charSelected(unicodeChar->text());
        writeChoices();
    }
    emit accept();
}

void UnicodeDialog::slotNextUnicode()
{
    QString text = unicodeNumber->text();
    unicodeNumber->setText(nextUnicode(text, Forward));
}

void UnicodeDialog::slotPrevUnicode()
{
    QString text = unicodeNumber->text();
    unicodeNumber->setText(nextUnicode(text, Backward));
}

void UnicodeDialog::wheelEvent(QWheelEvent * event)
{
    if (frame->underMouse()) {
        if (event->delta() > 0) slotNextUnicode();
        else slotPrevUnicode();
    }
}

#include "unicodedialog.moc"
