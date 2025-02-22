/*
 * Stray Photons - Copyright (C) 2025 Jacob Wirth
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "ecs/Ecs.hh"
#include "ecs/EventQueue.hh"
#include "ecs/SignalRef.hh"
#include "ecs/StructMetadata.hh"
#include "ecs/components/Focus.hh"

#include <functional>
#include <memory>
#include <robin_hood.h>
#include <string>
#include <variant>

namespace ecs {
    class ComponentBase;
    namespace expression {
        struct Node;
    }

    using SignalNodePtr = std::shared_ptr<expression::Node>;

    namespace expression {
        struct Context;
        using CompiledFunc = double (*)(const Context &, const Node &, size_t);

        struct ConstantNode {
            double value = 0.0f;

            CompiledFunc Compile() const;
            bool operator==(const ConstantNode &) const = default;
            size_t hash() const {
                return robin_hood::hash<double>()(value);
            }
        };
        struct IdentifierNode {
            StructField field;

            CompiledFunc Compile() const;
            bool operator==(const IdentifierNode &) const = default;
            size_t hash() const {
                return robin_hood::hash<std::string>()(field.name);
            }
        };
        struct SignalNode {
            SignalRef signal;

            CompiledFunc Compile() const;
            bool operator==(const SignalNode &other) const = default;
            size_t hash() const {
                return robin_hood::hash<std::string>()(signal.String());
            }
        };
        struct ComponentNode {
            EntityRef entity;
            const ComponentBase *component;
            StructField field;
            std::string path;

            CompiledFunc Compile() const;
            bool operator==(const ComponentNode &) const = default;
            size_t hash() const {
                return robin_hood::hash<std::string>()(entity.Name().String() + field.name + path);
            }
        };
        struct FocusCondition {
            FocusLayer ifFocused;

            CompiledFunc Compile() const;
            bool operator==(const FocusCondition &) const = default;
            size_t hash() const {
                return robin_hood::hash<std::string>()(std::string(magic_enum::enum_name(ifFocused)));
            }
        };
        struct OneInputOperation {
            std::string prefixStr, suffixStr;

            CompiledFunc Compile() const;
            bool operator==(const OneInputOperation &) const = default;
            size_t hash() const {
                return robin_hood::hash<std::string>()(prefixStr + suffixStr);
            }
        };
        struct TwoInputOperation {
            std::string prefixStr, middleStr, suffixStr;

            CompiledFunc Compile() const;
            bool operator==(const TwoInputOperation &) const = default;
            size_t hash() const {
                return robin_hood::hash<std::string>()(prefixStr + middleStr + suffixStr);
            }
        };
        struct DeciderOperation {
            CompiledFunc Compile() const;
            bool operator==(const DeciderOperation &) const = default;
            size_t hash() const {
                return 0;
            }
        };

        using NodeVariant = std::variant<ConstantNode,
            IdentifierNode,
            SignalNode,
            ComponentNode,
            FocusCondition,
            OneInputOperation,
            TwoInputOperation,
            DeciderOperation>;
        struct Node : public NodeVariant {
            std::string text;
            CompiledFunc evaluate = nullptr;
            sp::InlineVector<SignalNodePtr, 3> childNodes;
            std::vector<std::weak_ptr<Node>> subscribers;
            bool uncachable = false;

            mutable double lastValue = 0.0;
            mutable bool lastValueDirty = true;
            mutable uint32_t version = 0;

            template<typename T>
            Node(T &&arg, std::string text, std::initializer_list<SignalNodePtr> childNodes = {})
                : NodeVariant(arg), text(text) {
                this->childNodes.insert(this->childNodes.end(), childNodes.begin(), childNodes.end());
                if constexpr (std::is_same<T, IdentifierNode>() || std::is_same<T, ComponentNode>() ||
                              std::is_same<T, FocusCondition>()) {
                    uncachable = true;
                }
            }

            static const SignalNodePtr &subscribeToChildren(const SignalNodePtr &node);
            static void markDirty(const SignalNodePtr &node);

            CompiledFunc compile();
            double Evaluate(const Context &ctx, size_t depth) const;
            bool canEvaluate(const DynamicLock<ReadSignalsLock> &lock, size_t depth) const;
            SignalNodePtr setScope(const EntityScope &scope) const;
            size_t hash() const;

            bool operator==(const Node &other) const {
                return (const NodeVariant &)*this == (const NodeVariant &)other && childNodes == other.childNodes;
            }
        };
    } // namespace expression
} // namespace ecs

namespace std {
    // Thread-safe equality check without weak_ptr::lock()
    inline bool operator==(const ecs::SignalNodePtr &a, const std::weak_ptr<ecs::expression::Node> &b) {
        return !a.owner_before(b) && !b.owner_before(b);
    }
} // namespace std

template<>
struct robin_hood::hash<ecs::expression::Node> {
    std::size_t operator()(const ecs::expression::Node &node) const {
        return node.hash();
    }

    std::size_t operator()(const std::shared_ptr<ecs::expression::Node> &node) const {
        if (!node) return robin_hood::hash<size_t>()(0);
        return node->hash();
    }
};
