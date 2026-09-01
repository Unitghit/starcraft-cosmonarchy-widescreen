#include "scconsole.h"

#include "offsets.h"
#include "draw.h"

#include "constants/weapon.h"
#include "ai.h"
#include "cm.h"
#include "limits.h"
#include "presentation.h"
#include "runtime_diagnostics.h"
#include "pathing.h"
#include "player.h"
#include "resolution.h"
#include "selection.h"
#include "sprite.h"
#include "strings.h"
#include "tech.h"
#include "unit.h"
#include "upgrade.h"
#include "yms.h"

#include <string>
#include <algorithm>
#include <climits>
#include <cstdio>
#include <cstring>
#include <unordered_set>

// Every fopen call in this translation unit writes diagnostic output.
#define fopen runtime_diagnostics::Open

using namespace Common;
using std::get;

#pragma pack(push, 1)
struct Location {
    Rect32 area;
    uint16_t unk;
    uint16_t flags;
};
#pragma pack(pop)

struct TextLayout {
TextLayout(Common::Surface *surface) : surface(surface) {}

void Draw(Font *font, const std::string &str, const Point32 &default_pos, uint8_t color) {
    auto width = font->TextLength(str);
    auto height = 10;
    Point32 pos;
    for (int i = 0; i < 60; i++) {
        auto pos = Point32(default_pos.x, default_pos.y + i * 10);
        Rect32 suggest(pos.x, pos.y, pos.x + width, pos.y + height);
        if (TryDrawAt(font, str, suggest, color)) {
            return;
        }
        pos = Point32(default_pos.x, default_pos.y - i * 10);
        suggest = Rect32(pos.x, pos.y, pos.x + width, pos.y + height);
        if (TryDrawAt(font, str, suggest, color)) {
            return;
        }
    }
}

bool TryDrawAt(Font *font, const std::string &str, const Rect32 &suggest, uint8_t color) {
    if (suggest.top < 0 || suggest.bottom >= surface->height) {
        return false;
    }
    auto collide = std::any_of(blocks.begin(), blocks.end(), [&](const auto &block) {
        return block.left < suggest.right &&
            block.right > suggest.left &&
            block.top < suggest.bottom &&
            block.bottom > suggest.top;
    });
    if (!collide) {
        surface->DrawText(font, str, Point32(suggest.left, suggest.top), color);
        blocks.push_back(suggest);
        return true;
    } else {
        return false;
    }
}

Common::Surface *surface;
vector<Rect32> blocks;
};

ScConsole::ScConsole() {
show_fps = true;
show_frame = false;
draw_info = false;
draw_locations = false;
draw_crects = false;
draw_ai_towns = false;
draw_orders = OrderDrawMode::None;
draw_ai_data = true;
draw_ai_full = false;
draw_ai_named = true;
draw_ai_unit_homes = false;
draw_ai_guards = false;
draw_ai_regions = false;
for (int i = 0; i < Limits::Players; i++) {
    show_ai[i] = 1;
}
draw_coords = false;
draw_range = false;
draw_resource_areas = false;
draw_deaths = false;
draw_bullets = false;
for (int i = 0; i < Limits::Players; i++) {
    show_deaths[i] = 0;
}

AddCommand("gsw", &ScConsole::Gsw);
AddCommand("vis", &ScConsole::Vision);
AddCommand("vision", &ScConsole::Vision);
AddCommand("ally", &ScConsole::Alliance);
AddCommand("alliance", &ScConsole::Alliance);
AddCommand("supplymax", &ScConsole::SupplyMax);
AddCommand("airegion", &ScConsole::AiRegion);
AddCommand("aireg", &ScConsole::AiRegion);
AddCommand("player", &ScConsole::Player);
AddCommand("race", &ScConsole::Race);
AddCommand("u", &ScConsole::UnitCmd);
AddCommand("unit", &ScConsole::UnitCmd);
AddCommand("money", &ScConsole::Money);
AddCommand("resources", &ScConsole::Money);
AddCommand("supply", &ScConsole::Supply);
AddCommand("self", &ScConsole::Self);
AddCommand("frame", &ScConsole::Frame);
AddCommand("pause", &ScConsole::Pause);
AddCommand("show", &ScConsole::Show);
AddCommand("sai", &ScConsole::ShowAi);
AddCommand("saip", &ScConsole::ShowAiPlayer);
AddCommand("saig", &ScConsole::ShowAiGuards);
AddCommand("saiu", &ScConsole::ShowAiUnits);
AddCommand("grid", &ScConsole::Cmd_Grid);
AddCommand("spawn", &ScConsole::Spawn);
AddCommand("unitcount", &ScConsole::UnitCount);
AddCommand("ff", &ScConsole::FastForward);
}

ScConsole::~ScConsole()
{
}

void ScConsole::Hide() {
    Console::Hide();
    *bw::needs_full_redraw = true;
}

bool ScConsole::Cmd_Grid(const CmdArgs &args) {
    int width = atoi(args[2]);
    int height = atoi(args[3]);
    if (width == 0) {
        return false;
    }
    if (height == 0) {
        height = width;
    }
    int color = atoi(args[4]);
    if (color == 0) {
        color = 0x98;
    }

    Grid grid(width, height, color);
    if (strcmp(args[1], "-") == 0) {
        if (args[2][0] == 0) {
            grids.clear();
            return true;
        }
        for (int i = 0; i < grids.size(); i++) {
            if (grids[i].width == width && grids[i].height == height) {
                grids.erase(grids.begin() + i);
                return true;
            }
        }
        return false;
    }
    if (strcmp(args[1], "+") == 0) {
        for (int i = 0; i < grids.size(); i++) {
            if (grids[i].width == width && grids[i].height == height) {
                return false;
            }
        }
        grids.emplace_back(grid);
        return true;
    } else {
        Printf("grid (+|-) w [h] [color]");
        return false;
    }
}

static vector<UnitType> FindUnitFromName(std::string name) {
    std::transform(name.begin(), name.end(), name.begin(), tolower);
    vector<UnitType> results;
    for (auto unit_id : UnitType::All()) {
        std::string unit_name = cm::get_unit_tbl()->GetTblString(unit_id.Raw() + 1);
        std::transform(unit_name.begin(), unit_name.end(), unit_name.begin(), tolower);
        if (unit_name.find(name) != std::string::npos)
            results.emplace_back(unit_id);
    }
    return results;
}

vector<UnitType> ScConsole::ParseUnitId(const char *unit_str, int max_amt) {
    vector<UnitType> unit_ids;
    char *unit_str_end;
    int unit_id = strtoul(unit_str, &unit_str_end, 10);
    if (unit_str_end[0] != 0 || (unit_id == 0 && unit_str[0] != '0')) {
        unit_ids = FindUnitFromName(unit_str);
        if (unit_ids.size() > 30) {
            Printf("%d units match '%s'", unit_ids.size(), unit_str);
            return vector<UnitType>();
        } else if (unit_ids.size() > max_amt) {
            char buf[128];
            snprintf(buf, sizeof buf, "Too many canditates for '%s':", unit_str);
            std::string msg(buf);
            bool first = true;
            for (auto cand : unit_ids) {
                snprintf(buf, sizeof buf, " %s (%u)", cm::get_unit_tbl()->GetTblString(cand.Raw() + 1), cand.Raw());
                if (!first) {
                    msg.push_back(',');
                }
                msg += buf;
                first = false;
            }
            Printf(msg.c_str());
            return vector<UnitType>();
        }
    } else {
        unit_ids.emplace_back(UnitType(unit_id));
    }
    if (unit_ids.empty()) {
        Printf("'%s' is not valid unit id or unit name", unit_str);
    }
    return unit_ids;
}

bool ScConsole::Spawn(const CmdArgs &args) {
    if (!can_use_cheat_command()) {
        return false;
    }

    if (!is_in_game()) {
        return false;
    }
    auto unit_str = args[1];
    if (unit_str[0] == 0) {
        Printf("spawn <unit name or hex id> [amount] [player id]");
        return false;
    }
    vector<UnitType> unit_ids = ParseUnitId(unit_str, 1);

    if (unit_ids.empty()) {
        return false;
    }
    int amount = 1;
    if (args[2][0] != 0) {
        amount = atoi(args[2]);
    }
    int player = *bw::local_player_id;
    if (args[3][0] != 0) {
        player = atoi(args[3]);
    }

    Point16 pos = Point16(*bw::screen_x + *bw::mouse_clickpos_x, *bw::screen_y + *bw::mouse_clickpos_y);
    for (int i = 0; i < amount; i++) {
        Unit *unit = bw::CreateUnit(unit_ids[0], pos.x, pos.y, player);
        if (unit == nullptr) {
            return false;
        }
        bw::FinishUnit_Pre(unit);
        bw::FinishUnit(unit);
        bw::GiveAi(unit);
    }
    return true;
}

Unit *ScConsole::GetUnit() {
    if (!is_in_game()) {
        return 0;
    }
    return *bw::primary_selected;
}

array_offset<Unit*, SelectionLimit> ScConsole::GetSelectedUnits() {
    if (!is_in_game()) {
        return 0;
    }
    return cm::get_client_selection();
}

bool ScConsole::SupplyMax(const CmdArgs &args) {
    if (!isdigit(*args[1])) {
        return false;
    }

    int max = atoi(args[1]);
    for (unsigned int i = 0; i < Limits::Players; i++) {
        bw::zerg_supply_max[i] = max;
        bw::protoss_supply_max[i] = max;
        bw::terran_supply_max[i] = max;
    }
    return true;
}

// Singleplayer only!
int ScConsole::get_console_player() {
    for (int i = 0; i < Limits::ActivePlayers; i++) {
        if (player_is_human(i)) {
            return i;
        }
    }
    return -1; // If this happens, something went wrong!
}

int ScConsole::get_human_players() {
    int human_player_count = 0;
    if (!is_in_replay()) {
        for (int i = 0; i < Limits::ActivePlayers; i++) {
            if (player_is_human(i)) {
                human_player_count++;
            }
        }
    } else {
        human_player_count = *bw::replay_viewers;
    }
    return human_player_count;
}

bool ScConsole::can_use_cheat_command() {
    if (!is_in_game())  return true;
    if (is_in_replay()) return false;
    
    if (get_human_players() > 1) {
        Print("This command is reserved for singleplayer usage only!");
        return false;
    }
    if (!cm::are_cheats_enabled()) {
        Print("This command is forbidden on this map!");
        return false;
    }
    return true;
}

bool ScConsole::can_use_cheat_command_in_replay() {
    if (!is_in_replay()) return false;
    
    if (get_human_players() > 1) {
        Print("This command is reserved for singleplayer usage only!");
        return false;
    }
    return true;
}

extern bool all_visions;
bool ScConsole::Vision(const CmdArgs &args) {
    if (!can_use_cheat_command() && !can_use_cheat_command_in_replay()) {
        return false;
    }
    if (args[0] == "" || args[1] == "") {
        Print("Usage: vis <player|all> [on|off]");
        return false;
    }

    int console_player = get_console_player();
    bool enabled = true; // defaults to on
    if (args[2] != "")
        enabled = stricmp(args[2], "on") == 0; // ignore case
    std::vector<int> players;
    if (stricmp(args[1], "all") == 0) {
        for (int player = 0; player < Limits::ActivePlayers; player++) {
            if (!player_is_computer(player)) {
                continue;
            }
            players.push_back(player);
        }
    } else if (isdigit(*args[1])) {
        players.push_back(atoi(args[1]));
    }

    for (const int& player : players) {
        if (enabled) {
            bw::visions[player] |= 1 << console_player;
        } else {
            bw::visions[player] &= ~(1 << console_player);
        }
        Printf("Vision sharing %s for player %d", enabled ? "enabled" : "disabled", player);
    }
    return true;
}

bool ScConsole::Alliance(const CmdArgs &args) {
    if (!can_use_cheat_command()) {
        return false;
    }

    if (args[0] == "" || args[1] == "") {
        Print("Usage: alliance <player|all> [on|off]");
        return false;
    }

    int consolePlayer = get_console_player();
    bool enabled = true; // defaults to on
    if (args[2] != "") {
        enabled = stricmp(args[2], "on") == 0; // ignore case
    }
    std::vector<int> players;
    if (stricmp(args[1], "all") == 0) {
        for (int player = 0; player < Limits::ActivePlayers; player++) {
            if (!player_is_computer(player)) {
                continue;
            }
            players.push_back(player);
        }
    } else if (isdigit(*args[1])) {
        players.push_back(atoi(args[1]));
    }

    for (const int& player : players) {
        if (enabled) {
            bw::alliances[player][consolePlayer] = true;
            bw::alliances[consolePlayer][player] = true;
        } else {
            bw::alliances[player][consolePlayer] = false;
            bw::alliances[consolePlayer][player] = false;
        }

        Printf("Alliance %s for player %d", enabled ? "enabled" : "disabled", player);
    }
    return true;
}

bool ScConsole::Gsw(const CmdArgs &args) {
    if (!can_use_cheat_command() && !can_use_cheat_command_in_replay()) {
        return false;
    }
    if (!is_in_game() || !isdigit(*args[1])) {
        return false;
    }

    bw::game_speed_waits[*bw::game_speed] = atoi(args[1]);
    return true;
}

bool ScConsole::AiRegion(const CmdArgs &args) {
    if (!is_in_game() || !isdigit(*args[1]) || !isxdigit(*args[2])) {
        return false;
    }

    unsigned int player = atoi(args[1]), region_id = strtoul(args[2], 0, 16);
    if (player >= Limits::ActivePlayers || region_id > (*bw::pathing)->region_count) {
        return false;
    }

    Ai::Region *region = bw::player_ai_regions[player] + region_id;
    char buf[64];
    sprintf(buf, "State %d, flags %02x", region->state, region->flags);
    Print(buf);
    return true;
}


bool ScConsole::Player(const CmdArgs &args) {
    Unit *unit = GetUnit();
    if (!unit) {
        return false;
    }

    Printf("%d", unit->player);
    return true;
}

bool ScConsole::Race(const CmdArgs &args) {
    Unit *unit = GetUnit();
    if (!unit) {
        return false;
    }

    Printf("%d", bw::players[unit->player].race);
    return true;
}

const char* flag_desc[] = {
    "Completed", "Building", "Air", "Disabled?", "Burrowed", "In building", "In transport", "Unk", "Invisibility done", "Begin invisibility", "Disabled",
    "Free invisibility", "Uninterruptable order", "Nobrkcodestart", "Has disappearing creep", "Under disruption web", "Auto attack?", "Reacts",
    "Ignore collision?", "Move target moved?", "Collides?", "No collision", "Enemy collision?", "Harvesting", "Unk", "Unk", "Invincible",
    "Hold position", "Movement speed upgrade", "Attack speed upgrade", "Hallucination", "Self destructing"
};

