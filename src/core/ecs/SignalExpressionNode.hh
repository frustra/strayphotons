/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
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

            static double Evaluate(const Context &ctx, const Node &node, size_t depth);
            bool operator==(const ConstantNode &) const = default;
        };
        struct IdentifierNode {
            StructField field;

            static double Evaluate(const Context &ctx, const Node &node, size_t depth);
            bool operator==(const IdentifierNode &) const = default;
        };
        struct SignalNode {
            SignalRef signal;

            static double Evaluate(const Context &ctx, const Node &node, size_t depth);
            bool operator==(const SignalNode &) const = default;
        };
        struct ComponentNode {
            EntityRef entity;
            const ComponentBase *component;
            StructField field;
            std::string path;

            static double Evaluate(const Context &ctx, const Node &node, size_t depth);
            bool operator==(const ComponentNode &) const = default;
        };
        struct FocusCondition {
            FocusLayer ifFocused;
            SignalNodePtr inputNode = nullptr;
            CompiledFunc inputFunc = nullptr;

            static double Evaluate(const Context &ctx, const Node &node, size_t depth);
            bool operator==(const FocusCondition &) const = default;
        };
        struct OneInputOperation {
            SignalNodePtr inputNode = nullptr;
            double (*evaluate)(double) = nullptr;
            std::string prefixStr, suffixStr;
            CompiledFunc inputFunc = nullptr;

            static double Evaluate(const Context &ctx, const Node &node, size_t depth);
            bool operator==(const OneInputOperation &) const = default;
        };
        struct TwoInputOperation {
            SignalNodePtr inputNodeA = nullptr;
            SignalNodePtr inputNodeB = nullptr;
            double (*evaluate)(double, double) = nullptr;
            std::string prefixStr, middleStr, suffixStr;
            CompiledFunc inputFuncA = nullptr;
            CompiledFunc inputFuncB = nullptr;

            static double Evaluate(const Context &ctx, const Node &node, size_t depth);
            bool operator==(const TwoInputOperation &) const = default;
        };
        struct DeciderOperation {
            SignalNodePtr ifNode = nullptr;
            SignalNodePtr trueNode = nullptr;
            SignalNodePtr falseNode = nullptr;
            CompiledFunc ifFunc = nullptr;
            CompiledFunc trueFunc = nullptr;
            CompiledFunc falseFunc = nullptr;

            static double Evaluate(const Context &ctx, const Node &node, size_t depth);
            bool operator==(const DeciderOperation &) const = default;
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
            std::vector<const Node *> childNodes;

            template<typename T>
            Node(T &&arg, std::string text, std::initializer_list<const Node *> childNodes = {})
                : NodeVariant(arg), text(text) {
                size_t count = 0;
                for (auto &node : childNodes) {
                    count += node->childNodes.size();
                }
                this->childNodes.reserve(count);
                for (auto &node : childNodes) {
                    this->childNodes.insert(this->childNodes.end(), node->childNodes.begin(), node->childNodes.end());
                }
            }

            CompiledFunc compile(SignalExpression &expr, bool noCacheWrite);
            bool canEvaluate(const DynamicLock<ReadSignalsLock> &lock, size_t depth) const;
            SignalNodePtr setScope(const EntityScope &scope) const;

            bool operator==(const Node &other) const {
                return (const NodeVariant &)*this == (const NodeVariant &)other && text == other.text &&
                       evaluate == other.evaluate;
            }
        };
    } // namespace expression
} // namespace ecs

template<>
struct robin_hood::hash<ecs::expression::Node> {
    std::size_t operator()(const ecs::expression::Node &node) const {
        return robin_hood::hash<std::string>()(node.text);
    }

    std::size_t operator()(const std::shared_ptr<ecs::expression::Node> &node) const {
        if (!node) return robin_hood::hash<size_t>()(0);
        return robin_hood::hash<std::string>()(node->text);
    }
};
