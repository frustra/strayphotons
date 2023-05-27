#include "assets/JsonHelpers.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/StructFieldTypes.hh"

#include <cxxopts.hpp>
#include <fstream>

void saveFields(std::ofstream &file, const ecs::StructMetadata &metadata) {
    ecs::GetFieldType(metadata.type, [&](auto *typePtr) {
        using T = std::remove_pointer_t<decltype(typePtr)>;

        static const T defaultValue = {};

        for (auto &field : metadata.fields) {
            picojson::value output;
            field.Save(ecs::EntityScope(), output, &defaultValue, nullptr);

            if (field.name.empty()) {
                auto *subMetadata = ecs::StructMetadata::Get(field.type);
                if (subMetadata && field.type != metadata.type) {
                    saveFields(file, *subMetadata);
                } else {
                    Logf("Unsupported field overload: %s", field.type.name());
                }
            } else {
                file << "| **" << field.name << "** | " << field.type.name() << " | " << output.serialize()
                     << " | No description |" << std::endl;
            }
        }
    });
}

int main(int argc, char **argv) {
    cxxopts::Options options("docs_generator", "");
    options.positional_help("<output_path>");
    options.add_options()("output-path", "", cxxopts::value<std::string>());
    options.parse_positional({"output-path"});

    auto optionsResult = options.parse(argc, argv);

    if (!optionsResult.count("output-path")) {
        std::cout << options.help() << std::endl;
        return 1;
    }

    sp::logging::SetLogLevel(sp::logging::Level::Log);

    std::string outputPath = optionsResult["output-path"].as<std::string>();

    std::ofstream file(outputPath);
    if (!file) {
        Errorf("Failed to open output file: '%s'", outputPath);
        return 1;
    }

    ecs::EntityScope scope;
    ecs::ForEachComponent([&](const std::string &name, const ecs::ComponentBase &comp) {
        file << "## `" << name << "` Component" << std::endl;

        file << "| Field Name | Type | Default Value | Description |" << std::endl;
        file << "|------------|------|---------------|-------------|" << std::endl;

        saveFields(file, comp.metadata);

        file << std::endl << std::endl;
    });
    return 0;
}
