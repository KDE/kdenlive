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

#include "oauth2.h"
#include "kdenlive_debug.h"
#include "logindialog.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <QJsonParseError>
#include <QUrl>
#include <QUrlQuery>

OAuth2::OAuth2(QWidget *parent)

{
    //  m_strEndPoint = "https://www.freesound.org/apiv2/oauth2/logout_and_authorize/";
    m_strEndPoint = QStringLiteral("https://www.freesound.org/apiv2/oauth2/authorize/");
    m_strClientID = QStringLiteral("33e04f36da52710a28cc"); // obtained when ttguy registered the kdenlive application with freesound
    m_strRedirectURI = QStringLiteral("https://www.freesound.org/home/app_permissions/permission_granted/");
    m_strResponseType = QStringLiteral("code");
    m_pParent = parent;

    m_bAccessTokenRec = false;

    // read a previously saved access token from settings. If the access token is not more that 24hrs old
    // it will be valid and will work when OAuth2::obtainAccessToken() is called
    // if there is no access token in the settings it will be blank and OAuth2::obtainAccessToken()
    // will logon to freesound
    // If it is older than 24hrs OAuth2::obtainAccessToken() will fail but then the class will
    // request a new access token via a saved refresh_token - that is saved in the kdenlive settings file
    // $HOME/.config/kdenlive.rc
    // vvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvvn

    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup authGroup(config, "FreeSoundAuthentication");

    QString strAccessTokenFromSettings = authGroup.readEntry(QStringLiteral("freesound_access_token"));

    if (!strAccessTokenFromSettings.isEmpty()) {
        m_bAccessTokenRec = true;
        m_strAccessToken = strAccessTokenFromSettings;
    }
    buildLoginDialog();
}

void OAuth2::buildLoginDialog()
{
    m_pLoginDialog = new LoginDialog(m_pParent);
    connect(m_pLoginDialog, &LoginDialog::authCodeObtained, this, &OAuth2::SlotAuthCodeObtained);

    connect(m_pLoginDialog, &LoginDialog::accessDenied, this, &OAuth2::SlotAccessDenied);
    connect(m_pLoginDialog, &LoginDialog::canceled, this, &OAuth2::SlotCanceled);
    connect(m_pLoginDialog, &LoginDialog::useHQPreview, this, &OAuth2::SlotDownloadHQPreview);
    connect(this, &OAuth2::AuthCodeObtained, this, &OAuth2::SlotAuthCodeObtained);
}
/**
 * @brief OAuth2::getClientID - returns QString of the "clientID"
 * @return QString of the "clientID" which is a string that identifies the Kdenlive
 * application to the freesound website when the request for authentication is made
 */
QString OAuth2::getClientID() const
{
    return m_strClientID;
}
/**
 * @brief OAuth2::getClientSecret - returns QString of the "client secret"
 * @return - QString of the "client secret" which is another string that identifies the Kdenlive
 * application to the freesound website when the application asks for an access token
 */
QString OAuth2::getClientSecret() const
{
    return OAuth2_strClientSecret;
}

/**
 * @brief OAuth2::ForgetAccessToken - clear saved access token from settings  and memory /n
 * deletes the saved access token from the settings file and from memory.
 * Used when the authentication process has failed  and has the effect of forcing
 * the user to re-authenticate with freesound the next time they try and download a freesound HQ file
 */
void OAuth2::ForgetAccessToken()
{

    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup authGroup(config, "FreeSoundAuthentication");

    authGroup.deleteEntry(QStringLiteral("freesound_access_token"));

    m_bAccessTokenRec = false;
    m_strAccessToken.clear();
}

/**
 * @brief OAuth2::loginUrl - returns  QString containing URL to connect to freesound.
 * @return - QString containing URL to connect to freesound.  Substitutes clientid,redirecturi and response types into the string
 */
QString OAuth2::loginUrl()
{

    QString str = QStringLiteral("%1?client_id=%2&redirect_uri=%3&response_type=%4").arg(m_strEndPoint, m_strClientID, m_strRedirectURI, m_strResponseType);
    //  qCDebug(KDENLIVE_LOG) << "Login URL" << str;
    return str;
}

