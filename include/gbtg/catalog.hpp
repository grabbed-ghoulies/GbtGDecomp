#pragma once

#include "gbtg/types.hpp"
#include "gbtg/xbe.hpp"

namespace gbtg {

struct CatalogFile {
    std::string relative;
    u64 size = 0;
    std::string magic;
    std::string kind;
    u32 aid_hash = 0;
    std::string aid_name;  // resolved if known
    std::string sha256;
};

struct Catalog {
    std::filesystem::path game_dir;
    std::vector<CatalogFile> files;
    std::vector<std::string> unresolved_hashes;
    int verified = 0;
    int missing = 0;
    int mismatched = 0;
};

struct CatalogOptions {
    bool compute_sha = true;
    std::filesystem::path manifest;  // retail_files.txt
};

Catalog build_catalog(const std::filesystem::path& game_dir, const CatalogOptions& opt = {});

}  // namespace gbtg