bool ScConsole::UnitCmd(const CmdArgs &args) {
    enum class SubCmd {
        None,

        Ai,
        Hp,
        Energy,
        Shields,
        Cost,
        Kill,
        Duplicate,
    };

    std::string cmd = args[1];
    const auto subcmd = [&cmd] {
        if (cmd == "a" || cmd == "ai")        return SubCmd::Ai;
        if (cmd == "h" || cmd == "hp")        return SubCmd::Hp;
        if (cmd == "e" || cmd == "energy")    return SubCmd::Energy;
        if (cmd == "s" || cmd == "shields")   return SubCmd::Shields;
        if (cmd == "c" || cmd == "cost")      return SubCmd::Cost;
        if (cmd == "k" || cmd == "kill")      return SubCmd::Kill;
        if (cmd == "d" || cmd == "duplicate") return SubCmd::Duplicate;
        return SubCmd::None;
    }();

    switch (subcmd) {
        case SubCmd::Ai:
        case SubCmd::Hp:
        case SubCmd::Energy:
        case SubCmd::Shields:
        case SubCmd::Cost: {
            if (!can_use_cheat_command() && !is_in_replay()) {
                return false;
            }
        } break;
        case SubCmd::Kill:
        case SubCmd::Duplicate: {
            if (!can_use_cheat_command()) {
                return false;
            }
        } break;
    }

    array_offset <Unit*, SelectionLimit> selected_units = GetSelectedUnits();
    if (selected_units.size() == 0) {
        Print("No units selected");
        return false;
    }

    int total_mineral_cost = 0;
    int total_vespene_cost = 0;
    int total_time_cost = 0;
    for (int i = 0; i < selected_units.size(); i++) {
        Unit* unit = selected_units[i];
        if (!unit) {
            continue;
        }
        switch (subcmd) {
            case SubCmd::Ai: {
                if (!unit->ai) {
                    Print("Unit has no AI");
                } else {
                    Printf("%d", unit->ai->type);
                }
            } break;
            case SubCmd::Hp: {
                if (isdigit(*args[2])) {
                    unit->hitpoints = int32_t (std::min(unit->GetMaxHitPoints() * 256, atoi(args[2]) * 256));
                }
                Printf("Unit HP: %d", unit->hitpoints / 256);
            } break;
            case SubCmd::Energy: {
                if (isdigit(*args[2])) {
                    unit->energy = int32_t (std::min(unit->GetMaxEnergy() * 256, atoi(args[2]) * 256));
                }
                Printf("Unit Energy: %d", unit->energy / 256);
            } break;
            case SubCmd::Shields: {
                if (isdigit(*args[2])) {
                    unit->shields = int32_t (std::min(unit->GetMaxShields() * 256, atoi(args[2]) * 256));
                }
                Printf("Unit Shields: %d", unit->shields / 256);
            } break;
            case SubCmd::Kill: {
                unit->order = 0;
            } break;
            case SubCmd::Cost: {
                total_mineral_cost += unit->GetMineralCost();
                total_vespene_cost += unit->GetVespeneCost();
                total_time_cost += unit->GetTimeCost();
            } break;
            case SubCmd::Duplicate: {
                if (unit->unit_id == UnitId::Interceptor) {
                    continue;
                }
                Point16 pos = Point16(*bw::screen_x + *bw::mouse_clickpos_x, *bw::screen_y + *bw::mouse_clickpos_y);
                Unit* created_unit = bw::CreateUnit(unit->unit_id, pos.x, pos.y, unit->player);
                if (!created_unit) {
                    continue;
                }
                bw::FinishUnit_Pre(created_unit);
                bw::FinishUnit(created_unit);
                bw::GiveAi(created_unit);
            } break;
        }
    }

    switch (subcmd) {
        case SubCmd::Cost: {
            Printf("Selection costs %u minerals | %u vespene | %u seconds", total_mineral_cost, total_vespene_cost, total_time_cost / 24);
        } break;
        case SubCmd::None: {
            Printf("unit ai|hp|shields|energy|kill|cost|duplicate");
        } break;
    }

    return true;
}

bool ScConsole::Money(const CmdArgs &args) {
    if (!can_use_cheat_command()) {
        return false;
    }

    if (!is_in_game()) {
        return false;
    }

    if (strcmp(args[1], "set") == 0) {
        if (!isdigit(*args[2]) || !isdigit(*args[3]) || !isdigit(*args[4])) {
            return false;
        }
        int player = atoi(args[2]), minerals = atoi(args[3]), gas = atoi(args[4]);
        if (!is_active_player(player)) {
            return false;
        }
        bw::minerals[player] = minerals;
        bw::gas[player] = gas;
    } else {
        int player;
        if (isdigit(*args[1])) {
            player = atoi(args[1]);
        } else {
            if (!GetUnit()) {
                return false;
            }
            player = GetUnit()->player;
        }
        if (!is_active_player(player)) {
            return false;
        }
        Printf("Minerals: %d, Gas %d", bw::minerals[player], bw::gas[player]);
    }
    return true;
}

#include "offsets.h"
bool ScConsole::UnitCount(const CmdArgs &args) {
    if (!is_in_game()) {
        return false;
    }
    char buf[64];	
    int count = *((uint32_t*)0x006283F0);
    sprintf(buf, "Total Unit Count: %d", count);
    Printf(buf);
    return true;
}

bool ScConsole::FastForward(const CmdArgs& args) {
    if (!can_use_cheat_command() && !can_use_cheat_command_in_replay()) {
        return false;
    }

    if (isFastForwarding) {
        return false;
    }
    if (!isdigit(*args[1])) {
        Print("Usage: ff <seconds>");
        return false;
    }
    int frames = atoi(args[1])*24;
    fastForwardStartFrames = *bw::frame_count;
    fastForwardEndFrames = fastForwardStartFrames + frames;
    return true;
}

static void PrintSupply(char* buf, int used, int available) {
    sprintf(buf + strlen(buf), " %d", used / 2);
    if (used & 1) {
        strcat(buf, ".5");
    }
    sprintf(buf + strlen(buf), "/%d", available / 2);
    if (used & 1) {
        strcat(buf, ".5");
    }
}

bool ScConsole::Supply(const CmdArgs &args) {
    if (!can_use_cheat_command() && !is_in_replay()) {
        return false;
    }

    if (!is_in_game()) {
        return false;
    }

    int player;
    if (isdigit(*args[1])) {
        player = atoi(args[1]);
    } else {
        if (!GetUnit()) {
            return false;
        }
        player = GetUnit()->player;
    }
    if (!is_active_player(player)) {
        return false;
    }

    char buf[64] = "Zerg";
    PrintSupply(buf, bw::zerg_supply_used[player], bw::zerg_supply_available[player]);
    strcat(buf, ", Terran");
    PrintSupply(buf, bw::terran_supply_used[player], bw::terran_supply_available[player]);
    strcat(buf, ", Protoss");
    PrintSupply(buf, bw::protoss_supply_used[player], bw::protoss_supply_available[player]);
    Printf(buf);
    return true;
}

bool ScConsole::Self(const CmdArgs &args) {
    if (*bw::local_player_id == *bw::local_unique_player_id) {
        Printf("Game %d, Net %d", *bw::local_player_id, *bw::self_net_player);
    } else {
        Printf("Shared %d, Unique %d, Net %d", *bw::local_player_id, *bw::local_unique_player_id, *bw::self_net_player);
    }
    return true;
}

bool ScConsole::Frame(const CmdArgs &args) {
    if (!is_in_game()) {
        return false;
    }

    Printf("Frame %d", *bw::frame_count);
    return true;
}

bool ScConsole::Pause(const CmdArgs &args) {
    if (!can_use_cheat_command() && !can_use_cheat_command_in_replay()) {
        return false;
    }

    if (!is_in_game()) {
        return false;
    }

    *bw::is_paused ^= 1;
    return true;
}

void ScConsole::ConstructInfoLines() {
    info_lines.clear();
    if (show_frame) {
        char str[32];
        sprintf(str, "Frame: %d", *bw::frame_count);
        info_lines.emplace_back(str);
    }
}

void ScConsole::DrawLocations(uint8_t *framebuf, xuint w, yuint h) {
    if (!draw_locations) {
        return;
    }

    Common::Surface surface(framebuf, w, h);
    Point32 screen_pos(*bw::screen_x, *bw::screen_y);
    for (Location &location : bw::locations) {
        Rect32 &area = location.area;
        surface.DrawLine(Point32(area.left, area.top) - screen_pos, Point32(area.right, area.top) - screen_pos, 0x7c,
                [](int x, int y){ return !bw::IsOutsideGameScreen(x, y); });
        surface.DrawLine(Point32(area.left, area.top) - screen_pos, Point32(area.left, area.bottom) - screen_pos, 0x7c,
                [](int x, int y){ return !bw::IsOutsideGameScreen(x, y); });
        surface.DrawLine(Point32(area.right, area.top) - screen_pos, Point32(area.right, area.bottom) - screen_pos, 0x7c,
                [](int x, int y){ return !bw::IsOutsideGameScreen(x, y); });
        surface.DrawLine(Point32(area.left, area.bottom) - screen_pos, Point32(area.right, area.bottom) - screen_pos, 0x7c,
                [](int x, int y){ return !bw::IsOutsideGameScreen(x, y); });
    }
}

void ScConsole::DrawCrects(uint8_t *framebuf, xuint w, yuint h) {
    if (!draw_crects) {
        return;
    }

    Common::Surface surface(framebuf, w, h);
    Point32 screen_pos(*bw::screen_x, *bw::screen_y);
    for (Unit *unit : *bw::first_active_unit) {
        Rect16 crect = unit->GetCollisionRect();
        surface.DrawLine(Point32(crect.left, crect.top) - screen_pos, Point32(crect.right, crect.top) - screen_pos, 0x74,
                [](int x, int y){ return !bw::IsOutsideGameScreen(x, y); });
        surface.DrawLine(Point32(crect.left, crect.top) - screen_pos, Point32(crect.left, crect.bottom) - screen_pos, 0x74,
                [](int x, int y){ return !bw::IsOutsideGameScreen(x, y); });
        surface.DrawLine(Point32(crect.right, crect.top) - screen_pos, Point32(crect.right, crect.bottom) - screen_pos, 0x74,
                [](int x, int y){ return !bw::IsOutsideGameScreen(x, y); });
        surface.DrawLine(Point32(crect.left, crect.bottom) - screen_pos, Point32(crect.right, crect.bottom) - screen_pos, 0x74,
                [](int x, int y){ return !bw::IsOutsideGameScreen(x, y); });
    }
}

static const char *RequestStr(int req) {
    switch (req) {
        case 1:
            return "Train";
        case 2:
            return "Guard";
        case 3:
            return "Build";
        case 4:
            return "Worker";
        case 5:
            return "Upgrade";
        case 6:
            return "Tech";
        case 7:
            return "Addon";
        case 8:
            return "Observer";
        default:
            return "Error";
    }
}

static int CountScripts(int player) {
    int count = 0;
    for (Ai::Script *script : *bw::first_active_ai_script) {
        if (script->player == player)
            count++;
    }
    return count;
}

void ScConsole::GetTownRequests(uint32_t *out, int len, uint32_t *in) {
    int pos = 0;
    while (pos != len && in[pos] != 0) {
        auto req = in[pos++];
        auto unit_id = req >> 16;
        auto amt = (req & 0xf8) >> 3;
        bool skip = false;
        for (int i = 0; i < len && in[i] != 0; i++) {
            auto other_amt = (in[i] & 0xf8) >> 3;
            if (in[i] >> 16 == unit_id && amt < other_amt && (in[i] & 0x6) == (req & 0x6)) {
                if (~in[i] & 0x1 || req & 0x1) {
                    skip = true;
                    break;
                }
            }
        }
        if (!skip || draw_ai_full) {
            *out++ = req;
        }
    }
    *out = 0;
}

void ScConsole::DrawAiRegions(int player, Common::Surface *text_surf, const Point32 &pos)
{
    Point32 screen_pos(*bw::screen_x, *bw::screen_y);
    for (int i = 0; i < (*bw::pathing)->region_count; i++)
    {
        Pathing::Region *p_region = (*bw::pathing)->regions + i;
        Point32 draw_pos = Point32(p_region->x / 0x100, p_region->y / 0x100) - screen_pos + pos;
        if (draw_pos.x < 0 || draw_pos.y < 0)
           continue;
        if (draw_pos.x >= resolution::game_width || draw_pos.y >= resolution::game_height)
           continue;
        Ai::Region *region = Ai::GetRegion(player, i);
        char buf[128];
        snprintf(buf, sizeof buf, "State %x target %x", region->state, region->target_region_id);
        text_surf->DrawText(&font, buf, draw_pos, 0x55);
        draw_pos += Point32(0, 10);
        snprintf(buf, sizeof buf, "Defense priority %d", region->defense_priority);
        text_surf->DrawText(&font, buf, draw_pos, 0x55);
        draw_pos += Point32(0, 10);
        snprintf(buf, sizeof buf, "Need %d/%d, Current %d/%d",
                region->needed_ground_strength, region->needed_air_strength,
                region->local_ground_strength, region->local_air_strength);
        text_surf->DrawText(&font, buf, draw_pos, 0x55);
        draw_pos += Point32(0, 10);
        snprintf(buf, sizeof buf, "All %d/%d, Enemy %d/%d",
                region->all_ground_strength, region->all_air_strength,
                region->enemy_ground_strength, region->enemy_air_strength);
        text_surf->DrawText(&font, buf, draw_pos, 0x55);
    }
}

#include "Iquare_ShortUnitNames.h"
//IQUARE
void ScConsole::DrawDeaths(uint8_t *textbuf, uint8_t *framebuf, xuint w, yuint h) {
    if (!draw_deaths) {
        return;
    }
    Common::Surface surface(framebuf, w, h);
    Common::Surface text_surface(textbuf, w, h);
    Point32 screen_pos(*bw::screen_x, *bw::screen_y);
    Point32 info_pos(10, 10);
    for (unsigned int i = 0; i < Limits::ActivePlayers; i++) {
        if (show_deaths[i] == 1) {
            int row = 0;
            int column = 0;
            for (int j = 0; j < 228; j++) {
                uint32_t *deaths = (uint32_t*)0x0058A364 +((j*12)+i);
                uint32_t result = *deaths;

                char buf[128];
                char name_buf[128];
                sprintf(buf, "%d", result);
            
                snprintf(name_buf, sizeof name_buf, "%s", ShortUnitName[j]);
                
                //snprintf(name_buf, sizeof name_buf, "%s", (*bw::stat_txt_tbl)->GetTblString(j + 1));
                text_surface.DrawText(&font, name_buf, Point32(10+(100*column), 10+(10*row)), 0x55);
                if (result != 0) {
                    text_surface.DrawText(&font, buf, Point32(90 + (100 * column), 10 + (10 * row)), 0x80);
                }
                row++;
                if (row == 40) {
                    row = 0;
                    column++;
                }
                
            }
            /*
            char str[128];
        auto &ai_data = bw::player_ai[i];
        snprintf(str, sizeof str, "Player %d: money %d / %d - need %d / %d / %d - available %d / %d / %d", i,
                bw::minerals[i], bw::gas[i], ai_data.mineral_need, ai_data.gas_need, ai_data.supply_need,
                ai_data.minerals_available, ai_data.gas_available, ai_data.supply_available);
        text_surface.DrawText(&font, str, info_pos, 0x55);
            */
        }
    }
}

