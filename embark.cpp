#include "ai.h"
#include "camera.h"
#include "embark.h"
#include "event_manager.h"

#include "modules/Gui.h"
#include "modules/Screen.h"
#include "modules/Translation.h"

#include "df/region_map_entry.h"
#include "df/viewscreen_choose_start_sitest.h"
#include "df/viewscreen_dwarfmodest.h"
#include "df/viewscreen_loadgamest.h"
#include "df/viewscreen_new_regionst.h"
#include "df/viewscreen_setupdwarfgamest.h"
#include "df/viewscreen_textviewerst.h"
#include "df/viewscreen_titlest.h"
#include "df/viewscreen_update_regionst.h"
#include "df/world.h"
#include "df/world_data.h"

REQUIRE_GLOBAL(cur_year);
REQUIRE_GLOBAL(standing_orders_gather_refuse_outside);
REQUIRE_GLOBAL(standing_orders_job_cancel_announce);
REQUIRE_GLOBAL(world);

Embark::Embark(AI *ai) :
    ai(ai),
    selected_embark(false),
    embarking(false)
{
    if (config.random_embark)
    {
        embarking = true;
        events.onupdate_register_once("df-ai random_embark", [this](color_ostream & out) -> bool
        {
            return update(out);
        });
    }
}

Embark::~Embark()
{
}

command_result Embark::startup(color_ostream &)
{
    // do nothing
    return CR_OK;
}

command_result Embark::onupdate_register(color_ostream &)
{
    // do nothing
    return CR_OK;
}

command_result Embark::onupdate_unregister(color_ostream &)
{
    return CR_OK;
}

void Embark::register_restart_timer(color_ostream & out)
{
    if (config.random_embark)
    {
        ai->debug(out, "game over. restarting in 1 minute.");
        auto restart_wait = [this](color_ostream &) -> bool
        {
            if (!strict_virtual_cast<df::viewscreen_titlest>(Gui::getCurViewscreen(true)))
            {
                return false;
            }

            if (!config.no_quit)
            {
                Gui::getCurViewscreen(true)->breakdown_level = interface_breakdown_types::QUIT;
                return true;
            }

            extern bool full_reset_requested;
            full_reset_requested = true;
            return true;
        };
        ai->timeout_sameview(60, [this, restart_wait](color_ostream & out)
        {
            ai->debug(out, "restarting.");
            AI::feed_key(interface_key::LEAVESCREEN);

            events.onupdate_register_once("df-ai restart wait", restart_wait);
        });
    }
}

