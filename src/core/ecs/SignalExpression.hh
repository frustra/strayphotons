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
        };
        struct IdentifierNode {
            std::string id = "input";
        };
        struct SignalNode {
            EntityRef entity;
            std::string signalName = "value";
        };
        struct ComponentNode {
            EntityRef entity;
            const ComponentBase *component;
            std::string fieldName = "";
        };
        struct FocusCondition {
            FocusLayer ifFocused;
            int inputIndex = -1;
        };
        struct OneInputOperation {
            int inputIndex = -1;
            std::function<double(double)> evaluate;
        };
        struct TwoInputOperation {
            int inputIndexA = -1;
            int inputIndexB = -1;
            std::function<double(double, double)> evaluate;
        };
        struct DeciderOperation {
            int ifIndex = -1;
            int trueIndex = -1;
            int falseIndex = -1;
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
        bool CanEvaluate(LockType lock) const {
            if constexpr (LockType::template has_permissions<ReadAll>()) {
                return true;
            } else if constexpr (LockType::template has_permissions<ReadSignalsLock>()) {
                return canEvaluate(lock);
            } else {
                return false;
            }
        }

        template<typename LockType>
        double Evaluate(LockType lock, size_t depth = 0) const {
            if constexpr (LockType::template has_permissions<ReadAll>()) {
                return evaluate((Lock<ReadAll>)lock, depth);
            } else {
                return evaluate((DynamicLock<ReadSignalsLock>)lock, depth);
            }
        }

        template<typename LockType>
        double EvaluateEvent(LockType lock, const EventData &input, size_t depth = 0) const {
            if constexpr (LockType::template has_permissions<ReadAll>()) {
                return evaluateEvent((Lock<ReadAll>)lock, input, depth);
            } else {
                return evaluateEvent((DynamicLock<ReadSignalsLock>)lock, input, depth);
            }
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
        int parseNode(size_t &tokenIndex, uint8_t precedence = '\x0');

        bool canEvaluate(DynamicLock<ReadSignalsLock> lock) const;

        template<typename LockType, typename InputType>
        double evaluateNode(const LockType &lock, size_t depth, int nodeIndex, const InputType &input) const;

        double evaluate(DynamicLock<ReadSignalsLock> lock, size_t depth) const;
        double evaluate(Lock<ReadAll> lock, size_t depth) const;
        double evaluateEvent(DynamicLock<ReadSignalsLock> lock, const EventData &input, size_t depth) const;
        double evaluateEvent(Lock<ReadAll> lock, const EventData &input, size_t depth) const;

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
