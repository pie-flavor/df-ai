#include "ai.h"
#include "blueprint.h"

#include "modules/Filesystem.h"
#include "modules/Maps.h"

template<typename idx_t>
static bool apply_index(bool & has_idx, idx_t & idx, Json::Value & data, const std::string & name, std::string & error)
{
    Json::Value value = data.removeMember(name);
    if (!value.isInt())
    {
        error = name + " has wrong type (should be integer)";
        return false;
    }

    if (value.asInt() < 0)
    {
        error = name + " cannot be negative";
        return false;
    }

    has_idx = true;
    idx = idx_t(value.asInt());

    return true;
}

template<typename idx_t>
static bool apply_indexes(std::vector<idx_t> & idx, Json::Value & data, const std::string & name, std::string & error)
{
    Json::Value value = data.removeMember(name);
    if (!value.isArray() || std::find_if(value.begin(), value.end(), [](Json::Value & v) -> bool { return !v.isInt(); }) != value.end())
    {
        error = name + " has wrong type (should be array of integers)";
        return false;
    }

    for (auto & i : value)
    {
        if (i.asInt() < 0)
        {
            error = name + " cannot be negative";
            return false;
        }

        idx.push_back(idx_t(i.asInt()));
    }

    return true;
}

template<typename enum_t>
static bool apply_enum(enum_t & var, Json::Value & data, const std::string & name, std::string & error)
{
    Json::Value value = data.removeMember(name);
    if (!value.isString())
    {
        error = name + " has wrong type (should be string)";
        return false;
    }

    if (find_enum_item(&var, value.asString()))
    {
        return true;
    }

    error = "invalid " + name + " (" + value.asString() + ")";
    return false;
}

template<typename enum_t>
static bool apply_enum_set(std::set<enum_t> & var, Json::Value & data, const std::string & name, std::string & error)
{
    Json::Value value = data.removeMember(name);
    if (!value.isArray() || std::find_if(value.begin(), value.end(), [](Json::Value & v) -> bool { return !v.isString(); }) != value.end())
    {
        error = name + " has wrong type (should be array of strings)";
        return false;
    }

    for (auto & i : value)
    {
        enum_t e;
        if (!find_enum_item(&e, i.asString()))
        {
            error = "invalid " + name + " (" + i.asString() + ")";
            return false;
        }

        if (!var.insert(e).second)
        {
            error = "duplicate " + name + " (" + i.asString() + ")";
            return false;
        }
    }

    return true;
}

template<typename int_t>
static bool apply_int(int_t & var, Json::Value & data, const std::string & name, std::string & error)
{
    Json::Value value = data.removeMember(name);
    if (!value.isIntegral())
    {
        error = name + " has wrong type (should be integer)";
        return false;
    }

    if (std::is_unsigned<int_t>::value)
    {
        if (value.asLargestUInt() > std::numeric_limits<int_t>::max())
        {
            error = name + " is too big!";
            return false;
        }

        var = int_t(value.asLargestUInt());
    }
    else
    {
        if (value.asLargestInt() < std::numeric_limits<int_t>::min())
        {
            error = name + " is too small!";
            return false;
        }

        if (value.asLargestInt() > std::numeric_limits<int_t>::max())
        {
            error = name + " is too big!";
            return false;
        }

        var = int_t(value.asLargestInt());
    }

    return true;
}

static bool apply_bool(bool & var, Json::Value & data, const std::string & name, std::string & error)
{
    Json::Value value = data.removeMember(name);
    if (!value.isBool())
    {
        error = name + " has wrong type (should be true or false)";
        return false;
    }

    var = value.asBool();

    return true;
}

static bool apply_string(std::string & var, Json::Value & data, const std::string & name, std::string & error, bool append = false)
{
    Json::Value value = data.removeMember(name);
    if (!value.isString())
    {
        error = name + " has wrong type (should be string)";
        return false;
    }

    if (append)
    {
        var += value.asString();
    }
    else
    {
        var = value.asString();
    }

    return true;
}

static bool apply_coord(df::coord & var, Json::Value & data, const std::string & name, std::string & error)
{
    Json::Value value = data.removeMember(name);
    if (!value.isArray() || value.size() != 3 || !value[0].isInt() || !value[1].isInt() || !value[2].isInt())
    {
        error = name + " has the wrong type (should be an array of three integers)";
        return false;
    }

    var.x = int16_t(value[0].asInt());
    var.y = int16_t(value[1].asInt());
    var.z = int16_t(value[2].asInt());

    return true;
}

static bool check_indexes(const std::vector<room_base::furniture_t *> & layout, const std::vector<room_base::room_t *> & rooms, std::string & error)
{
    room_base::layoutindex_t layout_limit(layout.size());
    room_base::roomindex_t room_limit(rooms.size());

    for (auto f : layout)
    {
        if (f && !f->check_indexes(layout_limit, room_limit, error))
        {
            return false;
        }
    }

    for (auto r : layout)
    {
        if (r && !r->check_indexes(layout_limit, room_limit, error))
        {
            return false;
        }
    }

    return true;
}

room_base::furniture_t::furniture_t() :
    has_placeholder(false),
    placeholder(),
    type(layout_type::none),
    construction(construction_type::NONE),
    dig(tile_dig_designation::Default),
    pos(0, 0, 0),
    has_target(false),
    target(),
    has_users(false),
    ignore(false),
    makeroom(false),
    internal(false),
    comment()
{
}

bool room_base::furniture_t::apply(Json::Value data, std::string & error, bool allow_placeholders)
{
    std::ostringstream scratch;

    if (allow_placeholders && data.isMember("placeholder") && !apply_index(has_placeholder, placeholder, data, "placeholder", error))
    {
        return false;
    }

    if (data.isMember("type") && !apply_enum(type, data, "type", error))
    {
        return false;
    }

    if (data.isMember("construction") && !apply_enum(construction, data, "construction", error))
    {
        return false;
    }

    if (data.isMember("dig") && !apply_enum(dig, data, "dig", error))
    {
        return false;
    }

    if (data.isMember("x") && !apply_int(pos.x, data, "x", error))
    {
        return false;
    }
    if (data.isMember("y") && !apply_int(pos.y, data, "y", error))
    {
        return false;
    }
    if (data.isMember("z") && !apply_int(pos.z, data, "z", error))
    {
        return false;
    }

    if (data.isMember("target") && !apply_index(has_target, target, data, "target", error))
    {
        return false;
    }

    if (data.isMember("has_users") && !apply_bool(has_users, data, "has_users", error))
    {
        return false;
    }

    if (data.isMember("ignore") && !apply_bool(ignore, data, "ignore", error))
    {
        return false;
    }

    if (data.isMember("makeroom") && !apply_bool(makeroom, data, "makeroom", error))
    {
        return false;
    }

    if (data.isMember("internal") && !apply_bool(internal, data, "internal", error))
    {
        return false;
    }

    if (data.isMember("comment") && !apply_string(comment, data, "comment", error, true))
    {
        return false;
    }

    std::vector<std::string> remaining_members(data.getMemberNames());
    if (remaining_members.empty())
    {
        return true;
    }

    error = "";
    const char *before = "unhandled furniture properties: ";
    for (auto & m : remaining_members)
    {
        error += before;
        error += m;
        before = ", ";
    }

    return false;
}

