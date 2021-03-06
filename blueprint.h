#pragma once

#include "dfhack_shared.h"
#include "room.h"

#include "jsoncpp.h"

class AI;

struct room_base
{
    typedef size_t layoutindex_t;
    typedef size_t roomindex_t;
    typedef size_t placeholderindex_t;
    struct furniture_t
    {
        furniture_t();

        bool apply(Json::Value data, std::string & error, bool allow_placeholders = false);
        void shift(layoutindex_t layout_start, roomindex_t room_start);
        bool check_indexes(layoutindex_t layout_limit, roomindex_t room_limit, std::string & error) const;

        bool has_placeholder;
        placeholderindex_t placeholder;

        layout_type::type type;
        df::construction_type construction;
        df::tile_dig_designation dig;
        df::coord pos;

        bool has_target;
        layoutindex_t target;

        bool has_users;
        bool ignore;
        bool makeroom;
        bool internal;

        std::string comment;
    };
    struct room_t
    {
        room_t();

        bool apply(Json::Value data, std::string & error, bool allow_placeholders = false);
        void shift(layoutindex_t layout_start, roomindex_t room_start);
        bool check_indexes(layoutindex_t layout_limit, roomindex_t room_limit, std::string & error) const;

        bool has_placeholder;
        placeholderindex_t placeholder;

        room_type::type type;

        corridor_type::type corridor_type;
        farm_type::type farm_type;
        stockpile_type::type stockpile_type;
        nobleroom_type::type nobleroom_type;
        outpost_type::type outpost_type;
        location_type::type location_type;
        cistern_type::type cistern_type;
        df::workshop_type workshop_type;
        df::furnace_type furnace_type;

        std::string raw_type;

        std::string comment;

        df::coord min, max;

        std::vector<roomindex_t> accesspath;
        std::vector<layoutindex_t> layout;

        int32_t level;
        int32_t noblesuite;
        int32_t queue;

        bool has_workshop;
        roomindex_t workshop;

        std::set<df::stockpile_list> stock_disable;
        bool stock_specific1;
        bool stock_specific2;

        bool has_users;
        bool temporary;
        bool outdoor;

        bool require_walls;
        bool in_corridor;
        std::map<df::coord, std::set<std::string>> exits;
    };

    ~room_base();

    bool apply(Json::Value data, std::string & error, bool allow_placeholders = false);

    std::vector<furniture_t *> layout;
    std::vector<room_t *> rooms;
};

struct room_template : public room_base
{
    room_template(const std::string &, const std::string & name) : name(name), min_placeholders() {}

    bool apply(Json::Value data, std::string & error);

    const std::string name;
    placeholderindex_t min_placeholders;
};

struct room_instance : public room_base
{
    room_instance(const std::string & type, const std::string & name) : type(type), name(name), placeholders() {}
    ~room_instance();

    bool apply(Json::Value data, std::string & error);

    const std::string type, name;
    std::vector<Json::Value *> placeholders;
};

struct room_blueprint
{
    room_blueprint(const room_template *tmpl, const room_instance *inst);
    room_blueprint(const room_blueprint & rb, df::coord offset = df::coord(0, 0, 0));
    ~room_blueprint();

    df::coord origin;
    const room_template *tmpl;
    const room_instance *inst;

    const std::string type, tmpl_name, name;

    std::vector<room_base::furniture_t *> layout;
    std::vector<room_base::room_t *> rooms;

    std::set<df::coord> corridor;
    std::set<df::coord> interior;
    std::set<df::coord> no_room;
    std::set<df::coord> no_corridor;

    bool apply(std::string & error);
    void build_cache();
};

struct blueprint_plan_template;
class blueprints_t;

struct blueprint_plan
{
    ~blueprint_plan();

    std::vector<room_base::furniture_t *> layout;
    std::vector<room_base::room_t *> rooms;

    std::map<df::coord, std::pair<room_base::roomindex_t, std::set<std::string>>> room_connect;
    std::set<df::coord> corridor;
    std::set<df::coord> interior;
    std::set<df::coord> no_room;
    std::set<df::coord> no_corridor;