/**
 * @brief OAuth2::obtainAccessToken - Obtains freesound access  token. opens the login dialog to connect to freesound if it needs to. \n
 * Called by  ResourceWidget::slotSaveItem
 *
 */
void OAuth2::obtainAccessToken()
{

    if (m_bAccessTokenRec) {

        emit accessTokenReceived(m_strAccessToken);
        // if we already have the access token then carry on as if we have already  logged on and have the access token
    } else {
        //  login to free sound via our login dialog
        QUrl vUrl(loginUrl());
        if (!m_pLoginDialog) {
            buildLoginDialog();
        }
        m_pLoginDialog->setLoginUrl(vUrl);
        m_pLoginDialog->exec();
    }
}

/**
 * @brief OAuth2::SlotAccessDenied - fires when freesound web site denys access
 */
void OAuth2::SlotAccessDenied()
{
    qCDebug(KDENLIVE_LOG) << "access denied";
    emit accessDenied();
}
/**
 * @brief OAuth2::SlotAuthCodeObtained - fires when the LogonDialog has obtained an Auth Code and has sent the LogonDialog::AuthCodeObtained signal
 * This uses the Authorisation Code we have got to request an access token for downloading files
 * http://www.freesound.org/docs/api/authentication.html#oauth2-authentication
 * When the request finishes OAuth2::serviceRequestFinished will be notified and it will extract the access token and emit accessTokenReceived
 */
void OAuth2::SlotAuthCodeObtained()
{
    m_strAuthorizationCode = m_pLoginDialog->authCode(); // get the Auth code we have obtained
    // has a lifetime of 10 minutes
    OAuth2::RequestAccessCode(false, m_strAuthorizationCode);
}
/**
 * @brief OAuth2::RequestAccessCode - connect to freesound to exchange a authorization code or a refresh token for an access code
 * @param pIsReRequest - pass false if you are requesting an access code using a previously obtained authorization code. Pass true if you are requesting a new
 * access code via refresh token
 * @param pCode - pass an authorisation code here if pIsReRequest  is false. Otherwise pass a refresh token here.
 */
void OAuth2::RequestAccessCode(bool pIsReRequest, const QString &pCode)
{
    QString vGrantType;
    QString vCodeTypeParamName;
    // If the access code is older than 24hrs any request to the API using the token will return a 401 (Unauthorized) response showing an ‘Expired token’ error.
    // But you can how get a new access token using the refresh token
    // curl -X POST -d "client_id=YOUR_CLIENT_ID&client_secret=YOUR_CLIENT_SECRET&grant_type=refresh_token&refresh_token=REFRESH_TOKEN"
    // "https://www.freesound.org/apiv2/oauth2/access_token/"
    if (pIsReRequest) {
        vGrantType = QStringLiteral("refresh_token");
        vCodeTypeParamName = QStringLiteral("refresh_token");
    } else {
        vGrantType = QStringLiteral("authorization_code");
        vCodeTypeParamName = QStringLiteral("code");
    }

    auto *networkManager = new QNetworkAccessManager(this);
    QUrl serviceUrl = QUrl(QStringLiteral("https://www.freesound.org/apiv2/oauth2/access_token/"));

    QUrlQuery postData;

    postData.addQueryItem(QStringLiteral("client_id"), this->getClientID());
    postData.addQueryItem(QStringLiteral("client_secret"), this->getClientSecret());
    postData.addQueryItem(QStringLiteral("grant_type"), vGrantType);
    postData.addQueryItem(vCodeTypeParamName, pCode);
    connect(networkManager, &QNetworkAccessManager::finished, this, &OAuth2::serviceRequestFinished);
    QNetworkRequest request(serviceUrl);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    networkManager->post(request, postData.toString(QUrl::FullyEncoded).toUtf8());
}
/**
 * @brief OAuth2::serviceRequestFinished -  Fires when we finish downloading the access token.
 * This is the slot that is notified when OAuth2::SlotAuthCodeObtained  has finished downloading the access token.
 * It parses the result to extract out the access_token, refresh_token and expires_in values. If it
 * extracts the access token successfully if fires the  OAuth2::accessTokenReceived signal
 * which is picked up by ResourceWidget::slotAccessTokenReceived

 * @param reply
 *  */