void room_base::furniture_t::shift(room_base::layoutindex_t layout_start, room_base::roomindex_t room_start)
{
    if (has_target)
    {
        target += layout_start;
    }
}

bool room_base::furniture_t::check_indexes(room_base::layoutindex_t layout_limit, room_base::roomindex_t room_limit, std::string & error) const
{
    if (has_target && target >= layout_limit)
    {
        error = "invalid target";
        return false;
    }

    return true;
}

room_base::room_t::room_t() :
    has_placeholder(false),
    placeholder(),
    type(room_type::corridor),
    corridor_type(),
    farm_type(),
    stockpile_type(),
    nobleroom_type(),
    outpost_type(),
    location_type(),
    cistern_type(),
    workshop_type(),
    furnace_type(),
    raw_type(),
    comment(),
    min(),
    max(),
    accesspath(),
    layout(),
    level(-1),
    noblesuite(-1),
    queue(0),
    has_workshop(false),
    workshop(),
    stock_disable(),
    stock_specific1(false),
    stock_specific2(false),
    has_users(false),
    temporary(false),
    outdoor(false),
    require_walls(true),
    in_corridor(false),
    exits()
{
}

bool room_base::room_t::apply(Json::Value data, std::string & error, bool allow_placeholders)
{
    if (allow_placeholders && data.isMember("placeholder") && !apply_index(has_placeholder, placeholder, data, "placeholder", error))
    {
        return false;
    }

    if (data.isMember("type") && !apply_enum(type, data, "type", error))
    {
        return false;
    }

    if (data.isMember("corridor_type") && !apply_enum(corridor_type, data, "corridor_type", error))
    {
        return false;
    }
    if (data.isMember("farm_type") && !apply_enum(farm_type, data, "farm_type", error))
    {
        return false;
    }
    if (data.isMember("stockpile_type") && !apply_enum(stockpile_type, data, "stockpile_type", error))
    {
        return false;
    }
    if (data.isMember("nobleroom_type") && !apply_enum(nobleroom_type, data, "nobleroom_type", error))
    {
        return false;
    }
    if (data.isMember("outpost_type") && !apply_enum(outpost_type, data, "outpost_type", error))
    {
        return false;
    }
    if (data.isMember("location_type") && !apply_enum(location_type, data, "location_type", error))
    {
        return false;
    }
    if (data.isMember("cistern_type") && !apply_enum(cistern_type, data, "cistern_type", error))
    {
        return false;
    }
    if (data.isMember("workshop_type") && !apply_enum(workshop_type, data, "workshop_type", error))
    {
        return false;
    }
    if (data.isMember("furnace_type") && !apply_enum(furnace_type, data, "furnace_type", error))
    {
        return false;
    }

    if (data.isMember("raw_type") && !apply_string(raw_type, data, "raw_type", error))
    {
        return false;
    }

    if (data.isMember("comment") && !apply_string(comment, data, "comment", error, true))
    {
        return false;
    }

    if (!min.isValid())
    {
        if (data.isMember("min"))
        {
            if (!apply_coord(min, data, "min", error))
            {
                return false;
            }
        }
        else
        {
            if (data.isMember("max"))
            {
                error = "missing min on room";
            }
            else
            {
                error = "missing min and max on room";
            }
            return false;
        }

        if (data.isMember("max"))
        {
            if (!apply_coord(max, data, "max", error))
            {
                return false;
            }
        }
        else
        {
            error = "missing max on room";
            return false;
        }

        if (min.x > max.x)
        {
            error = "min.x > max.x";
            return false;
        }
        if (min.y > max.y)
        {
            error = "min.y > max.y";
            return false;
        }
        if (min.z > max.z)
        {
            error = "min.z > max.z";
            return false;
        }

        if (data.isMember("exits"))
        {
            Json::Value value = data.removeMember("exits");
            if (!value.isArray())
            {
                error = "exits has wrong type (should be array)";
                return false;
            }

            for (auto exit : value)
            {
                if (!exit.isArray() || exit.size() != 4 || !exit[0].isString() || !exit[1].isInt() || !exit[2].isInt() || !exit[3].isInt())
                {
                    error = "exit has wrong type (should be [string, integer, integer, integer])";
                    return false;
                }

                df::coord t(exit[1].asInt(), exit[2].asInt(), exit[3].asInt());
                exits[t].insert(exit[0].asString());
            }
        }
    }

    if (data.isMember("accesspath") && !apply_indexes(accesspath, data, "accesspath", error))
    {
        return false;
    }
    if (data.isMember("layout") && !apply_indexes(layout, data, "layout", error))
    {
        return false;
    }

    if (data.isMember("level") && !apply_int(level, data, "level", error))
    {
        return false;
    }
    if (data.isMember("noblesuite") && !apply_int(noblesuite, data, "noblesuite", error))
    {
        return false;
    }
    if (data.isMember("queue") && !apply_int(queue, data, "queue", error))
    {
        return false;
    }

    if (data.isMember("workshop") && !apply_index(has_workshop, workshop, data, "workshop", error))
    {
        return false;
    }

    if (data.isMember("stock_disable") && !apply_enum_set(stock_disable, data, "stock_disable", error))
    {
        return false;
    }
    if (data.isMember("stock_specific1") && !apply_bool(stock_specific1, data, "stock_specific1", error))
    {
        return false;
    }
    if (data.isMember("stock_specific2") && !apply_bool(stock_specific2, data, "stock_specific2", error))
    {
        return false;
    }

    if (data.isMember("has_users") && !apply_bool(has_users, data, "has_users", error))
    {
        return false;
    }
    if (data.isMember("temporary") && !apply_bool(temporary, data, "temporary", error))
    {
        return false;
    }
    if (data.isMember("outdoor") && !apply_bool(outdoor, data, "outdoor", error))
    {
        return false;
    }

    if (data.isMember("require_walls") && !apply_bool(in_corridor, data, "require_walls", error))
    {
        return false;
    }
    if (data.isMember("in_corridor") && !apply_bool(in_corridor, data, "in_corridor", error))
    {
        return false;
    }

    std::vector<std::string> remaining_members(data.getMemberNames());
    if (remaining_members.empty())
    {
        return true;
    }

    error = "";
    const char *before = "unhandled room properties: ";
    for (auto & m : remaining_members)
    {
        error += before;
        error += m;
        before = ", ";
    }

    return false;
}

