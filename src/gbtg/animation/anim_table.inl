namespace
{

// Xbox 0x002A09B8..0x002A0E18, sorted 0x118-byte records.
constexpr std::array<AnimationRecord, 4> button_bat_records
{
    AnimationRecord{"doh", "aid_anim_ghoulies_props_buttonbat_doh",
                    "aid_animevents_ghoulies_props_buttonbat_doh", 0.f, 0.f, 0.2f, 0.2f, 1, 0x31, 0x21, 0},
    AnimationRecord{"flybetween", "aid_anim_ghoulies_props_buttonbat_flybetween",
                    "aid_animevents_ghoulies_props_buttonbat_flybetween", 0.f, 0.f, 0.f, 0.2f, 1, 0x31, 0x21, 0},
    AnimationRecord{"hop", "aid_anim_ghoulies_props_buttonbat_hop",
                    "aid_animevents_ghoulies_props_buttonbat_hop", 0.f, 0.f, 0.2f, 0.2f, 1, 0x31, 0x21, 0},
    AnimationRecord{"stand", "aid_anim_ghoulies_props_buttonbat_stand",
                    "aid_animevents_ghoulies_props_buttonbat_stand", 0.f, 0.f, 0.2f, 0.2f, 0, 0x31, 0x21, 0},
};
const AnimationTable button_bat_table{button_bat_records, {}};

} // namespace

// Xbox 0x00032180.
const AnimationRecord* find_animation_record(const AnimationTable& table, std::string_view name)
{
    int high = int(table.records.size()) - 1;
    int low = 0;
    if (-1 < high)
    {
        do
        {
            const int middle = (low + high) >> 1;
            const int comparison = table.records[middle].name.compare(name);
            if (comparison < 0)
            {
                low = middle + 1;
            }
            else
            {
                if (comparison == 0)
                {
                    return &table.records[middle];
                }
                high = middle - 1;
            }
        } while (low <= high);
    }
    return nullptr;
}

// Xbox 0x00032210.
const AnimationAlias* find_animation_alias(const AnimationTable& table, std::string_view name)
{
    int high = int(table.aliases.size()) - 1;
    int low = 0;
    if (-1 < high)
    {
        do
        {
            const int middle = (low + high) >> 1;
            const int comparison = table.aliases[middle].name.compare(name);
            if (comparison < 0)
            {
                low = middle + 1;
            }
            else
            {
                if (comparison < 1)
                {
                    return &table.aliases[middle];
                }
                high = middle - 1;
            }
        } while (low <= high);
    }
    return nullptr;
}

// Xbox 0x00032150.
const AnimationRecord* find_animation(const AnimationTable& table, std::string_view name)
{
    const AnimationAlias* alias = find_animation_alias(table, name);
    if (alias != nullptr)
    {
        return find_animation_record(table, alias->target);
    }
    return find_animation_record(table, name);
}

// Xbox 0x000322A0.
AnimationMix animation_mix(const AnimationRecord& record, bool retain_position)
{
    AnimationMix mix;
    mix.duration = record.duration;
    mix.blend_mode = record.blend_mode;
    mix.fade_duration = record.fade_duration;
    mix.mode = record.mode;
    mix.priority = record.priority;
    mix.old_pose_duration = record.old_pose_duration;
    mix.events = record.events;
    mix.record = &record;
    float position = -1.f;
    if (!retain_position)
    {
        position = record.position;
    }
    mix.position = position;
    return mix;
}

const AnimationTable& button_bat_animation_table()
{
    return button_bat_table;
}
namespace
{

// Xbox 0x000FBEA0. The host string owns the original character buffer.
void append_animation_number(std::string& output, i32 value)
{
    if (value != 0)
    {
        if (value < 0)
        {
            output.push_back('-');
            value = std::bit_cast<i32>(0u - u32(value));
        }
        std::array<char, 16> digits{};
        int count = 0;
        if (value != 0)
        {
            do
            {
                const i32 quotient = value / 10;
                digits[count] = char(value % 10 + '0');
                ++count;
                value = quotient;
            } while (value != 0);
            for (; 0 < count; --count)
            {
                output.push_back(digits[count - 1]);
            }
        }
        return;
    }
    output.push_back('0');
}

} // namespace

// Xbox 0x0003AAA0.
const AnimationRecord* find_actor_animation(const AnimationTable* table, const char* name,
                                            const char* variant, u32& random_state)
{
    if (table == nullptr || name == nullptr)
    {
        return nullptr;
    }
    if (variant != nullptr)
    {
        const std::string qualified = std::string(name) + "_[" + variant + "]";
        const AnimationAlias* alias = find_animation_alias(*table, qualified);
        if (alias == nullptr)
        {
            return find_animation_record(*table, qualified);
        }
        return find_animation_record(*table, alias->target);
    }
    const std::string_view requested{name};
    const size_t wildcard = requested.find('?');
    if (wildcard != std::string_view::npos)
    {
        const std::string prefix(requested.substr(0, wildcard));
        int count = 0;
        const AnimationRecord* record;
        do
        {
            std::string numbered = prefix;
            append_animation_number(numbered, count + 1);
            const AnimationAlias* alias = find_animation_alias(*table, numbered);
            if (alias == nullptr)
            {
                record = find_animation_record(*table, numbered);
            }
            else
            {
                record = find_animation_record(*table, alias->target);
            }
            if (record != nullptr)
            {
                ++count;
            }
        } while (record != nullptr);
        if (count == 0)
        {
            return nullptr;
        }
        random_state = random_state * 0x19660du + 0x3c6ef35fu;
        const float random = std::bit_cast<float>((random_state & 0x7fffffu) | 0x3f800000u) - 1.f;
        std::string numbered = prefix;
        append_animation_number(numbered, int(random * float(count)) + 1);
        const AnimationAlias* alias = find_animation_alias(*table, numbered);
        if (alias == nullptr)
        {
            return find_animation_record(*table, numbered);
        }
        return find_animation_record(*table, alias->target);
    }
    return find_animation(*table, requested);
}

