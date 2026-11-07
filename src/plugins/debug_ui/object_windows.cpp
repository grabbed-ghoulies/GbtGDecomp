// The native scene views (object_windows.h): factory objects, actors and per-object struct inspectors.
#include "object_windows.h"

#include <algorithm>
#include <cinttypes>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace debug_ui
{

namespace
{

ImGuiTextFilter g_object_filter;
ImGuiTextFilter g_actor_filter;
int g_object_type = -1;
bool g_objects_with_struct_only = false;

struct Inspector
{
    uint32_t address = 0;
    bool open = true;
    int schema = -1; // -1: the type's own struct; else an index of schema_at.
    bool show_unnamed = true;
    bool show_raw = false;
    bool focus = true; // Bring the window to the front on the next frame.
    ImGuiTextFilter filter;
};

std::vector<Inspector> g_inspectors;

void inspect(uint32_t address)
{
    for (Inspector& inspector : g_inspectors)
    {
        if (inspector.address == address)
        {
            inspector.open = true;
            inspector.focus = true;
            return;
        }
    }
    Inspector inspector;
    inspector.address = address;
    g_inspectors.push_back(std::move(inspector));
}

const char* kind_name(uint32_t kind)
{
    switch (kind)
    {
    case GBTG_FIELD_BOOL: return "bool";
    case GBTG_FIELD_U8: return "u8";
    case GBTG_FIELD_I8: return "i8";
    case GBTG_FIELD_U16: return "u16";
    case GBTG_FIELD_I16: return "i16";
    case GBTG_FIELD_U32: return "u32";
    case GBTG_FIELD_I32: return "i32";
    case GBTG_FIELD_U64: return "u64";
    case GBTG_FIELD_I64: return "i64";
    case GBTG_FIELD_F32: return "f32";
    case GBTG_FIELD_F64: return "f64";
    case GBTG_FIELD_VEC3: return "Vec3";
    case GBTG_FIELD_QUAT: return "Quat";
    case GBTG_FIELD_TEXT: return "char";
    case GBTG_FIELD_WTEXT: return "wchar";
    case GBTG_FIELD_POINTER: return "ptr";
    case GBTG_FIELD_GROUP: return "struct";
    default: return "bytes";
    }
}

uint32_t kind_size(uint32_t kind)
{
    switch (kind)
    {
    case GBTG_FIELD_BOOL:
    case GBTG_FIELD_U8:
    case GBTG_FIELD_I8:
    case GBTG_FIELD_TEXT:
    case GBTG_FIELD_BYTES:
        return 1;
    case GBTG_FIELD_U16:
    case GBTG_FIELD_I16:
    case GBTG_FIELD_WTEXT:
        return 2;
    case GBTG_FIELD_U32:
    case GBTG_FIELD_I32:
    case GBTG_FIELD_F32:
        return 4;
    case GBTG_FIELD_VEC3:
        return 12;
    case GBTG_FIELD_QUAT:
        return 16;
    case GBTG_FIELD_POINTER:
        return uint32_t(sizeof(void*));
    default:
        return 8;
    }
}

template <typename T>
T load(const uint8_t* data)
{
    T value;
    std::memcpy(&value, data, sizeof(T));
    return value;
}

// One element of a scalar kind as text.
std::string element_text(uint32_t kind, const uint8_t* data)
{
    char text[96];
    switch (kind)
    {
    case GBTG_FIELD_BOOL: return data[0] != 0 ? "true" : "false";
    case GBTG_FIELD_U8: std::snprintf(text, sizeof(text), "%u (0x%02X)", data[0], data[0]); break;
    case GBTG_FIELD_I8: std::snprintf(text, sizeof(text), "%d", int(load<int8_t>(data))); break;
    case GBTG_FIELD_U16:
    {
        const uint16_t v = load<uint16_t>(data);
        std::snprintf(text, sizeof(text), "%u (0x%X)", v, v);
        break;
    }
    case GBTG_FIELD_I16: std::snprintf(text, sizeof(text), "%d", int(load<int16_t>(data))); break;
    case GBTG_FIELD_U32:
    {
        const uint32_t v = load<uint32_t>(data);
        std::snprintf(text, sizeof(text), "%u (0x%08X)", v, v);
        break;
    }
    case GBTG_FIELD_I32: std::snprintf(text, sizeof(text), "%d", load<int32_t>(data)); break;
    case GBTG_FIELD_U64: std::snprintf(text, sizeof(text), "%" PRIu64, load<uint64_t>(data)); break;
    case GBTG_FIELD_I64: std::snprintf(text, sizeof(text), "%" PRId64, load<int64_t>(data)); break;
    case GBTG_FIELD_F32: std::snprintf(text, sizeof(text), "%.4g", double(load<float>(data))); break;
    case GBTG_FIELD_F64: std::snprintf(text, sizeof(text), "%.6g", load<double>(data)); break;
    case GBTG_FIELD_VEC3:
        std::snprintf(text, sizeof(text), "(%.3f, %.3f, %.3f)", double(load<float>(data)),
            double(load<float>(data + 4)), double(load<float>(data + 8)));
        break;
    case GBTG_FIELD_QUAT:
        std::snprintf(text, sizeof(text), "(%.3f, %.3f, %.3f, %.3f)", double(load<float>(data)),
            double(load<float>(data + 4)), double(load<float>(data + 8)), double(load<float>(data + 12)));
        break;
    case GBTG_FIELD_POINTER:
        std::snprintf(text, sizeof(text), "%p", load<void*>(data));
        break;
    default:
        std::snprintf(text, sizeof(text), "%02X", data[0]);
        break;
    }
    return text;
}

std::string hex_text(const uint8_t* data, uint32_t size, uint32_t limit)
{
    std::string text;
    char byte[4];
    for (uint32_t i = 0; i < std::min(size, limit); ++i)
    {
        std::snprintf(byte, sizeof(byte), i == 0 ? "%02X" : " %02X", data[i]);
        text += byte;
    }
    if (size > limit)
    {
        text += " ...";
    }
    return text;
}

std::string text_value(const GbtgField& field, const uint8_t* data)
{
    std::string text = "\"";
    for (uint32_t i = 0; i < field.count; ++i)
    {
        const uint32_t c = field.kind == GBTG_FIELD_TEXT ? data[i] : load<uint16_t>(data + i * 2);
        if (c == 0)
        {
            break;
        }
        text += c >= 0x20 && c < 0x7F ? char(c) : '?';
    }
    return text + "\"";
}

// The whole field's value on one line.
std::string value_text(const GbtgField& field)
{
    const auto* data = static_cast<const uint8_t*>(field.data);
    if (field.kind == GBTG_FIELD_BYTES)
    {
        return hex_text(data, field.size, 16);
    }
    if (field.kind == GBTG_FIELD_TEXT || field.kind == GBTG_FIELD_WTEXT)
    {
        return text_value(field, data);
    }
    if (field.kind == GBTG_FIELD_GROUP)
    {
        return field.count > 1 ? "[" + std::to_string(field.count) + "]" : "";
    }
    if (field.count == 1)
    {
        return element_text(field.kind, data);
    }
    const uint32_t stride = field.size / field.count;
    std::string text = "[";
    const uint32_t shown = std::min<uint32_t>(field.count, 4);
    for (uint32_t i = 0; i < shown; ++i)
    {
        text += (i > 0 ? ", " : "") + element_text(field.kind, data + i * stride);
    }
    return text + (field.count > shown ? ", ... ]" : "]");
}

// A field copied out of the visit, which only lends its strings and data.
struct FieldRow
{
    std::string name;
    std::string note;
    std::string type_name;
    GbtgField field{};
};

void collect_field(void* context, const GbtgField* field)
{
    auto& rows = *static_cast<std::vector<FieldRow>*>(context);
    FieldRow row;
    row.name = field->name;
    row.note = field->note;
    row.type_name = field->type_name;
    row.field = *field;
    rows.push_back(std::move(row));
}

std::string type_label(const FieldRow& row)
{
    const GbtgField& field = row.field;
    if (field.kind == GBTG_FIELD_GROUP)
    {
        return row.type_name;
    }
    std::string label = kind_name(field.kind);
    if (field.kind != GBTG_FIELD_BYTES && field.count > 1)
    {
        label += "[" + std::to_string(field.count) + "]";
    }
    else if (field.kind == GBTG_FIELD_BYTES)
    {
        label += "[0x" + [&]
        {
            char text[16];
            std::snprintf(text, sizeof(text), "%X", field.size);
            return std::string(text);
        }() + "]";
    }
    return label;
}

// The rows under an expanded array or long byte run, one element or 16 bytes per line.
void draw_elements(const GbtgField& field)
{
    const auto* data = static_cast<const uint8_t*>(field.data);
    if (field.kind == GBTG_FIELD_BYTES)
    {
        for (uint32_t at = 0; at < field.size; at += 16)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("+%04X", field.offset + at);
            ImGui::TableNextColumn();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(hex_text(data + at, std::min<uint32_t>(16, field.size - at), 16).c_str());
            ImGui::TableNextColumn();
        }
        return;
    }
    const uint32_t stride = field.size / field.count;
    for (uint32_t i = 0; i < field.count; ++i)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("+%04X", field.offset + i * stride);
        ImGui::TableNextColumn();
        ImGui::Text("[%u]", i);
        ImGui::TableNextColumn();
        ImGui::TextUnformatted(element_text(field.kind, data + i * stride).c_str());
        ImGui::TableNextColumn();
    }
}