void OAuth2::serviceRequestFinished(QNetworkReply *reply)
{

    // QString sRefreshToken;
    // int iExpiresIn;
    QString sErrorText;

    if (reply->isFinished()) {
        QByteArray sReply = reply->readAll();

        QJsonParseError jsonError;
        QJsonDocument doc = QJsonDocument::fromJson(sReply, &jsonError);
        if (jsonError.error != QJsonParseError::NoError) {
            qCDebug(KDENLIVE_LOG) << "OAuth2::serviceRequestFinished jsonError.error:  " << jsonError.errorString();
            ForgetAccessToken();
            emit accessTokenReceived(QString()); // notifies ResourceWidget::slotAccessTokenReceived - empty string in access token indicates error

        } else {
            QVariant data = doc.toVariant();

            if (data.canConvert(QVariant::Map)) {
                QMap<QString, QVariant> map = data.toMap();

                if (map.contains(QStringLiteral("access_token"))) {
                    m_strAccessToken = map.value(QStringLiteral("access_token")).toString();
                    m_bAccessTokenRec = true;
                }
                if (map.contains(QStringLiteral("refresh_token"))) {
                    m_strRefreshToken = map.value(QStringLiteral("refresh_token")).toString();
                }
                if (map.contains(QStringLiteral("expires_in"))) {
                    // iExpiresIn = map.value("expires_in").toInt(); //time in seconds until the access_token expires
                }
                if (map.contains(QStringLiteral("error"))) {
                    m_bAccessTokenRec = false;
                    sErrorText = map.value(QStringLiteral("error")).toString();
                    qCDebug(KDENLIVE_LOG) << "OAuth2::serviceRequestFinished map error:  " << sErrorText;
                    ForgetAccessToken();
                    emit accessTokenReceived(QString()); // notifies ResourceWidget::slotAccessTokenReceived - empty string in access token indicates error
                }

                if (m_bAccessTokenRec) {

                    KSharedConfigPtr config = KSharedConfig::openConfig();
                    KConfigGroup authGroup(config, "FreeSoundAuthentication");
                    authGroup.writeEntry(QStringLiteral("freesound_access_token"), m_strAccessToken);
                    authGroup.writeEntry(QStringLiteral("freesound_refresh_token"), m_strRefreshToken);
                    //  access tokens have a limited lifetime of 24 hours.
                    emit accessTokenReceived(m_strAccessToken); // notifies ResourceWidget::slotAccessTokenReceived

                } else {

                    ForgetAccessToken();
                    emit accessTokenReceived(QString()); // notifies ResourceWidget::slotAccessTokenReceived - empty string in access token indicates error
                }
            }
        }
    }
    reply->deleteLater();
}
/**
 * @brief OAuth2::SlotCanceled
 * Fires when user cancels out of the free sound login dialog - LoginDialog
 */
void OAuth2::SlotCanceled()
{
    emit Canceled();
    m_pLoginDialog = nullptr;
}

/**
 * @brief OAuth2::SlotDownloadHQPreview
 * Fires when user clicks the Use HQ preview file button on the freesound login page LoginDialog.
 *  emits UseHQPreview signal that is picked up by    ResourceWidget::slotFreesoundUseHQPreview()
 */
void OAuth2::SlotDownloadHQPreview()
{
    emit UseHQPreview();
    m_pLoginDialog = nullptr;
}
/**
 * @brief OAuth2::obtainNewAccessToken
 * Use the refresh token to get a new access token - after the access token has expired. Called by
 *  ResourceWidget::DownloadRequestFinished
 */
void OAuth2::obtainNewAccessToken()
{

    KSharedConfigPtr config = KSharedConfig::openConfig();
    KConfigGroup authGroup(config, "FreeSoundAuthentication");

    m_strRefreshToken = authGroup.readEntry(QStringLiteral("freesound_refresh_token"));
    OAuth2::RequestAccessCode(true, m_strRefreshToken); // request new access code via the refresh token method
}
