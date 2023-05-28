#include "assets/JsonHelpers.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/StructFieldTypes.hh"

#include <cxxopts.hpp>
#include <fstream>
#include <sstream>

inline std::string escapeMarkdownString(const std::string &input) {
    size_t escapeCount = std::count(input.begin(), input.end(), '|');
    if (escapeCount == 0) return input;
    std::string result;
    result.reserve(input.length() + escapeCount);
    for (auto &ch : input) {
        if (ch == '|') {
            result += '\\';
        }
        result += ch;
    }
    return result;
}

template<typename T>
struct is_unordered_map : std::false_type {};

template<typename K, typename V>
struct is_unordered_map<robin_hood::unordered_flat_map<K, V>> : std::true_type {};
template<typename K, typename V>
struct is_unordered_map<robin_hood::unordered_node_map<K, V>> : std::true_type {};

template<typename T>
std::string fieldTypeName() {
    if constexpr (std::is_same_v<T, bool>) {
        return "bool";
    } else if constexpr (std::is_same_v<T, int32_t>) {
        return "int32";
    } else if constexpr (std::is_same_v<T, uint32_t>) {
        return "uint32";
    } else if constexpr (std::is_same_v<T, size_t>) {
        return "size_t";
    } else if constexpr (std::is_same_v<T, sp::angle_t>) {
        return "float (degrees)";
    } else if constexpr (std::is_same_v<T, float>) {
        return "float";
    } else if constexpr (std::is_same_v<T, double>) {
        return "double";
    } else if constexpr (std::is_same_v<T, std::string>) {
        return "string";
    } else if constexpr (std::is_same_v<T, sp::color_t>) {
        return "vec3 (red, green, blue)";
    } else if constexpr (std::is_same_v<T, sp::color_alpha_t>) {
        return "vec4 (red, green, blue, alpha)";
    } else if constexpr (std::is_same_v<T, glm::quat>) {
        return "vec4 (angle_degrees, axis_x, axis_y, axis_z)";
    } else if constexpr (std::is_same_v<T, glm::mat3>) {
        return "vec4 (angle_degrees, axis_x, axis_y, axis_z)";
    } else if constexpr (sp::is_glm_vec<T>()) {
        using U = typename T::value_type;

        if constexpr (std::is_same_v<U, float>) {
            return "vec" + std::to_string(T::length());
        } else if constexpr (std::is_same_v<U, double>) {
            return "dvec" + std::to_string(T::length());
        } else if constexpr (std::is_same_v<U, int>) {
            return "ivec" + std::to_string(T::length());
        } else if constexpr (std::is_same_v<U, unsigned int>) {
            return "uvec" + std::to_string(T::length());
        } else {
            return typeid(T).name();
        }
    } else if constexpr (sp::is_vector<T>()) {
        return "vector&lt;" + fieldTypeName<typename T::value_type>() + "&gt;";
    } else if constexpr (is_unordered_map<T>()) {
        return "map&lt;" + fieldTypeName<typename T::key_type>() + ", " + fieldTypeName<typename T::mapped_type>() +
               "&gt;";
    } else if constexpr (std::is_enum<T>()) {
        if constexpr (is_flags_enum<T>()) {
            return escapeMarkdownString("flags \""s + magic_enum::enum_flags_name(~(T)0) + "\"");
        } else {
            std::string result = "enum ";
            static const auto names = magic_enum::enum_names<T>();
            for (size_t i = 0; i < names.size(); i++) {
                if (i == 0) {
                    result += "\"" + std::string(names[i]) + "\"";
                } else if (i + 1 == names.size()) {
                    result += ", or \"" + std::string(names[i]) + "\"";
                } else {
                    result += ", \"" + std::string(names[i]) + "\"";
                }
            }
            return result;
        }
    } else {
        auto &metadata = ecs::StructMetadata::Get<T>();
        return "`"s + metadata.name + "`";
    }
}

template<typename T>
void saveFields(std::stringstream &ss) {
    static const T defaultStruct = {};

    auto *metadataPtr = ecs::StructMetadata::Get(typeid(T));
    if (!metadataPtr) {
        Errorf("Unknown saveFields metadata: %s", typeid(T).name());
        return;
    }
    auto &metadata = *metadataPtr;

    for (auto &field : metadata.fields) {
        const void *fieldPtr = field.Access(&defaultStruct);

        ecs::GetFieldType(field.type, fieldPtr, [&](auto &defaultValue) {
            using U = std::decay_t<decltype(defaultValue)>;

            picojson::value output;
            output.set<picojson::object>({});
            sp::json::Save(ecs::EntityScope(), output, defaultValue);

            if (field.name.empty()) {
                if constexpr (std::is_enum<U>()) {
                    ss << "|   | " << fieldTypeName<U>() << " | " << escapeMarkdownString(output.serialize())
                       << " | The `" << metadata.name << "` component is serialized directly as an enum string |"
                       << std::endl;
                } else if constexpr (sp::is_vector<U>()) {
                    ss << "|   | " << fieldTypeName<U>() << " | " << escapeMarkdownString(output.serialize())
                       << " | The `" << metadata.name << "` component is serialized directly as an array |"
                       << std::endl;
                } else if constexpr (is_unordered_map<U>()) {
                    ss << "|   | " << fieldTypeName<U>() << " | " << escapeMarkdownString(output.serialize())
                       << " | The `" << metadata.name << "` component is serialized directly as a map |" << std::endl;
                } else if constexpr (!std::is_same_v<T, U>) {
                    saveFields<U>(ss);
                } else {
                    Abortf("Unknown self reference state: %s", metadata.name);
                }
            } else {
                ss << "| **" << field.name << "** | " << fieldTypeName<U>() << " | "
                   << escapeMarkdownString(output.serialize()) << " | " << field.desc << " |" << std::endl;
            }
        });
    }
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

        std::stringstream ss;
        ecs::GetFieldType(comp.metadata.type, [&](auto *typePtr) {
            using T = std::remove_pointer_t<decltype(typePtr)>;
            saveFields<T>(ss);
        });

        if (ss.view().empty()) {
            file << "This component has no public fields" << std::endl;
        } else {
            file << "| Field Name | Type | Default Value | Description |" << std::endl;
            file << "|------------|------|---------------|-------------|" << std::endl;
            file << ss.view();
        }

        file << std::endl << std::endl;
    });
    return 0;
}
