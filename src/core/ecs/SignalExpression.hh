#pragma once

#include "ecs/Ecs.hh"
#include "ecs/EventQueue.hh"
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
        SignalExpression(const EntityRef &entity, const std::string &signalName);
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
            EntityRef entity;
            std::string signalName = "value";

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

        bool CanEvaluate(Lock<ReadSignalsLock, Optional<ReadAll>> lock) const;

        template<typename LockType>
        double Evaluate(LockType lock, size_t depth = 0) const {
            if constexpr (LockType::template has_permissions<ReadAll>()) {
                return evaluate((Lock<ReadAll>)lock, depth);
            } else {
                return evaluate((Lock<ReadSignalsLock, Optional<ReadAll>>)lock, depth);
            }
        }

        template<typename... Permissions>
        double Evaluate(const EntityLock<Permissions...> &entLock, size_t depth = 0) const {
            return evaluate((EntityLock<ReadSignalsLock, Optional<ReadAll>>)entLock, depth);
        }

        template<typename LockType>
        double EvaluateEvent(LockType lock, const EventData &input) const {
            if constexpr (LockType::template has_permissions<ReadAll>()) {
                return evaluateEvent((Lock<ReadAll>)lock, input);
            } else {
                return evaluateEvent((Lock<ReadSignalsLock, Optional<ReadAll>>)lock, input);
            }
        }

        template<typename... Permissions>
        double EvaluateEvent(const EntityLock<Permissions...> &entLock, const EventData &input) const {
            return evaluateEvent((EntityLock<ReadSignalsLock, Optional<ReadAll>>)entLock, input);
        }

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

        double evaluate(Lock<ReadSignalsLock, Optional<ReadAll>> lock, size_t depth) const;
        double evaluate(EntityLock<ReadSignalsLock, Optional<ReadAll>> lock, size_t depth) const;
        double evaluateEvent(Lock<ReadSignalsLock, Optional<ReadAll>> lock, const EventData &input) const;
        double evaluateEvent(EntityLock<ReadSignalsLock, Optional<ReadAll>> lock, const EventData &input) const;

        std::vector<std::string_view> tokens; // string_views into expr
    };

    std::pair<Name, std::string> ParseSignalString(std::string_view str, const EntityScope &scope = Name());

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