void room_base::room_t::shift(room_base::layoutindex_t layout_start, room_base::roomindex_t room_start)
{
    for (auto & r : accesspath)
    {
        r += room_start;
    }
    for (auto & f : layout)
    {
        f += layout_start;
    }
    if (has_workshop)
    {
        workshop += room_start;
    }
}

bool room_base::room_t::check_indexes(room_base::layoutindex_t layout_limit, room_base::roomindex_t room_limit, std::string & error) const
{
    for (auto r : accesspath)
    {
        if (r >= room_limit)
        {
            error = "invalid accesspath";
            return false;
        }
    }
    for (auto f : layout)
    {
        if (f >= layout_limit)
        {
            error = "invalid layout";
            return false;
        }
    }
    if (has_workshop && workshop >= room_limit)
    {
        error = "invalid workshop";
        return false;
    }

    return true;
}

room_base::~room_base()
{
    for (auto & f : layout)
    {
        delete f;
    }
    for (auto & r : rooms)
    {
        delete r;
    }
}

bool room_base::apply(Json::Value data, std::string & error, bool allow_placeholders)
{
    if (data.isMember("f"))
    {
        if (!data["f"].isArray())
        {
            error = "f (furniture list) must be an array";
            return false;
        }

        for (auto & fdata : data["f"])
        {
            furniture_t *f = new furniture_t();
            if (!f->apply(fdata, error, allow_placeholders))
            {
                delete f;
                return false;
            }

            layout.push_back(f);
        }

        data.removeMember("f");
    }

    if (data.isMember("r"))
    {
        if (!data["r"].isArray())
        {
            error = "r (room list) must be an array";
            return false;
        }

        for (auto & rdata : data["r"])
        {
            room_t *r = new room_t();
            if (!r->apply(rdata, error, allow_placeholders))
            {
                delete r;
                return false;
            }

            rooms.push_back(r);
        }

        data.removeMember("r");
    }

    if (!check_indexes(layout, rooms, error))
    {
        return false;
    }

    std::vector<std::string> remaining_members(data.getMemberNames());
    if (remaining_members.empty())
    {
        return true;
    }

    error = "";
    const char *before = "unhandled properties: ";
    for (auto & m : remaining_members)
    {
        error += before;
        error += m;
        before = ", ";
    }

    return false;
}

bool room_template::apply(Json::Value data, std::string & error)
{
    min_placeholders = 0;

    if (!room_base::apply(data, error, true))
    {
        return false;
    }

    for (auto & f : layout)
    {
        if (f->has_placeholder)
        {
            min_placeholders = std::max(min_placeholders, f->placeholder + 1);
        }
    }
    for (auto & r : rooms)
    {
        if (r->has_placeholder)
        {
            min_placeholders = std::max(min_placeholders, r->placeholder + 1);
        }
    }

    return true;
}

bool room_instance::apply(Json::Value data, std::string & error)
{
    if (data.isMember("p"))
    {
        if (!data["p"].isArray())
        {
            error = "p (placeholder list) must be an array";
            return false;
        }

        for (auto & p : data["p"])
        {
            if (!p.isObject())
            {
                error = "placeholders must be objects";
                return false;
            }

            placeholders.push_back(new Json::Value(p));
        }

        data.removeMember("p");
    }

    return room_base::apply(data, error, false);
}

room_instance::~room_instance()
{
    for (auto & p : placeholders)
    {
        delete p;
    }
}

room_blueprint::room_blueprint(const room_template *tmpl, const room_instance *inst) :
    origin(0, 0, 0),
    tmpl(tmpl),
    inst(inst),
    type(inst->type),
    tmpl_name(tmpl->name),
    name(inst->name),
    layout(tmpl->layout.size() + inst->layout.size()),
    rooms(tmpl->rooms.size() + inst->rooms.size()),
    interior(),
    no_room(),
    no_corridor()
{
}

room_blueprint::room_blueprint(const room_blueprint & rb, df::coord offset) :
    origin(rb.origin + offset),
    tmpl(),
    inst(),
    type(rb.type),
    tmpl_name(rb.tmpl_name),
    name(rb.name),
    layout(rb.layout.size()),
    rooms(rb.rooms.size()),
    interior(),
    no_room(),
    no_corridor()
{
    for (auto f : rb.layout)
    {
        layout.push_back(new room_base::furniture_t(*f));
    }

    for (auto r : rb.rooms)
    {
        r = new room_base::room_t(*r);

        r->min = r->min + offset;
        r->max = r->max + offset;

        rooms.push_back(r);
    }

    build_cache();
}

room_blueprint::~room_blueprint()
{
    for (auto & f : layout)
    {
        delete f;
    }
    for (auto & r : rooms)
    {
        delete r;
    }
}

