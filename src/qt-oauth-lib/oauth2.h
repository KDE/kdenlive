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
#ifndef OAUTH2_H
#define OAUTH2_H

#include <QNetworkReply>
#include <QObject>
#include <QString>

#ifndef DOXYGEN_SHOULD_SKIP_THIS // don't make this any more public than it is. 
const QLatin1String OAuth2_strClientSecret("441d88374716e7a3503997151e4780566f007313"); // obtained when ttguy registered the kdenlive application with freesound
#endif

#ifdef QT5_USE_WEBKIT

class LoginDialog;

/**
  \brief This object does oAuth2 authentication on the freesound web site. \n
Instantiated by ResourceWidget constructor. \n
Freesounds OAuth2 authentication API is documented here http://www.freesound.org/docs/api/authentication.html#oauth2-authentication
  */
class OAuth2 : public QObject
{
    Q_OBJECT

public:
    explicit OAuth2(QWidget *parent = nullptr);

    void obtainAccessToken();
    void obtainNewAccessToken();
    void ForgetAccessToken();

    QString getClientID() const;
    QString getClientSecret() const;

    static QString m_strClientSecret;

    QString loginUrl();

signals:

    /**
     * @brief AuthCodeObtained
     * Signal that is emitted when login is ended OK and auth code obtained
     */
    void AuthCodeObtained();

    /**
     * @brief accessDenied
     * signal emitted if the freesound has denied access to the application
     */
    void accessDenied();
    /**
     * @brief accessTokenReceived   emitted when we have obtained an access token from freesound. \n Connected to ResourceWidget::slotAccessTokenReceived
     * @param sAccessToken
     *
     */
    void accessTokenReceived(const QString &sAccessToken);

    /**
     * @brief DownloadCanceled
     */
    void DownloadCanceled();
    /**
     * @brief DownloadHQPreview
     */
    void DownloadHQPreview();
    /**
     * @brief UseHQPreview
     */
    void UseHQPreview();
    /**
     * @brief Canceled
     */
    void Canceled();

private slots:

    void SlotAccessDenied();
    void serviceRequestFinished(QNetworkReply *reply);
    void SlotAuthCodeObtained();
    void SlotCanceled();
    void SlotDownloadHQPreview();

private:
    QString m_strAuthorizationCode;
    QString m_strAccessToken;
    QString m_strEndPoint;

    QString m_strClientID;

    QString m_strRedirectURI;
    QString m_strResponseType;
    QString m_strRefreshToken;
    bool m_bAccessTokenRec;
    void RequestAccessCode(bool pIsReRequest, const QString &pCode);

    LoginDialog *m_pLoginDialog;
    QWidget *m_pParent;
    void buildLoginDialog();
};

#endif // QT5_USE_WEBKIT

#endif // OAUTH2_H
