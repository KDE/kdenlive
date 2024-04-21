/*
SPDX-FileCopyrightText: 2024 Ajay Chauhan
This file is part of Kdenlive. See www.kdenlive.org.
SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/

#pragma once

/** @class CropCalculator
    @brief A utility class for calculating crop parameters based on source and target aspect ratios.
 */
class CropCalculator
{
public:
    static void calculateCropParameters(double sourceAspectRatio, double targetAspectRatio, int sourceWidth, int sourceHeight, int &cropWidth, int &cropHeight,
                                        int &leftOffset, int &topOffset)
    {
        if (std::abs(sourceAspectRatio - targetAspectRatio) < 0.01) {
            // source and target aspect ratios are the same, no crop needed
            cropWidth = sourceWidth;
            cropHeight = sourceHeight;
            leftOffset = 0;
            topOffset = 0;
        } else if (sourceAspectRatio > targetAspectRatio) {
            // source aspect ratio is wider than target, crop the sides
            cropHeight = sourceHeight;
            cropWidth = static_cast<int>(sourceHeight * targetAspectRatio);
            leftOffset = (sourceWidth - cropWidth) / 2;
            topOffset = 0;
        } else {
            // source aspect ratio is taller than target, crop top and bottom
            cropWidth = sourceWidth;
            cropHeight = static_cast<int>(sourceWidth / targetAspectRatio);
            leftOffset = 0;
            topOffset = (sourceHeight - cropHeight) / 2;
        }
    }
};