bool Embark::update(color_ostream & out)
{
    df::viewscreen *curview = Gui::getCurViewscreen(false);
    if (!curview || curview->breakdown_level != interface_breakdown_types::NONE)
        return false;

    if (df::viewscreen_titlest *view = strict_virtual_cast<df::viewscreen_titlest>(curview))
    {
        ai->camera->check_record_status();

        switch (view->sel_subpage)
        {
        case df::viewscreen_titlest::None:
        {
            auto continue_game = std::find(view->menu_line_id.begin(), view->menu_line_id.end(), df::viewscreen_titlest::Continue);
            auto start_game = std::find(view->menu_line_id.begin(), view->menu_line_id.end(), df::viewscreen_titlest::Start);
            if (!config.random_embark_world.empty() && continue_game != view->menu_line_id.end() && std::ifstream("data/save/" + config.random_embark_world + "/world.sav").good())
            {
                ai->debug(out, "choosing \"Continue Game\"");
                view->sel_menu_line = continue_game - view->menu_line_id.begin();
            }
            else if (!config.random_embark_world.empty() && start_game != view->menu_line_id.end())
            {
                ai->debug(out, "choosing \"Start Game\"");
                view->sel_menu_line = start_game - view->menu_line_id.begin();
            }
            else
            {
                ai->debug(out, "choosing \"New World\"");
                view->sel_menu_line = std::find(view->menu_line_id.begin(), view->menu_line_id.end(), df::viewscreen_titlest::NewWorld) - view->menu_line_id.begin();
            }
            AI::feed_key(view, interface_key::SELECT);
            break;
        }
        case df::viewscreen_titlest::StartSelectWorld:
        {
            if (config.random_embark_world.empty())
            {
                ai->debug(out, "leaving \"Select World\" (no save name)");
                AI::feed_key(view, interface_key::LEAVESCREEN);
                return false;
            }
            auto save = std::find_if(view->start_savegames.begin(), view->start_savegames.end(), [](df::viewscreen_titlest::T_start_savegames *s) -> bool
            {
                return s->save_dir == config.random_embark_world;
            });
            if (save != view->start_savegames.end())
            {
                ai->debug(out, stl_sprintf("selecting save #%d (%s)",
                    save - view->start_savegames.begin(),
                    (*save)->world_name_str.c_str()));
                view->sel_submenu_line = save - view->start_savegames.begin();
                AI::feed_key(view, interface_key::SELECT);
            }
            else
            {
                ai->debug(out, "could not find save named " + config.random_embark_world);
                config.set(out, config.random_embark_world, std::string());
                AI::feed_key(view, interface_key::LEAVESCREEN);
            }
            break;
        }
        case df::viewscreen_titlest::StartSelectMode:
        {
            auto fortress_mode = std::find(view->submenu_line_id.begin(), view->submenu_line_id.end(), 0);
            if (fortress_mode != view->submenu_line_id.end())
            {
                ai->debug(out, "choosing \"Dwarf Fortress Mode\"");
                view->sel_menu_line = fortress_mode - view->submenu_line_id.begin();
                AI::feed_key(view, interface_key::SELECT);
            }
            else
            {
                ai->debug(out, "leaving \"Select Mode\" (no fortress mode available)");
                config.set(out, config.random_embark_world, std::string());
                AI::feed_key(view, interface_key::LEAVESCREEN);
            }
            break;
        }
        default:
            break;
        }
    }
    else if (df::viewscreen_loadgamest *view = strict_virtual_cast<df::viewscreen_loadgamest>(curview))
    {
        if (view->loading)
        {
            return false;
        }
        if (config.random_embark_world.empty())
        {
            ai->debug(out, "leaving \"Select World\" (no save name)");
            AI::feed_key(view, interface_key::LEAVESCREEN);
            return false;
        }
        auto save = std::find_if(view->saves.begin(), view->saves.end(), [](df::loadgame_save_info *s) -> bool
        {
            return s->folder_name == config.random_embark_world;
        });
        if (save != view->saves.end())
        {
            ai->debug(out, stl_sprintf("selecting save #%d (%s) (%s)",
                save - view->saves.begin(),
                (*save)->world_name.c_str(),
                (*save)->fort_name.c_str()));
            while (view->sel_idx != save - view->saves.begin())
            {
                AI::feed_key(view, interface_key::STANDARDSCROLL_DOWN);
            }
            AI::feed_key(view, interface_key::SELECT);
        }
        else
        {
            ai->debug(out, "could not find save named " + config.random_embark_world);
            config.set(out, config.random_embark_world, std::string());
            AI::feed_key(view, interface_key::LEAVESCREEN);
        }
    }
    else if (df::viewscreen_new_regionst *view = strict_virtual_cast<df::viewscreen_new_regionst>(curview))
    {
        config.set(out, config.random_embark_world, std::string());

        if (!view->welcome_msg.empty())
        {
            ai->debug(out, "leaving world gen disclaimer");
            AI::feed_key(view, interface_key::LEAVESCREEN);
        }
        else if (view->simple_mode == 1)
        {
            ai->debug(out, "choosing \"Generate World\"");
            view->world_size = config.world_size;
            view->mineral_occurence = 3;
            AI::feed_key(view, interface_key::MENU_CONFIRM);
        }
        else if (world->worldgen_status.state == 10 && view->simple_mode == 0)
        {
            ai->debug(out, "world gen finished, save name is " + world->cur_savegame.save_dir);
            config.set(out, config.random_embark_world, world->cur_savegame.save_dir);
            AI::feed_key(view, interface_key::SELECT);
        }
    }
    else if (df::viewscreen_update_regionst *view = strict_virtual_cast<df::viewscreen_update_regionst>(curview))
    {
        ai->debug(out, "updating world, goal: " + AI::timestamp(view->year, view->year_tick));
    }
    else if (df::viewscreen_choose_start_sitest *view = strict_virtual_cast<df::viewscreen_choose_start_sitest>(curview))
    {
        if (view->finder.finder_state == -1)
        {
            ai->debug(out, "choosing \"Site Finder\"");
            AI::feed_key(view, interface_key::SETUP_FIND);
            FOR_ENUM_ITEMS(embark_finder_option, o)
            {
                view->finder.options[o] = config.embark_options[o];
            }
            AI::feed_key(view, interface_key::SELECT);
        }
        else if (view->finder.search_x != -1 || view->finder.search_y != 0)
        {
            ai->debug(out, stl_sprintf("searching for a site (%d/%d, %d/%d)",
                view->finder.search_x,
                world->world_data->world_width / 16,
                view->finder.search_y,
                world->world_data->world_height / 16));
        }
        else if (selected_embark)
        {
            return false;
        }
        else
        {
            ai->debug(out, "choosing \"Embark\"");
            AI::feed_key(view, interface_key::LEAVESCREEN);

            df::coord2d start = view->location.region_pos;
            std::vector<df::coord2d> sites;
            for (int16_t x = 0; x < world->world_data->world_width; x++)
            {
                for (int16_t y = 0; y < world->world_data->world_height; y++)
                {
                    if (world->world_data->region_map[x][y].finder_rank >= 10000)
                    {
                        sites.push_back(df::coord2d(x, y));
                    }
                }
            }
            if (sites.empty())
            {
                ai->debug(out, "leaving embark selector (no good embarks)");
                config.set(out, config.random_embark_world, std::string());
                AI::abandon(out);
                return false;
            }
            ai->debug(out, stl_sprintf("found sites count: %d", sites.size()));
            for (int32_t i = 0; i < *cur_year; i++)
            {
                // Don't embark on the same region every time.
                ai->rng();
            }
            df::coord2d site = sites[std::uniform_int_distribution<size_t>(0, sites.size() - 1)(ai->rng)];
            df::coord2d diff = site - start;

            if (diff.x >= 0)
            {
                for (int16_t x = 0; x < diff.x; x++)
                {
                    AI::feed_key(view, interface_key::CURSOR_RIGHT);
                }
            }
            else
            {
                for (int16_t x = 0; x > diff.x; x--)
                {
                    AI::feed_key(view, interface_key::CURSOR_LEFT);
                }
            }

            if (diff.y >= 0)
            {
                for (int16_t y = 0; y < diff.y; y++)
                {
                    AI::feed_key(view, interface_key::CURSOR_DOWN);
                }
            }
            else
            {
                for (int16_t y = 0; y > diff.y; y--)
                {
                    AI::feed_key(view, interface_key::CURSOR_UP);
                }
            }

            selected_embark = true;

            ai->timeout_sameview(5, [this](color_ostream & out)
            {
                found_site_step(out);
            });
        }
    }
    else if (df::viewscreen_setupdwarfgamest *view = strict_virtual_cast<df::viewscreen_setupdwarfgamest>(curview))
    {
        ai->debug(out, "choosing \"Play Now\"");
        AI::feed_key(view, interface_key::SELECT);
        // TODO custom embark loadout
    }
    else if (strict_virtual_cast<df::viewscreen_textviewerst>(curview))
    {
        ai->debug(out, "site is ready.");
        ai->timeout_sameview([this](color_ostream & out)
        {
            embarking = false;
            ai->debug(out, "disabling minimap.");
            AI::feed_key(interface_key::LEAVESCREEN);
            Gui::setMenuWidth(3, 3);
            *standing_orders_gather_refuse_outside = 1;
            *standing_orders_job_cancel_announce = config.cancel_announce;
        });
        return true;
    }
    else if (strict_virtual_cast<df::viewscreen_dwarfmodest>(curview))
    {
        embarking = false;
        return true;
    }
    return false;
}

void Embark::found_site_step(color_ostream &)
{
    df::viewscreen_choose_start_sitest *view = strict_virtual_cast<df::viewscreen_choose_start_sitest>(Gui::getCurViewscreen(true));
    if (view->biome_highlighted)
    {
        int32_t biome_idx = view->biome_idx + 1;
        AI::feed_key(view, static_cast<df::interface_key>(interface_key::SETUP_BIOME_1 + biome_idx));
        if (!view->biome_highlighted || biome_idx != view->biome_idx)
        {
            AI::feed_key(view, interface_key::SETUP_EMBARK);
            // dismiss warnings
            AI::feed_key(view, interface_key::SELECT);
            return;
        }
    }
    else
    {
        AI::feed_key(view, interface_key::CHANGETAB);
        if (view->page == df::viewscreen_choose_start_sitest::T_page::Biome)
        {
            AI::feed_key(view, interface_key::SETUP_BIOME_1);
            if (!view->biome_highlighted)
            {
                AI::feed_key(view, interface_key::SETUP_EMBARK);
                // dismiss warnings
                AI::feed_key(view, interface_key::SELECT);
                return;
            }
        }
    }

    ai->timeout_sameview(5, [this](color_ostream & out)
    {
        found_site_step(out);
    });
}
