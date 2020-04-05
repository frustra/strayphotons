#include "ValvePak.hh"
#include <assets/Asset.hh>
#include <iostream>

namespace sp
{
	ValvePak::ValvePak(const string &name, shared_ptr<Asset> asset) : name(name), asset(asset)
    {
        Assert(asset != nullptr, "Reading null asset as Vpk");
        
        std::cout << "Starting parse" << std::endl;

        const uint8_t *buffer = asset->Buffer();
        int buffer_size = asset->Size();

        Assert(buffer_size >= sizeof(VpkHeader), "Asset is too small to contain VpkHeader");

        header = reinterpret_cast<const VpkHeader *>(buffer);
        Assert(header->magic == VpkMagicNumber, "Asset is not a valid Vpk");
        buffer += sizeof(VpkHeader);
        buffer_size -= sizeof(VpkHeader);

        if (header->version == 2)
        {
            Assert(buffer_size >= sizeof(VpkHeaderV2), "Asset is too small to contain VpkHeaderV2");

            header2 = reinterpret_cast<const VpkHeaderV2 *>(buffer);
            buffer += sizeof(VpkHeaderV2);
            buffer_size -= sizeof(VpkHeaderV2);
        }

        readEntries(buffer, buffer_size);
    }

    void ValvePak::readEntries(const uint8_t *buffer, int buffer_size)
    {
        Assert(buffer != nullptr, "Reading entries from null buffer");

        // Types
        while (buffer_size > 0)
        {
            size_t type_name_len = strnlen_s(reinterpret_cast<const char *>(buffer), static_cast<size_t>(buffer_size));
            if (type_name_len == 0)
            {
                buffer++;
                buffer_size--;
                break;
            }

            Assert(type_name_len < buffer_size, "Type name is not null-terminated");
            const char *type_name = reinterpret_cast<const char *>(buffer);
            buffer += type_name_len + 1;
            buffer_size -= type_name_len + 1;
            
            // Directories
            while (buffer_size > 0)
            {
                size_t directory_name_len = strnlen_s(reinterpret_cast<const char *>(buffer), static_cast<size_t>(buffer_size));
                if (directory_name_len == 0)
                {
                    buffer++;
                    buffer_size--;
                    break;
                }

                Assert(directory_name_len < buffer_size, "Directory name is not null-terminated");
                const char *directory_name = reinterpret_cast<const char *>(buffer);
                buffer += directory_name_len + 1;
                buffer_size -= directory_name_len + 1;
            
                // Files
                while (buffer_size > 0)
                {
                    size_t file_name_len = strnlen_s(reinterpret_cast<const char *>(buffer), static_cast<size_t>(buffer_size));
                    if (file_name_len == 0)
                    {
                        buffer++;
                        buffer_size--;
                        break;
                    }

                    Assert(file_name_len < buffer_size, "File name is not null-terminated");
                    const char *file_name = reinterpret_cast<const char *>(buffer);
                    buffer += file_name_len + 1;
                    buffer_size -= file_name_len + 1;

                    Assert(buffer_size >= sizeof(VpkEntryFields), "VpkEntryFields exceeds buffer length");
                    const VpkEntryFields *fields = reinterpret_cast<const VpkEntryFields *>(buffer);
                    Assert(fields->terminator == VpkEntryTerminator, "VpkEntry has invalid terminator");
                    buffer += sizeof(VpkEntryFields);
                    buffer_size -= sizeof(VpkEntryFields);
                    Assert(buffer_size >= fields->small_data_size, "VpkEntryFields small data exceeds buffer length");

                    // std::cout << "Entry: " << directory_name << "/" << file_name << "." << type_name << std::endl;

                    entries.emplace_back(directory_name, file_name, type_name, fields, fields->small_data_size > 0 ? buffer : nullptr);
                    if (fields->small_data_size > 0)
                    {
                        buffer += fields->small_data_size;
                        buffer_size -= fields->small_data_size;
                    }
                }
            }
        }
        
        std::cout << "Done entries" << std::endl;
    }
}
