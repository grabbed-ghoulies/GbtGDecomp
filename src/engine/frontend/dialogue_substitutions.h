#pragma once
#include "gbtg/types.h"
#include "dialogue_time.h"
namespace gbtg
{
struct DialogueBitsetView
{
    i32 byte_count; // retail +4
    const u8* bytes; // retail +8
};
i32 count_dialogue_bits(const DialogueBitsetView& bits); // Xbox 0x00066180
void dialogue_bit_count(char16_t*& result, char16_t* wide, char* narrow,
    const DialogueBitsetView& bits); // Xbox 0x0005A810
void dialogue_final_score(char16_t*& result, char16_t* wide,
    const i32* scores, i32 selected); // Xbox 0x0005A920
void dialogue_final_time(char16_t*& result, char16_t* wide,
    const i32* scores, i32 selected, LocalTextResourceContext& context); // Xbox 0x0005A960
void dialogue_elapsed_time(char16_t*& result, char16_t* wide, i32 elapsed,
    float additional, LocalTextResourceContext& context); // Xbox 0x0005A880
void dialogue_total_first(char16_t*& result, char16_t* wide, char* narrow, i32 count, i32 additional); // Xbox 0x0005A6C0
void dialogue_second_count(char16_t*& result, char16_t* wide, char* narrow, i32 count); // Xbox 0x0005A730
void dialogue_total_third(char16_t*& result, char16_t* wide, char* narrow, i32 count, i32 additional); // Xbox 0x0005A7A0
}