bool room_blueprint::apply(std::string & error)
{
    if (!tmpl || !inst)
    {
        error = "room_blueprint::apply has already been called!";
        return false;
    }

    if (size_t(tmpl->min_placeholders) > inst->placeholders.size())
    {
        error = "not enough placeholders in instance";
        return false;
    }

    room_base::layoutindex_t inst_layout_start(tmpl->layout.size());
    room_base::roomindex_t inst_rooms_start(tmpl->rooms.size());

    // use twos complement to avoid a compiler warning
    room_base::layoutindex_t inst_layout_start_inv = (~inst_layout_start) + 1;
    room_base::roomindex_t inst_rooms_start_inv = (~inst_rooms_start) + 1;

    for (auto & ft : tmpl->layout)
    {
        room_base::furniture_t *f = new room_base::furniture_t(*ft);
        Json::Value *p = f->placeholder != -1 ? inst->placeholders.at(size_t(f->placeholder)) : nullptr;
        if (p)
        {
            if (p->isMember("skip") && (*p)["skip"].asBool())
            {
                delete f;
                f = nullptr;
            }
            else
            {
                f->shift(inst_layout_start_inv, inst_rooms_start_inv);
                if (!f->apply(*p, error))
                {
                    delete f;
                    return false;
                }
                f->shift(inst_layout_start, inst_rooms_start);
            }
        }
        layout.push_back(f);
    }
    for (auto & rt : tmpl->rooms)
    {
        room_base::room_t *r = new room_base::room_t(*rt);
        Json::Value *p = r->placeholder != -1 ? inst->placeholders.at(size_t(r->placeholder)) : nullptr;
        if (p)
        {
            if (p->isMember("skip") && (*p)["skip"].asBool())
            {
                delete r;
                r = nullptr;
            }
            else
            {
                r->shift(inst_layout_start_inv, inst_rooms_start_inv);
                if (!r->apply(*p, error))
                {
                    delete r;
                    return false;
                }
                r->shift(inst_layout_start, inst_rooms_start);
            }
        }
        rooms.push_back(r);
    }
    for (auto & fi : inst->layout)
    {
        room_base::furniture_t *f = new room_base::furniture_t(*fi);
        f->shift(inst_layout_start, inst_rooms_start);
        layout.push_back(f);
    }
    for (auto & ri : inst->rooms)
    {
        room_base::room_t *r = new room_base::room_t(*ri);
        r->shift(inst_layout_start, inst_rooms_start);
        rooms.push_back(r);
    }

    if (!check_indexes(layout, rooms, error))
    {
        return false;
    }

    std::vector<room_base::layoutindex_t> layout_move;
    room_base::layoutindex_t layoutindex = 0;
    std::vector<room_base::roomindex_t> room_move;
    room_base::roomindex_t roomindex = 0;

    for (auto f : layout)
    {
        layout_move.push_back(layoutindex);
        if (f)
        {
            layoutindex++;

            if (f->has_target && !layout.at(f->target))
            {
                f->has_target = false;
            }
        }
    }

    for (auto r : rooms)
    {
        room_move.push_back(roomindex);
        if (r)
        {
            roomindex++;

            r->accesspath.erase(std::remove_if(r->accesspath.begin(), r->accesspath.end(), [this](room_base::roomindex_t idx) -> bool
            {
                return !rooms.at(idx);
            }), r->accesspath.end());

            r->layout.erase(std::remove_if(r->layout.begin(), r->layout.end(), [this](room_base::layoutindex_t idx) -> bool
            {
                return !layout.at(idx);
            }), r->layout.end());

            if (r->has_workshop && !rooms.at(r->workshop))
            {
                r->has_workshop = false;
            }
        }
    }

    layout.erase(std::remove_if(layout.begin(), layout.end(), [](room_base::furniture_t *f) -> bool { return !f; }), layout.end());
    rooms.erase(std::remove_if(rooms.begin(), rooms.end(), [](room_base::room_t *r) -> bool { return !r; }), rooms.end());

    for (auto f : layout)
    {
        if (f->has_target)
        {
            f->target = layout_move.at(f->target);
        }
    }

    for (auto r : rooms)
    {
        for (auto & a : r->accesspath)
        {
            a = room_move.at(a);
        }

        for (auto & f : r->layout)
        {
            f = layout_move.at(f);
        }

        if (r->has_workshop)
        {
            r->workshop = room_move.at(r->workshop);
        }
    }

    std::set<room_base::layoutindex_t> used_furniture;
    for (auto r : rooms)
    {
        for (auto f : r->layout)
        {
            if (!used_furniture.insert(f).second)
            {
                error = "multiple uses of a single furniture";
                return false;
            }
        }
    }

    layout_move.clear();
    layoutindex = 0;
    for (room_base::layoutindex_t i = 0; i < layout.size(); i++)
    {
        layout_move.push_back(layoutindex);
        if (!used_furniture.count(i))
        {
            delete layout.at(i);
            layout.at(i) = nullptr;
        }
        else
        {
            layoutindex++;
        }
    }

    for (auto f : layout)
    {
        if (f->has_target)
        {
            f->target = layout_move.at(f->target);
        }
    }

    for (auto r : rooms)
    {
        for (auto & f : r->layout)
        {
            f = layout_move.at(f);
        }
    }

    tmpl = nullptr;
    inst = nullptr;

    build_cache();

    return true;
}

void room_blueprint::build_cache()
{
    corridor.clear();
    interior.clear();
    no_room.clear();
    no_corridor.clear();

    for (auto r : rooms)
    {
        std::set<df::coord> holes;
        for (auto fi : r->layout)
        {
            auto f = layout.at(fi);
            if (f->dig == tile_dig_designation::No || f->construction == construction_type::Wall)
            {
                holes.insert(r->min + f->pos);
            }
            else if (r->type == room_type::corridor && r->corridor_type == corridor_type::corridor)
            {
                interior.insert(r->min + f->pos);
                no_room.insert(r->min + f->pos);
            }
            else if (r->in_corridor || !r->require_walls || r->outdoor)
            {
                interior.insert(r->min + f->pos);
                no_room.insert(r->min + f->pos);
            }
            else
            {
                interior.insert(r->min + f->pos);
                for (int16_t dx = -1; dx <= 1; dx++)
                {
                    for (int16_t dy = -1; dy <= 1; dy++)
                    {
                        no_room.insert(r->min + f->pos + df::coord(dx, dy, 0));
                    }
                }
            }
        }

        for (int16_t x = r->min.x; x <= r->max.x; x++)
        {
            for (int16_t y = r->min.y; y <= r->max.y; y++)
            {
                for (int16_t z = r->min.z; z <= r->max.z; z++)
                {
                    df::coord t(x, y, z);
                    if (holes.count(t))
                    {
                        continue;
                    }

                    if (r->type == room_type::corridor && r->corridor_type == corridor_type::corridor && r->min.z == r->max.z)
                    {
                        corridor.insert(t);
                        if (!r->in_corridor)
                        {
                            no_corridor.insert(t);
                        }
                    }
                    else if (r->in_corridor || !r->require_walls || r->outdoor)
                    {
                        interior.insert(t);
                        no_room.insert(t);
                        if (!r->in_corridor)
                        {
                            no_corridor.insert(t);
                        }
                    }
                    else
                    {
                        interior.insert(t);
                        for (int16_t dx = -1; dx <= 1; dx++)
                        {
                            for (int16_t dy = -1; dy <= 1; dy++)
                            {
                                no_room.insert(t + df::coord(dx, dy, 0));
                                no_corridor.insert(t + df::coord(dx, dy, 0));
                            }
                        }
                    }
                }
            }
        }
    }
}