void ScConsole::DrawAiInfo(uint8_t *textbuf, uint8_t *framebuf, xuint w, yuint h) {
    if (!draw_ai_towns) {
        return;
    }
    Common::Surface surface(framebuf, w, h);
    Common::Surface text_surface(textbuf, w, h);
    Point32 screen_pos(*bw::screen_x, *bw::screen_y);
    Point32 info_pos(10, 30);
    Point32 region_text_pos(0, 10);
    for (unsigned int i = 0; i < Limits::ActivePlayers; i++) {
        if (bw::players[i].type != 1 || show_ai[i] == 0) {
            continue;
        }
        for (Ai::Town *town = bw::active_ai_towns[i]; town; town = town->list.next) {
            Rect32 rect = Rect32(Point32(town->position), 5).OffsetBy(screen_pos.Negate());
            surface.DrawRect(rect, 0xb9);
            if (town->mineral != nullptr) {
                surface.DrawLine(town->mineral->sprite->position - screen_pos, town->position - screen_pos, 0x80);
            }
            for (int j = 0; j < 3; j++) {
                if (town->gas_buildings[j] != nullptr) {
                    surface.DrawLine(town->gas_buildings[j]->sprite->position - screen_pos, town->position - screen_pos, 0xba);
                }
            }
            if (town->building_scv != nullptr) {
                surface.DrawLine(town->building_scv->sprite->position - screen_pos, town->position - screen_pos, 0x71);
            }
            char str[64];
            snprintf(str, sizeof str, "Inited: %d, workers %d / %d", town->inited, town->worker_count, town->unk1b);
            Point32 draw_pos = town->position - screen_pos + Point32(0 - strlen(str) * 3, 20);
            text_surface.DrawText(&font, str, draw_pos, 0x55);
            draw_pos += Point32(0, 10);
            std::string req_str = "Requests: ";
            uint32_t requests[0x65];
            requests[0x64] = 0;
            GetTownRequests(requests, 0x64, town->build_requests);
            for (int i = 0; requests[i] != 0; ) {
                int line_len = draw_ai_named ? 4 : 8;
                for (int j = 0; j < line_len && requests[i] != 0; j++, i++) {
                    if (j != 0) {
                        req_str.append(", ");
                    }
                    char buf[128];
                    char name_buf[128];
                    uint32_t request = requests[i];
                    int unit_id = request >> 16;
                    if (draw_ai_named && request & 0x2) {
                        snprintf(name_buf, sizeof name_buf, "%s", UpgradeType(unit_id).Name());
                    } else if (draw_ai_named && request & 0x4) {
                        snprintf(name_buf, sizeof name_buf, "%s", TechType(unit_id).Name());
                    } else if (draw_ai_named) {
                        snprintf(name_buf, sizeof name_buf, "%s", cm::get_unit_tbl()->GetTblString(unit_id + 1));
                    } else {
                        snprintf(name_buf, sizeof name_buf, "%x:%x", (request & 0x6) >> 1, unit_id);
                    }

                    if (request & 1) {
                        snprintf(buf, sizeof buf, "%s (%d, if needed)", name_buf, (request & 0xf8) >> 3);
                    } else {
                        snprintf(buf, sizeof buf, "%s (%d)", name_buf, (request & 0xf8) >> 3);
                    }
                    req_str.append(buf);
                }
                text_surface.DrawText(&font, req_str, draw_pos + Point32(10, 0), 0x55);
                draw_pos += Point32(0, 10);
                req_str = "";
            }
        }
        char str[128];
        auto &ai_data = cm::get_player_ai(i);
        snprintf(str, sizeof str, "Player %d: money %d / %d - need %d / %d / %d - available %d / %d / %d", i,
                bw::minerals[i], bw::gas[i], ai_data.mineral_need, ai_data.gas_need, ai_data.supply_need,
                ai_data.minerals_available, ai_data.gas_available, ai_data.supply_available);
        text_surface.DrawText(&font, str, info_pos, 0x55);
        info_pos += Point32(0, 10);
        snprintf(str, sizeof str, "Request count %d, training unit %d, Script count %d",
                ai_data.request_count, ai_data.wanted_unit, CountScripts(i));
        text_surface.DrawText(&font, str, info_pos, 0x55);
        info_pos += Point32(0, 10);
        if (ai_data.attack_grouping_region != 0) {
            snprintf(str, sizeof str, "Attack region %x, started %d ago, failed %d", ai_data.attack_grouping_region - 1, *bw::elapsed_seconds - ai_data.last_attack_seconds, ai_data.attack_failed);
            text_surface.DrawText(&font, str, info_pos, 0x55);
            info_pos += Point32(0, 10);
        }
        if (ai_data.request_count) {
            std::string str = "Requests: ";
            std::unordered_set<uint32_t> collapsed_requests;
            for (int i = 0; i < ai_data.request_count;) {
                int line_len = draw_ai_named ? 4 : 8;
                for (int j = 0; j < line_len && i < ai_data.request_count;) {
                    bool skip = false;
                    auto unit_id = ai_data.requests[i].unit_id;
                    auto type = ai_data.requests[i].type;
                    int amt = 1;
                    uint32_t hashset_key = (unit_id << 16) | type;
                    if (i >= 4 && !draw_ai_full) {
                        if (collapsed_requests.count(hashset_key) != 0) {
                            skip = true;
                        } else {
                            for (int k = i + 1; k < ai_data.request_count; k++) {
                                auto other_req = ai_data.requests[k];
                                if (other_req.unit_id == unit_id && other_req.type == type) {
                                    amt += 1;
                                }
                            }
                            collapsed_requests.emplace(hashset_key);
                        }
                    }
                    if (!skip) {
                        if (j != 0) {
                            str.append(", ");
                        }
                        char buf[64];
                        const char *desc = RequestStr(type);
                        if (draw_ai_named && ai_data.requests[i].type == 5) {
                            snprintf(buf, sizeof buf, "%s %s", desc, UpgradeType(unit_id).Name());
                        } else if (draw_ai_named && ai_data.requests[i].type == 6) {
                            snprintf(buf, sizeof buf, "%s %s", desc, TechType(unit_id).Name());
                        } else if (draw_ai_named) {
                            auto name = cm::get_unit_tbl()->GetTblString(unit_id + 1);
                            snprintf(buf, sizeof buf, "%s %s", desc, name);
                        } else {
                            snprintf(buf, sizeof buf, "%s %x", desc, unit_id);
                        }
                        str.append(buf);
                        if (amt != 1) {
                            snprintf(buf, sizeof buf, " (x%d)", amt);
                            str.append(buf);
                        }
                        j += 1;
                    }
                    i += 1;
                }
                text_surface.DrawText(&font, str, info_pos + Point32(10, 0), 0x55);
                info_pos += Point32(0, 10);
                str = "";
            }
        }
        if (show_ai[i] == 2 && draw_ai_regions) {
            DrawAiRegions(i, &text_surface, region_text_pos);
            region_text_pos += Point32(0, 30);
        }
    }
}

void ScConsole::DrawAiUnitHomes(uint8_t *framebuf, xuint w, yuint h) {
    if (!draw_ai_unit_homes) {
        return;
    }

    Common::Surface surface(framebuf, w, h);
    Point32 screen_pos(*bw::screen_x, *bw::screen_y);
    for (Unit *unit : *bw::first_active_unit) {
        if (unit->ai != nullptr && show_ai[unit->player] != 0) {
            auto sprite_pos_screen = unit->sprite->position - screen_pos;
            switch (unit->ai->type) {
                case 1: {
                    auto ai = (Ai::GuardAi *)unit->ai;
                    surface.DrawLine(ai->home - screen_pos, sprite_pos_screen, 0x80);
                    if (ai->home != ai->unk_pos)
                    {
                        surface.DrawLine(ai->unk_pos - screen_pos, sprite_pos_screen, 0xba);
                    }
                } break;
                case 2: {
                    auto ai = (Ai::WorkerAi *)unit->ai;
                    surface.DrawLine(ai->town->position - screen_pos, sprite_pos_screen, 0x74);
                } break;
                case 3: {
                    auto ai = (Ai::BuildingAi *)unit->ai;
                    surface.DrawLine(ai->town->position - screen_pos, sprite_pos_screen, 0x74);
                } break;
                case 4: {
                    auto ai = (Ai::MilitaryAi *)unit->ai;
                    auto region = (*bw::pathing)->regions + ai->region->region_id;
                    auto region_pos = Point32(region->x / 0x100, region->y / 0x100);
                    surface.DrawLine(region_pos - screen_pos, sprite_pos_screen, 0x7c);
                } break;
            }
        }
    }
}

void ScConsole::DrawGuardAi(Common::Surface *surface, TextLayout *text_layout, Ai::GuardAi *ai, int player, bool alive) {
    char str[128];
    char unit_name[64];
    Point32 screen_pos(*bw::screen_x, *bw::screen_y);
    auto ai_pos = Point32(ai->home) - screen_pos;
    if (alive) {
        surface->DrawLine(ai->parent->sprite->position - screen_pos, ai_pos, 0x80);
    }

    Rect32 rect = Rect32(Point32(ai->home), 9).OffsetBy(screen_pos.Negate());
    surface->DrawRect(rect, 0xb9);
    if (ai->home != ai->unk_pos) {
        Rect32 rect = Rect32(Point32(ai->unk_pos), 9).OffsetBy(screen_pos.Negate());
        surface->DrawRect(rect, 0xb9);
    }
    if (ai->home != ai->unk_pos) {
        surface->DrawLine(ai->unk_pos - screen_pos, ai_pos, 0xba);
    }

    // Early exit if text is outside screen bounds
    int w = resolution::screen_width;
    int h = resolution::screen_height;
    if (ai_pos.x < -300 || ai_pos.y < 0 || ai_pos.x >= w + 100 || ai_pos.y >= h) {
        return;
    }
    if (draw_ai_named) {
        auto name = cm::get_unit_tbl()->GetTblString(ai->unit_id + 1);
        snprintf(unit_name, sizeof unit_name, "%s", name);
    } else {
        snprintf(unit_name, sizeof unit_name, "%x", ai->unit_id);
    }
    if (alive) {
        snprintf(str, sizeof str, "Player %d, alive %s, deaths %d", player, unit_name, ai->times_died);
    } else {
        snprintf(str, sizeof str, "Player %d, needed %s, deaths %d", player, unit_name, ai->times_died);
    }
    if (ai->previous_update != 0) {
        char buf2[sizeof str];
        auto time = *bw::elapsed_seconds - ai->previous_update;
        snprintf(buf2, sizeof buf2, "%s, requested %d ago", str, time);
        strcpy(str, buf2);
    }
    text_layout->Draw(&font, str, ai_pos + Point32(-100, 10), 0x55);
}

void ScConsole::DrawAiGuards(uint8_t *text_buf, uint8_t *framebuf, xuint w, yuint h) {
    if (!draw_ai_guards) {
        return;
    }

    Common::Surface surface(framebuf, w, h);
    Common::Surface text_surface(text_buf, w, h);
    TextLayout text(&text_surface);
    Point32 screen_pos(*bw::screen_x, *bw::screen_y);
    for (Unit *unit : *bw::first_active_unit) {
        if (unit->ai != nullptr && show_ai[unit->player] != 0 && unit->ai->type == 1) {
            DrawGuardAi(&surface, &text, (Ai::GuardAi *)unit->ai, unit->player, true);
        }
        if (unit->ai != nullptr && show_ai[unit->player] != 0 && unit->ai->type == 3) {
            auto ai = (Ai::BuildingAi *)unit->ai;
            for (int i = 0; i < 5; i++) {
                if (ai->train_queue_types[i] == 2 && ai->train_queue_values[i] != nullptr) {
                    auto guard_ai = (Ai::GuardAi *)ai->train_queue_values[i];
                    auto ai_pos = guard_ai->home - screen_pos;
                    surface.DrawLine(unit->sprite->position - screen_pos, ai_pos, 0xa4);
                }
            }
        }
    }
    for (Unit *unit : *bw::first_hidden_unit) {
        if (unit->ai != nullptr && show_ai[unit->player] != 0 && unit->ai->type == 1) {
            DrawGuardAi(&surface, &text, (Ai::GuardAi *)unit->ai, unit->player, true);
        }
    }
    for (int i = 0; i < Limits::ActivePlayers; i++) {
        if (show_ai[i]) {
            for (Ai::GuardAi *ai : bw::first_guard_ai[i]) {
                if (ai->parent == nullptr) {
                    DrawGuardAi(&surface, &text, ai, i, false);
                }
            }
        }
    }
}

void ScConsole::DrawResourceAreas(uint8_t *textbuf, uint8_t *framebuf, xuint w, yuint h) {
    if (!draw_resource_areas) {
        return;
    }

    Common::Surface surface(framebuf, w, h);
    Common::Surface text_surface(textbuf, w, h);
    Point32 screen_pos(*bw::screen_x, *bw::screen_y);
    for (int i = 0; i < bw::resource_areas->used_count; i++) {
        // First entry is not used
        const auto &area = bw::resource_areas->areas[i + 1];
        int x = area.position.x - screen_pos.x;
        int y = area.position.y - screen_pos.y;
        if (x >= 0 && x < w && y >= 0 && y < h) {
            char buf[256];
            snprintf(buf, sizeof buf, "Area %x [%d]: Mine %d in %d, Gas %d in %d, flags %02x", i + 1, i+1,
                    area.total_minerals, area.mineral_field_count,
                    area.total_gas, area.geyser_count, area.flags);
            text_surface.DrawText(&font, buf, Point32(x - 50, y + 20), 0x55);
            snprintf(buf, sizeof buf, "Unk: %02x %08x %08x %08x %08x", area.is_start_location,
                    area.unk10[0], area.unk10[1], area.unk10[2], area.unk10[3]);
            text_surface.DrawText(&font, buf, Point32(x - 50, y + 30), 0x55);
            snprintf(buf, sizeof buf, "%08x %08x %08x %08x",
                    area.unk10[4], area.unk10[5], area.unk10[6], area.unk10[7]);
            text_surface.DrawText(&font, buf, Point32(x - 50, y + 40), 0x55);
            Rect32 rect = Rect32(Point32(area.position), 15).OffsetBy(screen_pos.Negate());
            surface.DrawRect(rect, 0xb9);
        }
    }
}

