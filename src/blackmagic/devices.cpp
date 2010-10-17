/***************************************************************************
 *   Copyright (C) 2007 by Jean-Baptiste Mardelle (jb@kdenlive.org)        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA          *
 ***************************************************************************/


#include "devices.h"

#include <KDebug>
#include <KLocale>


BMInterface::BMInterface()
{
}

//static
bool BMInterface::getBlackMagicDeviceList(KComboBox *devicelist, KComboBox *modelist)
{
    IDeckLinkIterator* deckLinkIterator;
    IDeckLink* deckLink;
    int numDevices = 0;
    HRESULT result;
    bool found = false;

    // Create an IDeckLinkIterator object to enumerate all DeckLink cards in the system
    deckLinkIterator = CreateDeckLinkIteratorInstance();
    if(deckLinkIterator == NULL) {
        kDebug() << "A DeckLink iterator could not be created.  The DeckLink drivers may not be installed.";
        return found;
    }

    // Enumerate all cards in this system
    while(deckLinkIterator->Next(&deckLink) == S_OK) {
        char *      deviceNameString = NULL;

        // Increment the total number of DeckLink cards found
        numDevices++;
        //if (numDevices > 1)
        kDebug() << "// FOUND a BM device\n\n+++++++++++++++++++++++++++++++++++++";

        // *** Print the model name of the DeckLink card
        result = deckLink->GetModelName((const char **) &deviceNameString);
        if(result == S_OK) {
            QString deviceName(deviceNameString);
            free(deviceNameString);

            IDeckLinkInput*                 deckLinkInput = NULL;
            IDeckLinkDisplayModeIterator*       displayModeIterator = NULL;
            IDeckLinkDisplayMode*               displayMode = NULL;
            HRESULT                             result;

            // Query the DeckLink for its configuration interface
            result = deckLink->QueryInterface(IID_IDeckLinkInput, (void**)&deckLinkInput);
            if(result != S_OK) {
                kDebug() << "Could not obtain the IDeckLinkInput interface - result = " << result;
                return found;
            }

            // Obtain an IDeckLinkDisplayModeIterator to enumerate the display modes supported on output
            result = deckLinkInput->GetDisplayModeIterator(&displayModeIterator);
            if(result != S_OK) {
                kDebug() << "Could not obtain the video input display mode iterator - result = " << result;
                return found;
            }
            QStringList availableModes;
            // List all supported output display modes
            while(displayModeIterator->Next(&displayMode) == S_OK) {
                char *          displayModeString = NULL;

                result = displayMode->GetName((const char **) &displayModeString);
                if(result == S_OK) {
                    //char                  modeName[64];
                    int                     modeWidth;
                    int                     modeHeight;
                    BMDTimeValue            frameRateDuration;
                    BMDTimeScale            frameRateScale;
                    //int                       pixelFormatIndex = 0; // index into the gKnownPixelFormats / gKnownFormatNames arrays
                    //BMDDisplayModeSupport displayModeSupport;


                    // Obtain the display mode's properties
                    modeWidth = displayMode->GetWidth();
                    modeHeight = displayMode->GetHeight();
                    displayMode->GetFrameRate(&frameRateDuration, &frameRateScale);
                    QString description = QString(displayModeString) + " (" + QString::number(modeWidth) + "x" + QString::number(modeHeight) + " - " + QString::number((double)frameRateScale / (double)frameRateDuration) + i18n("fps") + ")";
                    availableModes << description;
                    //modelist->addItem(description);
                    //printf(" %-20s \t %d x %d \t %7g FPS\t", displayModeString, modeWidth, modeHeight, (double)frameRateScale / (double)frameRateDuration);

                    // Print the supported pixel formats for this display mode
                    /*while ((gKnownPixelFormats[pixelFormatIndex] != 0) && (gKnownPixelFormatNames[pixelFormatIndex] != NULL))
                    {
                        if ((deckLinkOutput->DoesSupportVideoMode(displayMode->GetDisplayMode(), gKnownPixelFormats[pixelFormatIndex], bmdVideoOutputFlagDefault, &displayModeSupport, NULL) == S_OK)
                            && (displayModeSupport != bmdDisplayModeNotSupported))
                        {
                            printf("%s\t", gKnownPixelFormatNames[pixelFormatIndex]);
                        }
                        pixelFormatIndex++;
                    }*/
                    free(displayModeString);
                }

                // Release the IDeckLinkDisplayMode object to prevent a leak
                displayMode->Release();
            }
            devicelist->addItem(deviceName, availableModes);
            found = true;
        }


        //print_attributes(deckLink);

        // ** List the video output display modes supported by the card
        //print_output_modes(deckLink);

        // ** List the input and output capabilities of the card
        //print_capabilities(deckLink);

        // Release the IDeckLink instance when we've finished with it to prevent leaks
        deckLink->Release();
    }

    deckLinkIterator->Release();
    if(modelist != NULL && devicelist->count() > 0) {
        QStringList modes = devicelist->itemData(devicelist->currentIndex()).toStringList();
        modelist->insertItems(0, modes);
    }
    return found;
}