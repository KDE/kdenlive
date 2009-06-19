/***************************************************************************
 *   Copyright (C) 2008 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/
 
#include "unicodedialog.h"

/// CONSTANTS

const int MAX_LENGTH_HEX = 4;
const uint MAX_UNICODE_V1 = 65535;
 
 
/// CONSTRUCTORS/DECONSTRUCTORS

UnicodeDialog::UnicodeDialog(InputMethod inputMeth) : inputMethod(inputMeth), lastCursorPos(0), lastUnicodeNumber("")
{
	setupUi(this);
	connect(unicodeNumber, SIGNAL(textChanged(QString)), this, SLOT(slotTextChanged(QString)));
	connect(unicodeNumber, SIGNAL(returnPressed()), this, SLOT(slotReturnPressed()));
	connect(arrowUp, SIGNAL(clicked()), this, SLOT(slotNextUnicode()));
	connect(arrowDown, SIGNAL(clicked()), this, SLOT(slotPrevUnicode()));
	
	switch (inputMethod) {
		case InputHex:
			unicodeNumber->setMaxLength(MAX_LENGTH_HEX);
		break;
		
		case InputDec:
		break;
	}
	
	arrowUp->setShortcut(Qt::Key_Up);
	arrowDown->setShortcut(Qt::Key_Down);
	
	arrowUp->setToolTip(i18n("Next Unicode character (Arrow Up)"));
	arrowDown->setToolTip(i18n("Previous Unicode character (Arrow Down)"));
	unicodeNumber->setToolTip(i18n("Enter your Unicode number here. Allowed characters: [0-9] and [a-f]."));
}

UnicodeDialog::~UnicodeDialog()
{
}


/// METHODS

void UnicodeDialog::showLastUnicode()
{
	unicodeNumber->setText(lastUnicodeNumber);
}

bool UnicodeDialog::controlCharacter(QString text)
{
	bool isControlCharacter = false;
	QString t = text.toLower();
	
	switch (inputMethod) {
		case InputHex:
			if (t == "" 
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

QString UnicodeDialog::unicodeInfo(QString unicode_number)
{
	QString infoText(i18n("<small>(no character selected)</small>"));
	if (unicode_number.length() == 0) return infoText;
	
	QString u = trimmedUnicodeNumber(unicode_number).toLower();
	
	if (controlCharacter(u)) {
		infoText = i18n("Control character. Cannot be inserted/printed. See <a href=\"http://en.wikipedia.org/wiki/Control_character\">Wikipedia:Control_character</a>");
	} else if (u == "a") {
		infoText = i18n("Line Feed (newline character, \\\\n)");
	} else if (u == "20") {
		infoText = i18n("Standard space character. (See U+00a0 and U+2000&#x2013;200b)");
	} else if (u == "a0") {
		infoText = i18n("No-break space. &amp;nbsp; in HTML. See U+0020.");
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
		infoText = i18n("Thin space, in HTML also &amp;thinsp;. See <a href=\"http://en.wikipedia.org/wiki/Space_(punctuation)\">Wikipedia:Space_(punctuation)</a>");
	} else if (u == "200a") {
		infoText = i18n("Hair Space. Thinner than U+2009.");
	} else if (u == "2019") {
		infoText = i18n("Punctuation Apostrophe. Should be used instead of U+0027. See <a href=\"http://en.wikipedia.org/wiki/Apostrophe\">Wikipedia:Apostrophe</a>");
	} else if (u == "2013") {
		infoText = i18n("An en Dash (dash of the width of an n). See <a href=\"http://en.wikipedia.org/wiki/Dash\">Wikipedia:Dash</a>");
	} else if (u == "2014") {
		infoText = i18n("An em Dash (dash of the widht of an m). See <a href=\"http://en.wikipedia.org/wiki/Dash\">Wikipedia:Dash</a>");
	} else if (u == "2026") {
		infoText = i18n("Ellipsis: If text has been left out. See <a href=\"http://en.wikipedia.org/wiki/Ellipsis\">Wikipedia:Ellipsis</a>");
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
		if (unicode > i && !controlCharacter(unicode-i)) {
			left = " " + left;
			left = QChar(unicode-i) + left;
		}
	}
	
	for (i = 1; i <= 8; i++) {
		if (unicode + i <= MAX_UNICODE_V1 && !controlCharacter(unicode+i)) {
			right += QChar(unicode+i);
			right += " ";
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
			if (value == (uint) -1) value = MAX_UNICODE_V1;
			if (value > MAX_UNICODE_V1) value = 0;
			
			newText.setNum(value, 16);
			break;
			
		case InputDec:
			break;
	}
	
	return newText;
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
		lastCursorPos = 0;
		lastUnicodeNumber = "";
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
		if (text != newText && newText == lastUnicodeNumber) {
			unicodeNumber->setCursorPosition(lastCursorPos);
		}
		
		lastCursorPos = unicodeNumber->cursorPosition();
		lastUnicodeNumber = newText;
		
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

#include "unicodedialog.moc"
