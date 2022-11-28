#include "SignalExpression.hh"

#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityRef.hh"

namespace ecs {
    std::pair<ecs::Name, std::string> ParseSignalString(std::string_view str, const Name &scope) {
        size_t delimiter = str.find('/');
        ecs::Name entityName(str.substr(0, delimiter), scope);
        if (entityName) {
            std::string signalName = "value";
            if (delimiter != std::string::npos) signalName = str.substr(delimiter + 1);
            return std::make_pair(entityName, signalName);
        } else {
            return std::make_pair(ecs::Name(), "");
        }
    }

    struct PrecedenceTable {
        constexpr PrecedenceTable() {
            values[0] = 6;
            values['('] = 5;
            values[','] = 5;
            values[')'] = 5;
            values['?'] = 4;
            values[':'] = 4;
            values['&'] = 3;
            values['|'] = 3;
            values['+'] = 2;
            values['-'] = 2;
            values['*'] = 1;
            values['/'] = 1;
            values['_'] = 0; // Used as unary - operator
        }

        bool Compare(char curr, char next) const {
            return values[curr] <= values[next];
        }

        int values[256] = {0};
    };

    std::string joinTokens(const SignalExpression &expr, size_t startToken, size_t endToken) {
        if (endToken >= expr.tokens.size()) endToken = expr.tokens.size() - 1;
        if (endToken <= startToken) return "";

        auto startPtr = expr.tokens[startToken].data();
        auto endPtr = expr.tokens[endToken].data() + expr.tokens[endToken].size();
        Assertf(endPtr >= startPtr && (size_t)(endPtr - startPtr) <= expr.expr.size(),
            "Token string views not in same buffer");
        return std::string(startPtr, (size_t)(endPtr - startPtr));
    }

