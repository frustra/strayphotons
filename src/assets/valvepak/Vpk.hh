#pragma once

#include <cstdint>

namespace sp
{
    static const uint32_t VpkMagicNumber = 0x55AA1234;
    static const uint16_t VpkEntryTerminator = 0xFFFF;

#pragma pack(push, 1)
    struct VpkHeader
    {
        uint32_t magic; // VpkMagicNumber == 0x55AA1234
        uint32_t version;
        uint32_t tree_size;
    };

    struct VpkHeaderV2
    {
        uint32_t file_data_section_size;
        uint32_t archive_md5_section_size;
        uint32_t other_md5_section_size;
        uint32_t signature_section_size;
    };

    struct VpkEntryFields
    {
        uint32_t crc32;
        uint16_t small_data_size;
        uint16_t archive_index;
        uint32_t offset;
        uint32_t length;
        uint16_t terminator; // VpkEntryTerminator == 0xFFFF;
    };
#pragma pack(pop)

    struct VpkEntry
    {
        const char *directory_name;
        const char *file_name;
        const char *type_name;
        const VpkEntryFields *fields;
        const uint8_t *small_data;

        VpkEntry(const char *directory_name, const char *file_name, const char *type_name, const VpkEntryFields *fields, const uint8_t *small_data)
        : directory_name(directory_name), file_name(file_name), type_name(type_name), fields(fields), small_data(small_data) {}
    };
}
