/*
 * 2.9 英寸电子纸的具体波形配置。
 * 波形字节用于控制面板电压刷新时序，属于型号相关参数，不能当作图片或随意修改。
 */

#ifndef EPD_DISPLAY_2_9_H
#define EPD_DISPLAY_2_9_H

#include "epd_display.h"

// 2.9-inch panel using the C221 25C waveforms.
class EpdDisplay_2_9 : public EpdDisplay {
public:
    using EpdDisplay::EpdDisplay;

protected:
    Waveform GetFullWaveform() const override;
    Waveform GetPartialWaveform() const override;

private:
    static const uint8_t WF_Full[];
    static const uint8_t WF_Partial[];
};

#endif  // EPD_DISPLAY_2_9_H