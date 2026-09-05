#ifndef COSMONARCHY_PIXEL_ZOOM_STEPS_H
#define COSMONARCHY_PIXEL_ZOOM_STEPS_H
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace pixel_zoom_steps
{
    // Match cnc-ddraw's aspect-fit rounding, including its rounded intermediate
    // height. Dimensions are the configured physical output, not logical input.
    inline void Fit(unsigned iw, unsigned ih, unsigned &ow, unsigned &oh, bool aspect)
    {
        if (!aspect || !iw || !ih || !ow || !oh) return;
        const unsigned height = static_cast<unsigned>((uint64_t(ih)*ow + iw/2)/iw);
        if (uint64_t(oh)*iw < uint64_t(ih)*ow)
            ow = (std::min)(ow, static_cast<unsigned>((uint64_t(ow)*oh + height/2)/height));
        else oh = (std::min)(oh, height);
    }
    inline std::vector<unsigned> Build(unsigned iw, unsigned ih, unsigned ow, unsigned oh, unsigned maximum)
    {
        // Full viewport is always an escape stop, even when the output itself
        // cannot present the base view at an integer scale.
        std::vector<unsigned> result{10000};
        if (!iw || !ih || !ow || !oh || ow > 16384 || oh > 16384) return result;
        const uint64_t ax=uint64_t(iw)*10000, ay=uint64_t(ih)*10000;
        for (unsigned scale=1; scale<=(std::min)(ow,oh); ++scale)
        {
            if (ow%scale || oh%scale) continue;
            const unsigned cw=ow/scale, ch=oh/scale;
            if (cw>iw || ch>ih) continue;
            // Find an integer zoom value satisfying BOTH existing floor crop
            // formulas. Do not round a percentage and assume its crop is exact.
            const uint64_t low=(std::max)({ax/(cw+1)+1, ay/(ch+1)+1, uint64_t(10000)});
            const uint64_t high=(std::min)({ax/cw, ay/ch, uint64_t(maximum)});
            if (low<=high) result.push_back(static_cast<unsigned>(high));
        }
        std::sort(result.begin(),result.end());
        result.erase(std::unique(result.begin(),result.end()),result.end());
        return result;
    }
    inline unsigned RegularNext(const std::vector<unsigned> &exact,unsigned current,bool inward,unsigned maximum)
    {
        constexpr unsigned step=1250;
        unsigned next=inward ? (std::min)((current/step+1)*step,maximum) :
            (std::max)(((current-1)/step)*step,10000u);
        for(auto value:exact)
        {
            if(inward && value>current && value<next) next=value;
            if(!inward && value<current && value>next) next=value;
        }
        return next;
    }
}
#endif