blueprint_plan::~blueprint_plan()
{
    clear();
}

bool blueprint_plan::build(color_ostream & out, AI *ai, const blueprints_t & blueprints)
{
    std::vector<const blueprint_plan_template *> templates;
    for (auto & bp : blueprints.plans)
    {
        templates.push_back(bp.second);
    }

    std::shuffle(templates.begin(), templates.end(), ai->rng);

    for (size_t i = 0; i < templates.size(); i++)
    {
        const blueprint_plan_template & plan = *templates.at(i);
        for (size_t retries = 0; retries < plan.max_retries; retries++)
        {
            ai->debug(out, stl_sprintf("Trying to create a blueprint using plan %d of %d: %s (attempt %d of %d)", i + 1, templates.size(), plan.name.c_str(), retries + 1, plan.max_retries));

            if (build(out, ai, blueprints, plan))
            {
                ai->debug(out, stl_sprintf("Successfully created a blueprint using plan: %s", plan.name.c_str()));
                return true;
            }

            ai->debug(out, "Plan failed. Resetting.");
            clear();
        }
    }

    ai->debug(out, "Ran out of plans. Failed to create a blueprint.");
    return false;
}

void blueprint_plan::create(std::vector<room *> & real_corridors, std::vector<room *> & real_rooms) const
{
    std::vector<furniture *> real_layout(layout.size());
    std::vector<room *> real_rooms_and_corridors(rooms.size());

    for (size_t i = 0; i < layout.size(); i++)
    {
        real_layout.push_back(new furniture());
    }
    for (size_t i = 0; i < rooms.size(); i++)
    {
        real_rooms_and_corridors.push_back(new room(room_type::type(), df::coord(), df::coord()));
    }

    for (size_t i = 0; i < layout.size(); i++)
    {
        auto in = layout.at(i);
        auto out = real_layout.at(i);

        out->type = in->type;
        out->construction = in->construction;
        out->dig = in->dig;
        out->pos = in->pos;

        if (in->has_target)
        {
            out->target = real_layout.at(in->target);
        }
        else
        {
            out->target = nullptr;
        }

        out->has_users = in->has_users;
        out->ignore = in->ignore;
        out->makeroom = in->makeroom;
        out->internal = in->internal;

        out->comment = in->comment;
    }
    for (size_t i = 0; i < rooms.size(); i++)
    {
        auto in = rooms.at(i);
        auto out = real_rooms_and_corridors.at(i);

        out->type = in->type;

        out->corridor_type = in->corridor_type;
        out->farm_type = in->farm_type;
        out->stockpile_type = in->stockpile_type;
        out->nobleroom_type = in->nobleroom_type;
        out->outpost_type = in->outpost_type;
        out->location_type = in->location_type;
        out->cistern_type = in->cistern_type;
        out->workshop_type = in->workshop_type;
        out->furnace_type = in->furnace_type;

        out->raw_type = in->raw_type;

        out->comment = in->comment;

        out->min = in->min;
        out->max = in->max;

        for (auto ri : in->accesspath)
        {
            out->accesspath.push_back(real_rooms_and_corridors.at(ri));
        }
        for (auto fi : in->layout)
        {
            out->layout.push_back(real_layout.at(fi));
        }

        out->level = in->level;
        out->noblesuite = in->noblesuite;
        out->queue = in->queue;

        if (in->has_workshop)
        {
            out->workshop = real_rooms_and_corridors.at(in->workshop);
        }
        else
        {
            out->workshop = nullptr;
        }

        out->stock_disable = in->stock_disable;
        out->stock_specific1 = in->stock_specific1;
        out->stock_specific2 = in->stock_specific2;

        out->has_users = in->has_users;
        out->temporary = in->temporary;
        out->outdoor = in->outdoor;
    }

    std::partition_copy(real_rooms_and_corridors.begin(), real_rooms_and_corridors.end(), real_corridors.end(), real_rooms.end(), [](room *r) -> bool { return r->type == room_type::corridor; });
}

bool blueprint_plan::add(const room_blueprint & rb, std::string & error)
{
    for (auto c : rb.corridor)
    {
        if (no_corridor.count(c))
        {
            error = "room corridor intersects no_corridor tile";
            return false;
        }
    }
    for (auto c : rb.interior)
    {
        if (no_room.count(c))
        {
            error = "room interior intersects no_room tile";
            return false;
        }
    }
    for (auto c : rb.no_corridor)
    {
        if (corridor.count(c))
        {
            error = "room no_corridor intersects corridor tile";
            return false;
        }
    }
    for (auto c : rb.no_room)
    {
        if (interior.count(c))
        {
            error = "room no_room intersects interior tile";
            return false;
        }
    }

    room_base::layoutindex_t layout_start = layout.size();
    room_base::roomindex_t room_start = rooms.size();

    for (auto f : rb.layout)
    {
        f = new room_base::furniture_t(*f);
        f->shift(layout_start, room_start);
        layout.push_back(f);
    }

    for (auto r : rb.rooms)
    {
        r = new room_base::room_t(*r);
        r->shift(layout_start, room_start);
        rooms.push_back(r);
        for (auto & exit : r->exits)
        {
            if (!no_room.count(r->min + exit.first))
            {
                room_connect[r->min + exit.first] = std::make_pair(rooms.size() - 1, exit.second);
            }
        }
    }

    for (auto c : rb.corridor)
    {
        corridor.insert(c);
    }

    for (auto c : rb.interior)
    {
        interior.insert(c);
    }

    for (auto c : rb.no_room)
    {
        no_room.insert(c);
        room_connect.erase(c);
    }

    for (auto c : rb.no_corridor)
    {
        no_corridor.insert(c);
    }

    return true;
}

