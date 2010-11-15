#ifndef __BMDCAPTURE_H__
#define __BMDCAPTURE_H__

#include "include/DeckLinkAPI.h"
#include "../stopmotion/capturehandler.h"

#include <QWidget>
#include <QObject>
#include <QLayout>
#if defined(Q_WS_MAC) || defined(Q_OS_FREEBSD)
#include <pthread.h>
#endif

class CDeckLinkGLWidget;
class PlaybackDelegate;

class DeckLinkCaptureDelegate : public QObject, public IDeckLinkInputCallback
{
    Q_OBJECT
public:
    DeckLinkCaptureDelegate();
    virtual ~DeckLinkCaptureDelegate();
    void setAnalyse(bool isOn);
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID /*iid*/, LPVOID */*ppv*/) {
        return E_NOINTERFACE;
    }
    virtual ULONG STDMETHODCALLTYPE AddRef(void);
    virtual ULONG STDMETHODCALLTYPE  Release(void);
    virtual HRESULT STDMETHODCALLTYPE VideoInputFormatChanged(BMDVideoInputFormatChangedEvents, IDeckLinkDisplayMode*, BMDDetectedVideoInputFormatFlags);
    virtual HRESULT STDMETHODCALLTYPE VideoInputFrameArrived(IDeckLinkVideoInputFrame*, IDeckLinkAudioInputPacket*);

private:
    ULONG               m_refCount;
    pthread_mutex_t     m_mutex;
    QList <IDeckLinkVideoInputFrame*> m_framesList;
    QStringList m_framePath;
    bool m_analyseFrame;

private slots:
    void slotProcessFrame();

signals:
    void gotTimeCode(ulong);
    void gotMessage(const QString &);
    void frameSaved(const QString);
    void gotFrame(QImage);
};

class BmdCaptureHandler : public CaptureHandler
{
    Q_OBJECT
public:
    BmdCaptureHandler(QVBoxLayout *lay, QWidget *parent = 0);
    ~BmdCaptureHandler();
    CDeckLinkGLWidget *previewView;
    void startPreview(int deviceId, int captureMode, bool audio = true);
    void stopPreview();
    void startCapture(const QString &path);
    void stopCapture();
    void captureFrame(const QString &fname);
    void showOverlay(QImage img, bool transparent = true);
    void hideOverlay();
    void hidePreview(bool hide);
    QStringList getDeviceName(QString);
    void setDevice(const QString input, QString size = QString());

private:
    IDeckLinkIterator       *deckLinkIterator;
    DeckLinkCaptureDelegate     *delegate;
    IDeckLinkDisplayMode        *displayMode;
    IDeckLink           *deckLink;
    IDeckLinkInput          *deckLinkInput;
    IDeckLinkDisplayModeIterator    *displayModeIterator;
};


#endif
