#ifndef YMS_H
#define YMS_H

#include "offsets.h"

inline bool is_in_game()
{ return *bw::is_ingame != 0; }

inline bool is_in_replay()
{ return *bw::is_replay != 0; }

inline bool is_multiplayer()
{ return *bw::is_multiplayer != 0; }

inline bool is_paused()
{ return *bw::is_paused != 0; }

inline Rect16 map_bounds()
{ return Rect16(0, 0, *bw::map_width, *bw::map_height); }

inline void refresh_ui() {
    *bw::force_button_refresh = 1;
    *bw::force_portrait_refresh = 1;
    *bw::ui_refresh = 1;
    *bw::unk_control = 0;
    *bw::unk_control_value = 0;
}

// retval 0 meinaa invalid / max len reached, 1 meinaa "" stringiä jne
int Sc_strlen(const char *str, int maxlen1, int maxlen2, bool accept_color_chars, bool accept_control_chars);

namespace Cheats {
    const uint32_t Operation_Cwal = 0x2;
    const uint32_t Power_Overwhelming = 0x4;
    const uint32_t Staying_Alive = 0x100;
    const uint32_t The_Gathering = 0x800;
}

inline bool is_cheat_active(uint32_t cheats) {
    return *bw::cheat_flags & cheats;
}

inline int Distance(const Point32 &first, const Point32 &second) {
    return bw::Distance(first.x, first.y, second.x, second.y);
}

#endif // YMS_H