void ScConsole::DrawOrders(uint8_t *framebuf, xuint w, yuint h) {
    if (draw_orders == OrderDrawMode::None) {
        return;
    }

    Common::Surface surface(framebuf, w, h);
    Point32 screen_pos(*bw::screen_x, *bw::screen_y);
    if (draw_orders == OrderDrawMode::All) {
        for (Unit *unit : *bw::first_active_unit) {
            if (unit->target) {
                surface.DrawLine(unit->target->sprite->position - screen_pos, unit->sprite->position - screen_pos, 0xa4);
            } else if (unit->order_target_pos != Point16(0, 0)) {
                surface.DrawLine(unit->order_target_pos - screen_pos, unit->sprite->position - screen_pos, 0xa4);
            }
        }
    } else if (draw_orders == OrderDrawMode::Selected) {
        for (Unit *unit : client_select) {
            if (unit->target) {
                surface.DrawLine(unit->target->sprite->position - screen_pos, unit->sprite->position - screen_pos, 0xa4);
            } else if (unit->order_target_pos != Point16(0, 0)) {
                surface.DrawLine(unit->order_target_pos - screen_pos, unit->sprite->position - screen_pos, 0xa4);
            }
        }
    }
}

void ScConsole::DrawCoords(uint8_t *framebuf, xuint w, yuint h) {
    if (!draw_coords) {
        return;
    }

    char buf[32];
    snprintf(buf, sizeof buf, "Game: %04hx.%04hx", *bw::screen_x + *bw::mouse_clickpos_x, *bw::screen_y + *bw::mouse_clickpos_y);
    info_lines.emplace_back(buf);
    snprintf(buf, sizeof buf, "Mouse: %04hx.%04hx", (int)*bw::mouse_clickpos_x, (int)*bw::mouse_clickpos_y);
    info_lines.emplace_back(buf);
    snprintf(buf, sizeof buf, "Screen: %04hx.%04hx", (int)*bw::screen_x, (int)*bw::screen_y);
    info_lines.emplace_back(buf);
}

void ScConsole::DrawRange(uint8_t *framebuf, xuint w, yuint h) {
    if (!draw_range) {
        return;
    }

    Common::Surface surface(framebuf, w, resolution::game_height);
    Point32 screen_pos(*bw::screen_x, *bw::screen_y);
    for (Unit *unit : *bw::first_active_unit) {
        Point32 pos = Point32(unit->sprite->position) - screen_pos;
        WeaponType ground_weapon = unit->GetGroundWeapon();
        WeaponType air_weapon = unit->GetAirWeapon();
        const auto dbox = unit->Type().DimensionBox();
        int unit_radius_approx = (dbox.top + dbox.bottom + dbox.left + dbox.right) / 4 + 1;
        if (ground_weapon != WeaponId::None) {
            surface.DrawCircle(pos, unit->GetWeaponRange(true) + unit_radius_approx, 0x75, [](int x, int y){ return !bw::IsOutsideGameScreen(x, y); });
        }
        if (air_weapon != WeaponId::None && ground_weapon != air_weapon) {
            surface.DrawCircle(pos, unit->GetWeaponRange(false) + unit_radius_approx, 0x7a, [](int x, int y){ return !bw::IsOutsideGameScreen(x, y); });
        }
    }
}

void ScConsole::DrawGrids(uint8_t *framebuf, xuint w, yuint h) {
    if (grids.empty()) {
        return;
    }

    Common::Surface surface(framebuf, w, resolution::game_height);
    Point32 screen_pos(*bw::screen_x, *bw::screen_y);
    x32 x_end = screen_pos.x + w;
    y32 y_end = screen_pos.y + h;
    for (const auto &grid : grids) {
        x32 x = 0 - screen_pos.x % grid.width - 1;
        while (x < x_end) {
            surface.DrawLine(Point32(x, 0), Point32(x, h), grid.color);
            x += grid.width;
        }
        y32 y =  0 - screen_pos.y % grid.height - 1;
        while (y < y_end) {
            surface.DrawLine(Point32(0, y), Point32(w, y), grid.color);
            y += grid.height;
        }
    }
}

void ScConsole::DrawDebugInfo(uint8_t *framebuf, xuint w, yuint h) {
    const bool has_visual_overlay =
        show_frame || draw_locations || draw_crects || !grids.empty() ||
        draw_ai_towns || draw_deaths || draw_ai_unit_homes || draw_ai_guards ||
        draw_resource_areas || draw_orders != OrderDrawMode::None || draw_coords ||
        draw_range || draw_region_data;
    if (!has_visual_overlay) {
        UpdateFastForwardProgress();
        return;
    }

    ConstructInfoLines();
    static uint8_t buffer[resolution::maximum_frame_size];
    static uint8_t text_buf[resolution::maximum_frame_size];
    const size_t active_frame_size =
        static_cast<size_t>(resolution::screen_width) *
        static_cast<unsigned>(resolution::screen_height);
    memset(buffer, 0, active_frame_size);
    memset(text_buf, 0, active_frame_size);
    DrawLocations(framebuf, w, h);
    DrawCrects(framebuf, w, h);
    DrawGrids(buffer, resolution::screen_width, resolution::screen_height);
    DrawAiInfo(text_buf, buffer, resolution::screen_width, resolution::screen_height);
    //IQUARE
    DrawDeaths(text_buf, buffer, resolution::screen_width, resolution::screen_height);
    DrawAiUnitHomes(buffer, resolution::screen_width, resolution::screen_height);
    DrawAiGuards(text_buf, buffer, resolution::screen_width, resolution::screen_height);
    DrawResourceAreas(text_buf, buffer, resolution::screen_width, resolution::screen_height);
    DrawOrders(buffer, resolution::screen_width, resolution::screen_height);
    DrawCoords(buffer, resolution::screen_width, resolution::screen_height);
    DrawRange(buffer, resolution::screen_width, resolution::screen_height);
    if (draw_region_data) {
        DrawRegionData(text_buf, w, h);
    }
    if (!info_lines.empty()) {
        int info_lines_width = font.TextLength(*std::max_element(info_lines.begin(), info_lines.end(),
            [this](const auto &a, const auto &b) {
            return font.TextLength(a) < font.TextLength(b);
        }));
        Point32 info_pos(resolution::screen_width - info_lines_width - 10, 40);
        Common::Surface surf(buffer, resolution::screen_width, resolution::screen_height);
        for (const auto &line : info_lines) {
            surf.DrawText(&font, line, info_pos, 0x55);
            info_pos += Point32(0, 10);
        }
    }
    for (unsigned y = 0; y < resolution::game_height; y++) {
        unsigned x = 0;
        for (; x + 4 <= resolution::game_width; x += 4) {
            if (*(uint32_t*)(buffer + y * resolution::screen_width + x) == 0) {
                continue;
            }
            if (buffer[y * resolution::screen_width + x] != 0 && !bw::IsOutsideGameScreen(x, y)) {
                framebuf[y * w + x] = buffer[y * resolution::screen_width + x];
            }
            if (buffer[y * resolution::screen_width + x + 1] != 0 && !bw::IsOutsideGameScreen(x + 1, y)) {
                framebuf[y * w + x + 1] = buffer[y * resolution::screen_width + x + 1];
            }
            if (buffer[y * resolution::screen_width + x + 2] != 0 && !bw::IsOutsideGameScreen(x + 2, y)) {
                framebuf[y * w + x + 2] = buffer[y * resolution::screen_width + x + 2];
            }
            if (buffer[y * resolution::screen_width + x + 3] != 0 && !bw::IsOutsideGameScreen(x + 3, y)) {
                framebuf[y * w + x + 3] = buffer[y * resolution::screen_width + x + 3];
            }
        }
        for (; x < resolution::game_width; ++x) {
            const uint8_t color =
                buffer[y * resolution::screen_width + x];
            if (color != 0 && !bw::IsOutsideGameScreen(x, y)) {
                framebuf[y * w + x] = color;
            }
        }
    }
    for (unsigned y = 1; y < resolution::screen_height - 1; y++) {
        for (unsigned x = 1; x < resolution::screen_width - 1; x++) {
            auto color = text_buf[y * resolution::screen_width + x];
            if (color != 0) {
                if (text_buf[y * resolution::screen_width + x - 1] == 0) {
                    framebuf[y * w + x - 1] = 0;
                    if (x > 1 && text_buf[y * resolution::screen_width + x - 2] == 0) {
                        framebuf[y * w + x - 2] = 0;
                    }
                }
                if (text_buf[y * resolution::screen_width + x + 1] == 0) {
                    framebuf[y * w + x + 1] = 0;
                }
                if (text_buf[(y + 1) * resolution::screen_width + x] == 0) {
                    framebuf[(y + 1) * w + x] = 0;
                }
                if (text_buf[(y - 1) * resolution::screen_width + x] == 0) {
                    framebuf[(y - 1) * w + x] = 0;
                }
                framebuf[y * w + x] = text_buf[y * resolution::screen_width + x];
            }
        }
    }
    UpdateFastForwardProgress();
}

void ScConsole::UpdateFastForwardProgress() {
    if (isFastForwarding) {
        int duration = fastForwardEndFrames - fastForwardStartFrames;
        int part = *bw::frame_count - fastForwardStartFrames;
        float progress = std::roundf(((float)part/ (float)duration) * 100);
        Clear();
        Printf("Fast forwarding %d game seconds... %2.0f%%.", duration / 24, progress);
        Print("To stop, press ESC.");

    }

    if (!isFastForwarding && fastForwardEndFrames > 0) {
        EndFastForward();
    }
}

void ScConsole::EndFastForward() {
    isFastForwarding = false;
    fastForwardStartFrames = 0;
    fastForwardEndFrames = 0;
    bw::game_speed_waits[*bw::game_speed] = 42;
    Clear();
}

bool ScConsole::ShowAi(const CmdArgs& args) {
    draw_ai_towns = !draw_ai_towns;
    draw_ai_data = !draw_ai_data;
    return true;
}

bool ScConsole::ShowAiGuards(const CmdArgs& args) {
    draw_ai_guards = !draw_ai_guards;
    draw_ai_towns = true;
    draw_ai_data = true;
    return true;
}

bool ScConsole::ShowAiUnits(const CmdArgs& args) {
    draw_ai_unit_homes = !draw_ai_unit_homes;
    return true;
}

bool ScConsole::ShowAiPlayer(const CmdArgs& args) {
    if (std::string(args[1]) == "all") {
        for (int i = 0; i < Limits::Players; i++) {
            show_ai[i] = 1;
        }
    } else {
        int player = atoi(args[1]);
        if (is_active_player(player)) {
            for (int i = 0; i < Limits::Players; i++) {
                show_ai[i] = 0;
            }
            show_ai[player] = 2;
        }
    }
    draw_ai_towns = true;
    draw_ai_data = true;
    return true;
}

bool ScConsole::Show(const CmdArgs &args) {
    std::string what(args[1]);

    if (what == "nothing") {
        //IQUARE
        draw_locations = draw_paths = draw_crects = draw_coords = draw_info =
            draw_range = draw_region_borders = draw_region_data = draw_ai_data = draw_ai_towns =
            show_fps = show_frame = draw_resource_areas = draw_deaths = false;
        draw_orders = OrderDrawMode::None;
    } else if (what == "frame") {
        show_frame = !show_frame;
    } else if (what == "locations") {
        if (!can_use_cheat_command() && !is_in_replay()) return false;

        draw_locations = !draw_locations;
    } else if (what == "paths") {
        draw_paths = !draw_paths;
    } else if (what == "collision") {
        if (!can_use_cheat_command() && !is_in_replay()) return false;

        draw_crects = !draw_crects;
    } else if (what == "coords") {
        draw_coords = !draw_coords;
    } else if (what == "info") {
        draw_info = !draw_info;
    } else if (what == "range") {
        if (!can_use_cheat_command() && !is_in_replay()) return false;

        draw_range = !draw_range;
    } else if (what == "resareas") {
        draw_resource_areas = !draw_resource_areas;
    } else if (what == "regions") {
        draw_region_borders = !draw_region_borders;
        draw_region_data = draw_region_borders;
    } else if (what == "orders") {
        if (!can_use_cheat_command() && !is_in_replay()) return false;

        std::string more(args[2]);
        if (more == "selected") {
            draw_orders = OrderDrawMode::Selected;
        } else if (more == "all") {
            draw_orders = OrderDrawMode::All;
        } else if (more != "") {
            return false;
        } else {
            if (draw_orders == OrderDrawMode::None) {
                draw_orders = OrderDrawMode::All;
            } else {
                draw_orders = OrderDrawMode::None;
            }
        }
    } else if (what == "deaths") {
        //IQUARE
        std::string more(args[2]);	
        if (more == "") {
            draw_deaths = false;
        } else {
            int player = atoi(args[2]);
            draw_deaths = true;
            for (int i = 0; i < Limits::Players; i++) {
                show_deaths[i] = 0;
            }
            show_deaths[player] = 1;

        }		
    } else if (what == "ai") {
        std::string more(args[2]);
        if (more == "full") {
            draw_ai_full = true;
        } else if (more == "simple") {
            draw_ai_full = false;
        } else if (more == "named") {
            draw_ai_named = true;
        } else if (more == "raw") {
            draw_ai_named = false;
        } else if (more == "player") {
            if (std::string(args[3]) == "all") {
                for (int i = 0; i < Limits::Players; i++) {
                    show_ai[i] = 1;
                }
            } else {
                int player = atoi(args[3]);
                if (is_active_player(player)) {
                    for (int i = 0; i < Limits::Players; i++) {
                        show_ai[i] = 0;
                    }
                    show_ai[player] = 2;
                }
            }
        } else if (more == "units") {
            draw_ai_unit_homes = !draw_ai_unit_homes;
        } else if (more == "guards") {
            draw_ai_guards = !draw_ai_guards;
        } else if (more == "regions") {
            draw_ai_regions = !draw_ai_regions;
        } else if (more == "") {
            draw_ai_towns = !draw_ai_towns;
            draw_ai_data = !draw_ai_data;
        } else {
            return false;
        }
        if (more != "" && more != "units") {
            draw_ai_towns = true;
            draw_ai_data = true;
        }
    } else {
        Printf("show <nothing|frame|regions|locations|paths|collision|coords|range|info|resareas>");
        Printf("show ai [full|simple|named|raw|units|guards|(player <player|all>>)]");
        Printf("show orders [all|selected]");
        return false;
    }
    return true;
}

