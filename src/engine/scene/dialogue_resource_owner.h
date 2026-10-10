#pragma once
#include "gbtg/aid.h"
#include "../frontend/dialogue_text_metrics.h"
#include "../../gbtg/text/localtext_resources.h"
#include <map>

namespace gbtg
{
// Native views and backing storage for the translated dialogue calls. The
// registry retains actual retail resources; no localized strings are invented.
class SceneDialogueResourceOwner
{
public:
    explicit SceneDialogueResourceOwner(AidStore& aids);
    SceneDialogueResourceOwner(const SceneDialogueResourceOwner&) = delete;
    SceneDialogueResourceOwner& operator=(const SceneDialogueResourceOwner&) = delete;
    AnimationResourceRegistry registry;
    ResourceErrorState error{};
    std::unordered_map<const u8*, LocalTextResourceView> text_views;
    LocalTextResourceContext text{registry, error, text_views, {}};
    std::array<RuntimeTextFont, 2> fonts{};
    std::array<const RuntimeTextFont*, 2> font_views{&fonts[0], &fonts[1]};
    std::array<RuntimeTextKerning, 2> kerning{};
    RuntimeTextMetrics metrics{font_views, kerning, 1.f, 1.f, 2.f};
    void load_fonts();

private:
    struct Resource
    {
        AnimationBundleRecord record;
        LooseResource loose;
        std::vector<u8> text_file;
        std::vector<RuntimeLocalTextKey> keys;
    };
    AidStore& aids_;
    std::map<std::string, Resource> resources_;
    std::array<std::vector<RuntimeTextGlyph>, 2> glyphs_;
    std::array<LooseResource, 2> font_files_;
    Resource* find_resource(const char* name);
    void prepare_text(Resource& resource);
};
}
