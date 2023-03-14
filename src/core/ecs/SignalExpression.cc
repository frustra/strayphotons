#include "SignalExpression.hh"

#include "assets/JsonHelpers.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityRef.hh"

namespace ecs {
    std::pair<ecs::Name, std::string> ParseSignalString(std::string_view str, const EntityScope &scope) {
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
            // Right associative unary operators (-X and !X)
            values[u8'_'] = 1;

            // Math operators
            values[u8'*'] = 2;
            values[u8'/'] = 2;
            values[u8'+'] = 3;
            values[u8'-'] = 3;

            // Comparison operators
            values[u8'>'] = 4;
            values[u8'<'] = 4;
            values[u8'='] = 4;
            values[u8'!'] = 4;

            // Boolean operators
            values[u8'&'] = 5;
            values[u8'|'] = 5;

            // Branch operators
            values[u8'?'] = 6;
            values[u8':'] = 6;

            // Function and expression braces
            values[u8'('] = 7;
            values[u8','] = 7;
            values[u8')'] = 7;

            values[0] = 8;
        }

        bool Compare(uint8_t curr, uint8_t next) const {
            int nextPrecedence = values[next];
            if (nextPrecedence == 0) return false;
            return values[curr] <= values[next];
        }

        int values[256] = {0};
    };

    std::string SignalExpression::joinTokens(size_t startToken, size_t endToken) const {
        if (endToken >= tokens.size()) endToken = tokens.size() - 1;
        if (endToken < startToken) return "";

        auto startPtr = tokens[startToken].data();
        auto endPtr = tokens[endToken].data() + tokens[endToken].size();
        Assertf(endPtr >= startPtr && (size_t)(endPtr - startPtr) <= expr.size(),
            "Token string views not in same buffer");
        return std::string(startPtr, (size_t)(endPtr - startPtr));
    }

    int SignalExpression::parseNode(size_t &tokenIndex, uint8_t precedence) {
        if (tokenIndex >= tokens.size()) {
            Errorf("Failed to parse signal expression, unexpected end of expression: %s", expr);
            return -1;
        }
        int index = -1;

        constexpr auto precedenceLookup = PrecedenceTable();

        size_t nodeStart = tokenIndex;
        while (tokenIndex < tokens.size()) {
            auto &token = tokens[tokenIndex];
            if (index >= 0 && token.size() >= 1 && precedenceLookup.Compare(precedence, token[0])) return index;

            if (token == "?") {
                if (index < 0) {
                    Errorf("Failed to parse signal expression, unexpected conditional '?': %s",
                        joinTokens(nodeStart, tokenIndex));
                    return -1;
                }
                int ifIndex = index;
                size_t startToken = nodes[index].startToken;

                tokenIndex++;
                int trueIndex = parseNode(tokenIndex, ':');
                if (trueIndex < 0) {
                    Errorf("Failed to parse signal expression, invalid true expression for conditional: %s",
                        joinTokens(startToken, tokenIndex));
                    return -1;
                } else if (tokenIndex >= tokens.size() || tokens[tokenIndex] != ":") {
                    Errorf("Failed to parse signal expression, conditional missing ':': %s",
                        joinTokens(startToken, tokenIndex));
                    return -1;
                }

                tokenIndex++;
                int falseIndex = parseNode(tokenIndex, precedence);
                if (falseIndex < 0) {
                    Errorf("Failed to parse signal expression, invalid false expression for conditional: %s",
                        joinTokens(startToken, tokenIndex));
                    return -1;
                }

                index = nodes.size();
                nodes.emplace_back(SignalExpression::DeciderOperation{ifIndex, trueIndex, falseIndex},
                    startToken,
                    tokenIndex);
                nodeStrings.emplace_back(
                    nodeStrings[ifIndex] + " ? " + nodeStrings[trueIndex] + " : " + nodeStrings[falseIndex]);
            } else if (token == "(") {
                if (index >= 0) {
                    Errorf("Failed to parse signal expression, unexpected expression '(': %s",
                        joinTokens(nodeStart, tokenIndex));
                    return -1;
                }
                size_t startToken = tokenIndex;

                tokenIndex++;
                index = parseNode(tokenIndex, ')');
                if (index < 0) {
                    // Errorf("Failed to parse signal expression, invalid expression: %s",
                    //     joinTokens(startToken, tokenIndex));
                    return -1;
                } else if (tokenIndex >= tokens.size() || tokens[tokenIndex] != ")") {
                    Errorf("Failed to parse signal expression, expression missing ')': %s",
                        joinTokens(startToken, tokenIndex));
                    return -1;
                }
                tokenIndex++;
                nodes[index].startToken = startToken;
                nodes[index].startToken = tokenIndex;
                nodeStrings[index] = "( " + nodeStrings[index] + " )";
            } else if (index < 0 && (token == "-" || token == "!")) {
                size_t startToken = tokenIndex;

                tokenIndex++;
                int inputIndex = parseNode(tokenIndex, '_');
                if (inputIndex < 0) {
                    // Errorf("Failed to parse signal expression, invalid expression: %s",
                    //     joinTokens(startToken, tokenIndex));
                    return -1;
                }

                auto *constantNode = std::get_if<SignalExpression::ConstantNode>(&nodes[inputIndex]);
                if (token == "-") {
                    if (constantNode) {
                        constantNode->value *= -1;
                        index = inputIndex;
                        nodes[index].startToken = startToken;
                        nodeStrings[index] = "-" + nodeStrings[index];
                    } else {
                        index = nodes.size();
                        nodes.emplace_back(SignalExpression::OneInputOperation{inputIndex,
                                               [](double input) {
                                                   return -input;
                                               }},
                            startToken,
                            tokenIndex);
                        nodeStrings.emplace_back("-" + nodeStrings[inputIndex]);
                    }
                } else if (token == "!") {
                    if (constantNode) {
                        constantNode->value = constantNode->value >= 0.5 ? 0.0 : 1.0;
                        index = inputIndex;
                        nodes[index].startToken = startToken;
                        nodeStrings[index] = "!" + nodeStrings[index];
                    } else {
                        index = nodes.size();
                        nodes.emplace_back(SignalExpression::OneInputOperation{inputIndex,
                                               [](double input) {
                                                   return input >= 0.5 ? 0.0 : 1.0;
                                               }},
                            startToken,
                            tokenIndex);
                        nodeStrings.emplace_back("!" + nodeStrings[inputIndex]);
                    }
                }
            } else if (token == "sin" || token == "cos" || token == "tan" || token == "floor" || token == "ceil" ||
                       token == "abs") {
                if (index >= 0) {
                    Errorf("Failed to parse signal expression, unexpected function '%s': %s",
                        std::string(token),
                        joinTokens(nodeStart, tokenIndex));
                    return -1;
                }
                size_t startToken = tokenIndex;

                tokenIndex++;
                if (tokenIndex >= tokens.size() || tokens[tokenIndex] != "(") {
                    Errorf("Failed to parse signal expression, '%s' function missing open brace: %s",
                        std::string(token),
                        joinTokens(startToken, tokenIndex));
                    return -1;
                }

                tokenIndex++;
                int inputIndex = parseNode(tokenIndex, ')');
                if (inputIndex < 0) {
                    Errorf("Failed to parse signal expression, invalid argument to function '%s': %s",
                        std::string(token),
                        joinTokens(startToken, tokenIndex));
                    return -1;
                } else if (tokenIndex >= tokens.size() || tokens[tokenIndex] != ")") {
                    Errorf("Failed to parse signal expression, '%s' function missing close brace: %s",
                        std::string(token),
                        joinTokens(startToken, tokenIndex));
                    return -1;
                }
                tokenIndex++;

                index = nodes.size();
                if (token == "sin") {
                    nodes.emplace_back(SignalExpression::OneInputOperation{inputIndex,
                                           [](double input) {
                                               return std::sin(input);
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "cos") {
                    nodes.emplace_back(SignalExpression::OneInputOperation{inputIndex,
                                           [](double input) {
                                               return std::cos(input);
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "tan") {
                    nodes.emplace_back(SignalExpression::OneInputOperation{inputIndex,
                                           [](double input) {
                                               return std::tan(input);
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "floor") {
                    nodes.emplace_back(SignalExpression::OneInputOperation{inputIndex,
                                           [](double input) {
                                               return std::floor(input);
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "ceil") {
                    nodes.emplace_back(SignalExpression::OneInputOperation{inputIndex,
                                           [](double input) {
                                               return std::ceil(input);
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "abs") {
                    nodes.emplace_back(SignalExpression::OneInputOperation{inputIndex,
                                           [](double input) {
                                               return std::abs(input);
                                           }},
                        startToken,
                        tokenIndex);
                } else {
                    Abortf("Invalid function token: %s", std::string(token));
                }

                nodeStrings.emplace_back(std::string(token) + "( " + nodeStrings[inputIndex] + " )");
            } else if (token == "if_focused" || token == "min" || token == "max") {
                if (index >= 0) {
                    Errorf("Failed to parse signal expression, unexpected function '%s': %s",
                        std::string(token),
                        joinTokens(nodeStart, tokenIndex));
                    return -1;
                }
                size_t startToken = tokenIndex;

                tokenIndex++;
                if (tokenIndex >= tokens.size() || tokens[tokenIndex] != "(") {
                    Errorf("Failed to parse signal expression, '%s' function missing open brace: %s",
                        std::string(token),
                        joinTokens(startToken, tokenIndex));
                    return -1;
                }

                tokenIndex++;
                int aIndex = parseNode(tokenIndex, ',');
                if (aIndex < 0) {
                    Errorf("Failed to parse signal expression, invalid 1st argument to function: '%s': %s",
                        std::string(token),
                        joinTokens(startToken, tokenIndex));
                    return -1;
                } else if (tokenIndex >= tokens.size() || tokens[tokenIndex] != ",") {
                    Errorf("Failed to parse signal expression, '%s' function expects 2 arguments: %s",
                        std::string(token),
                        joinTokens(startToken, tokenIndex));
                    return -1;
                }

                tokenIndex++;
                int bIndex = parseNode(tokenIndex, ')');
                if (bIndex < 0) {
                    Errorf("Failed to parse signal expression, invalid 2nd argument to function '%s': %s",
                        std::string(token),
                        joinTokens(startToken, tokenIndex));
                    return -1;
                } else if (tokenIndex >= tokens.size() || tokens[tokenIndex] != ")") {
                    Errorf("Failed to parse signal expression, '%s' function missing close brace: %s",
                        std::string(token),
                        joinTokens(startToken, tokenIndex));
                    return -1;
                }
                tokenIndex++;

                index = nodes.size();
                if (token == "if_focused") {
                    auto focusStr = tokens[nodes[aIndex].startToken];
                    nodeStrings[aIndex] = focusStr;
                    FocusLayer focus = FocusLayer::Always;
                    if (!focusStr.empty()) {
                        auto opt = magic_enum::enum_cast<FocusLayer>(focusStr);
                        if (opt) {
                            focus = *opt;
                        } else {
                            Errorf("Unknown enum value specified for if_focused: %s", std::string(focusStr));
                        }
                    } else {
                        Errorf("Blank focus layer specified for if_focused: %s", std::string(focusStr));
                    }
                    nodes.emplace_back(SignalExpression::FocusCondition{focus, bIndex}, startToken, tokenIndex);
                } else if (token == "min") {
                    nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                           bIndex,
                                           [](double a, double b) {
                                               return std::min(a, b);
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "max") {
                    nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                           bIndex,
                                           [](double a, double b) {
                                               return std::max(a, b);
                                           }},
                        startToken,
                        tokenIndex);
                } else {
                    Abortf("Invalid function token: %s", std::string(token));
                }

                nodeStrings.emplace_back(
                    std::string(token) + "( " + nodeStrings[aIndex] + " , " + nodeStrings[bIndex] + " )");
            } else if (token == "+" || token == "-" || token == "*" || token == "/" || token == "&&" || token == "||" ||
                       token == ">" || token == ">=" || token == "<" || token == "<=" || token == "==" ||
                       token == "!=") {
                if (index < 0) {
                    Errorf("Failed to parse signal expression, unexpected operator '%s': %s",
                        std::string(token),
                        joinTokens(nodeStart, tokenIndex));
                    return -1;
                }

                size_t startToken = tokenIndex;
                int aIndex = index;

                tokenIndex++;
                int bIndex = parseNode(tokenIndex, token[0]);
                if (bIndex < 0) {
                    Errorf("Failed to parse signal expression, invalid 2nd argument to operator '%s': %s",
                        std::string(token),
                        joinTokens(startToken, tokenIndex));
                    return -1;
                }

                index = nodes.size();
                if (token == "+") {
                    nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                           bIndex,
                                           [](double a, double b) {
                                               return a + b;
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "-") {
                    nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                           bIndex,
                                           [](double a, double b) {
                                               return a - b;
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "*") {
                    nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                           bIndex,
                                           [](double a, double b) {
                                               return a * b;
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "/") {
                    nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                           bIndex,
                                           [](double a, double b) {
                                               return a / b;
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "&&") {
                    nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                           bIndex,
                                           [](double a, double b) {
                                               return a >= 0.5 && b >= 0.5;
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "||") {
                    nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                           bIndex,
                                           [](double a, double b) {
                                               return a >= 0.5 || b >= 0.5;
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == ">") {
                    nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                           bIndex,
                                           [](double a, double b) {
                                               return a > b;
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == ">=") {
                    nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                           bIndex,
                                           [](double a, double b) {
                                               return a >= b;
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "<") {
                    nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                           bIndex,
                                           [](double a, double b) {
                                               return a < b;
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "<=") {
                    nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                           bIndex,
                                           [](double a, double b) {
                                               return a <= b;
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "==") {
                    nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                           bIndex,
                                           [](double a, double b) {
                                               return a == b;
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "!=") {
                    nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                           bIndex,
                                           [](double a, double b) {
                                               return a != b;
                                           }},
                        startToken,
                        tokenIndex);
                } else {
                    Abortf("Invalid operator token: %s", std::string(token));
                }

                nodeStrings.emplace_back(nodeStrings[aIndex] + " " + std::string(token) + " " + nodeStrings[bIndex]);
            } else if (sp::is_float(token)) {
                if (index >= 0) {
                    Errorf("Failed to parse signal expression, unexpected constant '%s': %s",
                        std::string(token),
                        joinTokens(nodeStart, tokenIndex));
                    return -1;
                }

                index = nodes.size();
                nodes.emplace_back(SignalExpression::ConstantNode{std::stod(std::string(token))},
                    tokenIndex,
                    tokenIndex + 1);
                nodeStrings.emplace_back(token);
                tokenIndex++;
            } else if (token == ")" || token == "," || token == ":") {
                Errorf("Failed to parse signal expression, unexpected token '%s': %s",
                    std::string(token),
                    joinTokens(nodeStart, tokenIndex));
                return -1;
            } else {
                if (index >= 0) {
                    Errorf("Failed to parse signal expression, unexpected signal '%s': %s",
                        std::string(token),
                        joinTokens(nodeStart, tokenIndex));
                    return -1;
                }

                auto [entityName, signalName] = ParseSignalString(token, this->scope);
                index = nodes.size();
                nodes.emplace_back(SignalExpression::SignalNode{entityName, signalName}, tokenIndex, tokenIndex + 1);
                nodeStrings.emplace_back(entityName.String() + "/" + signalName);
                tokenIndex++;
            }
        }
        Assertf(tokenIndex >= tokens.size(), "parseNode failed to parse all tokens: %u<%u", tokenIndex, tokens.size());
        if (index < 0) {
            Errorf("Failed to parse signal expression, blank expression: %s", joinTokens(nodeStart, tokenIndex));
        } else if (precedence == ')' || precedence == ',' || precedence == ':') {
            Errorf("Failed to parse signal expression, missing end token '%c': %s", precedence, expr);
        }
        return index;
    }

    SignalExpression::SignalExpression(const EntityRef &entity, const std::string &signalName)
        : scope(entity.Name().scene, ""), expr(entity.Name().String() + "/" + signalName) {
        tokens.emplace_back(expr);
        nodes.emplace_back(SignalNode{entity, signalName}, 0, 0);
        nodeStrings.emplace_back(expr);
    }

    SignalExpression::SignalExpression(std::string_view expr, const Name &scope) : scope(scope), expr(expr) {
        Parse();
    }

    bool SignalExpression::Parse() {
        // Tokenize the expression before parsing
        std::string_view exprView = this->expr;
        tokens.clear();
        size_t tokenStart = 0;
        for (size_t tokenEnd = 0; tokenEnd < exprView.size(); tokenEnd++) {
            const char &ch = exprView[tokenEnd];
            if (tokenEnd == tokenStart + 1 && exprView[tokenStart] == '!' && ch != '=') {
                tokens.emplace_back(exprView.substr(tokenStart, 1));
                tokenStart++;
            }
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

        nodes.clear();
        nodeStrings.clear();
        size_t tokenIndex = 0;
        rootIndex = parseNode(tokenIndex);
        if (rootIndex < 0) {
            Errorf("Failed to parse expression: %s", expr);
            return false;
        }
        Assertf(tokenIndex == tokens.size(), "Failed to parse signal expression, incomplete parse: %s", exprView);
        return true;
    }

    double SignalExpression::evaluateNode(const ReadSignalsLock &lock, size_t depth, int nodeIndex) const {
        if (nodeIndex < 0 || (size_t)nodeIndex >= nodes.size()) return 0.0f;

        double result = std::visit(
            [&](auto &&node) {
                using T = std::decay_t<decltype(node)>;
                if constexpr (std::is_same_v<T, SignalExpression::ConstantNode>) {
                    return node.value;
                } else if constexpr (std::is_same_v<T, SignalExpression::SignalNode>) {
                    return SignalBindings::GetSignal(lock, node.entity.Get(lock), node.signalName, depth + 1);
                } else if constexpr (std::is_same_v<T, SignalExpression::FocusCondition>) {
                    if (!lock.Has<FocusLock>() || !lock.Get<FocusLock>().HasPrimaryFocus(node.ifFocused)) {
                        return 0.0;
                    } else {
                        return evaluateNode(lock, depth, node.inputIndex);
                    }
                } else if constexpr (std::is_same_v<T, SignalExpression::OneInputOperation>) {
                    return node.evaluate(evaluateNode(lock, depth, node.inputIndex));
                } else if constexpr (std::is_same_v<T, SignalExpression::TwoInputOperation>) {
                    return node.evaluate(evaluateNode(lock, depth, node.inputIndexA),
                        evaluateNode(lock, depth, node.inputIndexB));
                } else if constexpr (std::is_same_v<T, SignalExpression::DeciderOperation>) {
                    double ifValue = evaluateNode(lock, depth, node.ifIndex);
                    if (ifValue >= 0.5) {
                        return evaluateNode(lock, depth, node.trueIndex);
                    } else {
                        return evaluateNode(lock, depth, node.falseIndex);
                    }
                } else {
                    Abortf("Invalid signal operation: %s", typeid(T).name());
                }
            },
            (SignalExpression::NodeVariant)nodes[nodeIndex]);

        // if (!std::holds_alternative<SignalExpression::ConstantNode>(nodes[nodeIndex])) {
        //     Debugf("     '%s' = %f", nodeStrings[nodeIndex], result);
        // }
        if (!std::isfinite(result)) {
            Warnf("Signal expression evaluation error: %s = %f", nodeStrings[nodeIndex], result);
            return 0.0;
        }
        return result;
    };

    double SignalExpression::Evaluate(ReadSignalsLock lock, size_t depth) const {
        // Debugf("Eval '%s'", expr);
        return evaluateNode(lock, depth, rootIndex);
    }

    template<>
    bool StructMetadata::Load<SignalExpression>(const EntityScope &scope,
        SignalExpression &dst,
        const picojson::value &src) {
        if (src.is<std::string>()) {
            dst = ecs::SignalExpression(src.get<std::string>(), scope);
            return !dst.nodes.empty();
        } else {
            Errorf("Invalid signal expression: %s", src.to_str());
            return false;
        }
    }

    template<>
    void StructMetadata::Save<SignalExpression>(const EntityScope &scope,
        picojson::value &dst,
        const SignalExpression &src,
        const SignalExpression &def) {
        if (src.scope != scope) {
            // TODO: Remap signal names to new scope instead of converting to fully qualified names
            // Warnf("Saving signal expression with missmatched scope: `%s`, scope '%s' != '%s'",
            //     src.expr,
            //     src.scope.String(),
            //     scope.String());
            DebugAssertf(src.rootIndex >= 0 && (size_t)src.rootIndex < src.nodeStrings.size(),
                "Saving invalid signal expression");
            dst = picojson::value(src.nodeStrings[src.rootIndex]);
        } else {
            dst = picojson::value(src.expr);
        }
    }
} // namespace ecs
