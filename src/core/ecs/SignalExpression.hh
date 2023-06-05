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
    class ComponentBase;

    static const char *DocsDescriptionSignalExpression = R"(
Signal expressions allow math and logic to be performed including almost any entity component property.
Expressions are defined as strings and automatically parsed and compiled for fast game logic evaluation.

A basic signal expression might look like this:  
`"(entity_name/signal_value + 1) > 10"`

The above will evaluate to `1` if the condition is true, or `0` if the condition is false.

Most "error" cases will evaulate to 0, such as an empty expression, missing referenced signals or entities, or division by 0.

Fields can be accessed on components using the following syntax:  
`"<entity_name>#<component_name>.<field_name>"`

For example: `light#renderable.emissive` will return the `emissive` value from the `light` entity's `renderable` component.

Vector fields such as position or color can be accessed as `pos.x` or `color.r`.

Note: Only number-compatble fields can be referenced. All evaluation is done using double floating point numbers.
)";

    static const size_t MAX_SIGNAL_EXPRESSION_NODES = 256;

    class SignalExpression {
    public:
        SignalExpression() {}
        SignalExpression(const SignalRef &signal);
        SignalExpression(std::string_view expr, const Name &scope = Name());

        SignalExpression(const SignalExpression &other)
            : scope(other.scope), expr(other.expr), nodes(other.nodes), nodeStrings(other.nodeStrings),
              rootIndex(other.rootIndex) {}

        struct Node;
        using Storage = std::array<double, MAX_SIGNAL_EXPRESSION_NODES>;

        struct Context {
            const DynamicLock<ReadSignalsLock> &lock;
            const SignalExpression &expr;
            Storage &cache;
            const EventData &input;

            Context(const DynamicLock<ReadSignalsLock> &lock,
                const SignalExpression &expr,
                Storage &cache,
                const EventData &input)
                : lock(lock), expr(expr), cache(cache), input(input) {}
        };
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

            static double Evaluate(const Context &ctx, const Node &node, size_t depth);
            bool operator==(const ComponentNode &) const = default;
        };
        struct FocusCondition {
            FocusLayer ifFocused;
            int inputIndex = -1;
            CompiledFunc inputFunc = nullptr;

            static double Evaluate(const Context &ctx, const Node &node, size_t depth);
            bool operator==(const FocusCondition &) const = default;
        };
        struct OneInputOperation {
            int inputIndex = -1;
            double (*evaluate)(double) = nullptr;
            CompiledFunc inputFunc = nullptr;

            static double Evaluate(const Context &ctx, const Node &node, size_t depth);
            bool operator==(const OneInputOperation &) const = default;
        };
        struct TwoInputOperation {
            int inputIndexA = -1;
            int inputIndexB = -1;
            double (*evaluate)(double, double) = nullptr;
            CompiledFunc inputFuncA = nullptr;
            CompiledFunc inputFuncB = nullptr;

            static double Evaluate(const Context &ctx, const Node &node, size_t depth);
            bool operator==(const TwoInputOperation &) const = default;
        };
        struct DeciderOperation {
            int ifIndex = -1;
            int trueIndex = -1;
            int falseIndex = -1;
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
            size_t startToken = 0;
            size_t endToken = 0;
            size_t index = std::numeric_limits<size_t>::max();
            CompiledFunc evaluate = nullptr;

            template<typename T>
            Node(T &&arg, size_t startToken, size_t endToken, size_t index)
                : NodeVariant(arg), startToken(startToken), endToken(endToken), index(index) {}

            CompiledFunc compile(SignalExpression &expr, bool noCacheWrite);
        };

        // Called automatically by constructor. Should be called when expression string is changed.
        bool Compile();

        template<typename LockType>
        bool CanEvaluate(const LockType &lock, size_t depth = 0) const {
            if constexpr (LockType::template has_permissions<ReadAll>()) {
                return true;
            } else if constexpr (LockType::template has_permissions<ReadSignalsLock>()) {
                return canEvaluate(lock, depth);
            } else {
                return false;
            }
        }

        double Evaluate(const DynamicLock<ReadSignalsLock> &lock, size_t depth = 0) const;
        double EvaluateEvent(const DynamicLock<ReadSignalsLock> &lock, const EventData &input) const;

        bool operator==(const SignalExpression &other) const {
            return expr == other.expr && scope == other.scope;
        }

        // True if the expression is valid and can be evaluated.
        // An empty expression is valid and evaluates to 0.
        explicit operator bool() const {
            return rootIndex >= 0 && rootIndex < nodes.size();
        }

        // Returns true if this expression is default constructed.
        // Both empty expressions and invalid expressions are considered set (not null).
        bool IsNull() const {
            return expr.empty() && rootIndex < 0;
        }

        void SetScope(const EntityScope &scope);

        EntityScope scope;
        std::string expr;
        std::vector<Node> nodes;
        std::vector<std::string> nodeStrings;
        int rootIndex = -1;

    private:
        std::string joinTokens(size_t startToken, size_t endToken) const;
        int deduplicateNode(int index);
        int parseNode(size_t &tokenIndex, uint8_t precedence = '\x0');

        bool canEvaluate(const DynamicLock<ReadSignalsLock> &lock, size_t depth) const;

        std::vector<std::string_view> tokens; // string_views into expr
    };

    static StructMetadata MetadataSignalExpression(typeid(SignalExpression),
        "SignalExpression",
        DocsDescriptionSignalExpression);
    template<>
    bool StructMetadata::Load<SignalExpression>(SignalExpression &dst, const picojson::value &src);
    template<>
    void StructMetadata::Save<SignalExpression>(const EntityScope &scope,
        picojson::value &dst,
        const SignalExpression &src,
        const SignalExpression *def);
    template<>
    void StructMetadata::SetScope<SignalExpression>(SignalExpression &dst, const EntityScope &scope);
    template<>
    void StructMetadata::DefineSchema<SignalExpression>(picojson::value &dst,
        sp::json::SchemaTypeReferences *references);
} // namespace ecs