static void DrawHook(uint8_t *framebuf, xuint w, yuint h) {
    if (console) {
        if (is_in_game()) {
            ((ScConsole *)console)->DrawDebugInfo(framebuf, w, h);
        }
        console->Draw(framebuf, w, h);
    }
}

void PatchConsole() {
    console = new ScConsole;
    if (!console->IsOk()) {
        return;
    }
    AddDrawHook(&DrawHook, 500);
    AddDrawHook(&DrawPathingInfo, 450);
}

typedef LRESULT (CALLBACK WndProc)(HWND, UINT, WPARAM, LPARAM);
static WndProc *OldWndProc;
static HWND console_hwnd = NULL;
namespace {
    struct InputTraceState {
        bool initialized = false;
        bool left_down = false;
        int max_raw_x = 0;
        int max_raw_y = 0;
        int max_forwarded_x = 0;
        int max_forwarded_y = 0;
        int max_engine_x = 0;
        int max_engine_y = 0;
        int last_zone = -1;
        DWORD last_sample_tick = 0;
    } input_trace;

    bool IsTracedMouseMessage(UINT msg) {
        switch (msg) {
            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MBUTTONDBLCLK:
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
            case WM_XBUTTONDBLCLK:
                return true;
            default:
                return false;
        }
    }

    const char *MouseMessageName(UINT msg) {
        switch (msg) {
            case WM_MOUSEMOVE: return "move";
            case WM_LBUTTONDOWN: return "left-down";
            case WM_LBUTTONUP: return "left-up";
            case WM_LBUTTONDBLCLK: return "left-double";
            case WM_RBUTTONDOWN: return "right-down";
            case WM_RBUTTONUP: return "right-up";
            case WM_RBUTTONDBLCLK: return "right-double";
            case WM_MBUTTONDOWN: return "middle-down";
            case WM_MBUTTONUP: return "middle-up";
            case WM_MBUTTONDBLCLK: return "middle-double";
            case WM_XBUTTONDOWN: return "x-down";
            case WM_XBUTTONUP: return "x-up";
            case WM_XBUTTONDBLCLK: return "x-double";
            default: return "unknown";
        }
    }

    bool NativeMaskConsumesInput(int native_x, int native_y) {
        if (native_x < 0 ||
            native_x >= static_cast<int>(resolution::native_width) ||
            native_y < static_cast<int>(resolution::native_hud_top) ||
            native_y >= static_cast<int>(resolution::native_height))
            return false;

        // This is the same native STrans mask StarCraft uses to distinguish
        // solid console artwork from visible/clickable terrain gaps.
        if (!*bw::trans_list)
            return true;
        if (native_y < *bw::trans_mouse_y_min)
            return false;
        if (native_y >= *bw::trans_mouse_y_max)
            return true;
        return bw::STransGetPixel(*bw::trans_list, native_x, native_y) == 0;
    }

    bool NativeHudConsumesInput(int physical_x, int physical_y) {
        return NativeMaskConsumesInput(
            physical_x - static_cast<int>(resolution::hud_left),
            physical_y - static_cast<int>(resolution::hud_top -
                resolution::native_hud_top));
    }

    // The Game Menu button is a transparent 71x19 control whose exact native
    // bounds were verified from its live BinDlg. It must participate in input
    // routing even where the STrans artwork mask is transparent.
    bool NativeGameMenuControlContains(int native_x, int native_y) {
        return native_x >= 416 && native_x <= 486 &&
            native_y >= 388 && native_y <= 406;
    }

    bool PresentedGameMenuControlContains(int physical_x, int physical_y) {
        const int native_x = physical_x -
            static_cast<int>(resolution::hud_left);
        const int native_y = physical_y -
            static_cast<int>(resolution::hud_top -
                resolution::native_hud_top);
        return NativeGameMenuControlContains(native_x, native_y);
    }

    bool ExpandedHudConsumesInput(int physical_x, int physical_y) {
        if (physical_x < static_cast<int>(resolution::hud_left) ||
            physical_x >= static_cast<int>(resolution::hud_left +
                resolution::native_width) ||
            physical_y < static_cast<int>(resolution::hud_top) ||
            physical_y >= static_cast<int>(resolution::screen_height))
            return false;

        // Above the logical battlefield, retain StarCraft's transparency mask
        // so terrain visible between HUD ornaments stays interactive. The
        // bottom presentation row has no battlefield behind it; translate the
        // whole row so minimap pixels and unmasked control art cannot fall
        // through to the expanded gameplay handler.
        return PresentedGameMenuControlContains(physical_x, physical_y) ||
            physical_y >= static_cast<int>(resolution::game_height) ||
            NativeHudConsumesInput(physical_x, physical_y);
    }

    bool HiddenNativeHudConsumesInput(int physical_x, int physical_y) {
        // The live dialog tree remains at native coordinates even though the
        // compositor presents it elsewhere. Detect only solid pixels in that
        // obsolete location so they can be bypassed for gameplay input.
        return physical_x >= 0 && physical_y >= 0 &&
            physical_x < static_cast<int>(resolution::native_width) &&
            physical_y < static_cast<int>(resolution::native_height) &&
            (NativeMaskConsumesInput(physical_x, physical_y) ||
             NativeGameMenuControlContains(physical_x, physical_y));
    }

    typedef void (__fastcall *GameplayClickProc)(void *);
    GameplayClickProc traced_left_click_proc;
    GameplayClickProc traced_right_click_proc;
    int expected_gameplay_click_x;
    int expected_gameplay_click_y;
    bool expected_gameplay_click_valid;
    int latest_physical_mouse_x;
    int latest_physical_mouse_y;
    bool latest_physical_mouse_valid;
    bool suppress_legacy_hud_tooltip;
    bool translated_hud_drag_active;
    bool expanded_battlefield_drag_active;
    bool translated_minimap_drag_active;
    bool suppress_translated_hud_cursor_warp;
    uint16_t latest_status_tooltip_control_index;
    bool hold_expanded_context_help;

    bool IsStatusInformationControlIndex(uint16_t index) {
        return index >= 9 && index <= 12;
    }

    struct DialogControlGroupHit {
        bool inside_anchor;
        bool inside_group;
    };

    DialogControlGroupHit HitDialogControlGroup(
        void *anchor_control, void *event,
        uint16_t first_index, uint16_t last_index) {
        DialogControlGroupHit result = {};
        if (!anchor_control || !event)
            return result;

        const uint8_t *anchor = static_cast<const uint8_t *>(
            anchor_control);
        const uint8_t *parent = *reinterpret_cast<uint8_t *const *>(
            anchor + 0x32);
        const uint8_t *coordinate_root = parent;
        if (coordinate_root &&
            *reinterpret_cast<const uint16_t *>(coordinate_root + 0x22) != 0)
        {
            coordinate_root = *reinterpret_cast<uint8_t *const *>(
                coordinate_root + 0x32);
        }

        const uint8_t *event_bytes = static_cast<const uint8_t *>(event);
        int local_x = static_cast<int>(
            *reinterpret_cast<const uint16_t *>(event_bytes + 0x0e));
        int local_y = static_cast<int>(
            *reinterpret_cast<const uint16_t *>(event_bytes + 0x10));
        if (coordinate_root)
        {
            local_x -= static_cast<int>(
                *reinterpret_cast<const int16_t *>(coordinate_root + 0x04));
            local_y -= static_cast<int>(
                *reinterpret_cast<const int16_t *>(coordinate_root + 0x06));
        }

        const int anchor_left = static_cast<int>(
            *reinterpret_cast<const int16_t *>(anchor + 0x04));
        const int anchor_top = static_cast<int>(
            *reinterpret_cast<const int16_t *>(anchor + 0x06));
        const int anchor_right = static_cast<int>(
            *reinterpret_cast<const int16_t *>(anchor + 0x08));
        const int anchor_bottom = static_cast<int>(
            *reinterpret_cast<const int16_t *>(anchor + 0x0a));
        result.inside_anchor = local_x >= anchor_left &&
            local_x <= anchor_right && local_y >= anchor_top &&
            local_y <= anchor_bottom;

        int group_left = INT_MAX;
        int group_top = INT_MAX;
        int group_right = INT_MIN;
        int group_bottom = INT_MIN;
        const uint8_t *sibling = parent ?
            *reinterpret_cast<uint8_t *const *>(parent + 0x42) : nullptr;
        unsigned sibling_count = 0;
        while (sibling && sibling_count++ < 128)
        {
            const uint16_t sibling_index =
                *reinterpret_cast<const uint16_t *>(sibling + 0x20);
            if (sibling_index >= first_index && sibling_index <= last_index)
            {
                group_left = std::min(group_left,
                    static_cast<int>(*reinterpret_cast<const int16_t *>(
                        sibling + 0x04)));
                group_top = std::min(group_top,
                    static_cast<int>(*reinterpret_cast<const int16_t *>(
                        sibling + 0x06)));
                group_right = std::max(group_right,
                    static_cast<int>(*reinterpret_cast<const int16_t *>(
                        sibling + 0x08)));
                group_bottom = std::max(group_bottom,
                    static_cast<int>(*reinterpret_cast<const int16_t *>(
                        sibling + 0x0a)));
            }
            sibling = *reinterpret_cast<uint8_t *const *>(sibling);
        }
        result.inside_group = group_left <= group_right &&
            group_top <= group_bottom && local_x >= group_left &&
            local_x <= group_right && local_y >= group_top &&
            local_y <= group_bottom;
        return result;
    }

    bool HitInteractiveStatusEnvelope(void *anchor_control, void *event) {
        if (!anchor_control || !event)
            return false;

        const uint8_t *anchor = static_cast<const uint8_t *>(anchor_control);
        const uint8_t *parent = *reinterpret_cast<uint8_t *const *>(
            anchor + 0x32);
        const uint8_t *coordinate_root = parent;
        if (coordinate_root &&
            *reinterpret_cast<const uint16_t *>(coordinate_root + 0x22) != 0)
        {
            coordinate_root = *reinterpret_cast<uint8_t *const *>(
                coordinate_root + 0x32);
        }

        const uint8_t *event_bytes = static_cast<const uint8_t *>(event);
        int local_x = static_cast<int>(
            *reinterpret_cast<const uint16_t *>(event_bytes + 0x0e));
        int local_y = static_cast<int>(
            *reinterpret_cast<const uint16_t *>(event_bytes + 0x10));
        if (coordinate_root)
        {
            local_x -= static_cast<int>(
                *reinterpret_cast<const int16_t *>(coordinate_root + 0x04));
            local_y -= static_cast<int>(
                *reinterpret_cast<const int16_t *>(coordinate_root + 0x06));
        }

        int left = INT_MAX;
        int top = INT_MAX;
        int right = INT_MIN;
        int bottom = INT_MIN;
        const uint8_t *sibling = parent ?
            *reinterpret_cast<uint8_t *const *>(parent + 0x42) : nullptr;
        unsigned sibling_count = 0;
        while (sibling && sibling_count++ < 128)
        {
            const uint16_t index =
                *reinterpret_cast<const uint16_t *>(sibling + 0x20);
            void *callback = *reinterpret_cast<void *const *>(sibling + 0x26);
            if (index != 0 && callback)
            {
                left = std::min(left, static_cast<int>(
                    *reinterpret_cast<const int16_t *>(sibling + 0x04)));
                top = std::min(top, static_cast<int>(
                    *reinterpret_cast<const int16_t *>(sibling + 0x06)));
                right = std::max(right, static_cast<int>(
                    *reinterpret_cast<const int16_t *>(sibling + 0x08)));
                bottom = std::max(bottom, static_cast<int>(
                    *reinterpret_cast<const int16_t *>(sibling + 0x0a)));
            }
            sibling = *reinterpret_cast<uint8_t *const *>(sibling);
        }
        return left <= right && top <= bottom &&
            local_x >= left && local_x <= right &&
            local_y >= top && local_y <= bottom;
    }

    bool CorrectGameplayClickEvent(void *event, unsigned *original_x,
                                   unsigned *original_y) {
        uint8_t *bytes = static_cast<uint8_t *>(event);
        uint16_t *event_x = event ?
            reinterpret_cast<uint16_t *>(bytes + 0x0e) : nullptr;
        uint16_t *event_y = event ?
            reinterpret_cast<uint16_t *>(bytes + 0x10) : nullptr;
        *original_x = event_x ? *event_x : 0;
        *original_y = event_y ? *event_y : 0;
        if (!event_x || !event_y || !expected_gameplay_click_valid ||
            (*original_x == static_cast<unsigned>(expected_gameplay_click_x) &&
             *original_y == static_cast<unsigned>(expected_gameplay_click_y)))
            return false;
        *event_x = static_cast<uint16_t>(expected_gameplay_click_x);
        *event_y = static_cast<uint16_t>(expected_gameplay_click_y);
        return true;
    }

    void __fastcall TraceExpandedLeftClick(void *event) {
        unsigned original_x = 0;
        unsigned original_y = 0;
        const bool corrected = CorrectGameplayClickEvent(
            event, &original_x, &original_y);
        const int engine_x_before = static_cast<int>(*bw::mouse_clickpos_x);
        const int engine_y_before = static_cast<int>(*bw::mouse_clickpos_y);
        const bool placement_click = traced_left_click_proc ==
                reinterpret_cast<GameplayClickProc>(0x0048E5D0) ||
            *bw::is_placing_building != 0;
        const bool corrected_placement_globals = corrected && placement_click;
        if (corrected_placement_globals)
        {
            // input_place_building (0x0048E5D0) first reads the corrected
            // event point for IsOutsideGameScreen, but then 0x0048DDC0
            // recalculates placement tile 0x00640890 from the global mouse
            // point. During an invisible-native-HUD bypass those globals still
            // contain the expanded-only decoy (typically x=704). Present the
            // same physical point through both coordinate sources for the
            // duration of this callback, exactly as an unobstructed building
            // click does.
            *bw::mouse_clickpos_x = expected_gameplay_click_x;
            *bw::mouse_clickpos_y = expected_gameplay_click_y;
        }
        FILE *log = fopen("fixed_zoom_input.log", "a");
        if (log) {
            fprintf(log,
                "%lu left-command event=(%u,%u) expected=(%d,%d) "
                "corrected=%u placement=%u engine=(%d,%d)->(%d,%d) "
                "proc=%p\n",
                static_cast<unsigned long>(GetTickCount()),
                original_x, original_y,
                expected_gameplay_click_x, expected_gameplay_click_y,
                static_cast<unsigned>(corrected),
                static_cast<unsigned>(placement_click),
                engine_x_before, engine_y_before,
                static_cast<int>(*bw::mouse_clickpos_x),
                static_cast<int>(*bw::mouse_clickpos_y),
                reinterpret_cast<void *>(traced_left_click_proc));
            fclose(log);
        }
        if (traced_left_click_proc)
            traced_left_click_proc(event);
        if (corrected_placement_globals)
        {
            *bw::mouse_clickpos_x = engine_x_before;
            *bw::mouse_clickpos_y = engine_y_before;
        }
    }

