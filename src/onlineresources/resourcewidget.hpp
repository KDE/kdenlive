#ifndef REOURCEWIDGET_H
#define REOURCEWIDGET_H

#include "ui_resourcewidget_ui.h"
#include "providersrepository.hpp"

#include <QWidget>
#include <QUrl>
#include <QNetworkReply>
#include <QTemporaryFile>
#include <QSlider>
#include <QListWidgetItem>
#include <QProcess>
//#include <QOAuth2AuthorizationCodeFlow>

class OAuth2;

const int imageRole = Qt::UserRole;
const int urlRole = Qt::UserRole + 1;
const int downloadRole = Qt::UserRole + 2;
const int durationRole = Qt::UserRole + 3;
const int previewRole = Qt::UserRole + 4;
const int authorRole = Qt::UserRole + 5;
const int authorUrl = Qt::UserRole + 6;
const int infoUrl = Qt::UserRole + 7;
const int infoData = Qt::UserRole + 8;
const int idRole = Qt::UserRole + 9;
const int licenseRole = Qt::UserRole + 10;
const int descriptionRole = Qt::UserRole + 11;
const int widthRole = Qt::UserRole + 12;
const int heightRole = Qt::UserRole + 13;
const int nameRole = Qt::UserRole + 14;
const int singleDownloadRole = Qt::UserRole + 15;
const int filetypeRole = Qt::UserRole + 16;
const int downloadLabelRole = Qt::UserRole + 17;

class ResourceWidget : public QWidget, public Ui::ResourceWidget_UI
{
    Q_OBJECT

public:
    explicit ResourceWidget(QWidget *parent = nullptr);
    ~ResourceWidget() override;

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override;

private slots:
    void slotChangeProvider();
    /**
     * @brief ResourceWidget::slotOpenUrl. Opens the file on the URL using the associated application via a KRun object
     * @param url
     */
    void slotOpenUrl(const QString &url);
    void slotStartSearch();
    void slotSearchFinished(QList<ResourceItemInfo> &list, const int pageCount);
    void slotUpdateCurrentItem();
    void slotSetIconSize(int size);
    void slotZoomView(bool zoomIn);
    void slotPreviewItem();
    void slotChooseVersion(const QStringList &urls, const QStringList &labels, const QString &accessToken = QString());
    void slotSaveItem(const QString &originalUrl = QString(), const QString &accessToken = QString());
    void slotGotFile(KJob *job);
    void slotAccessTokenReceived(const QString &accessToken);

private:
    std::unique_ptr<ProviderModel> *m_currentProvider;
    QListWidgetItem *m_currentItem;
    QTemporaryFile *m_tmpThumbFile;
    OAuth2 *m_pOAuth2;
    //QSlider *m_slider;
    /** @brief Default icon size for the views. */
    QSize m_iconSize;
    int wheelAccumulatedDelta;
    ResourceItemInfo getItemById(const QString &id);
    void loadConfig();
    void saveConfig();
    void blockUI(bool block);
    QString licenseNameFromUrl(const QString &licenseUrl, const bool shortName);

signals:
    void addClip(const QUrl &, const QString &);
    void addLicenseInfo(const QString &);
    void previewClip(const QString &path);
};

#endif // REOURCEWIDGET_H
