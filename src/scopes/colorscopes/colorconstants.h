#pragma once
/*
    SPDX-FileCopyrightText: 2020 Simon Andreas Eugster <simon.eu@gmail.com>
    This file is part of kdenlive. See www.kdenlive.org.
    SPDX-License-Identifier: GPL-3.0-only OR LicenseRef-KDE-Accepted-GPL
*/
/**
 * ITU-R Recommendation for luminance calculation.
 * See http://www.poynton.com/ColorFAQ.html for details.
 */
enum class ITURec {
    Rec_601, Rec_709
};

// CIE 601 luminance factors
constexpr float REC_601_R = .299f;
constexpr float REC_601_G = .587f;
constexpr float REC_601_B = .114f;

// CIE 709 luminance factors
constexpr float REC_709_R = .2125f;
constexpr float REC_709_G = .7154f;
constexpr float REC_709_B = .0721f;