    void __fastcall IgnoreExpandedHudGameplayClick(void *event) {
        const uint8_t *bytes = static_cast<const uint8_t *>(event);
        const unsigned event_x = event ?
            *reinterpret_cast<const uint16_t *>(bytes + 0x0e) : 0;
        const unsigned event_y = event ?
            *reinterpret_cast<const uint16_t *>(bytes + 0x10) : 0;
        FILE *log = fopen("fixed_zoom_input.log", "a");
        if (log) {
            fprintf(log,
                "%lu ignored presented-HUD gameplay fallback "
                "control-index=%u event=(%u,%u)\n",
                static_cast<unsigned long>(GetTickCount()),
                static_cast<unsigned>(latest_status_tooltip_control_index),
                event_x, event_y);
            fclose(log);
        }
    }

    void __fastcall TraceExpandedRightClick(void *event) {
        unsigned original_x = 0;
        unsigned original_y = 0;
        const bool corrected = CorrectGameplayClickEvent(
            event, &original_x, &original_y);

        FILE *log = fopen("fixed_zoom_input.log", "a");
        if (log) {
            fprintf(log,
                "%lu right-command event=(%u,%u) expected=(%d,%d) "
                "corrected=%u camera=(%lu,%lu) selected=(%p,%p) "
                "portrait=%p proc=%p\n",
                static_cast<unsigned long>(GetTickCount()),
                original_x, original_y,
                expected_gameplay_click_x, expected_gameplay_click_y,
                static_cast<unsigned>(corrected),
                static_cast<unsigned long>(*bw::screen_x),
                static_cast<unsigned long>(*bw::screen_y),
                static_cast<void *>(bw::client_selection_group[0]),
                static_cast<void *>(bw::client_selection_group2[0]),
                static_cast<void *>(*bw::active_portrait_unit),
                reinterpret_cast<void *>(traced_right_click_proc));
            fclose(log);
        }
        if (traced_right_click_proc)
            traced_right_click_proc(event);
    }

    int PhysicalInputZone(int x, int y) {
        if (x < 0 || y < 0 ||
            x >= static_cast<int>(resolution::screen_width) ||
            y >= static_cast<int>(resolution::screen_height))
            return 0; // Outside the physical client.
        if (ExpandedHudConsumesInput(x, y))
            return 3;
        if (y < static_cast<int>(resolution::screen_height))
            return x < static_cast<int>(resolution::native_width) ? 1 : 2;
        return 4;
    }

    void TraceInputEvent(UINT msg, int raw_x, int raw_y,
                         int forwarded_x, int forwarded_y,
                         int engine_before_x, int engine_before_y,
                         bool translated_hud_event) {
        if (!runtime_diagnostics::Enabled())
            return;
        const int engine_x = static_cast<int>(*bw::mouse_clickpos_x);
        const int engine_y = static_cast<int>(*bw::mouse_clickpos_y);
        const DWORD now = GetTickCount();
        const int zone = PhysicalInputZone(raw_x, raw_y);

        input_trace.max_raw_x = std::max(input_trace.max_raw_x, raw_x);
        input_trace.max_raw_y = std::max(input_trace.max_raw_y, raw_y);
        input_trace.max_forwarded_x = std::max(
            input_trace.max_forwarded_x, forwarded_x);
        input_trace.max_forwarded_y = std::max(
            input_trace.max_forwarded_y, forwarded_y);
        input_trace.max_engine_x = std::max(input_trace.max_engine_x, engine_x);
        input_trace.max_engine_y = std::max(input_trace.max_engine_y, engine_y);

        if (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK)
            input_trace.left_down = true;
        else if (msg == WM_LBUTTONUP)
            input_trace.left_down = false;

        const bool button_event = msg != WM_MOUSEMOVE;
        const bool zone_changed = zone != input_trace.last_zone;
        const DWORD interval = input_trace.left_down ? 250 : 1500;
        const bool periodic = now - input_trace.last_sample_tick >= interval;
        if (!button_event && !zone_changed && !periodic)
            return;

        FILE *log = fopen("fixed_zoom_input.log", "a");
        if (!log)
            return;
        if (!input_trace.initialized) {
            fprintf(log,
                "\n--- input trace session pid=%lu output=%ux%u "
                "battlefield=%ux%u native=%ux%u ---\n",
                static_cast<unsigned long>(GetCurrentProcessId()),
                static_cast<unsigned>(resolution::screen_width),
                static_cast<unsigned>(resolution::screen_height),
                static_cast<unsigned>(resolution::game_width),
                static_cast<unsigned>(resolution::game_height),
                static_cast<unsigned>(resolution::native_width),
                static_cast<unsigned>(resolution::native_game_height));
            input_trace.initialized = true;
        }
        const DrawLayer &selection = bw::draw_layers[1];
        fprintf(log,
            "%lu %-12s raw=(%d,%d) forwarded=(%d,%d) "
            "engine=(%d,%d)->(%d,%d) zone=%d expanded_inside=%u "
            "legacy_inside=%u popup=%lu left_drag=%u "
            "hud_translate=%u hud_capture=%u "
            "selection=(draw=%u area=%d,%d,%d,%d) "
            "max=(raw:%d,%d fwd:%d,%d engine:%d,%d)\n",
            static_cast<unsigned long>(now), MouseMessageName(msg),
            raw_x, raw_y, forwarded_x, forwarded_y,
            engine_before_x, engine_before_y, engine_x, engine_y, zone,
            static_cast<unsigned>(raw_x >= 0 && raw_y >= 0 &&
                raw_x < static_cast<int>(resolution::game_width) &&
                raw_y < static_cast<int>(resolution::game_height)),
            static_cast<unsigned>(forwarded_x >= 0 && forwarded_y >= 0 &&
                forwarded_x < static_cast<int>(resolution::native_width) &&
                forwarded_y < static_cast<int>(
                    resolution::native_game_height)),
            static_cast<unsigned long>(*bw::popup_dialog_active),
            static_cast<unsigned>(input_trace.left_down),
            static_cast<unsigned>(translated_hud_event),
            static_cast<unsigned>(translated_hud_drag_active),
            static_cast<unsigned>(selection.draw),
            static_cast<int>(selection.area.left),
            static_cast<int>(selection.area.top),
            static_cast<int>(selection.area.right),
            static_cast<int>(selection.area.bottom),
            input_trace.max_raw_x, input_trace.max_raw_y,
            input_trace.max_forwarded_x, input_trace.max_forwarded_y,
            input_trace.max_engine_x, input_trace.max_engine_y);
        fclose(log);
        input_trace.last_zone = zone;
        input_trace.last_sample_tick = now;
    }
}

bool ShouldSuppressLegacyHudTooltip()
{
    const bool suppress = is_in_game() && latest_physical_mouse_valid &&
        *bw::popup_dialog_active == 0 && suppress_legacy_hud_tooltip;
    static bool initialized;
    static bool previous;
    if (!initialized || suppress != previous)
    {
        FILE *log = fopen("fixed_zoom_tooltip.log", "a");
        if (log)
        {
            fprintf(log,
                "%lu legacy-HUD tooltip suppression=%u physical=(%d,%d) "
                "popup=%lu\n",
                static_cast<unsigned long>(GetTickCount()),
                static_cast<unsigned>(suppress),
                latest_physical_mouse_x, latest_physical_mouse_y,
                static_cast<unsigned long>(*bw::popup_dialog_active));
            fclose(log);
        }
        initialized = true;
        previous = suppress;
    }
    return suppress;
}

bool ShouldSuppressLegacyGameMenuTooltip(void *dialog)
{
    if (!is_in_game() || !latest_physical_mouse_valid ||
        *bw::popup_dialog_active != 0 || !dialog)
        return false;

    const uint8_t *control = static_cast<const uint8_t *>(dialog);
    const uint8_t *parent = *reinterpret_cast<uint8_t *const *>(
        control + 0x32);
    if (!parent)
        return false;

    // draw_game_menu_context uses these same dialog and parent bounds. Convert
    // the live native control rectangle to its bottom-centered presentation
    // position, then allow context help only at that visible rectangle. The
    // dedicated caller has already identified this control as Game Menu, so a
    // physical point anywhere else belongs to its invisible native duplicate.
    const int control_x = static_cast<int>(
        *reinterpret_cast<const int16_t *>(control + 0x04));
    const int control_y = static_cast<int>(
        *reinterpret_cast<const int16_t *>(control + 0x06));
    const int control_width = static_cast<int>(
        *reinterpret_cast<const int16_t *>(control + 0x08));
    const int control_height = static_cast<int>(
        *reinterpret_cast<const int16_t *>(control + 0x0a));
    const int native_left = control_x +
        static_cast<int>(*reinterpret_cast<const int16_t *>(parent + 0x04));
    const int native_top = control_y +
        static_cast<int>(*reinterpret_cast<const int16_t *>(parent + 0x06));
    const int native_right = native_left + std::max(0, control_width - 1);
    const int native_bottom = native_top + std::max(0, control_height - 1);
    const int presented_left = native_left +
        static_cast<int>(resolution::hud_left);
    const int presented_top = native_top +
        static_cast<int>(resolution::hud_top - resolution::native_hud_top);
    const int presented_right = native_right +
        static_cast<int>(resolution::hud_left);
    const int presented_bottom = native_bottom +
        static_cast<int>(resolution::hud_top - resolution::native_hud_top);
    const bool inside_presented =
        latest_physical_mouse_x >= presented_left &&
        latest_physical_mouse_x <= presented_right &&
        latest_physical_mouse_y >= presented_top &&
        latest_physical_mouse_y <= presented_bottom;
    const bool suppress = !inside_presented;
    void **hover_owner = reinterpret_cast<void **>(
        const_cast<uint8_t *>(parent) + 0x3e);
    const void *hover_before = *hover_owner;
    if (suppress && hover_before == dialog)
        *hover_owner = nullptr;
    if (suppress)
    {
        uint32_t *flags = reinterpret_cast<uint32_t *>(
            const_cast<uint8_t *>(control) + 0x18);
        // DialogFlags::MouseHovering is the actual visual highlight owner.
        // Mark the control dirty while clearing only that stale native state.
        *flags = (*flags & ~0x00000080u) | 0x00000001u;
    }

    return suppress;
}

void SynchronizeGameMenuHoverState(void *dialog)
{
    if (!dialog || !is_in_game() || !latest_physical_mouse_valid ||
        *bw::popup_dialog_active != 0)
        return;

    uint32_t *flags = reinterpret_cast<uint32_t *>(
        static_cast<uint8_t *>(dialog) + 0x18);
    const bool should_hover = PresentedGameMenuControlContains(
        latest_physical_mouse_x, latest_physical_mouse_y);
    const bool is_hovering = (*flags & 0x00000080u) != 0;
    if (should_hover == is_hovering)
        return;

    // Synchronize the visual state immediately before Game Menu's dedicated
    // update function draws the button. The generic dialog engine sees the
    // native 4:3 control rectangle, while the compositor presents this control
    // at the bottom-centered HUD rectangle.
    if (should_hover)
        *flags |= 0x00000080u;
    else
        *flags &= ~0x00000080u;
    *flags |= 0x00000001u;
}

void *PrepareGameMenuControlLookup(void *event)
{
    if (!event)
        return nullptr;
    if (!is_in_game() || !latest_physical_mouse_valid ||
        *bw::popup_dialog_active != 0 ||
        !PresentedGameMenuControlContains(latest_physical_mouse_x,
                                          latest_physical_mouse_y))
        return event;

    // Game Menu refreshes its hover owner after window-message dispatch, when
    // its retained DialogEvent still contains expanded physical coordinates.
    // Translate a private copy while preserving ECX and EDI for StarCraft's
    // register-based control lookup.
    static alignas(4) uint8_t translated_event[0x14];
    memcpy(translated_event, event, sizeof(translated_event));
    const int native_x = latest_physical_mouse_x -
        static_cast<int>(resolution::hud_left);
    const int native_y = latest_physical_mouse_y -
        static_cast<int>(resolution::hud_top -
            resolution::native_hud_top);
    *reinterpret_cast<uint16_t *>(translated_event + 0x0e) =
        static_cast<uint16_t>(native_x);
    *reinterpret_cast<uint16_t *>(translated_event + 0x10) =
        static_cast<uint16_t>(native_y);
    return translated_event;
}

void *PrepareExpandedHudControlLookup(void *event)
{
    if (!event || ShouldSuppressLegacyHudTooltip())
        return nullptr;

    uint8_t *event_bytes = static_cast<uint8_t *>(event);
    const int physical_x = static_cast<int>(
        *reinterpret_cast<uint16_t *>(event_bytes + 0x0e));
    const int physical_y = static_cast<int>(
        *reinterpret_cast<uint16_t *>(event_bytes + 0x10));
    const bool translate = is_in_game() &&
        *bw::popup_dialog_active == 0 &&
        ExpandedHudConsumesInput(physical_x, physical_y);
    if (!translate)
        return event;

    // control_at_mouse reads only the first 0x14 bytes of its input event.
    // Give it a translated copy so later tooltip/control polling can search
    // the native dialog tree without mutating the expanded mouse event used
    // by placement, cursor, and gameplay layers.
    static alignas(4) uint8_t translated_event[0x14];
    memcpy(translated_event, event, sizeof(translated_event));
    const int native_x = std::max(0, std::min(
        static_cast<int>(resolution::native_width) - 1,
        physical_x - static_cast<int>(resolution::hud_left)));
    const int native_y = std::max(0, std::min(
        static_cast<int>(resolution::native_height) - 1,
        physical_y - static_cast<int>(resolution::hud_top -
            resolution::native_hud_top)));
    *reinterpret_cast<uint16_t *>(translated_event + 0x0e) =
        static_cast<uint16_t>(native_x);
    *reinterpret_cast<uint16_t *>(translated_event + 0x10) =
        static_cast<uint16_t>(native_y);

    static int last_physical_x = -1;
    static int last_physical_y = -1;
    const bool periodic = std::abs(physical_x - last_physical_x) >= 32 ||
        std::abs(physical_y - last_physical_y) >= 32;
    if (periodic)
    {
        FILE *log = fopen("fixed_zoom_tooltip.log", "a");
        if (log)
        {
            fprintf(log,
                "%lu expanded-HUD control lookup physical=(%d,%d) "
                "native=(%d,%d) event=%p copy=%p\n",
                static_cast<unsigned long>(GetTickCount()),
                physical_x, physical_y, native_x, native_y,
                event, translated_event);
            fclose(log);
        }
        last_physical_x = physical_x;
        last_physical_y = physical_y;
    }
    return translated_event;
}

