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
#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QString>
#include <QUrl>

namespace Ui {
class LoginDialog;
}

/**
  \brief This is the dialog that is used to login to freesound
 \details It contains a QWebView object to display the freesound web page. I did try using a QTextBrowser
 for this purpose but it responds to the URL that is used to connect with
 "No document for
 https://www.freesound.org/apiv2/oauth2/authorize/?client_id=3duhagdr874c&redirect_uri=https://www.freesound.org/home/app_permissions/permission_granted/&response_type=code"
 The use of QWebView adds a dependency on the KF5WebKit to kdenlive. Need install libkf5webkit5-dev package on ubuntu
 */
class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog() override;
    void setLoginUrl(const QUrl &url);

    QString authCode() const;

signals:
    /**
     * @brief authCodeObtained - emitted when freesound gives us an Authorisation code \n
     * Authorisation codes last 10mins and must be exchanged for an access token in that time
     */
    void authCodeObtained();
    /**
     * @brief accessDenied -signal emitted if freesound denies access - eg bad password or user has denied access to Kdenlive app.
     */
    void accessDenied();
    /**
     * @brief canceled - signal emitted when user clicks cancel button in the logon dialog
     */
    void canceled();
    /**
     * @brief useHQPreview - signal emitted when user clicks the "use HQ preview" button in the logon dialog
     */
    void useHQPreview();

private slots:
    void urlChanged(const QUrl &url);

    void slotGetHQPreview();
    void slotRejected();

private:
    Ui::LoginDialog *m_ui;
    QString m_strAuthCode;
};

#endif // LOGINDIALOG_H