bool blueprint_plan::add(const room_blueprint & rb, room_base::roomindex_t parent, std::string & error)
{
    parent += (~rooms.size()) + 1;

    room_blueprint rb_parent(rb);
    for (auto r : rb_parent.rooms)
    {
        r->accesspath.push_back(parent);
    }

    return add(rb_parent, error);
}

bool blueprint_plan::build(color_ostream & out, AI *ai, const blueprints_t & blueprints, const blueprint_plan_template & plan)
{
    std::map<std::string, size_t> counts;
    std::map<std::string, std::map<std::string, size_t>> instance_counts;

    std::vector<const room_blueprint *> available_blueprints;

    ai->debug(out, "Placing starting rooms...");
    for (;;)
    {
        available_blueprints.clear();
        find_available_blueprints_start(out, ai, available_blueprints, counts, instance_counts, blueprints, plan);
        if (available_blueprints.empty())
        {
            ai->debug(out, "No available blueprints.");
            break;
        }

        std::shuffle(available_blueprints.begin(), available_blueprints.end(), ai->rng);

        bool stop = false;
        size_t failures = 0;
        while (!stop)
        {
            for (auto rb : available_blueprints)
            {
                if (!try_add_room_start(out, ai, *rb, counts, instance_counts, plan))
                {
                    failures++;
                    ai->debug(out, stl_sprintf("Failed to place room %s/%s/%s. Failure count: %d of %d.", rb->type.c_str(), rb->tmpl_name.c_str(), rb->name.c_str(), failures, plan.max_failures));
                    if (failures >= plan.max_failures)
                    {
                        stop = true;
                        break;
                    }
                }
                else
                {
                    stop = true;
                    break;
                }
            }
        }
        if (failures >= plan.max_failures)
        {
            ai->debug(out, "Failed too many times in a row.");
            break;
        }
    }

    ai->debug(out, "Finished initial phase. Building remainder of fortress...");

    for (;;)
    {
        available_blueprints.clear();
        find_available_blueprints_connect(out, ai, available_blueprints, counts, instance_counts, blueprints, plan);
        if (available_blueprints.empty())
        {
            ai->debug(out, "No available blueprints.");
            break;
        }

        std::shuffle(available_blueprints.begin(), available_blueprints.end(), ai->rng);

        bool stop = false;
        size_t failures = 0;
        while (!stop)
        {
            for (auto rb : available_blueprints)
            {
                if (!try_add_room_connect(out, ai, *rb, counts, instance_counts, plan))
                {
                    failures++;
                    ai->debug(out, stl_sprintf("Failed to place room %s/%s. Failure count: %d of %d.", rb->type.c_str(), rb->name.c_str(), failures, plan.max_failures));
                    if (failures >= plan.max_failures)
                    {
                        stop = true;
                        break;
                    }
                }
                else
                {
                    stop = true;
                    break;
                }
            }
        }
        if (failures >= plan.max_failures)
        {
            ai->debug(out, "Failed too many times in a row.");
            break;
        }
    }

    if (!plan.have_minimum_requirements(out, ai, counts, instance_counts))
    {
        ai->debug(out, "Cannot place rooms, but minimum requirements were not met.");
        return false;
    }
    return true;
}

void blueprint_plan::clear()
{
    for (auto f : layout)
    {
        delete f;
    }
    layout.clear();

    for (auto r : rooms)
    {
        delete r;
    }
    rooms.clear();

    room_connect.clear();
    corridor.clear();
    interior.clear();
    no_room.clear();
    no_corridor.clear();
}

void blueprint_plan::find_available_blueprints(color_ostream & out, AI *ai, std::vector<const room_blueprint *> & available_blueprints, const std::map<std::string, size_t> & counts, const std::map<std::string, std::map<std::string, size_t>> & instance_counts, const blueprints_t & blueprints, const blueprint_plan_template & plan, const std::set<std::string> & available_tags, const std::function<bool(const room_blueprint &)> & check)
{
    const static std::map<std::string, size_t> no_instance_counts;
    const static std::map<std::string, std::pair<size_t, size_t>> no_instance_limits;

    for (auto & type : blueprints.blueprints)
    {
        if (!available_tags.count(type.first))
        {
            continue;
        }

        auto type_limit = plan.limits.find(type.first);
        auto type_count_it = counts.find(type.first);
        size_t type_count = type_count_it == counts.end() ? 0 : type_count_it->second;
        if (type_limit != plan.limits.end() && type_count >= type_limit->second.second)
        {
            continue;
        }

        auto instance_limits_it = plan.instance_limits.find(type.first);
        if (type_limit == plan.limits.end() && instance_limits_it == plan.instance_limits.end())
        {
            continue;
        }
        const auto & instance_limits = instance_limits_it == plan.instance_limits.end() ? no_instance_limits : instance_limits_it->second;

        auto type_instance_counts_it = instance_counts.find(type.first);
        const auto & type_instance_counts = type_instance_counts_it == instance_counts.end() ? no_instance_counts : type_instance_counts_it->second;

        for (auto & rb : type.second)
        {
            auto instance_limit = instance_limits.find(rb->name);
            auto instance_count_it = type_instance_counts.find(rb->name);
            size_t instance_count = instance_count_it == type_instance_counts.end() ? 0 : instance_count_it->second;

            if (type_limit == plan.limits.end() && instance_limit == instance_limits.end())
            {
                continue;
            }

            if (instance_limit != instance_limits.end() && instance_count >= instance_limit->second.second)
            {
                continue;
            }

            if (check(*rb))
            {
                available_blueprints.push_back(rb);
            }
        }
    }
}

void blueprint_plan::find_available_blueprints_start(color_ostream & out, AI *ai, std::vector<const room_blueprint *> & available_blueprints, const std::map<std::string, size_t> & counts, const std::map<std::string, std::map<std::string, size_t>> & instance_counts, const blueprints_t & blueprints, const blueprint_plan_template & plan)
{
    find_available_blueprints(out, ai, available_blueprints, counts, instance_counts, blueprints, plan, plan.start, [](const room_blueprint & rb) -> bool
    {
        for (auto r : rb.rooms)
        {
            if (!r->outdoor)
            {
                return false;
            }
        }

        return true;
    });
}

