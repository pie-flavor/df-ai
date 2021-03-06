#pragma once

#include "event_manager.h"
#include "room.h"
#include "stocks.h"

#include <functional>
#include <list>
#include <map>
#include <set>

#include "df/coord.h"
#include "df/furnace_type.h"
#include "df/tile_dig_designation.h"
#include "df/tiletype_material.h"
#include "df/tiletype_shape_basic.h"
#include "df/workshop_type.h"

namespace df
{
    struct building;
    struct building_stockpilest;
    struct item;
    struct stockpile_settings;
}

class AI;

namespace task_type
{
    enum type
    {
        check_construct,
        check_furnish,
        check_idle,
        check_rooms,
        construct_activityzone,
        construct_farmplot,
        construct_furnace,
        construct_stockpile,
        construct_tradedepot,
        construct_workshop,
        dig_cistern,
        dig_garbage,
        dig_room,
        furnish,
        monitor_cistern,
        monitor_farm_irrigation,
        setup_farmplot,
        want_dig,

        _task_type_count
    };
}

std::ostream & operator <<(std::ostream & stream, task_type::type type);

struct task
{
    task_type::type type;
    room *r;
    furniture *f;

    task(task_type::type type, room *r = nullptr, furniture *f = nullptr) :
        type(type), r(r), f(f)
    {
    }
};

class Plan
{
    AI *ai;
    OnupdateCallback *onupdate_handle;
    std::map<int32_t, size_t> nrdig;
    std::list<task *> tasks_generic;
    std::list<task *> tasks_furniture;
    std::list<task *>::iterator bg_idx_generic;
    std::list<task *>::iterator bg_idx_furniture;
    std::vector<room *> rooms;
    std::map<room_type::type, std::vector<room *>> room_category;
    std::map<int32_t, std::set<room *>> room_by_z;
    std::vector<room *> corridors;
    std::set<stock_item::item> cache_nofurnish;
public:
    room *fort_entrance;
    std::map<int32_t, std::set<df::coord>> map_veins;
private:
    std::vector<df::workshop_type> important_workshops;
    std::vector<df::furnace_type> important_workshops2;
    std::vector<df::workshop_type> important_workshops3;
    furniture *m_c_lever_in;
    furniture *m_c_lever_out;
    room *m_c_cistern;
    room *m_c_reserve;
    int32_t m_c_testgate_delay;
    size_t checkroom_idx;
    size_t trycistern_count;
    std::map<int32_t, std::vector<std::pair<df::coord, df::tile_dig_designation>>> map_vein_queue;
    std::set<df::coord> dug_veins;
    int32_t noblesuite;
    int16_t cavern_max_level;
    int32_t last_idle_year;
    bool allow_ice;
public:
    bool should_search_for_metal;
    bool past_initial_phase;
private:
    bool cistern_channel_requested;

public:
    Plan(AI *ai);
    ~Plan();

    command_result startup(color_ostream & out);
    command_result onupdate_register(color_ostream & out);
    command_result onupdate_unregister(color_ostream & out);

    command_result persist(color_ostream & out);
    command_result unpersist(color_ostream & out);

    void update(color_ostream & out);

    void save(std::ostream & out);
    void load(std::istream & in);

    static uint16_t getTileWalkable(df::coord t);

    task *is_digging();
    bool is_idle();

    void new_citizen(color_ostream & out, int32_t uid);
    void del_citizen(color_ostream & out, int32_t uid);

    bool checkidle(color_ostream & out);
    void idleidle(color_ostream & out);

    void checkrooms(color_ostream & out);
    void checkroom(color_ostream & out, room *r);

    void getbedroom(color_ostream & out, int32_t id);
    void getdiningroom(color_ostream & out, int32_t id);
    void attribute_noblerooms(color_ostream & out, const std::set<int32_t> & id_list);

    void getsoldierbarrack(color_ostream & out, int32_t id);
    void assign_barrack_squad(color_ostream & out, df::building *bld, int32_t squad_id);

    void getcoffin(color_ostream & out);

    void freebedroom(color_ostream & out, int32_t id);
    void freecommonrooms(color_ostream & out, int32_t id, room_type::type subtype);
    void freecommonrooms(color_ostream & out, int32_t id);
    void freesoldierbarrack(color_ostream & out, int32_t id);

    df::building *getpasture(color_ostream & out, int32_t pet_id);
    void freepasture(color_ostream & out, int32_t pet_id);

    void set_owner(color_ostream & out, room *r, int32_t uid);

    static void dig_tile(df::coord t, df::tile_dig_designation dig = tile_dig_designation::Default);
    void wantdig(color_ostream & out, room *r);
    void digroom(color_ostream & out, room *r);
    bool construct_room(color_ostream & out, room *r);
    bool furnish_room(color_ostream & out, room *r);
    bool try_furnish(color_ostream & out, room *r, furniture *f);
    bool try_furnish_well(color_ostream & out, room *r, furniture *f, df::coord t);
    bool try_furnish_archerytarget(color_ostream & out, room *r, furniture *f, df::coord t);
    bool try_furnish_construction(color_ostream & out, df::construction_type ctype, df::coord t);
    bool try_furnish_windmill(color_ostream & out, room *r, furniture *f, df::coord t);
    bool try_furnish_roller(color_ostream & out, room *r, furniture *f, df::coord t);

