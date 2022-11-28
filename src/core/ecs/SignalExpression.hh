#pragma once

#include "ecs/Ecs.hh"

#include <functional>
#include <memory>
#include <string>
#include <variant>

namespace ecs {
    using ReadSignalsLock = Lock<Read<Name, SignalOutput, SignalBindings, FocusLayer, FocusLock>>;

    struct SignalExpression {
        SignalExpression() {}
        SignalExpression(const Name &entityName, const std::string &signalName);
        SignalExpression(std::string_view expr, const Name &scope = Name());

        struct ConstantNode {
            double value = 0.0f;
        };
        struct SignalNode {
            Name entityName;
            std::string signalName = "value";
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

        using NodeVariant =
            std::variant<ConstantNode, SignalNode, OneInputOperation, TwoInputOperation, DeciderOperation>;
        struct Node : public NodeVariant {
            size_t startToken = 0;
            size_t endToken = 0;

            template<typename T>
            Node(T &&arg, size_t startToken, size_t endToken) : NodeVariant(arg), startToken(startToken) {}
        };

        double Evaluate(ReadSignalsLock lock) const;

        const std::string expr;
        std::vector<std::string_view> tokens; // string_views into expr
        std::vector<Node> nodes;
        std::vector<std::string> nodeDebug;
        int rootIndex = 0;
    };

    std::pair<ecs::Name, std::string> ParseSignalString(std::string_view str, const Name &scope = Name());
} // namespace ecs
