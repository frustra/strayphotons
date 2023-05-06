#pragma once

#include "ecs/Ecs.hh"
#include "ecs/EventQueue.hh"
#include "ecs/SignalRef.hh"
#include "ecs/StructMetadata.hh"
#include "ecs/components/Focus.hh"

#include <functional>
#include <memory>
#include <string>
#include <variant>

namespace ecs {
    using ReadSignalsLock = Lock<Read<Name, SignalOutput, SignalBindings, FocusLock>>;

    class ComponentBase;

    class SignalExpression {
    public:
        SignalExpression() {}
        SignalExpression(const SignalRef &signal);
        SignalExpression(std::string_view expr, const Name &scope = Name());

        SignalExpression(const SignalExpression &other)
            : scope(other.scope), expr(other.expr), nodes(other.nodes), nodeStrings(other.nodeStrings),
              rootIndex(other.rootIndex) {}

        struct ConstantNode {
            double value = 0.0f;

            bool operator==(const ConstantNode &) const = default;
        };
        struct IdentifierNode {
            StructField field;

            bool operator==(const IdentifierNode &) const = default;
        };
        struct SignalNode {
            SignalRef signal;

            bool operator==(const SignalNode &) const = default;
        };
        struct ComponentNode {
            EntityRef entity;
            const ComponentBase *component;
            StructField field;

            bool operator==(const ComponentNode &) const = default;
        };
        struct FocusCondition {
            FocusLayer ifFocused;
            int inputIndex = -1;

            bool operator==(const FocusCondition &) const = default;
        };
        struct OneInputOperation {
            int inputIndex = -1;
            double (*evaluate)(double) = nullptr;

            bool operator==(const OneInputOperation &) const = default;
        };
        struct TwoInputOperation {
            int inputIndexA = -1;
            int inputIndexB = -1;
            double (*evaluate)(double, double) = nullptr;

            bool operator==(const TwoInputOperation &) const = default;
        };
        struct DeciderOperation {
            int ifIndex = -1;
            int trueIndex = -1;
            int falseIndex = -1;

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
            size_t startToken = 0;
            size_t endToken = 0;

            template<typename T>
            Node(T &&arg, size_t startToken, size_t endToken)
                : NodeVariant(arg), startToken(startToken), endToken(endToken) {}
        };

        // Called automatically by constructor. Should be called when expression string is changed.
        bool Parse();

        template<typename LockType>
        bool CanEvaluate(const LockType &lock) const {
            if constexpr (LockType::template has_permissions<ReadAll>()) {
                return true;
            } else if constexpr (LockType::template has_permissions<ReadSignalsLock>()) {
                return canEvaluate(lock);
            } else {
                return false;
            }
        }

        double Evaluate(const DynamicLock<ReadSignalsLock> &lock, size_t depth = 0) const;
        double EvaluateEvent(const DynamicLock<ReadSignalsLock> &lock, const EventData &input) const;

        bool operator==(const SignalExpression &other) const {
            return expr == other.expr && scope == other.scope;
        }

        explicit operator bool() const {
            return !expr.empty() && rootIndex >= 0 && rootIndex < nodes.size();
        }

        EntityScope scope;
        std::string expr;
        std::vector<Node> nodes;
        std::vector<std::string> nodeStrings;
        int rootIndex = -1;

    private:
        std::string joinTokens(size_t startToken, size_t endToken) const;
        int deduplicateNode(int index);
        int parseNode(size_t &tokenIndex, uint8_t precedence = '\x0');

        bool canEvaluate(const DynamicLock<ReadSignalsLock> &lock) const;

        std::vector<std::string_view> tokens; // string_views into expr
    };

    static StructMetadata MetadataSignalExpression(typeid(SignalExpression));
    template<>
    bool StructMetadata::Load<SignalExpression>(const EntityScope &scope,
        SignalExpression &dst,
        const picojson::value &src);
    template<>
    void StructMetadata::Save<SignalExpression>(const EntityScope &scope,
        picojson::value &dst,
        const SignalExpression &src,
        const SignalExpression &def);
} // namespace ecs