void blueprint_plan::find_available_blueprints_connect(color_ostream & out, AI *ai, std::vector<const room_blueprint *> & available_blueprints, const std::map<std::string, size_t> & counts, const std::map<std::string, std::map<std::string, size_t>> & instance_counts, const blueprints_t & blueprints, const blueprint_plan_template & plan)
{
    std::set<std::string> available_tags;
    for (auto & c : room_connect)
    {
        for (auto & tag : c.second.second)
        {
            available_tags.insert(tag);
            auto it = plan.tags.find(tag);
            if (it != plan.tags.end())
            {
                for (auto & t : it->second)
                {
                    available_tags.insert(t);
                }
            }
        }
    }

    find_available_blueprints(out, ai, available_blueprints, counts, instance_counts, blueprints, plan, available_tags, [](const room_blueprint &) -> bool { return true; });
}

bool blueprint_plan::try_add_room_start(color_ostream & out, AI *ai, const room_blueprint & rb, std::map<std::string, size_t> & counts, std::map<std::string, std::map<std::string, size_t>> & instance_counts, const blueprint_plan_template & plan)
{
    ai->debug(out, "TODO: try_add_room_start");
    // TODO
    return false;
}

bool blueprint_plan::try_add_room_connect(color_ostream & out, AI *ai, const room_blueprint & rb, std::map<std::string, size_t> & counts, std::map<std::string, std::map<std::string, size_t>> & instance_counts, const blueprint_plan_template & plan)
{
    ai->debug(out, "TODO: try_add_room_connect");
    // TODO
    return false;
}

bool blueprint_plan_template::apply(Json::Value data, std::string & error)
{
    if (data.isMember("start"))
    {
        Json::Value value = data.removeMember("start");
        if (!value.isArray() || std::find_if(value.begin(), value.end(), [](Json::Value & v) -> bool { return !v.isString(); }) != value.end())
        {
            error = "start has wrong type (should be array of strings)";
            return false;
        }

        for (auto & v : value)
        {
            if (!start.insert(v.asString()).second)
            {
                error = "duplicate start tag: " + v.asString();
                return false;
            }
        }
    }

    if (data.isMember("tags"))
    {
        Json::Value value = data.removeMember("tags");
        if (!value.isObject())
        {
            error = "tags has wrong type (should be object)";
            return false;
        }

        auto tag_names = value.getMemberNames();
        for (auto & tag_name : tag_names)
        {
            Json::Value & tag = value[tag_name];
            if (!tag.isArray() || std::find_if(tag.begin(), tag.end(), [](Json::Value & v) -> bool { return !v.isString(); }) != tag.end())
            {
                error = "tag " + tag_name + " has wrong type (should be array of strings)";
                return false;
            }
            auto & tag_set = tags[tag_name];
            for (auto & v : tag)
            {
                if (!tag_set.insert(v.asString()).second)
                {
                    error = "duplicate tag: " + tag_name + " -> " + v.asString();
                    return false;
                }
            }
        }
    }

    if (data.isMember("limits"))
    {
        Json::Value value = data.removeMember("limits");
        if (value.isObject())
        {
            error = "limits has wrong type (should be object)";
            return false;
        }

        auto limit_names = value.getMemberNames();
        for (auto & limit_name : limit_names)
        {
            Json::Value & limit = value[limit_name];
            if (!limit.isArray() || limit.size() != 2 || !limit[0].isIntegral() || !limit[1].isIntegral())
            {
                error = "limit " + limit_name + " has wrong type (should be [integer, integer])";
                return false;
            }

            if (limit[0].asInt() < 0)
            {
                error = "limit " + limit_name + " has invalid minimum";
                return false;
            }

            if (limit[1].asInt() < limit[0].asInt())
            {
                error = "limit " + limit_name + " has invalid maximum";
                return false;
            }

            limits[limit_name] = std::make_pair(size_t(limit[0].asInt()), size_t(limit[1].asInt()));
        }
    }

    if (data.isMember("instance_limits"))
    {
        Json::Value value = data.removeMember("instance_limits");
        if (value.isObject())
        {
            error = "instance_limits has wrong type (should be object)";
            return false;
        }

        auto type_names = value.getMemberNames();
        for (auto & type_name : type_names)
        {
            Json::Value & type = value[type_name];
            if (!type.isObject())
            {
                error = "instance_limits " + type_name + " has wrong type (should be object)";
                return false;
            }

            auto & limits = instance_limits[type_name];
            auto instance_names = type.getMemberNames();
            for (auto & instance_name : instance_names)
            {
                Json::Value & limit = type[instance_name];
                if (!limit.isArray() || limit.size() != 2 || !limit[0].isIntegral() || !limit[1].isIntegral())
                {
                    error = "instance_limit " + type_name + " " + instance_name + " has wrong type (should be [integer, integer])";
                    return false;
                }

                if (limit[0].asInt() < 0)
                {
                    error = "instance_limit " + type_name + " " + instance_name + " has invalid minimum";
                    return false;
                }

                if (limit[1].asInt() < limit[0].asInt())
                {
                    error = "instance_limit " + type_name + " " + instance_name + " has invalid maximum";
                    return false;
                }

                limits[instance_name] = std::make_pair(size_t(limit[0].asInt()), size_t(limit[1].asInt()));
            }
        }
    }

    std::vector<std::string> remaining_members(data.getMemberNames());
    if (remaining_members.empty())
    {
        return true;
    }

    error = "";
    const char *before = "unhandled properties: ";
    for (auto & m : remaining_members)
    {
        error += before;
        error += m;
        before = ", ";
    }
    return false;
}

