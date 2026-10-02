#include "gbtg/resource.hpp"

namespace gbtg
{

const char* resource_type_name(ResourceType type)
{
    switch (type)
    {
    case ResourceType::Texture:
        return "texture";
    case ResourceType::Anim:
        return "anim";
    case ResourceType::Unknown3:
        return "unknown3";
    case ResourceType::Model:
        return "model";
    case ResourceType::AnimEvents:
        return "animevents";
    case ResourceType::Cutscene:
        return "cutscene";
    case ResourceType::CutsceneEvents:
        return "cutsceneevents";
    case ResourceType::Misc:
        return "misc";
    case ResourceType::ActorGoals:
        return "actorgoals";
    case ResourceType::Marker:
        return "marker";
    case ResourceType::FxCallout:
        return "callout";
    case ResourceType::AidList:
        return "aidlist";
    case ResourceType::Loctext:
        return "loctext";
    case ResourceType::XSoundbank:
        return "xsoundbank";
    case ResourceType::XDsp:
        return "xdsp";
    case ResourceType::XCueList:
        return "xcuelist";
    case ResourceType::Font:
        return "font";
    case ResourceType::GhoulyBox:
        return "ghoulybox";
    case ResourceType::GhoulySpawn:
        return "ghoulyspawn";
    case ResourceType::Script:
        return "script";
    case ResourceType::ActorAttribs:
        return "actorattribs";
    case ResourceType::Emitter:
        return "emitter";
    case ResourceType::Particle:
        return "particle";
    case ResourceType::Rumble:
        return "rumble";
    case ResourceType::ShakeCam:
        return "shakecam";
    default:
        return "unknown";
    }
}

bool parse_resource_header(std::span<const u8> bytes, ResourceHeader& out)
{
    if (bytes.size() < 32)
    {
        return false;
    }
    out.type = static_cast<ResourceType>(read_u32(bytes, 0));
    out.unknown = read_u32(bytes, 4);
    out.magic = read_u32(bytes, 8);
    out.chunk_count = read_u32(bytes, 12);
    out.file_chunk.offset = read_u32(bytes, 16);
    out.file_chunk.size = read_u32(bytes, 20);
    out.resource_chunk.offset = read_u32(bytes, 24);
    out.resource_chunk.size = read_u32(bytes, 28);
    return true;
}

static std::vector<ResourceSubchunk> read_gpu_table(std::span<const u8> bytes, const ResourceHeader& h)
{
    std::vector<ResourceSubchunk> chunks;
    if (h.resource_chunk.size < 8)
    {
        return chunks;
    }
    const size_t off = h.resource_chunk.offset;
    if (off + 8 > bytes.size())
    {
        return chunks;
    }
    const u32 table_size = read_u32(bytes, off);
    const u32 count = read_u32(bytes, off + 4);
    if (count > 4096 || off + 8 + count * 8 > bytes.size())
    {
        return chunks;
    }
    (void)table_size;
    chunks.resize(count);
    for (u32 i = 0; i < count; ++i)
    {
        chunks[i].offset = read_u32(bytes, off + 8 + i * 8);
        chunks[i].size = read_u32(bytes, off + 12 + i * 8);
    }
    return chunks;
}

LooseResource parse_loose_resource(std::vector<u8> bytes)
{
    LooseResource r;
    r.owned = std::move(bytes);
    if (!parse_resource_header(r.owned, r.header))
    {
        throw Error("truncated resource header");
    }
    auto all = std::span<const u8>(r.owned);
    r.gpu_chunks = read_gpu_table(all, r.header);
    const auto& fc = r.header.file_chunk;
    const auto& rc = r.header.resource_chunk;
    if (fc.offset + fc.size <= r.owned.size())
    {
        r.cpu = all.subspan(fc.offset, fc.size);
    }
    if (rc.offset + rc.size <= r.owned.size())
    {
        r.gpu = all.subspan(rc.offset, rc.size);
    }
    return r;
}

} // namespace gbtg