    bool build(color_ostream & out, AI *ai, const blueprints_t & blueprints);
    void create(room * & fort_entrance, std::vector<room *> & real_corridors, std::vector<room *> & real_rooms) const;

private:
    typedef void (blueprint_plan::*find_fn)(color_ostream &, AI *, std::vector<const room_blueprint *> &, const std::map<std::string, size_t> &, const std::map<std::string, std::map<std::string, size_t>> &, const blueprints_t &, const blueprint_plan_template &);
    typedef bool (blueprint_plan::*try_add_fn)(color_ostream &, AI *, const room_blueprint &, std::map<std::string, size_t> &, std::map<std::string, std::map<std::string, size_t>> &, const blueprint_plan_template &);

    bool add(const room_blueprint & rb, std::string & error);
    bool add(const room_blueprint & rb, room_base::roomindex_t parent, std::string & error);
    bool build(color_ostream & out, AI *ai, const blueprints_t & blueprints, const blueprint_plan_template & plan);
    void place_rooms(color_ostream & out, AI *ai, std::map<std::string, size_t> & counts, std::map<std::string, std::map<std::string, size_t>> & instance_counts, const blueprints_t & blueprints, const blueprint_plan_template & plan, find_fn find, try_add_fn try_add);
    void clear();
    void find_available_blueprints(color_ostream & out, AI *ai, std::vector<const room_blueprint *> & available_blueprints, const std::map<std::string, size_t> & counts, const std::map<std::string, std::map<std::string, size_t>> & instance_counts, const blueprints_t & blueprints, const blueprint_plan_template & plan, const std::set<std::string> & available_tags_base, const std::function<bool(const room_blueprint &)> & check);
    void find_available_blueprints_start(color_ostream & out, AI *ai, std::vector<const room_blueprint *> & available_blueprints, const std::map<std::string, size_t> & counts, const std::map<std::string, std::map<std::string, size_t>> & instance_counts, const blueprints_t & blueprints, const blueprint_plan_template & plan);
    void find_available_blueprints_outdoor(color_ostream & out, AI *ai, std::vector<const room_blueprint *> & available_blueprints, const std::map<std::string, size_t> & counts, const std::map<std::string, std::map<std::string, size_t>> & instance_counts, const blueprints_t & blueprints, const blueprint_plan_template & plan);
    void find_available_blueprints_connect(color_ostream & out, AI *ai, std::vector<const room_blueprint *> & available_blueprints, const std::map<std::string, size_t> & counts, const std::map<std::string, std::map<std::string, size_t>> & instance_counts, const blueprints_t & blueprints, const blueprint_plan_template & plan);
    bool can_add_room(color_ostream & out, AI *ai, const room_blueprint & rb, df::coord pos);
    bool try_add_room_outdoor(color_ostream & out, AI *ai, const room_blueprint & rb, std::map<std::string, size_t> & counts, std::map<std::string, std::map<std::string, size_t>> & instance_counts, const blueprint_plan_template & plan);
    bool try_add_room_connect(color_ostream & out, AI *ai, const room_blueprint & rb, std::map<std::string, size_t> & counts, std::map<std::string, std::map<std::string, size_t>> & instance_counts, const blueprint_plan_template & plan);
};

struct blueprint_plan_template
{
    blueprint_plan_template(const std::string &, const std::string & name) :
        name(name),
        max_retries(25),
        max_failures(100)
    {
    }

    const std::string name;
    size_t max_retries;
    size_t max_failures;
    std::string start;
    std::set<std::string> outdoor;
    std::map<std::string, std::set<std::string>> tags;
    std::map<std::string, std::pair<size_t, size_t>> limits;
    std::map<std::string, std::map<std::string, std::pair<size_t, size_t>>> instance_limits;

    bool apply(Json::Value data, std::string & error);
    bool have_minimum_requirements(color_ostream & out, AI *ai, const std::map<std::string, size_t> & counts, const std::map<std::string, std::map<std::string, size_t>> & instance_counts) const;
};

class blueprints_t
{
public:
    blueprints_t(color_ostream & out);
    ~blueprints_t();

private:
    std::map<std::string, std::vector<room_blueprint *>> blueprints;
    std::map<std::string, blueprint_plan_template *> plans;
    friend struct blueprint_plan;
};