bool expandable(const GbtgField& field)
{
    if (field.kind == GBTG_FIELD_GROUP)
    {
        return true;
    }
    if (field.kind == GBTG_FIELD_BYTES)
    {
        return field.size > 16;
    }
    if (field.kind == GBTG_FIELD_TEXT || field.kind == GBTG_FIELD_WTEXT)
    {
        return false;
    }
    return field.count > 4 && field.kind < GBTG_FIELD_GROUP && field.size / field.count == kind_size(field.kind);
}

void draw_field_table(const std::vector<FieldRow>& rows, const Inspector& inspector)
{
    const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_Resizable;
    if (!ImGui::BeginTable("fields", 4, flags, ImVec2(0.f, 0.f)))
    {
        return;
    }
    ImGui::TableSetupColumn("Offset", ImGuiTableColumnFlags_WidthFixed, 56.f);
    ImGui::TableSetupColumn("Field", ImGuiTableColumnFlags_WidthStretch, 0.9f);
    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 1.4f);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 0.5f);
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();

    const bool filtering = inspector.filter.IsActive();
    int open_depth = 0; // Tree nodes pushed.
    int skip_below = -1; // Rows deeper than this are inside a closed group.
    for (size_t i = 0; i < rows.size(); ++i)
    {
        const FieldRow& row = rows[i];
        const GbtgField& field = row.field;
        const int depth = int(field.depth);
        if (skip_below >= 0)
        {
            if (depth > skip_below)
            {
                continue;
            }
            skip_below = -1;
        }
        while (open_depth > depth)
        {
            ImGui::TreePop();
            --open_depth;
        }
        const bool unnamed = row.name.empty();
        if (unnamed && !inspector.show_unnamed)
        {
            continue;
        }
        if (filtering && field.kind != GBTG_FIELD_GROUP && !inspector.filter.PassFilter(row.name.c_str()))
        {
            continue;
        }
        ImGui::PushID(int(i));
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("+%04X", field.offset);
        ImGui::TableNextColumn();
        const char* label = unnamed ? "(unnamed)" : row.name.c_str();
        bool opened = false;
        if (expandable(field))
        {
            ImGuiTreeNodeFlags node = ImGuiTreeNodeFlags_SpanFullWidth;
            if (filtering && field.kind == GBTG_FIELD_GROUP)
            {
                node |= ImGuiTreeNodeFlags_DefaultOpen;
            }
            if (unnamed)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            }
            opened = ImGui::TreeNodeEx(label, node);
            if (unnamed)
            {
                ImGui::PopStyleColor();
            }
        }
        else
        {
            if (unnamed)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            }
            ImGui::TreeNodeEx(label, ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen |
                ImGuiTreeNodeFlags_SpanFullWidth);
            if (unnamed)
            {
                ImGui::PopStyleColor();
            }
        }
        if (!row.note.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            ImGui::SetNextWindowSize(ImVec2(420.f, 0.f));
            ImGui::BeginTooltip();
            ImGui::TextWrapped("%s", row.note.c_str());
            ImGui::EndTooltip();
        }
        ImGui::TableNextColumn();
        const std::string value = value_text(field);
        if (unnamed)
        {
            ImGui::TextDisabled("%s", value.c_str());
        }
        else
        {
            ImGui::TextUnformatted(value.c_str());
        }
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%s", type_label(row).c_str());
        ImGui::PopID();
        if (field.kind == GBTG_FIELD_GROUP)
        {
            if (opened)
            {
                ++open_depth;
            }
            else
            {
                skip_below = depth;
            }
        }
        else if (opened)
        {
            draw_elements(field);
            ImGui::TreePop();
        }
    }
    while (open_depth > 0)
    {
        ImGui::TreePop();
        --open_depth;
    }
    ImGui::EndTable();
}

