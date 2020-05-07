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
constexpr float REC_601_R = .299;
constexpr float REC_601_G = .587;
constexpr float REC_601_B = .114;

// CIE 709 luminance factors
constexpr float REC_709_R = .2125;
constexpr float REC_709_G = .7154;
constexpr float REC_709_B = .0721;

#endif //KDENLIVE_COLORCONSTANTS_H
