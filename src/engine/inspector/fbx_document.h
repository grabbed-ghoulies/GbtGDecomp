#pragma once
// Binary FBX (version 7.4) documents for the inspector's model export and import: a tree of named records, each with
// typed properties and child records, read from and written to the binary layout Autodesk's tools use. Plugin builds
// only.
#if defined(GBTG_INSPECTOR_APP) && !defined(GBTG_RETAIL_GAME_ONLY)
#include "gbtg/types.h"

#include <cstdint>
#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace gbtg
{
struct FbxProperty
{
    // Y i16, C bool, I i32, F float, D double, L i64, S string, R raw, and arrays f d l i b.
    char type = 'I';
    std::int64_t integer = 0;
    double real = 0.0;
    std::string text; // S and R.
    std::vector<double> reals; // f and d.
    std::vector<std::int64_t> integers; // i, l and b.

    static FbxProperty i16(int value);
    static FbxProperty boolean(bool value);
    static FbxProperty i32(std::int64_t value);
    static FbxProperty f32(double value);
    static FbxProperty f64(double value);
    static FbxProperty i64(std::int64_t value);
    static FbxProperty string(std::string value);
    static FbxProperty raw(std::string value);
    static FbxProperty f64s(std::vector<double> values);
    static FbxProperty f32s(std::vector<double> values);
    static FbxProperty i32s(std::vector<std::int64_t> values);
    static FbxProperty i64s(std::vector<std::int64_t> values);

    double number() const; // Any scalar as a number.
    // An array as numbers (f d i l b), or a scalar as one number.
    std::vector<double> numbers() const;
};

struct FbxNode
{
    std::string name;
    std::vector<FbxProperty> properties;
    std::vector<FbxNode> children;

    FbxNode& add(std::string child_name);
    FbxNode& add(std::string child_name, std::vector<FbxProperty> child_properties);
    const FbxNode* find(std::string_view child_name) const;
    std::vector<const FbxNode*> all(std::string_view child_name) const;
    // A Properties70 entry ("P") by its name, else null.
    const FbxNode* p(std::string_view property_name) const;
};

// The top-level records of a binary FBX file.
std::vector<FbxNode> read_fbx(std::span<const u8> bytes);
std::vector<u8> write_fbx(const std::vector<FbxNode>& top, u32 version = 7400);
}
#endif
