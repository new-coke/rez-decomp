/* Debug GUI sliders. */

#ifndef REZ_GW_SLIDER_H
#define REZ_GW_SLIDER_H

#include "types_gw.h"

struct GwSlider;
struct GwWin;
struct GwSliderVals;

void gw_slider_set_callback(struct GwSlider *slider, GwSliderFn cb);
float gw_slider_get_min(struct GwSlider *slider);
float gw_slider_get_max(struct GwSlider *slider);
void gw_slider_set_value(struct GwSlider *slider, float val);
float gw_slider_get_value(struct GwSlider *slider);
struct GwSlider *gw_slider_create(int vertical, struct GwSliderVals *args, int x, int y, int width, int height, struct GwWin *parent);

#endif /* REZ_GW_SLIDER_H */
