/***************************************************************************
 *   Copyright (C) 2008 by Simon Andreas Eugster (simon.eu@gmail.com)      *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef UNICODEDIALOG_H
#define UNICODEDIALOG_H

#include "ui_unicodewidget_ui.h"
#include <QDialog>

class UnicodeWidget;
class UnicodeDialog : public QDialog
{
    Q_OBJECT
public:
    /** \brief The input method for the dialog. Atm only InputHex supported. */
    enum InputMethod { InputHex, InputDec };
    explicit UnicodeDialog(InputMethod inputMeth, QWidget *parent = nullptr);
    ~UnicodeDialog() override;

private Q_SLOTS:
    void slotAccept();

Q_SIGNALS:
    /** \brief Contains the selected unicode character; emitted when Enter is pressed. */
    void charSelected(const QString &);

private:
    UnicodeWidget *m_unicodeWidget;
};

class UnicodeWidget : public QWidget, public Ui::UnicodeWidget_UI
{
    Q_OBJECT
public:
    explicit UnicodeWidget(UnicodeDialog::InputMethod inputMeth, QWidget *parent = nullptr);
    ~UnicodeWidget() override;

    /** \brief Returns infos about a unicode number. Extendable/improvable ;) */
    QString unicodeInfo(const QString &unicode);

    void showLastUnicode();

protected:
    void wheelEvent(QWheelEvent *event) override;

private:
    enum Direction { Forward, Backward };

    /** Selected input method */
    UnicodeDialog::InputMethod m_inputMethod;

    /** \brief Validates text and removes all invalid characters (non-hex e.g.) */
    QString validateText(const QString &text);
    /** \brief Removes all leading zeros */
    QString trimmedUnicodeNumber(QString text);
    /** \brief Checks whether the given string is a control character */
    bool controlCharacter(const QString &text);
    /** \brief Checks whether the given uint is a control character */
    bool controlCharacter(uint value);

    /** \brief Returns the next available unicode. */
    QString nextUnicode(const QString &text, Direction direction);

    /** \brief Paints previous and next characters around current char */
    void updateOverviewChars(uint unicode);
    void clearOverviewChars();

    int m_lastCursorPos;
    QString m_lastUnicodeNumber;

    /** \brief Reads the last used unicode number from the config file. */
    void readChoices();
    /** \brief Writes the last used unicode number into the config file. */
    void writeChoices();

signals:
    /** \brief Contains the selected unicode character; emitted when Enter is pressed. */
    void charSelected(const QString &);

public slots:
    void slotReturnPressed();

private slots:
    void slotTextChanged(const QString &text);
    void slotNextUnicode();
    void slotPrevUnicode();
};

#endif