void draw_raw_bytes(const GbtgObjectView& view)
{
    ImGui::BeginChild("raw", ImVec2(0.f, 160.f), ImGuiChildFlags_Borders);
    ImGuiListClipper clipper;
    clipper.Begin(int((view.bytes + 15) / 16));
    while (clipper.Step())
    {
        for (int line = clipper.DisplayStart; line < clipper.DisplayEnd; ++line)
        {
            const uint32_t at = uint32_t(line) * 16;
            ImGui::Text("+%04X  %s", at, hex_text(view.data + at, std::min<uint32_t>(16, view.bytes - at), 16).c_str());
        }
    }
    ImGui::EndChild();
}

void draw_inspector(const GbtgPluginHost* host, const GbtgGame* game, Inspector& inspector, ImGuiWindowFlags flags)
{
    char title[64];
    std::snprintf(title, sizeof(title), "Object %08X###inspect%08X", inspector.address, inspector.address);
    ImGui::SetNextWindowSize(ImVec2(620.f, 520.f), ImGuiCond_FirstUseEver);
    if (inspector.focus)
    {
        ImGui::SetNextWindowFocus();
        inspector.focus = false;
    }
    if (!ImGui::Begin(title, &inspector.open, flags))
    {
        ImGui::End();
        return;
    }
    GbtgObjectView view{};
    view.size = sizeof(view);
    if (host->object_view(game, inspector.address, &view) == 0)
    {
        ImGui::TextUnformatted("The object no longer exists (released, or the scene changed).");
        ImGui::End();
        return;
    }
    ImGui::Text("Type 0x%02X %s  (parent 0x%02X %s)", view.type, view.retail_name, view.parent_type,
        host->type_name(game, view.parent_type));
    ImGui::Text("0x%X bytes, vtable %08X%s", view.bytes, view.vtable, view.is_actor ? ", actor" : "");

    const size_t schema_count = host->schema_count();
    const char* chosen = inspector.schema >= 0 && size_t(inspector.schema) < schema_count
        ? host->schema_at(size_t(inspector.schema), nullptr) : nullptr;
    std::string preview = chosen != nullptr ? chosen
        : (view.schema[0] != '\0' ? std::string(view.schema) + " (its type's)" : "none (its type's)");
    ImGui::SetNextItemWidth(260.f);
    if (ImGui::BeginCombo("View as", preview.c_str(), ImGuiComboFlags_HeightLarge))
    {
        if (ImGui::Selectable("Its type's struct", inspector.schema < 0))
        {
            inspector.schema = -1;
        }
        for (size_t i = 0; i < schema_count; ++i)
        {
            uint32_t size = 0;
            const char* name = host->schema_at(i, &size);
            char item[128];
            std::snprintf(item, sizeof(item), "%s  (0x%X)", name, size);
            const bool fits = size <= view.bytes;
            if (!fits)
            {
                ImGui::BeginDisabled();
            }
            if (ImGui::Selectable(item, inspector.schema == int(i)))
            {
                inspector.schema = int(i);
            }
            if (!fits)
            {
                ImGui::EndDisabled();
            }
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Unnamed bytes", &inspector.show_unnamed);
    ImGui::SameLine();
    ImGui::Checkbox("Raw", &inspector.show_raw);
    inspector.filter.Draw("Field filter", 200.f);
    if (inspector.show_raw)
    {
        draw_raw_bytes(view);
    }

    std::vector<FieldRow> rows;
    host->object_fields(game, inspector.address, chosen, collect_field, &rows);
    uint32_t named = 0;
    uint32_t total = 0;
    for (const FieldRow& row : rows)
    {
        if (row.field.depth == 0)
        {
            total += row.field.size;
            // unk_ fields are declared but not understood yet.
            named += row.name.empty() || row.name.rfind("unk_", 0) == 0 ? 0 : row.field.size;
        }
    }
    if (total > 0)
    {
        ImGui::ProgressBar(float(named) / float(total), ImVec2(-1.f, 0.f),
            (std::to_string(named * 100 / total) + "% of the bytes named (unk_ and gaps excluded)").c_str());
    }
    draw_field_table(rows, inspector);
    ImGui::End();
}

struct ObjectTableContext
{
    const GbtgPluginHost* host;
    const GbtgGame* game;
};

} // namespace

bool has_object_views(const GbtgPluginHost* host)
{
    return host->size >= offsetof(GbtgPluginHost, actors) + sizeof(host->actors);
}

void draw_object_table(const GbtgPluginHost* host, const GbtgGame* game, int has_native_scene, bool* open,
    ImGuiWindowFlags flags)
{
    ImGui::SetNextWindowSize(ImVec2(760.f, 460.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Native objects", open, flags))
    {
        ImGui::End();
        return;
    }
    if (!has_native_scene)
    {
        ImGui::TextUnformatted("No native scene factory bound.");
        ImGui::End();
        return;
    }
    const bool views = has_object_views(host);
    ImGui::SetNextItemWidth(120.f);
    ImGui::InputInt("Type (-1 = all)", &g_object_type, 1, 16, ImGuiInputTextFlags_CharsHexadecimal);
    ImGui::SameLine();
    ImGui::Checkbox("Named struct only", &g_objects_with_struct_only);
    g_object_filter.Draw("Filter (name or struct)", 260.f);
    ImGui::TextDisabled("Factory ownership; addresses are retail tokens.%s", views ? " Click a row to inspect it." : "");
    if (ImGui::BeginTable("objects", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_Resizable, ImVec2(0.f, 0.f)))
    {
        ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 72.f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 36.f);
        ImGui::TableSetupColumn("Retail name");
        ImGui::TableSetupColumn("Struct");
        ImGui::TableSetupColumn("Bytes", ImGuiTableColumnFlags_WidthFixed, 56.f);
        ImGui::TableSetupColumn("Vtable", ImGuiTableColumnFlags_WidthFixed, 72.f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        ObjectTableContext context{host, game};
        host->native_objects(game, [](void* raw, const GbtgNativeObject* object)
        {
            auto& context = *static_cast<ObjectTableContext*>(raw);
            if (g_object_type >= 0 && object->type != uint32_t(g_object_type))
            {
                return;
            }
            const bool views = has_object_views(context.host);
            const char* name = views ? context.host->type_name(context.game, object->type) : "";
            const char* schema = views ? context.host->type_schema(context.game, object->type) : "";
            if (g_objects_with_struct_only && schema[0] == '\0')
            {
                return;
            }
            if (g_object_filter.IsActive() && !g_object_filter.PassFilter(name) && !g_object_filter.PassFilter(schema))
            {
                return;
            }
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            char address[16];
            std::snprintf(address, sizeof(address), "%08X", object->address);
            ImGui::PushID(int(object->address));
            if (ImGui::Selectable(address, false, ImGuiSelectableFlags_SpanAllColumns) && views)
            {
                inspect(object->address);
            }
            ImGui::PopID();
            ImGui::TableNextColumn();
            ImGui::Text("%02X", object->type);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(name);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(schema);
            ImGui::TableNextColumn();
            ImGui::Text("0x%X", object->size);
            ImGui::TableNextColumn();
            ImGui::Text("%08X", object->vtable);
        }, &context);
        ImGui::EndTable();
    }
    ImGui::End();
}

void draw_actor_table(const GbtgPluginHost* host, const GbtgGame* game, bool* open, ImGuiWindowFlags flags)
{
    ImGui::SetNextWindowSize(ImVec2(900.f, 420.f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Actors", open, flags))
    {
        ImGui::End();
        return;
    }
    g_actor_filter.Draw("Filter (name)", 260.f);
    ImGui::TextDisabled("The scene's native actors. Click a row to inspect its object; hover for more.");
    if (ImGui::BeginTable("actors", 9, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_Resizable, ImVec2(0.f, 0.f)))
    {
        ImGui::TableSetupColumn("Address", ImGuiTableColumnFlags_WidthFixed, 72.f);
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Class", ImGuiTableColumnFlags_WidthFixed, 44.f);
        ImGui::TableSetupColumn("Marker", ImGuiTableColumnFlags_WidthFixed, 80.f);
        ImGui::TableSetupColumn("Position");
        ImGui::TableSetupColumn("Euler");
        ImGui::TableSetupColumn("Age", ImGuiTableColumnFlags_WidthFixed, 56.f);
        ImGui::TableSetupColumn("Model", ImGuiTableColumnFlags_WidthFixed, 64.f);
        ImGui::TableSetupColumn("Attached", ImGuiTableColumnFlags_WidthFixed, 72.f);
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableHeadersRow();
        host->actors(game, [](void*, const GbtgActorInfo* actor)
        {
            if (g_actor_filter.IsActive() && !g_actor_filter.PassFilter(actor->retail_name))
            {
                return;
            }
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            char address[16];
            std::snprintf(address, sizeof(address), "%08X", actor->address);
            ImGui::PushID(int(actor->address));
            if (ImGui::Selectable(address, false, ImGuiSelectableFlags_SpanAllColumns))
            {
                inspect(actor->address);
            }
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            {
                ImGui::BeginTooltip();
                ImGui::Text("Ticks %u, animation rate %.3f", actor->ticks, double(actor->animation_rate));
                ImGui::Text("Model mode %d, shape mode %d, model flags %08X", actor->model_mode, actor->shape_mode,
                    actor->model_flags);
                ImGui::Text("Damage class %u", actor->damage_class);
                ImGui::EndTooltip();
            }
            ImGui::PopID();
            ImGui::TableNextColumn();
            ImGui::Text("%02X %s", actor->type, actor->retail_name);
            ImGui::TableNextColumn();
            ImGui::Text("%X", actor->class_key);
            ImGui::TableNextColumn();
            ImGui::Text("%X -> %X", actor->marker_id, actor->marker_next);
            ImGui::TableNextColumn();
            ImGui::Text("%.2f %.2f %.2f", double(actor->position[0]), double(actor->position[1]),
                double(actor->position[2]));
            ImGui::TableNextColumn();
            ImGui::Text("%.2f %.2f %.2f", double(actor->euler[0]), double(actor->euler[1]), double(actor->euler[2]));
            ImGui::TableNextColumn();
            ImGui::Text("%.1f", double(actor->age));
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(actor->has_model ? "yes" : "-");
            ImGui::TableNextColumn();
            if (actor->attached_to != 0)
            {
                ImGui::Text("%08X", actor->attached_to);
            }
            else
            {
                ImGui::TextUnformatted("-");
            }
        }, nullptr);
        ImGui::EndTable();
    }
    ImGui::End();
}

void draw_inspectors(const GbtgPluginHost* host, const GbtgGame* game, ImGuiWindowFlags flags)
{
    for (Inspector& inspector : g_inspectors)
    {
        draw_inspector(host, game, inspector, flags);
    }
    g_inspectors.erase(std::remove_if(g_inspectors.begin(), g_inspectors.end(), [](const Inspector& inspector)
    {
        return !inspector.open;
    }), g_inspectors.end());
}

} // namespace debug_ui
