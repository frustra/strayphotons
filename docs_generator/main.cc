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
struct is_optional : std::false_type {};
template<typename T>
struct is_optional<std::optional<T>> : std::true_type {};

template<typename T>
struct is_unordered_map : std::false_type {};
template<typename K, typename V>
struct is_unordered_map<robin_hood::unordered_flat_map<K, V>> : std::true_type {};
template<typename K, typename V>
struct is_unordered_map<robin_hood::unordered_node_map<K, V>> : std::true_type {};

struct DocField {
    std::string name, typeString, description;
    std::type_index type;
    picojson::value defaultValue;
};

struct DocsContext {
    std::vector<DocField> fields;
    std::set<std::pair<std::string, std::type_index>> references;

private:
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
        } else if constexpr (is_optional<T>()) {
            return "optional&lt;" + fieldTypeName<typename T::value_type>() + "&gt;";
        } else if constexpr (std::is_enum<T>()) {
            static const auto enumName = magic_enum::enum_type_name<T>();
            references.emplace(enumName, typeid(T));
            if constexpr (is_flags_enum<T>()) {
                return "enum flags `" + std::string(enumName) + "`";
            } else {
                return "enum `" + std::string(enumName) + "`";
            }
        } else {
            auto &metadata = ecs::StructMetadata::Get<T>();
            references.emplace(metadata.name, metadata.type);
            return "`"s + metadata.name + "`";
        }
    }

public:
    template<typename T>
    void SaveFields() {
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
                        fields.emplace_back(DocField{"",
                            fieldTypeName<U>(),
                            "The `"s + metadata.name + "` component is serialized directly as an enum string",
                            typeid(U),
                            output});
                    } else if constexpr (sp::is_vector<U>()) {
                        fields.emplace_back(DocField{"",
                            fieldTypeName<U>(),
                            "The `"s + metadata.name + "` component is serialized directly as an array",
                            typeid(U),
                            output});
                    } else if constexpr (is_unordered_map<U>()) {
                        fields.emplace_back(DocField{"",
                            fieldTypeName<U>(),
                            "The `"s + metadata.name + "` component is serialized directly as a map",
                            typeid(U),
                            output});
                    } else if constexpr (!std::is_same_v<T, U>) {
                        SaveFields<U>();
                    } else {
                        Abortf("Unknown self reference state: %s", metadata.name);
                    }
                } else {
                    fields.emplace_back(DocField{field.name, fieldTypeName<U>(), field.desc, typeid(U), output});
                }
            });
        }
    }
};

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

        DocsContext docs;
        ecs::GetFieldType(comp.metadata.type, [&](auto *typePtr) {
            using T = std::remove_pointer_t<decltype(typePtr)>;
            docs.SaveFields<T>();
        });

        if (docs.fields.empty()) {
            file << "The `" << name << "` component has no public fields" << std::endl;
        } else if (docs.fields.size() == 1 && docs.fields.front().name.empty()) {
            file << "The `" << name << "` component has type: " << docs.fields.front().typeString << std::endl;
        } else {
            file << "| Field Name | Type | Default Value | Description |" << std::endl;
            file << "|------------|------|---------------|-------------|" << std::endl;
            for (auto &field : docs.fields) {
                file << "| **" << field.name << "** | " << field.typeString << " | "
                     << escapeMarkdownString(field.defaultValue.serialize()) << " | " << field.description << " |"
                     << std::endl;
            }
        }

        if (!docs.references.empty()) {
            std::set<std::string> savedRefs;
            std::vector<std::string> seeAlso;

            auto saveReferencedType = [&](const std::string &refName, const std::type_index &refType, auto &selfFunc) {
                savedRefs.emplace(refName);

                DocsContext refDocs;
                bool isEnumFlags = false;
                bool isEnum = false;
                ecs::GetFieldType(refType, [&](auto *typePtr) {
                    using T = std::remove_pointer_t<decltype(typePtr)>;

                    if constexpr (std::is_enum<T>()) {
                        isEnum = true;
                        if constexpr (is_flags_enum<T>()) {
                            isEnumFlags = true;
                        }

                        static const auto names = magic_enum::enum_names<T>();
                        for (auto &enumName : names) {
                            refDocs.fields.emplace_back(DocField{std::string(enumName), "", "", typeid(T), {}});
                        }
                    } else {
                        refDocs.SaveFields<T>();
                    }
                });

                if (refDocs.fields.empty()) {
                    seeAlso.emplace_back(refName);
                    return;
                }

                file << std::endl << "### `" << refName << "` Type" << std::endl;
                if (isEnumFlags) {
                    file << "This is a flags enum type. Multiple flags can be combined using the '|' character "
                         << "(e.g. `\"One|Two\"` with no whitespace)." << std::endl;
                }
                if (isEnum) {
                    file << "Note: Enum string names are case-sensitive." << std::endl;
                    file << "| Enum Value |" << std::endl;
                    file << "|------------|" << std::endl;
                    for (auto &field : refDocs.fields) {
                        file << "| \"" << field.name << "\" |" << std::endl;
                    }
                } else {
                    file << "| Field Name | Type | Default Value | Description |" << std::endl;
                    file << "|------------|------|---------------|-------------|" << std::endl;
                    for (auto &field : refDocs.fields) {
                        file << "| **" << field.name << "** | " << field.typeString << " | "
                             << escapeMarkdownString(field.defaultValue.serialize()) << " | " << field.description
                             << " |" << std::endl;
                    }
                }

                for (auto &[subRefName, subRefType] : refDocs.references) {
                    if (!savedRefs.count(subRefName)) {
                        selfFunc(subRefName, subRefType, selfFunc);
                    }
                }
            };

            for (auto &[refName, refType] : docs.references) {
                saveReferencedType(refName, refType, saveReferencedType);
            }

            if (!seeAlso.empty()) {
                file << std::endl << "**See Also:**" << std::endl;
                for (auto &refName : seeAlso) {
                    file << "`" << refName << "`" << std::endl;
                }
            }
        }

        file << std::endl << std::endl;
    });
    return 0;
}
