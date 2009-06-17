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
 
 
/// CONSTRUCTORS/DECONSTRUCTORS

UnicodeDialog::UnicodeDialog(InputMethod inputMeth) : inputMethod(inputMeth), lastCursorPos(0), lastUnicodeNumber("")
{
	setupUi(this);
	connect(unicodeNumber, SIGNAL(textChanged(QString)), this, SLOT(slotTextChanged(QString)));
	connect(unicodeNumber, SIGNAL(returnPressed()), this, SLOT(slotReturnPressed()));
	
	switch (inputMethod) {
		case InputHex:
			unicodeNumber->setMaxLength(MAX_LENGTH_HEX);
		break;
		
		case InputDec:
		break;
	}
}

UnicodeDialog::~UnicodeDialog()
{
}


/// METHODS

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
			break;
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
	QString infoText("");
	QString u = trimmedUnicodeNumber(unicode_number);
	
	if (controlCharacter(u)) {
		infoText = i18n("Control character. Cannot be inserted/printed. See <a href=\"http://en.wikipedia.org/wiki/Control_character\">Wikipedia:Control_character</a>");
	} else if (u == "2009") {
		infoText = i18n("A thin space, in HTML also &amp;thinsp;. See <a href=\"http://en.wikipedia.org/wiki/Space_(punctuation)\">Wikipedia:Space_(punctuation)</a>");
	} else if (u == "2019") {
		infoText = i18n("Punctuation Apostrophe. Should be used instead of U+0027. See <a href=\"http://en.wikipedia.org/wiki/Apostrophe\">Wikipedia:Apostrophe</a>");
	} else if (u == "2013") {
		infoText = i18n("An en Dash (dash of the width of an n). See <a href=\"http://en.wikipedia.org/wiki/Dash\">Wikipedia:Dash</a>");
	} else if (u == "2014") {
		infoText = i18n("An em Dash (dash of the widht of an m). See <a href=\"http://en.wikipedia.org/wiki/Dash\">Wikipedia:Dash</a>");
	} else if (u == "2026") {
		infoText = i18n("Ellipsis: If text has been left out. See <a href=\"http://en.wikipedia.org/wiki/Ellipsis\">Wikipedia:Ellipsis</a>");
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


/// SLOTS

/**
 * \brief Validates the entered Unicode number and displays its Unicode character.
 */
void UnicodeDialog::slotTextChanged(QString text)
{
	unicodeNumber->blockSignals(true);
	
	bool ok;
	int cursorPos = unicodeNumber->cursorPosition();
	QString newText = validateText(text);
	
	unicodeNumber->setText(newText);
	unicodeNumber->setCursorPosition(cursorPos);
	
	// Get the decimal number as uint to create the QChar from
	uint value;
	switch (inputMethod) {
		case InputHex:
			value = newText.toUInt(&ok, 16);
		break;
		case InputDec:
			value = newText.toUInt(&ok, 10);
		break;
	}
	
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

#include "unicodedialog.moc"
