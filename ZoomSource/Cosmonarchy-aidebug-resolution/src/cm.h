#pragma once
#include "types.h"
#include "offsets.h"

namespace cm {

Ai::PlayerData& get_player_ai(int player);
Tbl* get_unit_tbl();
array_offset<Unit*, SelectionLimit> get_client_selection();
bool should_use_f12_for_console();
void set_console_visible(bool is_visible);
bool are_cheats_enabled();

};

