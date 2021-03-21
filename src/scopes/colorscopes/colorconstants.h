#ifndef KDENLIVE_COLORCONSTANTS_H
#define KDENLIVE_COLORCONSTANTS_H

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

#endif //KDENLIVE_COLORCONSTANTS_H
