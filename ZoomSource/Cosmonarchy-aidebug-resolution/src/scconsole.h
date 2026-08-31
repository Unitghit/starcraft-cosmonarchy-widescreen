#ifndef SC_CONSOLE_H
#define SC_CONSOLE_H

#include "console/genericconsole.h"
#include "types.h"
#include "offsets.h"
#include "limits.h"

struct Grid {
    Grid() {}
    Grid(int w, int h, uint8_t c) : width(w), height(h), color(c) {}
    int width;
    int height;
    uint8_t color;
};

struct TextLayout;

class ScConsole : public Common::GenericConsole {
    public:
        ScConsole();
        ~ScConsole();

        virtual void Hide();
        bool show_fps;
        bool show_frame;

        void DrawDebugInfo(uint8_t *framebuf, xuint w, yuint h);

        static void HookWndProc(void *hwnd);

        bool Sc_KeyDown(int key, int scan);
        bool Sc_KeyUp(int key, int scan);

    private:
        int get_console_player();
        int get_human_players();
        bool can_use_cheat_command();
        bool can_use_cheat_command_in_replay();

        //bool Heal(const CmdArgs &args);
        //bool Kill(const CmdArgs &args);
        //bool Give(const CmdArgs &args);
        bool Gsw(const CmdArgs &args);
        //bool Tcr(const CmdArgs &args);
        bool SupplyMax(const CmdArgs &args);
        //bool AiScript(const CmdArgs &args);
        bool AiRegion(const CmdArgs &args);
        bool Player(const CmdArgs &args);
        bool Race(const CmdArgs &args);
        bool UnitCmd(const CmdArgs &args);
        bool Money(const CmdArgs &args);
        bool Supply(const CmdArgs &args);
        bool Self(const CmdArgs &args);
        bool Pause(const CmdArgs &args);
        bool Vision(const CmdArgs &args);
        bool Alliance (const CmdArgs& args);
        bool Cmd_Grid(const CmdArgs &args);

        bool Frame(const CmdArgs &args);
        bool ShowAi(const CmdArgs& args);
        bool ShowAiUnits(const CmdArgs& args);
        bool ShowAiGuards(const CmdArgs& args);
        bool ShowAiPlayer(const CmdArgs& args);
        bool Show(const CmdArgs &args);
        //bool Test(const CmdArgs &args);
        bool Spawn(const CmdArgs &args);
		bool UnitCount(const CmdArgs &args);
        //bool AiscriptExec(const CmdArgs &args);
        bool FastForward(const CmdArgs &args);
        void UpdateFastForwardProgress();
        void EndFastForward();

        //bool Death(const CmdArgs &args, bool print, bool clear);

        vector<UnitType> ParseUnitId(const char *unit_str, int max_amt);

        void GetTownRequests(uint32_t *out, int len, uint32_t *in);
        void DrawAiRegions(int player, Common::Surface *text_surf, const Point32 &pos);
		
        void DrawLocations(uint8_t *framebuf, xuint w, yuint h);
        void DrawCrects(uint8_t *framebuf, xuint w, yuint h);
        void DrawAiInfo(uint8_t *textbuf, uint8_t *framebuf, xuint w, yuint h);
		void DrawDeaths(uint8_t *textbuf, uint8_t *framebuf, xuint w, yuint h);

        void DrawAiUnitHomes(uint8_t *framebuf, xuint w, yuint h);
        void DrawAiGuards(uint8_t *textbuf, uint8_t *framebuf, xuint w, yuint h);
        void DrawGuardAi(
            Common::Surface *surface,
            TextLayout *text_surface,
            Ai::GuardAi *ai,
            int player,
            bool alive
        );
        void DrawOrders(uint8_t *framebuf, xuint w, yuint h);
        void DrawCoords(uint8_t *framebuf, xuint w, yuint h);
        //void DrawDeaths(uint8_t *framebuf, xuint w, yuint h);
        void DrawRange(uint8_t *framebuf, xuint w, yuint h);
        void DrawGrids(uint8_t *framebuf, xuint w, yuint h);
        //void DrawBullets(uint8_t *framebuf, xuint w, yuint h);
        void DrawResourceAreas(uint8_t *textbuf, uint8_t *framebuf, xuint w, yuint h);

        Unit *GetUnit();
        array_offset <Unit*, SelectionLimit> GetSelectedUnits();

        void ConstructInfoLines();

        bool draw_locations;
        bool draw_crects;
        bool draw_ai_towns;
        bool draw_ai_regions;
        enum class OrderDrawMode {
            None,
            All,
            Selected,
        } draw_orders;


		//IQUARE
		bool draw_deaths;


        bool draw_ai_data;
        bool draw_ai_full;
        bool draw_ai_named;
        bool draw_ai_unit_homes;
        bool draw_ai_guards;
		//IQUARE
		int show_deaths[Limits::Players];
        int show_ai[Limits::Players]; // 0 no, 1 yes, 2 extra
        bool draw_coords;
        bool draw_range;
        bool draw_info;
        bool draw_bullets;
        bool draw_resource_areas;

        // player_mask, unit_id
        vector<tuple<uint16_t, UnitType>> death_counters;
        vector<std::string> info_lines;
        vector<Grid> grids;
};

void PatchConsole();
bool ShouldSuppressLegacyHudTooltip();
void *PrepareExpandedHudControlLookup(void *event);
void *StabilizeExpandedSelectionTooltip(void *control, void *event);
bool ShouldHoldExpandedContextHelp();
uint16_t GetExpandedStatusTooltipControlIndex();
bool ShouldSuppressTranslatedHudCursorWarp();

#endif // SC_CONSOLE_H