bool blueprint_plan_template::have_minimum_requirements(color_ostream & out, AI *ai, const std::map<std::string, size_t> & counts, const std::map<std::string, std::map<std::string, size_t>> & instance_counts) const
{
    bool ok = true;

    for (auto & limit : limits)
    {
        auto type = counts.find(limit.first);
        if (type == counts.end())
        {
            if (limit.second.first > 0)
            {
                ai->debug(out, stl_sprintf("Requirement not met: have 0 %s but want between %d and %d.", limit.first.c_str(), limit.second.first, limit.second.second));
                ok = false;
            }
            else
            {
                ai->debug(out, stl_sprintf("have 0 %s (want between %d and %d)", limit.first.c_str(), limit.second.first, limit.second.second));
            }
        }
        else
        {
            if (limit.second.first > type->second)
            {
                ai->debug(out, stl_sprintf("Requirement not met: have %d %s but want between %d and %d.", type->second, limit.first.c_str(), limit.second.first, limit.second.second));
                ok = false;
            }
            else
            {
                ai->debug(out, stl_sprintf("have %d %s (want between %d and %d)", type->second, limit.first.c_str(), limit.second.first, limit.second.second));
            }
        }
    }

    for (auto & type_limits : instance_limits)
    {
        auto type_counts = instance_counts.find(type_limits.first);
        if (type_counts == instance_counts.end())
        {
            for (auto & limit : type_limits.second)
            {
                if (limit.second.first > 0)
                {
                    ai->debug(out, stl_sprintf("Requirement not met: have 0 %s/%s but want between %d and %d.", type_limits.first.c_str(), limit.first.c_str(), limit.second.first, limit.second.second));
                    ok = false;
                }
                else
                {
                    ai->debug(out, stl_sprintf("have 0 %s/%s (want between %d and %d)", type_limits.first.c_str(), limit.first.c_str(), limit.second.first, limit.second.second));
                }
            }
        }
        else
        {
            for (auto & limit : type_limits.second)
            {
                auto count = type_counts->second.find(limit.first);
                if (count == type_counts->second.end())
                {
                    if (limit.second.first > 0)
                    {
                        ai->debug(out, stl_sprintf("Requirement not met: have 0 %s/%s but want between %d and %d.", type_limits.first.c_str(), limit.first.c_str(), limit.second.first, limit.second.second));
                        ok = false;
                    }
                    else
                    {
                        ai->debug(out, stl_sprintf("have 0 %s/%s (want between %d and %d)", type_limits.first.c_str(), limit.first.c_str(), limit.second.first, limit.second.second));
                    }
                }
                else
                {
                    if (limit.second.first > count->second)
                    {
                        ai->debug(out, stl_sprintf("Requirement not met: have %d %s/%s but want between %d and %d.", count->second, type_limits.first.c_str(), limit.first.c_str(), limit.second.first, limit.second.second));
                        ok = false;
                    }
                    else
                    {
                        ai->debug(out, stl_sprintf("have %d %s/%s (want between %d and %d)", count->second, type_limits.first.c_str(), limit.first.c_str(), limit.second.first, limit.second.second));
                    }
                }
            }
        }
    }

    return ok;
}

template<typename T>
static T *load_json(const std::string & filename, const std::string & type, const std::string & name, std::string & error)
{
    error.clear();
    std::ifstream f(filename);
    Json::Value val;
    Json::CharReaderBuilder b;
    if (!Json::parseFromStream(b, f, &val, &error))
    {
    }
    else if (f.good())
    {
        if (val.isObject())
        {
            T *t = new T(type, name);
            if (t->apply(val, error))
            {
                return t;
            }
            delete t;
        }
        else
        {
            error = "must be a JSON object";
        }
    }
    else
    {
        error = "error reading file";
    }
    error = filename + ": " + error;
    return nullptr;
}

blueprints_t::blueprints_t(color_ostream & out)
{
    std::string error;

    std::map<std::string, std::pair<std::vector<std::pair<std::string, room_template *>>, std::vector<std::pair<std::string, room_instance *>>>> rooms;

    std::vector<std::string> types;
    if (!Filesystem::listdir("df-ai-blueprints/rooms/templates", types))
    {
        for (auto & type : types)
        {
            if (type.find('.') != std::string::npos)
            {
                continue;
            }

            std::vector<std::string> names;
            if (!Filesystem::listdir("df-ai-blueprints/rooms/templates/" + type, names))
            {
                for (auto & name : names)
                {
                    auto ext = name.rfind(".json");
                    if (ext == std::string::npos || ext != name.size() - strlen(".json"))
                    {
                        continue;
                    }

                    std::string path = "df-ai-blueprints/rooms/templates/" + type + "/" + name;

                    if (auto inst = load_json<room_template>(path, type, name.substr(0, ext), error))
                    {
                        rooms[type].first.push_back(std::make_pair(path, inst));
                    }
                    else
                    {
                        out.printerr("%s\n", error.c_str());
                    }
                }
            }
        }
    }
    types.clear();
    if (!Filesystem::listdir("df-ai-blueprints/rooms/instances", types))
    {
        for (auto & type : types)
        {
            if (type.find('.') != std::string::npos)
            {
                continue;
            }

            std::vector<std::string> names;
            if (!Filesystem::listdir("df-ai-blueprints/rooms/instances/" + type, names))
            {
                for (auto & name : names)
                {
                    auto ext = name.rfind(".json");
                    if (ext == std::string::npos || ext != name.size() - strlen(".json"))
                    {
                        continue;
                    }

                    std::string path = "df-ai-blueprints/rooms/instances/" + type + "/" + name;

                    if (auto inst = load_json<room_instance>(path, type, name.substr(0, ext), error))
                    {
                        rooms[type].second.push_back(std::make_pair(path, inst));
                    }
                    else
                    {
                        out.printerr("%s\n", error.c_str());
                    }
                }
            }
        }
    }

    for (auto & type : rooms)
    {
        auto & rbs = blueprints[type.first];
        rbs.reserve(type.second.first.size() * type.second.second.size());
        for (auto tmpl : type.second.first)
        {
            for (auto inst : type.second.second)
            {
                room_blueprint *rb = new room_blueprint(tmpl.second, inst.second);
                if (!rb->apply(error))
                {
                    out.printerr("%s + %s: %s\n", tmpl.first.c_str(), inst.first.c_str(), error.c_str());
                    delete rb;
                    continue;
                }
                rbs.push_back(rb);
            }
            delete tmpl.second;
        }
        for (auto inst : type.second.second)
        {
            delete inst.second;
        }
    }

    std::vector<std::string> names;
    if (!Filesystem::listdir("df-ai-blueprints/plans", names))
    {
        for (auto & name : names)
        {
            auto ext = name.rfind(".json");
            if (ext == std::string::npos || ext != name.size() - strlen(".json"))
            {
                continue;
            }

            std::string path = "df-ai-blueprints/plans/" + name;

            if (auto plan = load_json<blueprint_plan_template>(path, "plan", name.substr(0, ext), error))
            {
                plans[name.substr(0, ext)] = plan;
            }
            else
            {
                out.printerr("%s\n", error.c_str());
            }
        }
    }
}

blueprints_t::~blueprints_t()
{
    for (auto & type : blueprints)
    {
        for (auto rb : type.second)
        {
            delete rb;
        }
    }

    for (auto & plan : plans)
    {
        delete plan.second;
    }
}
