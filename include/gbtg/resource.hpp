#pragma once

#include "gbtg/types.hpp"

namespace gbtg {

enum class ResourceType : u32 {
    Texture = 1,
    Anim = 2,
    Unknown3 = 3,
    Model = 4,
    AnimEvents = 5,
    Cutscene = 7,
    CutsceneEvents = 8,
    Misc = 10,
    ActorGoals = 11,
    Marker = 12,
    FxCallout = 13,
    AidList = 14,
    Loctext = 16,
    XSoundbank = 18,
    XDsp = 19,
    XCueList = 20,
    Font = 21,
    GhoulyBox = 22,
    GhoulySpawn = 23,
    Script = 24,
    ActorAttribs = 25,
    Emitter = 26,
    Particle = 27,
    Rumble = 28,
    ShakeCam = 29,
};

constexpr u32 kResourceMagic = 0x48E58836u;

const char* resource_type_name(ResourceType type);

struct ChunkRef {
    u32 offset = 0;
    u32 size = 0;
};

struct ResourceHeader {
    ResourceType type{};
    u32 unknown = 0;
    u32 magic = 0;
    u32 chunk_count = 0;
    ChunkRef file_chunk;
    ChunkRef resource_chunk;
};

struct ResourceSubchunk {
    u32 offset = 0;
    u32 size = 0;
};

bool parse_resource_header(std::span<const u8> bytes, ResourceHeader& out);

struct LooseResource {
    ResourceHeader header;
    std::vector<ResourceSubchunk> gpu_chunks;
    std::span<const u8> cpu;  // file chunk
    std::span<const u8> gpu;  // resource payload
    std::vector<u8> owned;
};

LooseResource parse_loose_resource(std::vector<u8> bytes);

}  // namespace gbtg