void *StabilizeExpandedSelectionTooltip(void *control, void *event)
{
    // Cosmonarchy's statdata_mouseover_interact maps every multiselection
    // roster control (indices 33..44) to the same "Selection instructions"
    // context help.  Returning each individual sibling makes StarCraft tear
    // down and recreate that identical tooltip while the pointer traverses
    // the roster.  Keep the first live sibling as the lookup identity until
    // the pointer leaves this semantic control group.  This function is used
    // only by status_update_tooltip's polling lookup; clicks still receive
    // the actual control from the normal dialog event path.
    static void *selection_roster_anchor = nullptr;
    static void *status_control_previous = nullptr;
    static void *status_dialog_anchor = nullptr;
    const uint16_t control_index = control ?
        *reinterpret_cast<const uint16_t *>(
            static_cast<const uint8_t *>(control) + 0x20) : 0;
    latest_status_tooltip_control_index = control_index;

    const bool selection_roster_control = control &&
        control_index >= 33 && control_index <= 44;
    const bool interactive_status_control = control && control_index != 0 &&
        !selection_roster_control &&
        *reinterpret_cast<void *const *>(
            static_cast<const uint8_t *>(control) + 0x26) != nullptr;
    if (interactive_status_control || selection_roster_control)
        status_dialog_anchor = control;
    hold_expanded_context_help = status_dialog_anchor &&
        HitInteractiveStatusEnvelope(status_dialog_anchor, event);
    if (!hold_expanded_context_help)
        status_dialog_anchor = nullptr;
    const uint16_t previous_status_index = status_control_previous ?
        *reinterpret_cast<const uint16_t *>(
            static_cast<const uint8_t *>(status_control_previous) + 0x20) : 0;
    const DialogControlGroupHit previous_status_hit =
        HitDialogControlGroup(status_control_previous, event,
                              previous_status_index,
                              previous_status_index);
    if (status_control_previous && previous_status_hit.inside_anchor)
    {
        // Native hit-testing follows opaque artwork pixels. A transparent
        // pixel inside a visible Cosmonarchy unit/stat icon may therefore
        // return its parent, an overlapping sibling, or null. Tooltip polling
        // is semantic: retain the interactive control until the pointer
        // leaves that control's actual runtime dialog bounds.
        latest_status_tooltip_control_index = previous_status_index;
        return status_control_previous;
    }
    if (status_control_previous && hold_expanded_context_help &&
        !interactive_status_control && !selection_roster_control)
    {
        // Building stat icons behave as one continuously-owned rectangular
        // control: moving across an unpainted pixel does not hand tooltip
        // ownership back to the parent dialog.  Unit stat controls use sparse
        // artwork masks, so native control_at_mouse intermittently returns the
        // parent or null while the pointer is still inside the live status
        // control envelope.  Give them the same ownership rule as the stable
        // building icon.  A real sibling control still replaces the owner
        // immediately, and leaving the derived runtime envelope releases it.
        latest_status_tooltip_control_index = previous_status_index;
        return status_control_previous;
    }
    if (interactive_status_control)
    {
        if (status_control_previous != control)
        {
            FILE *log = fopen("fixed_zoom_tooltip.log", "a");
            if (log)
            {
                fprintf(log,
                    "%lu status tooltip identity=%p index=%u callback=%p "
                    "previous=%p previous-index=%u\n",
                    static_cast<unsigned long>(GetTickCount()), control,
                    static_cast<unsigned>(control_index),
                    *reinterpret_cast<void *const *>(
                        static_cast<const uint8_t *>(control) + 0x26),
                    status_control_previous,
                    static_cast<unsigned>(previous_status_index));
                fclose(log);
            }
        }
        status_control_previous = control;
    }
    else
    {
        status_control_previous = nullptr;
    }

    if (selection_roster_control)
    {
        if (selection_roster_anchor)
        {
            const uint16_t anchor_index =
                *reinterpret_cast<const uint16_t *>(
                    static_cast<const uint8_t *>(selection_roster_anchor) +
                    0x20);
            if (anchor_index < 33 || anchor_index > 44)
                selection_roster_anchor = nullptr;
        }
        if (!selection_roster_anchor)
        {
            selection_roster_anchor = control;
            FILE *log = fopen("fixed_zoom_tooltip.log", "a");
            if (log)
            {
                fprintf(log,
                    "%lu selection-roster tooltip anchor=%p index=%u\n",
                    static_cast<unsigned long>(GetTickCount()),
                    selection_roster_anchor,
                    static_cast<unsigned>(control_index));
                fclose(log);
            }
        }
        return selection_roster_anchor;
    }

    if (selection_roster_anchor &&
        HitDialogControlGroup(selection_roster_anchor, event, 33, 44).
            inside_group)
    {
        // All roster siblings dispatch the same selection-instructions help.
        // Keep it alive across transparent pixels and inter-icon gaps.
        return selection_roster_anchor;
    }

    if (selection_roster_anchor)
    {
        FILE *log = fopen("fixed_zoom_tooltip.log", "a");
        if (log)
        {
            fprintf(log,
                "%lu selection-roster tooltip released next=%p index=%u\n",
                static_cast<unsigned long>(GetTickCount()), control,
                static_cast<unsigned>(control_index));
            fclose(log);
        }
        selection_roster_anchor = nullptr;
    }
    return control;
}

bool ShouldHoldExpandedContextHelp()
{
    return hold_expanded_context_help;
}

uint16_t GetExpandedStatusTooltipControlIndex()
{
    return latest_status_tooltip_control_index;
}

bool ShouldSuppressTranslatedHudCursorWarp()
{
    // GPTP polls minimap dragging between window messages. Keep every
    // verified ClipCursor and SetCursorPos import guarded for the complete
    // captured drag, not only while ConsoleWndProc dispatches one translated
    // message. Otherwise an inter-message poll can restore the obsolete
    // native minimap clip and synchronously pull the pointer up and left.
    return suppress_translated_hud_cursor_warp ||
        translated_minimap_drag_active;
}

bool IsTranslatedMinimapDragActive()
{
    return translated_minimap_drag_active;
}

