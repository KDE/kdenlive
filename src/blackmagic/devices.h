#ifndef __DEVICES_H__
#define __DEVICES__


#include "include/DeckLinkAPI.h"

#include <KComboBox>


class BMInterface
{
public:
    BMInterface();
    ~BMInterface();
    static bool getBlackMagicDeviceList(KComboBox *devicelist);
    static bool getBlackMagicOutputDeviceList(KComboBox *devicelist);
    static bool isSupportedProfile(int card, QMap< QString, QString > properties);
    static QStringList supportedModes(int card);
};

#endif
