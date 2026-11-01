#pragma once

#include "gbtg/bundle.h"

#include <map>
#include <string>
#include <vector>

namespace gbtg
{

// Packs resources into a new BNL the way retail bundles are laid out: the file table sorted by name, then each
// resource's CPU bytes, its GPU table (chunk 1) and its GPU sub-chunks (chunk 2), each chunk packed in table order.
// Copied resources keep their header words; only the chunk offsets are rewritten.
class BundleBuilder
{
public:
    // Copies `entry` as it is loaded (bundle entries and loose resources alike), named `name` when given.
    void add(const BundleEntry& entry, std::string_view name = {});
    // A resource with CPU bytes only, such as a script, markers, an aidlist or objparams.
    void add_cpu(std::string_view name, ResourceType type, std::span<const u8> cpu);
    bool contains(std::string_view name) const;
    void remove(std::string_view name);
    size_t size() const
    {
        return resources_.size();
    }
    // The bundle in its loaded form: entries index its own heaps.
    Bundle build() const;

private:
    struct Resource
    {
        std::string name;
        ResourceHeader header;
        std::vector<u8> cpu;
        std::vector<std::vector<u8>> gpu;
    };
    std::map<std::string, Resource> resources_; // Key: the name in lower case, as AIDs fold case.
};

} // namespace gbtg