LRESULT CALLBACK ConsoleWndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    // Front-end menus are rendered natively at 640x480 and aspect-fitted into
    // the output. Invert that presentation transform before StarCraft's
    // dialog tree sees mouse input. Points in the pillar/letterbox area map to
    // a clipped native point below the menu, so invisible controls cannot be
    // activated there.
    const bool frontend_mouse = !is_in_game() && IsTracedMouseMessage(msg);
    if (frontend_mouse) {
        const int physical_x = static_cast<short>(LOWORD(lparam));
        const int physical_y = static_cast<short>(HIWORD(lparam));
        const int menu_left = static_cast<int>(resolution::menu_left);
        const int menu_top = static_cast<int>(resolution::menu_top);
        const int menu_width = static_cast<int>(resolution::menu_width);
        const int menu_height = static_cast<int>(resolution::menu_height);
        const bool inside_menu =
            physical_x >= menu_left && physical_x < menu_left + menu_width &&
            physical_y >= menu_top && physical_y < menu_top + menu_height;
        int native_x = 0;
        int native_y = static_cast<int>(resolution::native_height);
        if (inside_menu) {
            native_x = (physical_x - menu_left) *
                static_cast<int>(resolution::native_width) / menu_width;
            native_y = (physical_y - menu_top) *
                static_cast<int>(resolution::native_height) / menu_height;
        }
        lparam = MAKELPARAM(static_cast<WORD>(native_x),
                           static_cast<WORD>(native_y));

        static bool logged_frontend_transform;
        if (!logged_frontend_transform) {
            FILE *log = fopen("fixed_zoom_input.log", "a");
            if (log) {
                fprintf(log,
                    "front-end menu transform: output=(%d,%d %dx%d) "
                    "native=%ux%u first=(%d,%d)->(%d,%d) inside=%u\n",
                    menu_left, menu_top, menu_width, menu_height,
                    static_cast<unsigned>(resolution::native_width),
                    static_cast<unsigned>(resolution::native_height),
                    physical_x, physical_y, native_x, native_y,
                    static_cast<unsigned>(inside_menu));
                fclose(log);
            }
            logged_frontend_transform = true;
        }
    }
    // Translate input inside the derived bottom-centered native HUD rectangle
    // back to native control coordinates. The expanded battlefield outside it
    // remains one-to-one at every configured output size.
    const bool trace_mouse = is_in_game() && IsTracedMouseMessage(msg);
    const int trace_raw_x = trace_mouse ?
        static_cast<short>(LOWORD(lparam)) : 0;
    const int trace_raw_y = trace_mouse ?
        static_cast<short>(HIWORD(lparam)) : 0;
    const int trace_engine_before_x = trace_mouse ?
        static_cast<int>(*bw::mouse_clickpos_x) : 0;
    const int trace_engine_before_y = trace_mouse ?
        static_cast<int>(*bw::mouse_clickpos_y) : 0;
    if (trace_mouse) {
        latest_physical_mouse_x = trace_raw_x;
        latest_physical_mouse_y = trace_raw_y;
        latest_physical_mouse_valid = true;
        suppress_legacy_hud_tooltip =
            *bw::popup_dialog_active == 0 &&
            trace_raw_x >= 0 && trace_raw_y >= 0 &&
            trace_raw_x < static_cast<int>(resolution::game_width) &&
            trace_raw_y < static_cast<int>(resolution::game_height) &&
            !ExpandedHudConsumesInput(trace_raw_x, trace_raw_y) &&
            HiddenNativeHudConsumesInput(trace_raw_x, trace_raw_y);
    }
    const bool direct_expanded_hud_hit = trace_mouse &&
        *bw::popup_dialog_active == 0 &&
        ExpandedHudConsumesInput(trace_raw_x, trace_raw_y);
    const bool translated_hud_event = trace_mouse &&
        *bw::popup_dialog_active == 0 &&
        !expanded_battlefield_drag_active &&
        (direct_expanded_hud_hit || translated_hud_drag_active);
    const bool translated_popup_event = trace_mouse &&
        *bw::popup_dialog_active != 0;
    const bool translated_ui_event =
        translated_hud_event || translated_popup_event;
    const bool translated_ui_button_event = translated_ui_event &&
        (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP ||
         msg == WM_LBUTTONDBLCLK || msg == WM_RBUTTONDOWN ||
         msg == WM_RBUTTONUP || msg == WM_RBUTTONDBLCLK ||
         msg == WM_MBUTTONDOWN || msg == WM_MBUTTONUP ||
         msg == WM_MBUTTONDBLCLK || msg == WM_XBUTTONDOWN ||
         msg == WM_XBUTTONUP || msg == WM_XBUTTONDBLCLK);
    POINT physical_cursor_before_hud_click = {};
    const bool physical_cursor_before_valid =
        translated_ui_button_event &&
        GetCursorPos(&physical_cursor_before_hud_click) != FALSE;
    const bool gameplay_button_down =
        msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK ||
        msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK ||
        msg == WM_MBUTTONDOWN || msg == WM_MBUTTONDBLCLK;
    const bool hidden_native_ui_hit = is_in_game() &&
        gameplay_button_down && *bw::popup_dialog_active == 0 &&
        trace_raw_x >= 0 && trace_raw_y >= 0 &&
        trace_raw_x < static_cast<int>(resolution::game_width) &&
        trace_raw_y < static_cast<int>(resolution::game_height) &&
        !ExpandedHudConsumesInput(trace_raw_x, trace_raw_y) &&
        HiddenNativeHudConsumesInput(trace_raw_x, trace_raw_y);

    if (is_in_game()) {
        if ((msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) &&
            *bw::popup_dialog_active == 0 &&
            !direct_expanded_hud_hit &&
            trace_raw_x >= 0 &&
            trace_raw_x < static_cast<int>(resolution::game_width) &&
            trace_raw_y >= 0 &&
            trace_raw_y < static_cast<int>(resolution::screen_height)) {
            PrepareExpandedDragClip();
            expanded_battlefield_drag_active = true;
            translated_hud_drag_active = false;
        }
        if ((msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) &&
            direct_expanded_hud_hit) {
            translated_hud_drag_active = true;
            expanded_battlefield_drag_active = false;
        }
        switch (msg) {
            case WM_MOUSEMOVE:
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MBUTTONDBLCLK:
            case WM_XBUTTONDOWN:
            case WM_XBUTTONUP:
            case WM_XBUTTONDBLCLK: {
                int mouse_x = static_cast<short>(LOWORD(lparam));
                int mouse_y = static_cast<short>(HIWORD(lparam));
                int cursor_offset_x = 0;
                int cursor_offset_y = 0;
                if (*bw::popup_dialog_active != 0) {
                    const int physical_x = mouse_x;
                    const int physical_y = mouse_y;
                    mouse_x = std::max(0, std::min(
                        static_cast<int>(resolution::native_width) - 1,
                        mouse_x - static_cast<int>(
                            resolution::native_ui_left)));
                    mouse_y = std::max(0, std::min(
                        static_cast<int>(resolution::native_height) - 1,
                        mouse_y - static_cast<int>(
                            resolution::native_ui_top)));
                    cursor_offset_x = physical_x - mouse_x;
                    cursor_offset_y = physical_y - mouse_y;
                    lparam = MAKELPARAM(static_cast<WORD>(mouse_x),
                                       static_cast<WORD>(mouse_y));
                }
                else if (translated_hud_event) {
                    cursor_offset_x = static_cast<int>(resolution::hud_left);
                    cursor_offset_y = static_cast<int>(resolution::hud_top -
                        resolution::native_hud_top);
                    mouse_x = std::max(0, std::min(
                        static_cast<int>(resolution::native_width) - 1,
                        mouse_x - cursor_offset_x));
                    mouse_y = std::max(0, std::min(
                        static_cast<int>(resolution::native_height) - 1,
                        mouse_y - cursor_offset_y));
                    lparam = MAKELPARAM(static_cast<WORD>(mouse_x),
                                       static_cast<WORD>(mouse_y));
                    if (msg == WM_LBUTTONDOWN ||
                        msg == WM_LBUTTONDBLCLK) {
                        const Rect32 &minimap =
                            *reinterpret_cast<const Rect32 *>(0x00512D00);
                        translated_minimap_drag_active =
                            mouse_x >= minimap.left &&
                            mouse_x < minimap.right &&
                            mouse_y >= minimap.top &&
                            mouse_y < minimap.bottom;
                        if (translated_minimap_drag_active) {
                            FILE *log = fopen("fixed_zoom_input.log", "a");
                            if (log) {
                                fprintf(log,
                                    "%lu translated minimap capture began "
                                    "native=(%d,%d) rect=(%ld,%ld,%ld,%ld)\n",
                                    static_cast<unsigned long>(GetTickCount()),
                                    mouse_x, mouse_y,
                                    static_cast<long>(minimap.left),
                                    static_cast<long>(minimap.top),
                                    static_cast<long>(minimap.right),
                                    static_cast<long>(minimap.bottom));
                                fclose(log);
                            }
                        }
                    }
                }
                else if (hidden_native_ui_hit) {
                    // Route the dialog phase through a known expanded-only
                    // point so the invisible native HUD cannot consume it.
                    // The gameplay callback below restores the real point.
                    mouse_x = std::min(
                        static_cast<int>(resolution::game_width) - 64,
                        static_cast<int>(resolution::native_width) + 64);
                    mouse_y = std::min(mouse_y,
                        static_cast<int>(resolution::game_height) - 64);
                    lparam = MAKELPARAM(static_cast<WORD>(mouse_x),
                                       static_cast<WORD>(mouse_y));
                }
                SetExpandedCursorOffset(cursor_offset_x, cursor_offset_y);
            } break;
        }
    }
    const int trace_forwarded_x = trace_mouse ?
        static_cast<short>(LOWORD(lparam)) : 0;
    const int trace_forwarded_y = trace_mouse ?
        static_cast<short>(HIWORD(lparam)) : 0;
    // Cosmonarchy dispatches gameplay clicks through a second internal dialog
    // event. Correct that final stage after bypassing an invisible old HUD.
    const int expected_click_x = hidden_native_ui_hit ?
        trace_raw_x : trace_forwarded_x;
    const int expected_click_y = hidden_native_ui_hit ?
        trace_raw_y : trace_forwarded_y;
    void *saved_left_click_proc = nullptr;
    void *saved_right_click_proc = nullptr;
    const bool trace_left_command = is_in_game() &&
        (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) &&
        *bw::popup_dialog_active == 0;
    const bool trace_right_command = is_in_game() &&
        (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONDBLCLK) &&
        *bw::popup_dialog_active == 0;
    // A translated HUD message must remain available to StarCraft's native
    // dialog controls (buttons, minimap, roster, and status information), but
    // it must never fall through to the battlefield command callback.  That
    // fallback is what clips/warps the physical cursor when a decorative HUD
    // edge or other non-control pixel is clicked.  Suppressing the callback
    // here does not suppress the dialog procedure itself.
    const bool expanded_hud_left_click =
        trace_left_command && direct_expanded_hud_hit;
    const bool expanded_hud_right_click =
        trace_right_command && direct_expanded_hud_hit;
    expected_gameplay_click_valid =
        (trace_left_command && !expanded_hud_left_click) ||
        (trace_right_command && !expanded_hud_right_click);
    expected_gameplay_click_x = expected_click_x;
    expected_gameplay_click_y = expected_click_y;
    if (trace_left_command) {
        saved_left_click_proc = *bw::left_click_proc;
        traced_left_click_proc = reinterpret_cast<GameplayClickProc>(
            saved_left_click_proc);
        if (expanded_hud_left_click && saved_left_click_proc) {
            *bw::left_click_proc = reinterpret_cast<void *>(
                &IgnoreExpandedHudGameplayClick);
        }
        else if (saved_left_click_proc &&
            saved_left_click_proc != reinterpret_cast<void *>(
                &TraceExpandedLeftClick)) {
            *bw::left_click_proc = reinterpret_cast<void *>(
                &TraceExpandedLeftClick);
        }
    }
    if (trace_right_command) {
        saved_right_click_proc = *bw::right_click_proc;
        traced_right_click_proc = reinterpret_cast<GameplayClickProc>(
            saved_right_click_proc);
        if (expanded_hud_right_click && saved_right_click_proc) {
            *bw::right_click_proc = reinterpret_cast<void *>(
                &IgnoreExpandedHudGameplayClick);
        }
        else if (saved_right_click_proc &&
            saved_right_click_proc != reinterpret_cast<void *>(
                &TraceExpandedRightClick)) {
            *bw::right_click_proc = reinterpret_cast<void *>(
                &TraceExpandedRightClick);
        }
    }
    if (console) {
        switch (msg) {
            case WM_KEYDOWN: {
                if (((ScConsole*)console)->Sc_KeyDown(wparam, (lparam >> 16) & 0xff)) {
                    return 0;
                }
            } break;
            case WM_KEYUP: {
                if (((ScConsole*)console)->Sc_KeyUp(wparam, (lparam >> 16) & 0xff)) {
                    return 0;
                }
            } break;
        }
    }
    // Relocated native HUD controls sometimes call SetCursorPos with the
    // translated 640x480 point. Restoring the cursor afterward is too late:
    // Windows has already queued a synthetic WM_MOUSEMOVE at that old point,
    // which breaks continuous minimap camera dragging. Scope the IAT guard to
    // the native dispatch so unrelated engine cursor warps remain untouched.
    const uint32_t camera_before_x = *bw::screen_x;
    const uint32_t camera_before_y = *bw::screen_y;
    if (translated_ui_event) {
        // Cosmonarchy's minimap_game_mouse_update reads g_mouse directly
        // rather than the DialogEvent point. Keep that shared store in the
        // same native coordinate space as lparam for the duration of native
        // UI dispatch. Popup dialogs use the same native coordinate system.
        // The store is restored below before expanded rendering consumes it.
        *bw::mouse_clickpos_x = trace_forwarded_x;
        *bw::mouse_clickpos_y = trace_forwarded_y;
    }
    const bool saved_cursor_warp_suppression =
        suppress_translated_hud_cursor_warp;
    suppress_translated_hud_cursor_warp = translated_ui_event;
    const LRESULT result = CallWindowProcA(OldWndProc, hwnd, msg, wparam, lparam);
    suppress_translated_hud_cursor_warp = saved_cursor_warp_suppression;
    uint32_t camera_after_x = *bw::screen_x;
    uint32_t camera_after_y = *bw::screen_y;
    // Cosmonarchy's status portrait callback is outside the native portrait
    // target writers and still centers its live sprite in a 640x400 viewport.
    // Recognize that exact, grid-aligned result and repeat the same operation
    // with the active renderer profile's battlefield center.
    if (translated_hud_event && msg == WM_LBUTTONUP &&
        (camera_after_x != camera_before_x ||
         camera_after_y != camera_before_y)) {
        Unit *portrait_unit = *bw::active_portrait_unit;
        Sprite *portrait_sprite = portrait_unit ?
            portrait_unit->sprite.get() : nullptr;
        const auto normalized_camera_origin = [](int origin, int maximum) {
            origin = std::max(0, std::min(origin, maximum));
            return static_cast<uint32_t>(origin & ~7);
        };
        if (portrait_sprite) {
            const int world_x = static_cast<int>(portrait_sprite->position.x);
            const int world_y = static_cast<int>(portrait_sprite->position.y);
            const uint32_t legacy_x = normalized_camera_origin(
                world_x - 320, static_cast<int>(*bw::screen_x_max));
            const uint32_t legacy_y = normalized_camera_origin(
                world_y - 200, static_cast<int>(*bw::screen_y_max));
            if (camera_after_x == legacy_x && camera_after_y == legacy_y) {
                const int expanded_x = world_x -
                static_cast<int>(resolution::camera_center_x);
                const int expanded_y = world_y -
                    static_cast<int>(resolution::camera_center_y);
                bw::MoveScreen(expanded_x, expanded_y);
                camera_after_x = *bw::screen_x;
                camera_after_y = *bw::screen_y;
            }
        }
    }
    if (translated_hud_event &&
        (camera_after_x != camera_before_x ||
         camera_after_y != camera_before_y)) {
        FILE *log = fopen("fixed_zoom_input.log", "a");
        if (log) {
            fprintf(log,
                "%lu translated-HUD camera raw=(%d,%d) native=(%d,%d) "
                "camera=(%lu,%lu)->(%lu,%lu) msg=%04X\n",
                static_cast<unsigned long>(GetTickCount()),
                trace_raw_x, trace_raw_y,
                trace_forwarded_x, trace_forwarded_y,
                static_cast<unsigned long>(camera_before_x),
                static_cast<unsigned long>(camera_before_y),
                static_cast<unsigned long>(camera_after_x),
                static_cast<unsigned long>(camera_after_y),
                static_cast<unsigned>(msg));
            fclose(log);
        }
    }
    if (trace_left_command) {
        if (*bw::left_click_proc == reinterpret_cast<void *>(
                &TraceExpandedLeftClick) ||
            *bw::left_click_proc == reinterpret_cast<void *>(
                &IgnoreExpandedHudGameplayClick)) {
            *bw::left_click_proc = saved_left_click_proc;
        }
    }
    if (trace_right_command) {
        if (*bw::right_click_proc == reinterpret_cast<void *>(
                &TraceExpandedRightClick) ||
            *bw::right_click_proc == reinterpret_cast<void *>(
                &IgnoreExpandedHudGameplayClick)) {
            *bw::right_click_proc = saved_right_click_proc;
        }
    }
    expected_gameplay_click_valid = false;
    const bool hold_native_minimap_mouse =
        translated_minimap_drag_active && msg != WM_LBUTTONUP;
    if (hidden_native_ui_hit || translated_ui_event) {
        if (hold_native_minimap_mouse) {
            // minimap_game_mouse_update is polled between window messages
            // while capture is held. Preserve its native point for that full
            // lifetime; the draw-time cursor offset below keeps the visible
            // cursor at the unchanged physical expanded point.
            *bw::mouse_clickpos_x = trace_forwarded_x;
            *bw::mouse_clickpos_y = trace_forwarded_y;
        }
        else {
            // Other native controls need translated coordinates only while
            // their window procedure handles this message. Restore the
            // expanded coordinate for building ghosts and custom layers.
            *bw::mouse_clickpos_x = trace_raw_x;
            *bw::mouse_clickpos_y = trace_raw_y;
        }
    }
    if (translated_ui_event || hidden_native_ui_hit) {
        if (hold_native_minimap_mouse) {
            SetExpandedCursorOffset(
                static_cast<int>(resolution::hud_left),
                static_cast<int>(resolution::hud_top -
                    resolution::native_hud_top));
        }
        else {
            // Native dispatch has already rebuilt cursor layer 0 using the
            // forwarded 640x480 coordinate. Writing the expanded g_mouse
            // values back does not rebuild that layer by itself. Run the
            // engine's own cursor refresh after the restore so the very next
            // composed frame uses the physical position instead of flashing
            // the stale native cursor for one frame.
            bw::RefreshCursorLayer();
            SetExpandedCursorOffset(0, 0);
        }
    }
    if (physical_cursor_before_valid) {
        POINT physical_cursor_after_hud_click = {};
        if (GetCursorPos(&physical_cursor_after_hud_click) &&
            (physical_cursor_after_hud_click.x !=
                 physical_cursor_before_hud_click.x ||
             physical_cursor_after_hud_click.y !=
                 physical_cursor_before_hud_click.y)) {
            // Native dialog controls receive a translated client point, but
            // that translation must never become a physical cursor warp.
            // Some status-panel controls pass their native point through the
            // cursor/capture manager while processing a click. Restore the
            // exact screen-space point observed at the window boundary.
            SetCursorPos(physical_cursor_before_hud_click.x,
                         physical_cursor_before_hud_click.y);
            FILE *log = fopen("fixed_zoom_input.log", "a");
            if (log) {
                fprintf(log,
                    "%lu restored translated-HUD cursor screen=(%ld,%ld) "
                    "from=(%ld,%ld) msg=%04X raw=(%d,%d)\n",
                    static_cast<unsigned long>(GetTickCount()),
                    physical_cursor_before_hud_click.x,
                    physical_cursor_before_hud_click.y,
                    physical_cursor_after_hud_click.x,
                    physical_cursor_after_hud_click.y,
                    static_cast<unsigned>(msg), trace_raw_x, trace_raw_y);
                fclose(log);
            }
        }
    }
    if (trace_mouse && msg == WM_LBUTTONUP) {
        translated_hud_drag_active = false;
        expanded_battlefield_drag_active = false;
        translated_minimap_drag_active = false;
    }
    if (msg == WM_CANCELMODE || msg == WM_CAPTURECHANGED ||
        msg == WM_KILLFOCUS) {
        translated_hud_drag_active = false;
        expanded_battlefield_drag_active = false;
        translated_minimap_drag_active = false;
        if (latest_physical_mouse_valid) {
            *bw::mouse_clickpos_x = latest_physical_mouse_x;
            *bw::mouse_clickpos_y = latest_physical_mouse_y;
        }
        SetExpandedCursorOffset(0, 0);
    }
    if (trace_mouse) {
        TraceInputEvent(msg, trace_raw_x, trace_raw_y,
            trace_forwarded_x, trace_forwarded_y,
            trace_engine_before_x, trace_engine_before_y,
            translated_hud_event);
    }
    // Window-state messages cover client-size correction. Avoid querying the
    // unchanged client rectangle for every mouse packet.
    switch (msg) {
        case WM_SIZE:
        case WM_WINDOWPOSCHANGED:
        case WM_ACTIVATE:
        case WM_PAINT:
            presentation::EnsureClient(hwnd);
            break;
    }
    return result;
}

void ScConsole::HookWndProc(void *hwnd) {
    console_hwnd = (HWND)hwnd;
    OldWndProc = (WndProc *)SetWindowLongPtr((HWND)hwnd, GWLP_WNDPROC, (LONG)&ConsoleWndProc);
}

bool ScConsole::Sc_KeyDown(int key, int scan) {
    if (state != shown) {
        switch (key) {
            case VK_OEM_PLUS:
            case VK_ADD: {
                frame_skip_ms = 120;
                bw::game_speed_waits[*bw::game_speed] = 0;
            } break;
        }
    }
    switch (key) {
        case VK_ESCAPE: {
            if (isFastForwarding) {
                EndFastForward();
            }
        } break;
    }
    return false;
}

bool ScConsole::Sc_KeyUp(int key, int scan) {
    if (state != shown) {
        switch (key) {
            case VK_OEM_PLUS:
            case VK_ADD: {
                frame_skip_ms = 0;
                bw::game_speed_waits[*bw::game_speed] = 42;
            } break;
        }
    }
    return false;
}
