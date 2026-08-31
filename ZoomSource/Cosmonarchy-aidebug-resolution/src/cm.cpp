#include "cm.h"

#include "ai.h"
#include "offsets_funcs_1161.h"

namespace cm {

enum class AiseSignal {
    GetAiMain = 2,
    GetUnitTbl = 8,
    GetClientSelection = 12,
    GetAltConsoleHotkey = 13,
    SetConsoleVisible = 14,
    AreCheatsEnabled = 15,
};

static int send_aise_signal(Unit* unit1, Unit* unit2, AiseSignal signal, int v1, int v2, int v3) {
    return bw::WeaponDamage(v1, static_cast<int>(signal), 33333, unit1, v2, unit2, v3);
}

Ai::PlayerData& get_player_ai(int player) {
    Ai::PlayerData* ai = (Ai::PlayerData*)send_aise_signal(0, 0, AiseSignal::GetAiMain, 0, 0, 0);
    return ai[player];
}

Tbl* get_unit_tbl() {
    Tbl* tbl = (Tbl*)send_aise_signal(0, 0, AiseSignal::GetUnitTbl, 0, 0, 0);
    return tbl;
}

array_offset<Unit*, SelectionLimit> get_client_selection() {
    return send_aise_signal(0, 0, AiseSignal::GetClientSelection, 0, 0, 0);
}

bool should_use_f12_for_console() {
    return send_aise_signal(0, 0, AiseSignal::GetAltConsoleHotkey, 0, 0, 0);
}

void set_console_visible(bool is_visible) {
    send_aise_signal(0, 0, AiseSignal::SetConsoleVisible, is_visible, 0, 0);
}

bool are_cheats_enabled() {
    return send_aise_signal(0, 0, AiseSignal::AreCheatsEnabled, 0, 0, 0);
}

};
