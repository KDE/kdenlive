#ifndef __BMDCAPTURE_H__
#define __BMDCAPTURE_H__

#include "include/DeckLinkAPI.h"
#include "../stopmotion/capturehandler.h"

#include <QWidget>
#include <QObject>
#include <QLayout>
#ifdef Q_WS_MAC
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
signals:
    void gotTimeCode(ulong);
    void gotMessage(const QString &);
};

class BmdCaptureHandler : public CaptureHandler
{
    Q_OBJECT
public:
    BmdCaptureHandler(QVBoxLayout *lay, QWidget *parent = 0);
    ~BmdCaptureHandler();
    CDeckLinkGLWidget *previewView;
    void startPreview(int deviceId, int captureMode);
    void stopPreview();
    void startCapture(const QString &path);
    void stopCapture();
    void captureFrame(const QString &fname);
    void showOverlay(QImage img, bool transparent = true);
    void hideOverlay();
    void hidePreview(bool hide);

private:
    IDeckLinkIterator       *deckLinkIterator;
    DeckLinkCaptureDelegate     *delegate;
    IDeckLinkDisplayMode        *displayMode;
    IDeckLink           *deckLink;
    IDeckLinkInput          *deckLinkInput;
    IDeckLinkDisplayModeIterator    *displayModeIterator;
};


#endif