    bool try_construct_tradedepot(color_ostream & out, room *r);
    bool try_construct_workshop(color_ostream & out, room *r);
    bool try_construct_furnace(color_ostream & out, room *r);
    bool try_construct_stockpile(color_ostream & out, room *r);
    bool try_construct_activityzone(color_ostream & out, room *r);

    bool monitor_farm_irrigation(color_ostream & out, room *r);
    bool can_place_farm(color_ostream & out, room *r, bool cheat);
    bool try_construct_farmplot(color_ostream & out, room *r);

    void move_dininghall_fromtemp(color_ostream & out, room *r, room *t);

    bool smooth_room(color_ostream & out, room *r, bool engrave = false);
    void smooth_room_access(color_ostream & out, room *r);
    void smooth_cistern(color_ostream & out, room *r);
    void smooth_cistern_access(color_ostream & out, room *r);
    bool construct_cistern(color_ostream & out, room *r);
    bool dump_items_access(color_ostream & out, room *r);
    void room_items(color_ostream & out, room *r, std::function<void(df::item *)> f);
    bool smooth_xyz(df::coord min, df::coord max, bool engrave = false);
    bool smooth(std::set<df::coord> tiles, bool engrave = false);
    bool is_smooth(df::coord t, bool engrave = false);

    bool try_digcistern(color_ostream & out, room *r);
    void dig_garbagedump(color_ostream & out);
    bool try_diggarbage(color_ostream & out, room *r);
    bool try_setup_farmplot(color_ostream & out, room *r);
    bool try_endfurnish(color_ostream & out, room *r, furniture *f);

    bool setup_lever(color_ostream & out, room *r, furniture *f);
    bool link_lever(color_ostream & out, furniture *src, furniture *dst);
    bool pull_lever(color_ostream & out, furniture *f);

    void monitor_cistern(color_ostream & out);

    bool try_endconstruct(color_ostream & out, room *r);

    df::coord scan_river(color_ostream & out);

    command_result make_map_walkable(color_ostream & out);
    command_result list_map_veins(color_ostream & out);

    int32_t dig_vein(color_ostream & out, int32_t mat, int32_t want_boulders = 1);
    int32_t do_dig_vein(color_ostream & out, int32_t mat, df::coord b);

    static df::coord spiral_search(df::coord t, int16_t max, int16_t min, int16_t step, std::function<bool(df::coord)> b);
    static inline df::coord spiral_search(df::coord t, int16_t max, int16_t min, std::function<bool(df::coord)> b)
    {
        return spiral_search(t, max, min, 1, b);
    }
    static inline df::coord spiral_search(df::coord t, int16_t max, std::function<bool(df::coord)> b)
    {
        return spiral_search(t, max, 0, 1, b);
    }
    static inline df::coord spiral_search(df::coord t, std::function<bool(df::coord)> b)
    {
        return spiral_search(t, 100, 0, 1, b);
    }

#include "plan_blueprint.h"

    bool map_tile_in_rock(df::coord tile);
    bool map_tile_nocavern(df::coord tile);
    bool map_tile_cavernfloor(df::coord tile);
    bool map_tile_undiscovered_cavern(df::coord tile);

    df::coord surface_tile_at(int16_t tx, int16_t ty, bool allow_trees = false);

    std::string status();
    std::string report();

    void categorize_all();

    std::string describe_room(room *r);
    std::string describe_furniture(furniture *f);

    room *find_room(room_type::type type);
    room *find_room(room_type::type type, std::function<bool(room *)> b);
    room *find_room_at(df::coord t);
    bool map_tile_intersects_room(df::coord t);

    static df::coord find_tree_base(df::coord t);

private:
    void fixup_open(color_ostream & out, room *r);
    void fixup_open_tile(color_ostream & out, room *r, df::coord t, df::tile_dig_designation d, furniture *f = nullptr);
    void fixup_open_helper(color_ostream & out, room *r, df::coord t, df::construction_type c, furniture *f = nullptr);

protected:
    bool corridor_include_hack(const room *r, df::coord t1, df::coord t2);
    friend struct room;
};

struct farm_allowed_materials_t
{
    std::set<df::tiletype_material> set;
    farm_allowed_materials_t();
};
extern farm_allowed_materials_t farm_allowed_materials;

extern const size_t dwarves_per_table;
extern const int32_t dwarves_per_farmtile_num;
extern const int32_t dwarves_per_farmtile_den;
extern const size_t wantdig_max;
extern const int32_t spare_bedroom;
extern const int32_t extra_farms;

// vim: et:sw=4:ts=4
