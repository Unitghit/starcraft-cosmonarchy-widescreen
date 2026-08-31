#include "player.h"

#include "constants/order.h"
#include "ai.h"
#include "limits.h"
#include "offsets.h"
#include "order.h"
#include "unit.h"
#include "yms.h"

bool player_is_human(int player) {
    if (player >= 0 && player < Limits::ActivePlayers) {
        return bw::players[player].type == 2;
    }
    return false;
}

bool player_is_computer(int player) {
    if (player >= 0 && player < Limits::ActivePlayers) {
        return bw::players[player].type == 1;
    }
    return false;
}

bool player_has_enemies(int player) {
    Assert(player >= 0 && player < Limits::Players);
    for (int i = 0; i < Limits::ActivePlayers; i++) {
        if (bw::alliances[player][i] == 0) {
            return true;
        }
    }
    return false;
}

static bool player_is_active(int player) {
    int type = bw::players[player].type;
    switch (type) {
        case 1:
        case 2: {
            return bw::victory_status[player] == 0;
        }
        case 3:
        case 4:
        case 7: {
            return true;
        }
    }
    return false;
}

int ActivePlayerIterator::next_active_player(int beg) {
    for (int i = beg + 1; i < 8; i++) {
        if (player_is_active(i)) {
            return i;
        }
    }
    return -1;
}

int net_player_to_game(int net_player) {
    for (int i = 0; i < Limits::ActivePlayers; i++) {
        if (bw::players[i].storm_id == net_player) {
            return i;
        }
    }
    return -1;
}
