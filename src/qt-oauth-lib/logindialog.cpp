/********************************************************************************************************
 * Copyright (C) 2015 Roger Morton (ttguy1@gmail.com)
 *
 * Purpose: implements client access to  freesound.org using ver2 of the freesound API.
 *
 * Based on code at  https://code.google.com/p/qt-oauth-lib/
 * Which is Qt Library created by Integrated Computer Solutions, Inc. (ICS)
 * to provide OAuth2.0 for the Google API.
 *
 *       Licence: GNU Lesser General Public License
 * http://www.gnu.org/licenses/lgpl.html
 * This version of the GNU Lesser General Public License incorporates the terms
 * and conditions of version 3 of the GNU General Public License http://www.gnu.org/licenses/gpl-3.0-standalone.html
 * supplemented by the additional permissions listed at http://www.gnu.org/licenses/lgpl.html
 *
 *      Disclaimer of Warranty.
 * THERE IS NO WARRANTY FOR THE PROGRAM, TO THE EXTENT PERMITTED BY APPLICABLE LAW.
 * EXCEPT WHEN OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER PARTIES PROVIDE
 * THE PROGRAM “AS IS” WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE. THE ENTIRE RISK AS TO THE QUALITY AND PERFORMANCE OF THE PROGRAM IS WITH YOU.
 * SHOULD THE PROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL NECESSARY SERVICING, REPAIR
 * OR CORRECTION.
 *
 *     Limitation of Liability.
 * IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING WILL ANY COPYRIGHT HOLDER,
 * OR ANY OTHER PARTY WHO MODIFIES AND/OR CONVEYS THE PROGRAM AS PERMITTED ABOVE, BE LIABLE TO
 * YOU FOR DAMAGES, INCLUDING ANY GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING
 * OUT OF THE USE OR INABILITY TO USE THE PROGRAM (INCLUDING BUT NOT LIMITED TO LOSS OF DATA OR
 * DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY YOU OR THIRD PARTIES OR A FAILURE OF
 * THE PROGRAM TO OPERATE WITH ANY OTHER PROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 *                                                                                                       *
 *   You should have received a copy of the GNU General Public License                                   *
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.                              *
 *********************************************************************************************************/
#include "logindialog.h"
#include "ui_logindialog_ui.h"

#include "kdenlive_debug.h"
#include <QWebView>

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    , m_ui(new Ui::LoginDialog)
{
    m_ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(i18n("Freesound Login"));

    connect(m_ui->CancelButton, &QPushButton::clicked, this, &LoginDialog::slotRejected);
    connect(m_ui->GetHQpreview, &QPushButton::clicked, this, &LoginDialog::slotGetHQPreview);
    m_ui->FreeSoundLoginLabel->setText(
        i18n("Enter your freesound account details to download the highest quality version of this file. Or use the High Quality "
             "preview file instead (no freesound account required)."));
    // m_ui->textBrowser
    connect(m_ui->webView, &QWebView::urlChanged, this, &LoginDialog::urlChanged);
}

LoginDialog::~LoginDialog()
{
    delete m_ui;
}

void LoginDialog::slotGetHQPreview()
{
    emit useHQPreview();
    QDialog::accept();
}

void LoginDialog::slotRejected()
{
    emit canceled();
    QDialog::reject();
}

/**
 * @brief LoginDialog::urlChanged
 * @param url \n
 *  If we successfully get a Auth code in our URL we extract the code here and emit  LoginDialog::AuthCodeObtained signal
 * http://www.freesound.org/docs/api/authentication.html#oauth2-authentication
 */
void LoginDialog::urlChanged(const QUrl &url)
{
    // qCDebug(KDENLIVE_LOG) << "URL =" << url;
    const QString str = url.toString();
    const int posCode = str.indexOf(QLatin1String("&code="));
    const int posErr = str.indexOf(QLatin1String("&error="));
    if (posCode != -1) {
        m_strAuthCode = str.mid(posCode + 6);
        emit authCodeObtained();
        QDialog::accept();
    } else if (posErr != -1) {
        QString sError = str.mid(posErr + 7);
        if (sError == QLatin1String("access_denied")) {
            emit accessDenied();
        }
        QDialog::accept();
    }
}

QString LoginDialog::authCode() const
{
    return m_strAuthCode;
}

void LoginDialog::setLoginUrl(const QUrl &url)
{
    m_ui->webView->setUrl(url);
}