    int parseNode(SignalExpression &expr, const Name &scope, size_t &tokenIndex, char precedence = '\x0') {
        if (tokenIndex >= expr.tokens.size()) {
            Errorf("Failed to parse signal expression, unexpected end of expression: %s", expr.expr);
            return -1;
        }
        int index = -1;

        constexpr auto precedenceLookup = PrecedenceTable();

        size_t nodeStart = tokenIndex;
        while (tokenIndex < expr.tokens.size()) {
            auto &token = expr.tokens[tokenIndex];
            if (index >= 0 && token.size() == 1 && precedenceLookup.Compare(precedence, token[0])) return index;

            if (token == "?") {
                if (index < 0) {
                    Errorf("Failed to parse signal expression, unexpected conditional '?': %s",
                        joinTokens(expr, nodeStart, tokenIndex));
                    return -1;
                }
                int ifIndex = index;
                size_t startToken = expr.nodes[index].startToken;

                tokenIndex++;
                int trueIndex = parseNode(expr, scope, tokenIndex, ':');
                if (trueIndex < 0) {
                    Errorf("Failed to parse signal expression, invalid true expression for conditional: %s",
                        joinTokens(expr, startToken, tokenIndex));
                    return -1;
                } else if (tokenIndex >= expr.tokens.size() || expr.tokens[tokenIndex] != ":") {
                    Errorf("Failed to parse signal expression, conditional missing ':': %s",
                        joinTokens(expr, startToken, tokenIndex));
                    return -1;
                }

                tokenIndex++;
                int falseIndex = parseNode(expr, scope, tokenIndex, precedence);
                if (falseIndex < 0) {
                    Errorf("Failed to parse signal expression, invalid false expression for conditional: %s",
                        joinTokens(expr, startToken, tokenIndex));
                    return -1;
                }

                index = expr.nodes.size();
                expr.nodes.emplace_back(SignalExpression::DeciderOperation{ifIndex, trueIndex, falseIndex},
                    startToken,
                    tokenIndex);
                expr.nodeDebug.emplace_back(
                    expr.nodeDebug[ifIndex] + " ? " + expr.nodeDebug[trueIndex] + " : " + expr.nodeDebug[falseIndex]);
            } else if (token == "(") {
                if (index >= 0) {
                    Errorf("Failed to parse signal expression, unexpected expression '(': %s",
                        joinTokens(expr, nodeStart, tokenIndex));
                    return -1;
                }
                size_t startToken = tokenIndex;

                tokenIndex++;
                index = parseNode(expr, scope, tokenIndex, ')');
                if (index < 0) {
                    // Errorf("Failed to parse signal expression, invalid expression: %s",
                    //     joinTokens(expr, startToken, tokenIndex));
                    return -1;
                } else if (tokenIndex >= expr.tokens.size() || expr.tokens[tokenIndex] != ")") {
                    Errorf("Failed to parse signal expression, expression missing ')': %s",
                        joinTokens(expr, startToken, tokenIndex));
                    return -1;
                }
                tokenIndex++;
                expr.nodes[index].startToken = startToken;
                expr.nodes[index].startToken = tokenIndex;
                expr.nodeDebug[index] = "( " + expr.nodeDebug[index] + " )";
            } else if (token == "-" && index < 0) {
                size_t startToken = tokenIndex;

                tokenIndex++;
                int inputIndex = parseNode(expr, scope, tokenIndex, '_');
                if (inputIndex < 0) {
                    // Errorf("Failed to parse signal expression, invalid expression: %s",
                    //     joinTokens(expr, startToken, tokenIndex));
                    return -1;
                }

                auto *constantNode = std::get_if<SignalExpression::ConstantNode>(&expr.nodes[inputIndex]);
                if (constantNode) {
                    constantNode->value *= -1;
                    index = inputIndex;
                    expr.nodes[index].startToken = startToken;
                    expr.nodeDebug[index] = "-" + expr.nodeDebug[index];
                } else {
                    index = expr.nodes.size();
                    expr.nodes.emplace_back(SignalExpression::OneInputOperation{inputIndex,
                                                [](double input) {
                                                    return -input;
                                                }},
                        startToken,
                        tokenIndex);
                    expr.nodeDebug.emplace_back("-" + expr.nodeDebug[inputIndex]);
                }
            } else if (token == "sin" || token == "cos" || token == "tan") {
                if (index >= 0) {
                    Errorf("Failed to parse signal expression, unexpected function '%s': %s",
                        std::string(token),
                        joinTokens(expr, nodeStart, tokenIndex));
                    return -1;
                }
                size_t startToken = tokenIndex;

                tokenIndex++;
                if (tokenIndex >= expr.tokens.size() || expr.tokens[tokenIndex] != "(") {
                    Errorf("Failed to parse signal expression, '%s' function missing open brace: %s",
                        std::string(token),
                        joinTokens(expr, startToken, tokenIndex));
                    return -1;
                }

                tokenIndex++;
                int inputIndex = parseNode(expr, scope, tokenIndex, ')');
                if (inputIndex < 0) {
                    Errorf("Failed to parse signal expression, invalid argument to function '%s': %s",
                        std::string(token),
                        joinTokens(expr, startToken, tokenIndex));
                    return -1;
                } else if (tokenIndex >= expr.tokens.size() || expr.tokens[tokenIndex] != ")") {
                    Errorf("Failed to parse signal expression, '%s' function missing close brace: %s",
                        std::string(token),
                        joinTokens(expr, startToken, tokenIndex));
                    return -1;
                }
                tokenIndex++;

                index = expr.nodes.size();
                if (token == "sin") {
                    expr.nodes.emplace_back(SignalExpression::OneInputOperation{inputIndex,
                                                [](double input) {
                                                    return std::sin(input);
                                                }},
                        startToken,
                        tokenIndex);
                } else if (token == "cos") {
                    expr.nodes.emplace_back(SignalExpression::OneInputOperation{inputIndex,
                                                [](double input) {
                                                    return std::cos(input);
                                                }},
                        startToken,
                        tokenIndex);
                } else if (token == "tan") {
                    expr.nodes.emplace_back(SignalExpression::OneInputOperation{inputIndex,
                                                [](double input) {
                                                    return std::tan(input);
                                                }},
                        startToken,
                        tokenIndex);
                } else {
                    Abortf("Invalid function token: %s", std::string(token));
                }

                expr.nodeDebug.emplace_back(std::string(token) + "( " + expr.nodeDebug[inputIndex] + " )");
            } else if (token == "min" || token == "max") {
                if (index >= 0) {
                    Errorf("Failed to parse signal expression, unexpected function '%s': %s",
                        std::string(token),
                        joinTokens(expr, nodeStart, tokenIndex));
                    return -1;
                }
                size_t startToken = tokenIndex;

                tokenIndex++;
                if (tokenIndex >= expr.tokens.size() || expr.tokens[tokenIndex] != "(") {
                    Errorf("Failed to parse signal expression, '%s' function missing open brace: %s",
                        std::string(token),
                        joinTokens(expr, startToken, tokenIndex));
                    return -1;
                }

                tokenIndex++;
                int aIndex = parseNode(expr, scope, tokenIndex, ',');
                if (aIndex < 0) {
                    Errorf("Failed to parse signal expression, invalid 1st argument to function: '%s': %s",
                        std::string(token),
                        joinTokens(expr, startToken, tokenIndex));
                    return -1;
                } else if (tokenIndex >= expr.tokens.size() || expr.tokens[tokenIndex] != ",") {
                    Errorf("Failed to parse signal expression, '%s' function expects 2 arguments: %s",
                        std::string(token),
                        joinTokens(expr, startToken, tokenIndex));
                    return -1;
                }

                tokenIndex++;
                int bIndex = parseNode(expr, scope, tokenIndex, ')');
                if (bIndex < 0) {
                    Errorf("Failed to parse signal expression, invalid 2nd argument to function '%s': %s",
                        std::string(token),
                        joinTokens(expr, startToken, tokenIndex));
                    return -1;
                } else if (tokenIndex >= expr.tokens.size() || expr.tokens[tokenIndex] != ")") {
                    Errorf("Failed to parse signal expression, '%s' function missing close brace: %s",
                        std::string(token),
                        joinTokens(expr, startToken, tokenIndex));
                    return -1;
                }
                tokenIndex++;

                index = expr.nodes.size();
                if (token == "min") {
                    expr.nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                                bIndex,
                                                [](double a, double b) {
                                                    return std::min(a, b);
                                                }},
                        startToken,
                        tokenIndex);
                } else if (token == "max") {
                    expr.nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                                bIndex,
                                                [](double a, double b) {
                                                    return std::max(a, b);
                                                }},
                        startToken,
                        tokenIndex);
                } else {
                    Abortf("Invalid function token: %s", std::string(token));
                }

                expr.nodeDebug.emplace_back(
                    std::string(token) + "( " + expr.nodeDebug[aIndex] + " , " + expr.nodeDebug[bIndex] + " )");
            } else if (token == "+" || token == "-" || token == "*" || token == "/" || token == "&&" || token == "||") {
                if (index < 0) {
                    Errorf("Failed to parse signal expression, unexpected operator '%s': %s",
                        std::string(token),
                        joinTokens(expr, nodeStart, tokenIndex));
                    return -1;
                }

                size_t startToken = tokenIndex;
                int aIndex = index;

                tokenIndex++;
                int bIndex = parseNode(expr, scope, tokenIndex, token[0]);
                if (bIndex < 0) {
                    Errorf("Failed to parse signal expression, invalid 2nd argument to operator '%s': %s",
                        std::string(token),
                        joinTokens(expr, startToken, tokenIndex));
                    return -1;
                }

                index = expr.nodes.size();
                if (token == "+") {
                    expr.nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                                bIndex,
                                                [](double a, double b) {
                                                    return a + b;
                                                }},
                        startToken,
                        tokenIndex);
                } else if (token == "-") {
                    expr.nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                                bIndex,
                                                [](double a, double b) {
                                                    return a - b;
                                                }},
                        startToken,
                        tokenIndex);
                } else if (token == "*") {
                    expr.nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                                bIndex,
                                                [](double a, double b) {
                                                    return a * b;
                                                }},
                        startToken,
                        tokenIndex);
                } else if (token == "/") {
                    expr.nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                                bIndex,
                                                [](double a, double b) {
                                                    return a / b;
                                                }},
                        startToken,
                        tokenIndex);
                } else if (token == "&&") {
                    expr.nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                                bIndex,
                                                [](double a, double b) {
                                                    return a >= 0.5 && b >= 0.5;
                                                }},
                        startToken,
                        tokenIndex);
                } else if (token == "||") {
                    expr.nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                                bIndex,
                                                [](double a, double b) {
                                                    return a >= 0.5 || b >= 0.5;
                                                }},
                        startToken,
                        tokenIndex);
                } else {
                    Abortf("Invalid operator token: %s", std::string(token));
                }

                expr.nodeDebug.emplace_back(
                    expr.nodeDebug[aIndex] + " " + std::string(token) + " " + expr.nodeDebug[bIndex]);
            } else if (sp::is_float(token)) {
                if (index >= 0) {
                    Errorf("Failed to parse signal expression, unexpected constant '%s': %s",
                        std::string(token),
                        joinTokens(expr, nodeStart, tokenIndex));
                    return -1;
                }

                index = expr.nodes.size();
                expr.nodes.emplace_back(SignalExpression::ConstantNode{std::stod(std::string(token))},
                    tokenIndex,
                    tokenIndex + 1);
                expr.nodeDebug.emplace_back(token);
                tokenIndex++;
            } else if (token == ")" || token == "," || token == ":") {
                Errorf("Failed to parse signal expression, unexpected token '%s': %s",
                    std::string(token),
                    joinTokens(expr, nodeStart, tokenIndex));
                return -1;
            } else {
                if (index >= 0) {
                    Errorf("Failed to parse signal expression, unexpected signal '%s': %s",
                        std::string(token),
                        joinTokens(expr, nodeStart, tokenIndex));
                    return -1;
                }

                auto [entityName, signalName] = ParseSignalString(token, scope);
                index = expr.nodes.size();
                expr.nodes.emplace_back(SignalExpression::SignalNode{entityName, signalName},
                    tokenIndex,
                    tokenIndex + 1);
                expr.nodeDebug.emplace_back(token);
                tokenIndex++;
            }
        }
        Assertf(tokenIndex >= expr.tokens.size(),
            "parseNode failed to parse all tokens: %u<%u",
            tokenIndex,
            expr.tokens.size());
        if (index < 0) {
            Errorf("Failed to parse signal expression, blank expression: %s", joinTokens(expr, nodeStart, tokenIndex));
        } else if (precedence == ')' || precedence == ',' || precedence == ':') {
            Errorf("Failed to parse signal expression, missing end token '%c': %s", precedence, expr.expr);
        }
        return index;
    }

    SignalExpression::SignalExpression(const Name &entityName, const std::string &signalName)
        : expr(entityName.String() + "/" + signalName) {
        tokens.emplace_back(expr);
        nodes.emplace_back(SignalNode{entityName, signalName}, 0, 0);
        nodeDebug.emplace_back(expr);
    }

    SignalExpression::SignalExpression(std::string_view expr, const Name &scope) : expr(expr) {
        // Tokenize the expression before parsing
        std::string_view exprView = this->expr;
        size_t tokenStart = 0;
        for (size_t tokenEnd = 0; tokenEnd < exprView.size(); tokenEnd++) {
            const char &ch = exprView[tokenEnd];
            if (std::isspace(ch)) {
                if (tokenEnd > tokenStart) tokens.emplace_back(exprView.substr(tokenStart, tokenEnd - tokenStart));
                tokenStart = tokenEnd + 1;
            } else if (ch == '(' || ch == ')' || ch == ',') {
                if (tokenEnd > tokenStart) tokens.emplace_back(exprView.substr(tokenStart, tokenEnd - tokenStart));
                tokens.emplace_back(exprView.substr(tokenEnd, 1));
                tokenStart = tokenEnd + 1;
            } else if (tokenEnd == tokenStart) {
                if (ch == '+' || ch == '-' || ch == '*' || ch == '/') {
                    tokens.emplace_back(exprView.substr(tokenEnd, 1));
                    tokenStart = tokenEnd + 1;
                }
            }
        }
        if (tokenStart < exprView.size()) tokens.emplace_back(exprView.substr(tokenStart));

        // Debugf("Expr: %s", expr);
        // for (auto &token : tokens) {
        //     Debugf("  %s", std::string(token));
        // }

        size_t tokenIndex = 0;
        rootIndex = parseNode(*this, scope, tokenIndex);
        if (rootIndex < 0) {
            Errorf("Failed to parse expression: %s", this->expr);
        } else {
            Assertf(tokenIndex == tokens.size(), "Failed to parse signal expression, incomplete parse: %s", exprView);
        }
    }

    double evaluateNode(ReadSignalsLock lock, const SignalExpression &expr, int nodeIndex) {
        if (nodeIndex < 0 || nodeIndex >= expr.nodes.size()) return 0.0f;

        double result = std::visit(
            [&](auto &&node) {
                using T = std::decay_t<decltype(node)>;
                if constexpr (std::is_same_v<T, SignalExpression::ConstantNode>) {
                    return node.value;
                } else if constexpr (std::is_same_v<T, SignalExpression::SignalNode>) {
                    return SignalBindings::GetSignal(lock, EntityRef(node.entityName).Get(lock), node.signalName);
                } else if constexpr (std::is_same_v<T, SignalExpression::OneInputOperation>) {
                    return node.evaluate(evaluateNode(lock, expr, node.inputIndex));
                } else if constexpr (std::is_same_v<T, SignalExpression::TwoInputOperation>) {
                    return node.evaluate(evaluateNode(lock, expr, node.inputIndexA),
                        evaluateNode(lock, expr, node.inputIndexB));
                } else if constexpr (std::is_same_v<T, SignalExpression::DeciderOperation>) {
                    double ifValue = evaluateNode(lock, expr, node.ifIndex);
                    if (ifValue >= 0.5) {
                        return evaluateNode(lock, expr, node.trueIndex);
                    } else {
                        return evaluateNode(lock, expr, node.falseIndex);
                    }
                }
            },
            expr.nodes[nodeIndex]);

        // Debugf("Eval '%s' = %f", expr.nodeDebug[nodeIndex], result);
        if (!std::isfinite(result)) {
            Warnf("Signal expression evaluation error: %s = %f", expr.nodeDebug[nodeIndex], result);
            return 0.0;
        }
        return result;
    };

    double SignalExpression::Evaluate(ReadSignalsLock lock) const {
        if (nodes.empty() || rootIndex < 0) return 0.0f;
        return evaluateNode(lock, *this, rootIndex);
    }
} // namespace ecs
