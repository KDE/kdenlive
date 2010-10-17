#ifndef __DEVICES_H__
#define __DEVICES__


#include "include/DeckLinkAPI.h"

#include <KComboBox>


class BMInterface
{
public:
    BMInterface();
    ~BMInterface();
    static bool getBlackMagicDeviceList(KComboBox *devicelist, KComboBox *modelist);
};

#endif