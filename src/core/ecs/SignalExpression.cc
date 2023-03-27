#include "SignalExpression.hh"

#include "assets/JsonHelpers.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/Components.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityRef.hh"
#include "ecs/SignalStructAccess.hh"

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

    int SignalExpression::deduplicateNode(int index) {
        if (index < 0) return index;
        for (int i = 0; i < (int)nodes.size(); i++) {
            if (i != index && nodes[i] == nodes[index]) {
                nodes.erase(nodes.begin() + index);
                nodeStrings.erase(nodeStrings.begin() + index);
                Assertf(i < index, "Deduped invalid node index: %d < %d", i, index);
                return i;
            }
        }
        return index;
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
            if (index >= 0) {
                index = deduplicateNode(index);
                if (token.size() >= 1 && precedenceLookup.Compare(precedence, token[0])) return index;
            }

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
                        index = nodes.size();
                        nodes.emplace_back(SignalExpression::ConstantNode{constantNode->value * -1},
                            startToken,
                            tokenIndex);
                        nodeStrings.emplace_back("-" + nodeStrings[inputIndex]);
                    } else {
                        index = nodes.size();
                        nodes.emplace_back(SignalExpression::OneInputOperation{inputIndex,
                                               [](double input) -> double {
                                                   return -input;
                                               }},
                            startToken,
                            tokenIndex);
                        nodeStrings.emplace_back("-" + nodeStrings[inputIndex]);
                    }
                } else if (token == "!") {
                    if (constantNode) {
                        index = nodes.size();
                        nodes.emplace_back(SignalExpression::ConstantNode{constantNode->value >= 0.5 ? 0.0 : 1.0},
                            startToken,
                            tokenIndex);
                        nodeStrings.emplace_back("!" + nodeStrings[inputIndex]);
                    } else {
                        index = nodes.size();
                        nodes.emplace_back(SignalExpression::OneInputOperation{inputIndex,
                                               [](double input) -> double {
                                                   return input >= 0.5 ? 0.0 : 1.0;
                                               }},
                            startToken,
                            tokenIndex);
                        nodeStrings.emplace_back("!" + nodeStrings[inputIndex]);
                    }
                }
            } else if (token == "is_focused" || token == "sin" || token == "cos" || token == "tan" ||
                       token == "floor" || token == "ceil" || token == "abs") {
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
                if (token == "is_focused") {
                    auto focusStr = tokens[nodes[inputIndex].startToken];
                    nodeStrings[inputIndex] = focusStr;
                    FocusLayer focus = FocusLayer::Always;
                    if (!focusStr.empty()) {
                        auto opt = magic_enum::enum_cast<FocusLayer>(focusStr);
                        if (opt) {
                            focus = *opt;
                        } else {
                            Errorf("Unknown enum value specified for is_focused: %s", std::string(focusStr));
                        }
                    } else {
                        Errorf("Blank focus layer specified for is_focused: %s", std::string(focusStr));
                    }
                    nodes.emplace_back(SignalExpression::FocusCondition{focus, -1}, startToken, tokenIndex);
                } else if (token == "sin") {
                    nodes.emplace_back(SignalExpression::OneInputOperation{inputIndex,
                                           [](double input) -> double {
                                               return std::sin(input);
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "cos") {
                    nodes.emplace_back(SignalExpression::OneInputOperation{inputIndex,
                                           [](double input) -> double {
                                               return std::cos(input);
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "tan") {
                    nodes.emplace_back(SignalExpression::OneInputOperation{inputIndex,
                                           [](double input) -> double {
                                               return std::tan(input);
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "floor") {
                    nodes.emplace_back(SignalExpression::OneInputOperation{inputIndex,
                                           [](double input) -> double {
                                               return std::floor(input);
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "ceil") {
                    nodes.emplace_back(SignalExpression::OneInputOperation{inputIndex,
                                           [](double input) -> double {
                                               return std::ceil(input);
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "abs") {
                    nodes.emplace_back(SignalExpression::OneInputOperation{inputIndex,
                                           [](double input) -> double {
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
                                           [](double a, double b) -> double {
                                               return std::min(a, b);
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "max") {
                    nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                           bIndex,
                                           [](double a, double b) -> double {
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
                                           [](double a, double b) -> double {
                                               return a + b;
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "-") {
                    nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                           bIndex,
                                           [](double a, double b) -> double {
                                               return a - b;
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "*") {
                    nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                           bIndex,
                                           [](double a, double b) -> double {
                                               return a * b;
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "/") {
                    nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                           bIndex,
                                           [](double a, double b) -> double {
                                               return a / b;
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "&&") {
                    nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                           bIndex,
                                           [](double a, double b) -> double {
                                               return a >= 0.5 && b >= 0.5;
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "||") {
                    nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                           bIndex,
                                           [](double a, double b) -> double {
                                               return a >= 0.5 || b >= 0.5;
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == ">") {
                    nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                           bIndex,
                                           [](double a, double b) -> double {
                                               return a > b;
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == ">=") {
                    nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                           bIndex,
                                           [](double a, double b) -> double {
                                               return a >= b;
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "<") {
                    nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                           bIndex,
                                           [](double a, double b) -> double {
                                               return a < b;
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "<=") {
                    nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                           bIndex,
                                           [](double a, double b) -> double {
                                               return a <= b;
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "==") {
                    nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                           bIndex,
                                           [](double a, double b) -> double {
                                               return a == b;
                                           }},
                        startToken,
                        tokenIndex);
                } else if (token == "!=") {
                    nodes.emplace_back(SignalExpression::TwoInputOperation{aIndex,
                                           bIndex,
                                           [](double a, double b) -> double {
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
                    Errorf("Failed to parse signal expression, unexpected identifier/signal '%s': %s",
                        std::string(token),
                        joinTokens(nodeStart, tokenIndex));
                    return -1;
                }

                index = nodes.size();

                auto delimiter = token.find_first_of("/#");
                if (delimiter >= token.length()) {
                    if (token == "event" || sp::starts_with(token, "event.")) {
                        auto field = GetStructField(typeid(EventData), token);
                        if (field) {
                            nodes.emplace_back(SignalExpression::IdentifierNode{*field}, tokenIndex, tokenIndex + 1);
                            nodeStrings.emplace_back(token);
                        } else {
                            Errorf("Failed to parse signal expression, unexpected identifier '%s': %s",
                                std::string(token),
                                joinTokens(nodeStart, tokenIndex));
                            return -1;
                        }
                    } else {
                        StructField field(std::string(token), typeid(double), 0, FieldAction::None);
                        nodes.emplace_back(SignalExpression::IdentifierNode{field}, tokenIndex, tokenIndex + 1);
                        nodeStrings.emplace_back(token);
                    }
                } else if (token[delimiter] == '/') {
                    ecs::Name entityName(token.substr(0, delimiter), this->scope);
                    std::string signalName(token.substr(delimiter + 1));
                    nodes.emplace_back(SignalExpression::SignalNode{entityName, signalName},
                        tokenIndex,
                        tokenIndex + 1);
                    nodeStrings.emplace_back(entityName.String() + "/" + signalName);
                } else if (token[delimiter] == '#') {
                    ecs::Name entityName(token.substr(0, delimiter), this->scope);
                    std::string componentPath(token.substr(delimiter + 1));
                    std::string componentName = componentPath.substr(0, componentPath.find('.'));

                    auto *componentBase = LookupComponent(componentName);
                    if (!componentBase) {
                        Errorf("Failed to parse signal expression, unknown component '%s': %s",
                            std::string(token),
                            componentName);
                        return -1;
                    }
                    auto componentField = GetStructField(componentBase->metadata.type, componentPath);
                    if (!componentField) {
                        Errorf("Failed to parse signal expression, unknown component field '%s': %s",
                            componentName,
                            componentPath);
                        return -1;
                    }
                    nodes.emplace_back(SignalExpression::ComponentNode{entityName, componentBase, *componentField},
                        tokenIndex,
                        tokenIndex + 1);
                    nodeStrings.emplace_back(entityName.String() + "#" + componentPath);
                } else {
                    Abortf("Unexpected delimiter: '%c' %s", token[delimiter], std::string(token));
                }
                tokenIndex++;
            }
        }
        Assertf(tokenIndex >= tokens.size(), "parseNode failed to parse all tokens: %u<%u", tokenIndex, tokens.size());
        if (index < 0) {
            Errorf("Failed to parse signal expression, blank expression: %s", joinTokens(nodeStart, tokenIndex));
            return -1;
        } else if (precedence == ')' || precedence == ',' || precedence == ':') {
            Errorf("Failed to parse signal expression, missing end token '%c': %s", precedence, expr);
            return -1;
        }
        return deduplicateNode(index);
    }

    SignalExpression::SignalExpression(const EntityRef &entity, const std::string &signalName)
        : scope(entity.Name().scene, ""), expr(entity.Name().String() + "/" + signalName) {
        tokens.emplace_back(expr);
        rootIndex = nodes.size();
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

    bool SignalExpression::canEvaluate(DynamicLock<ReadSignalsLock> lock) const {
        for (auto &node : nodes) {
            bool success = std::visit(
                [&lock](auto &&node) {
                    using T = std::decay_t<decltype(node)>;
                    if constexpr (std::is_same_v<T, SignalExpression::SignalNode>) {
                        Entity ent = node.entity.Get(lock);
                        if (!ent.Has<SignalBindings>(lock)) return true;

                        auto &bindings = ent.Get<const SignalBindings>(lock);
                        return bindings.GetBinding(node.signalName).canEvaluate(lock);
                    } else if constexpr (std::is_same_v<T, SignalExpression::ComponentNode>) {
                        const ComponentBase *base = node.component;
                        if (!base) return true;
                        Entity ent = node.entity.Get(lock);
                        if (!ent) return true;
                        return GetFieldType(base->metadata.type, [&](auto *typePtr) {
                            using T = std::remove_pointer_t<decltype(typePtr)>;
                            if constexpr (!ECS::IsComponent<T>() || Tecs::is_global_component<T>()) {
                                return true;
                            } else {
                                return lock.TryLock<Read<T>>().has_value();
                            }
                        });
                    } else if constexpr (std::is_same_v<T, SignalExpression::FocusCondition>) {
                        return lock.template Has<FocusLock>();
                    } else {
                        return true;
                    }
                },
                (SignalExpression::NodeVariant)node);
            if (!success) return false;
        }
        return true;
    }

    struct ExpressionEvaluator {
        const SignalExpression &expr;
        sp::InlineVector<double, 1024> cache;
        size_t depth = 0;

        ExpressionEvaluator(const SignalExpression &expr, size_t depth = 0) : expr(expr), depth(depth) {
            cache.resize(expr.nodes.size());
            cache.fill(-std::numeric_limits<double>::infinity());
        }

        template<typename LockType, typename InputType>
        double EvaluateNode(const LockType &lock, int nodeIndex, const InputType &input) {
            if (nodeIndex < 0 || (size_t)nodeIndex >= expr.nodes.size()) return 0.0f;
            if (cache[nodeIndex] != -std::numeric_limits<double>::infinity()) return cache[nodeIndex];

            double result = std::visit(
                [&](auto &&node) {
                    using T = std::decay_t<decltype(node)>;
                    if constexpr (std::is_same_v<T, SignalExpression::ConstantNode>) {
                        return node.value;
                    } else if constexpr (std::is_same_v<T, SignalExpression::IdentifierNode>) {
                        // ZoneScopedN("IdentifierNode:evaluate");
                        if (node.field.type != typeid(InputType)) {
                            Warnf("SignalExpression can't convert %s from %s to %s",
                                expr.nodeStrings[nodeIndex],
                                node.field.type.name(),
                                typeid(InputType).name());
                            return 0.0;
                        }
                        return ecs::ReadStructField(&input, node.field);
                    } else if constexpr (std::is_same_v<T, SignalExpression::SignalNode>) {
                        // ZoneScopedN("SignalNode:evaluate");
                        return SignalBindings::GetSignal(lock, node.entity.Get(lock), node.signalName, depth + 1);
                    } else if constexpr (std::is_same_v<T, SignalExpression::ComponentNode>) {
                        const ComponentBase *base = node.component;
                        if (!base) return 0.0;
                        Entity ent = node.entity.Get(lock);
                        if (!ent) return 0.0;
                        // ZoneScopedN("ComponentNode:evaluate");
                        return GetFieldType(base->metadata.type, [&](auto *typePtr) {
                            using T = std::remove_pointer_t<decltype(typePtr)>;
                            if constexpr (!ECS::IsComponent<T>() || Tecs::is_global_component<T>()) {
                                Warnf("SignalExpression can't evaluate component type: %s", typeid(T).name());
                                return 0.0;
                            } else if constexpr (Tecs::is_read_allowed<T, LockType>()) {
                                auto &component = ent.Get<const T>(lock);
                                return ecs::ReadStructField(&component, node.field);
                            } else {
                                auto tryLock = lock.template TryLock<Read<T>>();
                                if (tryLock) {
                                    auto &component = ent.Get<const T>(*tryLock);
                                    return ecs::ReadStructField(&component, node.field);
                                } else {
                                    Warnf("SignalExpression can't evaluate component '%s' without lock: %s",
                                        node.field.name,
                                        typeid(T).name());
                                    return 0.0;
                                }
                            }
                        });
                    } else if constexpr (std::is_same_v<T, SignalExpression::FocusCondition>) {
                        // ZoneScopedN("FocusCondition:evaluate");
                        if (!lock.template Has<FocusLock>() ||
                            !lock.template Get<FocusLock>().HasPrimaryFocus(node.ifFocused)) {
                            return 0.0;
                        } else if (node.inputIndex < 0) {
                            return 1.0;
                        } else {
                            return EvaluateNode(lock, node.inputIndex, input);
                        }
                    } else if constexpr (std::is_same_v<T, SignalExpression::OneInputOperation>) {
                        // ZoneScopedN("OneInputOperation:evaluate");
                        return node.evaluate(EvaluateNode(lock, node.inputIndex, input));
                    } else if constexpr (std::is_same_v<T, SignalExpression::TwoInputOperation>) {
                        // ZoneScopedN("TwoInputOperation:evaluate");
                        return node.evaluate(EvaluateNode(lock, node.inputIndexA, input),
                            EvaluateNode(lock, node.inputIndexB, input));
                    } else if constexpr (std::is_same_v<T, SignalExpression::DeciderOperation>) {
                        // ZoneScopedN("DeciderOperation:evaluate");
                        double ifValue = EvaluateNode(lock, node.ifIndex, input);
                        if (ifValue >= 0.5) {
                            return EvaluateNode(lock, node.trueIndex, input);
                        } else {
                            return EvaluateNode(lock, node.falseIndex, input);
                        }
                    } else {
                        static_assert(false, "Invalid signal expression node type");
                    }
                },
                (SignalExpression::NodeVariant)expr.nodes[nodeIndex]);

            // if (!std::holds_alternative<SignalExpression::ConstantNode>(nodes[nodeIndex]) && nodes.size() > 2) {
            //     Debugf("     '%s' = %f", nodeStrings[nodeIndex], result);
            // }
            if (!std::isfinite(result)) {
                Warnf("Signal expression evaluation error: %s = %f", expr.nodeStrings[nodeIndex], result);
                return 0.0;
            } else {
                cache[nodeIndex] = result;
                return result;
            }
        };
    };

    double SignalExpression::evaluate(DynamicLock<ReadSignalsLock> lock, size_t depth) const {
        // ZoneScoped;
        // ZoneStr(expr);
        ExpressionEvaluator eval(*this, depth);
        return eval.EvaluateNode(lock, rootIndex, 0.0);
    }

    double SignalExpression::evaluate(Lock<ReadAll> lock, size_t depth) const {
        // ZoneScoped;
        // ZoneStr(expr);
        ExpressionEvaluator eval(*this, depth);
        return eval.EvaluateNode(lock, rootIndex, 0.0);
    }

    double SignalExpression::evaluateEvent(DynamicLock<ReadSignalsLock> lock, const EventData &input) const {
        // ZoneScoped;
        // ZoneStr(expr);
        ExpressionEvaluator eval(*this);
        return eval.EvaluateNode(lock, rootIndex, input);
    }

    double SignalExpression::evaluateEvent(Lock<ReadAll> lock, const EventData &input) const {
        // ZoneScoped;
        // ZoneStr(expr);
        ExpressionEvaluator eval(*this);
        return eval.EvaluateNode(lock, rootIndex, input);
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
            Assertf(src.rootIndex >= 0 && (size_t)src.rootIndex < src.nodeStrings.size(),
                "Saving invalid signal expression: %s",
                src.expr);
            dst = picojson::value(src.nodeStrings[src.rootIndex]);
        } else {
            dst = picojson::value(src.expr);
        }
    }
} // namespace ecs
