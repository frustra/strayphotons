/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "SignalExpression.hh"

#include "assets/JsonHelpers.hh"
#include "common/Common.hh"
#include "common/Logging.hh"
#include "ecs/Components.hh"
#include "ecs/EcsImpl.hh"
#include "ecs/EntityRef.hh"
#include "ecs/SignalExpressionNode.hh"
#include "ecs/SignalManager.hh"
#include "ecs/SignalStructAccess.hh"

namespace ecs {
    using namespace expression;

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

    struct CompileContext {
        SignalManager &manager;
        SignalExpression &expr;
        std::vector<std::string_view> tokens; // string_views into expr
        size_t nodeCount = 0;

        static constexpr PrecedenceTable precedenceLookup = PrecedenceTable();

        std::string joinTokens(size_t startToken, size_t endToken) const;
        SignalNodePtr parseNode(size_t &tokenIndex, uint8_t precedence = '\x0');
    };

    std::string CompileContext::joinTokens(size_t startToken, size_t endToken) const {
        if (endToken >= tokens.size()) endToken = tokens.size() - 1;
        if (endToken < startToken) return "";

        auto startPtr = tokens[startToken].data();
        auto endPtr = tokens[endToken].data() + tokens[endToken].size();
        Assertf(endPtr >= startPtr && (size_t)(endPtr - startPtr) <= expr.expr.size(),
            "Token string views not in same buffer");
        return std::string(startPtr, (size_t)(endPtr - startPtr));
    }

    SignalNodePtr CompileContext::parseNode(size_t &tokenIndex, uint8_t precedence) {
        if (nodeCount >= MAX_SIGNAL_EXPRESSION_NODES) {
            Errorf("Failed to parse expression, too many nodes %u >= %u: %s",
                nodeCount,
                MAX_SIGNAL_EXPRESSION_NODES,
                expr.expr);
            return nullptr;
        }
        if (tokens.empty() && tokenIndex == 0) {
            // Treat an empty string as a constant 0.0
            nodeCount++;
            return manager.GetNode(Node(ConstantNode(0.0), picojson::value(0.0).serialize()));
        } else if (tokenIndex >= tokens.size()) {
            Errorf("Failed to parse signal expression, unexpected end of expression: %s", expr.expr);
            return nullptr;
        }

        SignalNodePtr node;
        size_t nodeStart = tokenIndex;
        while (tokenIndex < tokens.size()) {
            auto &token = tokens[tokenIndex];
            if (node && token.size() >= 1 && precedenceLookup.Compare(precedence, token[0])) {
                nodeCount++;
                return node;
            }

            if (token == "?") {
                if (!node) {
                    Errorf("Failed to parse signal expression, unexpected conditional '?': %s",
                        joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                }
                SignalNodePtr ifNode = node;

                tokenIndex++;
                SignalNodePtr trueNode = parseNode(tokenIndex, ':');
                if (!trueNode) {
                    Errorf("Failed to parse signal expression, invalid true expression for conditional: %s",
                        joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                } else if (tokenIndex >= tokens.size() || tokens[tokenIndex] != ":") {
                    Errorf("Failed to parse signal expression, conditional missing ':': %s",
                        joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                }

                tokenIndex++;
                SignalNodePtr falseNode = parseNode(tokenIndex, precedence);
                if (!falseNode) {
                    Errorf("Failed to parse signal expression, invalid false expression for conditional: %s",
                        joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                }

                node = manager.GetNode(Node(DeciderOperation{ifNode, trueNode, falseNode},
                    ifNode->text + " ? " + trueNode->text + " : " + falseNode->text,
                    {ifNode.get(), trueNode.get(), falseNode.get()}));
            } else if (token == "(") {
                if (node) {
                    Errorf("Failed to parse signal expression, unexpected expression '(': %s",
                        joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                }

                tokenIndex++;
                SignalNodePtr inputNode = parseNode(tokenIndex, ')');
                if (!inputNode) {
                    // Errorf("Failed to parse signal expression, invalid expression: %s",
                    //     joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                } else if (tokenIndex >= tokens.size() || tokens[tokenIndex] != ")") {
                    Errorf("Failed to parse signal expression, expression missing ')': %s",
                        joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                }

                tokenIndex++;
                node = manager.GetNode(Node(OneInputOperation{inputNode,
                                                [](double input) -> double {
                                                    return input;
                                                },
                                                "( ",
                                                " )"},
                    "( " + inputNode->text + " )",
                    {inputNode.get()}));
            } else if (!node && (token == "-" || token == "!")) {
                tokenIndex++;
                SignalNodePtr inputNode = parseNode(tokenIndex, '_');
                if (!inputNode) {
                    // Errorf("Failed to parse signal expression, invalid expression: %s",
                    //     joinTokens(startToken, tokenIndex));
                    return nullptr;
                }

                auto *constantNode = std::get_if<ConstantNode>(inputNode.get());
                if (token == "-") {
                    if (constantNode) {
                        double newVal = constantNode->value * -1;
                        node = manager.GetNode(Node(ConstantNode{newVal}, picojson::value(newVal).serialize()));
                    } else {
                        node = manager.GetNode(Node(OneInputOperation{inputNode,
                                                        [](double input) -> double {
                                                            return -input;
                                                        },
                                                        "-",
                                                        ""},
                            "-" + inputNode->text,
                            {inputNode.get()}));
                    }
                } else if (token == "!") {
                    if (constantNode) {
                        double newVal = constantNode->value >= 0.5 ? 0.0 : 1.0;
                        node = manager.GetNode(Node(ConstantNode{newVal}, picojson::value(newVal).serialize()));
                    } else {
                        node = manager.GetNode(Node(OneInputOperation{inputNode,
                                                        [](double input) -> double {
                                                            return input >= 0.5 ? 0.0 : 1.0;
                                                        },
                                                        "!",
                                                        ""},
                            "!" + inputNode->text,
                            {inputNode.get()}));
                    }
                }
            } else if (token == "is_focused" || token == "sin" || token == "cos" || token == "tan" ||
                       token == "floor" || token == "ceil" || token == "abs") {
                // Parse as 1 argument function
                if (node) {
                    Errorf("Failed to parse signal expression, unexpected function '%s': %s",
                        std::string(token),
                        joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                }

                tokenIndex++;
                if (tokenIndex >= tokens.size() || tokens[tokenIndex] != "(") {
                    Errorf("Failed to parse signal expression, '%s' function missing open brace: %s",
                        std::string(token),
                        joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                }

                tokenIndex++;
                SignalNodePtr inputNode = parseNode(tokenIndex, ')');
                if (!inputNode) {
                    Errorf("Failed to parse signal expression, invalid argument to function '%s': %s",
                        std::string(token),
                        joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                } else if (tokenIndex >= tokens.size() || tokens[tokenIndex] != ")") {
                    Errorf("Failed to parse signal expression, '%s' function missing close brace: %s",
                        std::string(token),
                        joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                }

                tokenIndex++;
                NodeVariant tmpNode;
                if (token == "is_focused") {
                    std::string &focusStr = inputNode->text;
                    FocusLayer focus = FocusLayer::Always;
                    if (!focusStr.empty()) {
                        auto opt = magic_enum::enum_cast<FocusLayer>(focusStr);
                        if (opt) {
                            focus = *opt;
                        } else {
                            Errorf("Unknown enum value specified for is_focused: %s", focusStr);
                        }
                    } else {
                        Errorf("Blank focus layer specified for is_focused: %s", focusStr);
                    }
                    tmpNode = FocusCondition{focus, nullptr};
                } else if (token == "sin") {
                    tmpNode = OneInputOperation{inputNode,
                        [](double input) -> double {
                            return std::sin(input);
                        },
                        "sin( ",
                        " )"};
                } else if (token == "cos") {
                    tmpNode = OneInputOperation{inputNode,
                        [](double input) -> double {
                            return std::cos(input);
                        },
                        "cos( ",
                        " )"};
                } else if (token == "tan") {
                    tmpNode = OneInputOperation{inputNode,
                        [](double input) -> double {
                            return std::tan(input);
                        },
                        "tan( ",
                        " )"};
                } else if (token == "floor") {
                    tmpNode = OneInputOperation{inputNode,
                        [](double input) -> double {
                            return std::floor(input);
                        },
                        "floor( ",
                        " )"};
                } else if (token == "ceil") {
                    tmpNode = OneInputOperation{inputNode,
                        [](double input) -> double {
                            return std::ceil(input);
                        },
                        "ceil( ",
                        " )"};
                } else if (token == "abs") {
                    tmpNode = OneInputOperation{inputNode,
                        [](double input) -> double {
                            return std::abs(input);
                        },
                        "abs( ",
                        " )"};
                } else {
                    Abortf("Invalid function token: %s", std::string(token));
                }

                node = manager.GetNode(
                    Node(tmpNode, std::string(token) + "( " + inputNode->text + " )", {inputNode.get()}));
            } else if (token == "if_focused" || token == "min" || token == "max") {
                // Parse as 2 argument function
                if (node) {
                    Errorf("Failed to parse signal expression, unexpected function '%s': %s",
                        std::string(token),
                        joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                }

                tokenIndex++;
                if (tokenIndex >= tokens.size() || tokens[tokenIndex] != "(") {
                    Errorf("Failed to parse signal expression, '%s' function missing open brace: %s",
                        std::string(token),
                        joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                }

                tokenIndex++;
                SignalNodePtr aNode = parseNode(tokenIndex, ',');
                if (!aNode) {
                    Errorf("Failed to parse signal expression, invalid 1st argument to function: '%s': %s",
                        std::string(token),
                        joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                } else if (tokenIndex >= tokens.size() || tokens[tokenIndex] != ",") {
                    Errorf("Failed to parse signal expression, '%s' function expects 2 arguments: %s",
                        std::string(token),
                        joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                }

                tokenIndex++;
                SignalNodePtr bNode = parseNode(tokenIndex, ')');
                if (!bNode) {
                    Errorf("Failed to parse signal expression, invalid 2nd argument to function '%s': %s",
                        std::string(token),
                        joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                } else if (tokenIndex >= tokens.size() || tokens[tokenIndex] != ")") {
                    Errorf("Failed to parse signal expression, '%s' function missing close brace: %s",
                        std::string(token),
                        joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                }

                tokenIndex++;
                NodeVariant tmpNode;
                if (token == "if_focused") {
                    std::string &focusStr = aNode->text;
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
                    tmpNode = FocusCondition{focus, bNode};
                } else if (token == "min") {
                    tmpNode = TwoInputOperation{aNode,
                        bNode,
                        [](double a, double b) -> double {
                            return std::min(a, b);
                        },
                        "min( ",
                        " , ",
                        " )"};
                } else if (token == "max") {
                    tmpNode = TwoInputOperation{aNode,
                        bNode,
                        [](double a, double b) -> double {
                            return std::max(a, b);
                        },
                        "max( ",
                        " , ",
                        " )"};
                } else {
                    Abortf("Invalid function token: %s", std::string(token));
                }

                node = manager.GetNode(Node(tmpNode,
                    std::string(token) + "( " + aNode->text + " , " + bNode->text + " )",
                    {aNode.get(), bNode.get()}));
            } else if (token == "+" || token == "-" || token == "*" || token == "/" || token == "&&" || token == "||" ||
                       token == ">" || token == ">=" || token == "<" || token == "<=" || token == "==" ||
                       token == "!=") {
                if (!node) {
                    Errorf("Failed to parse signal expression, unexpected operator '%s': %s",
                        std::string(token),
                        joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                }

                SignalNodePtr &aNode = node;

                tokenIndex++;
                SignalNodePtr bNode = parseNode(tokenIndex, token[0]);
                if (!bNode) {
                    Errorf("Failed to parse signal expression, invalid 2nd argument to operator '%s': %s",
                        std::string(token),
                        joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                }

                NodeVariant tmpNode;
                if (token == "+") {
                    tmpNode = TwoInputOperation{aNode,
                        bNode,
                        [](double a, double b) -> double {
                            double result = a + b;
                            if (!std::isfinite(result)) {
                                Warnf("Signal expression evaluation error: %f + %f = %f", a, b, result);
                                return 0.0;
                            }
                            return result;
                        },
                        "",
                        " + ",
                        ""};
                } else if (token == "-") {
                    tmpNode = TwoInputOperation{aNode,
                        bNode,
                        [](double a, double b) -> double {
                            double result = a - b;
                            if (!std::isfinite(result)) {
                                Warnf("Signal expression evaluation error: %f - %f = %f", a, b, result);
                                return 0.0;
                            }
                            return result;
                        },
                        "",
                        " - ",
                        ""};
                } else if (token == "*") {
                    tmpNode = TwoInputOperation{aNode,
                        bNode,
                        [](double a, double b) -> double {
                            double result = a * b;
                            if (!std::isfinite(result)) {
                                Warnf("Signal expression evaluation error: %f * %f = %f", a, b, result);
                                return 0.0;
                            }
                            return result;
                        },
                        "",
                        " * ",
                        ""};
                } else if (token == "/") {
                    tmpNode = TwoInputOperation{aNode,
                        bNode,
                        [](double a, double b) -> double {
                            double result = a / b;
                            if (!std::isfinite(result)) {
                                Warnf("Signal expression evaluation error: %f / %f = %f", a, b, result);
                                return 0.0;
                            }
                            return result;
                        },
                        "",
                        " / ",
                        ""};
                } else if (token == "&&") {
                    tmpNode = TwoInputOperation{aNode,
                        bNode,
                        [](double a, double b) -> double {
                            return a >= 0.5 && b >= 0.5;
                        },
                        "",
                        " && ",
                        ""};
                } else if (token == "||") {
                    tmpNode = TwoInputOperation{aNode,
                        bNode,
                        [](double a, double b) -> double {
                            return a >= 0.5 || b >= 0.5;
                        },
                        "",
                        " || ",
                        ""};
                } else if (token == ">") {
                    tmpNode = TwoInputOperation{aNode,
                        bNode,
                        [](double a, double b) -> double {
                            return a > b;
                        },
                        "",
                        " > ",
                        ""};
                } else if (token == ">=") {
                    tmpNode = TwoInputOperation{aNode,
                        bNode,
                        [](double a, double b) -> double {
                            return a >= b;
                        },
                        "",
                        " >= ",
                        ""};
                } else if (token == "<") {
                    tmpNode = TwoInputOperation{aNode,
                        bNode,
                        [](double a, double b) -> double {
                            return a < b;
                        },
                        "",
                        " < ",
                        ""};
                } else if (token == "<=") {
                    tmpNode = TwoInputOperation{aNode,
                        bNode,
                        [](double a, double b) -> double {
                            return a <= b;
                        },
                        "",
                        " <= ",
                        ""};
                } else if (token == "==") {
                    tmpNode = TwoInputOperation{aNode,
                        bNode,
                        [](double a, double b) -> double {
                            return a == b;
                        },
                        "",
                        " == ",
                        ""};
                } else if (token == "!=") {
                    tmpNode = TwoInputOperation{aNode,
                        bNode,
                        [](double a, double b) -> double {
                            return a != b;
                        },
                        "",
                        " != ",
                        ""};
                } else {
                    Abortf("Invalid operator token: %s", std::string(token));
                }

                node = manager.GetNode(Node(tmpNode,
                    aNode->text + " " + std::string(token) + " " + bNode->text,
                    {aNode.get(), bNode.get()}));
            } else if (sp::is_float(token)) {
                if (node) {
                    Errorf("Failed to parse signal expression, unexpected constant '%s': %s",
                        std::string(token),
                        joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                }

                double newVal = std::stod(std::string(token));
                node = manager.GetNode(Node(ConstantNode{newVal}, picojson::value(newVal).serialize()));
                tokenIndex++;
            } else if (token == ")" || token == "," || token == ":") {
                Errorf("Failed to parse signal expression, unexpected token '%s': %s",
                    std::string(token),
                    joinTokens(nodeStart, tokenIndex));
                return nullptr;
            } else {
                if (node) {
                    Errorf("Failed to parse signal expression, unexpected identifier/signal '%s': %s",
                        std::string(token),
                        joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                }

                auto delimiter = token.find_first_of("/#");
                if (delimiter >= token.length()) {
                    if (token == "event" || sp::starts_with(token, "event.")) {
                        auto field = GetStructField(typeid(EventData), token);
                        if (field) {
                            node = manager.GetNode(Node(IdentifierNode{*field}, std::string(token)));
                        } else {
                            Errorf("Failed to parse signal expression, unexpected identifier '%s': %s",
                                std::string(token),
                                joinTokens(nodeStart, tokenIndex));
                            return nullptr;
                        }
                    } else {
                        StructField field(std::string(token), "", typeid(double), 0, FieldAction::None);
                        node = manager.GetNode(Node(IdentifierNode{field}, std::string(token)));
                    }
                } else if (token[delimiter] == '/') {
                    SignalRef signalRef(token, expr.scope);
                    if (!signalRef) {
                        Errorf("Failed to parse signal expression, invalid signal '%s': %s",
                            std::string(token),
                            joinTokens(nodeStart, tokenIndex));
                        return nullptr;
                    }
                    node = manager.GetNode(Node(SignalNode{signalRef}, signalRef.String()));
                } else if (token[delimiter] == '#') {
                    ecs::Name entityName(token.substr(0, delimiter), expr.scope);
                    std::string componentPath(token.substr(delimiter + 1));
                    std::string componentName = componentPath.substr(0, componentPath.find('.'));

                    auto *componentBase = LookupComponent(componentName);
                    if (!componentBase) {
                        Errorf("Failed to parse signal expression, unknown component '%s': %s",
                            std::string(token),
                            componentName);
                        return nullptr;
                    }
                    auto componentField = GetStructField(componentBase->metadata.type, componentPath);
                    if (!componentField) {
                        Errorf("Failed to parse signal expression, unknown component field '%s': %s",
                            componentName,
                            componentPath);
                        return nullptr;
                    }
                    node = manager.GetNode(
                        Node(ComponentNode{entityName, componentBase, *componentField, componentPath},
                            entityName.String() + "#" + componentPath));
                } else {
                    Abortf("Unexpected delimiter: '%c' %s", token[delimiter], std::string(token));
                }
                tokenIndex++;
            }
        }
        Assertf(tokenIndex >= tokens.size(), "parseNode failed to parse all tokens: %u<%u", tokenIndex, tokens.size());
        if (!node) {
            Errorf("Failed to parse signal expression, blank expression: %s", joinTokens(nodeStart, tokenIndex));
            return nullptr;
        } else if (precedence == ')' || precedence == ',' || precedence == ':') {
            Errorf("Failed to parse signal expression, missing end token '%c': %s", precedence, expr.expr);
            return nullptr;
        }
        nodeCount++;
        return node;
    }

    SignalExpression::SignalExpression(const SignalRef &signal) {
        this->scope = EntityScope(signal.GetEntity().Name().scene, "");
        this->expr = signal.String();
        SignalManager &manager = GetSignalManager();
        this->rootNode = manager.GetNode(Node(SignalNode{signal}, expr));
        rootNode->compile(*this, true);
        Assertf(rootNode->evaluate, "Failed to compile expression: %s", expr);
    }

    SignalExpression::SignalExpression(std::string_view expr, const Name &scope) : scope(scope), expr(expr) {
        Compile();
    }

    bool SignalExpression::Compile() {
        CompileContext ctx = {GetSignalManager(), *this, {}};
        // Tokenize the expression
        std::string_view exprView = this->expr;
        size_t tokenStart = 0;
        for (size_t tokenEnd = 0; tokenEnd < exprView.size(); tokenEnd++) {
            const char &ch = exprView[tokenEnd];
            if (tokenEnd == tokenStart + 1 && exprView[tokenStart] == '!' && ch != '=') {
                ctx.tokens.emplace_back(exprView.substr(tokenStart, 1));
                tokenStart++;
            }
            if (std::isspace(ch)) {
                if (tokenEnd > tokenStart) ctx.tokens.emplace_back(exprView.substr(tokenStart, tokenEnd - tokenStart));
                tokenStart = tokenEnd + 1;
            } else if (ch == '(' || ch == ')' || ch == ',') {
                if (tokenEnd > tokenStart) ctx.tokens.emplace_back(exprView.substr(tokenStart, tokenEnd - tokenStart));
                ctx.tokens.emplace_back(exprView.substr(tokenEnd, 1));
                tokenStart = tokenEnd + 1;
            } else if (tokenEnd == tokenStart) {
                if (ch == '+' || ch == '-' || ch == '*' || ch == '/') {
                    ctx.tokens.emplace_back(exprView.substr(tokenEnd, 1));
                    tokenStart = tokenEnd + 1;
                }
            }
        }
        if (tokenStart < exprView.size()) ctx.tokens.emplace_back(exprView.substr(tokenStart));

        // Debugf("Expr: %s", expr);
        // for (auto &token : ctx.tokens) {
        //     Debugf("  %s", std::string(token));
        // }

        // Parse the expression into a deduplicated tree of nodes
        rootNode.reset();
        size_t tokenIndex = 0;
        rootNode = ctx.parseNode(tokenIndex);
        if (!rootNode) {
            Errorf("Failed to parse expression: %s", expr);
            return false;
        }
        Assertf(tokenIndex == ctx.tokens.size(), "Failed to parse signal expression, incomplete parse: %s", exprView);

        // Compile the parsed expression tree into a lambda function
        rootNode->compile(*this, false);
        Assertf(rootNode->evaluate, "Failed to compile expression: %s", expr);
        return true;
    } // namespace ecs

    // double cacheLookup(const SignalExpression::Context &ctx, const Node &node, size_t depth) {
    //     return ctx.cache[node.index];
    // }

    CompiledFunc Node::compile(SignalExpression &expr, bool noCacheWrite) {
        return std::visit(
            [&](auto &node) {
                using T = std::decay_t<decltype(node)>;

                // if (evaluate) {
                //     return &cacheLookup;
                // } else {
                if constexpr (std::is_same_v<T, FocusCondition>) {
                    // Force no cache writes for branching nodes
                    node.inputFunc = node.inputNode ? node.inputNode->compile(expr, true) : nullptr;
                } else if constexpr (std::is_same_v<T, OneInputOperation>) {
                    node.inputFunc = node.inputNode->compile(expr, noCacheWrite);
                } else if constexpr (std::is_same_v<T, TwoInputOperation>) {
                    node.inputFuncA = node.inputNodeA->compile(expr, noCacheWrite);
                    node.inputFuncB = node.inputNodeB->compile(expr, noCacheWrite);
                } else if constexpr (std::is_same_v<T, DeciderOperation>) {
                    node.ifFunc = node.ifNode->compile(expr, noCacheWrite);
                    // Force no cache writes for branching nodes
                    node.trueFunc = node.trueNode->compile(expr, true);
                    node.falseFunc = node.falseNode->compile(expr, true);
                }

                // if (noCacheWrite) {
                return evaluate = &T::Evaluate;
                // } else {
                //     evaluate = [](const Context &ctx, const Node &node, size_t depth) {
                //         return (ctx.cache[node.index] = T::Evaluate(ctx, node, depth));
                //     };
                //     return evaluate;
                // }
                // }
            },
            (NodeVariant &)*this);
    }

    double ConstantNode::Evaluate(const Context &ctx, const Node &node, size_t depth) {
        // ZoneScoped;
        return std::get<ConstantNode>(node).value;
    }

    double IdentifierNode::Evaluate(const Context &ctx, const Node &node, size_t depth) {
        // ZoneScoped;
        auto &identNode = std::get<IdentifierNode>(node);
        if (identNode.field.type != typeid(EventData)) {
            Warnf("SignalExpression can't convert %s from %s to EventData", node.text, identNode.field.type.name());
            return 0.0;
        }
        return ecs::ReadStructField(&ctx.input, identNode.field);
    }

    double SignalNode::Evaluate(const Context &ctx, const Node &node, size_t depth) {
        // ZoneScoped;
        auto &signalNode = std::get<SignalNode>(node);
        if (depth >= MAX_SIGNAL_BINDING_DEPTH) {
            Errorf("Max signal binding depth exceeded: %s -> %s", ctx.expr.expr, signalNode.signal.String());
            return 0.0;
        }
        return signalNode.signal.GetSignal(ctx.lock, depth + 1);
    }

    double ComponentNode::Evaluate(const Context &ctx, const Node &node, size_t depth) {
        // ZoneScoped;
        auto &componentNode = std::get<ComponentNode>(node);
        if (!componentNode.component) return 0.0;
        Entity ent = componentNode.entity.Get(ctx.lock);
        if (!ent) return 0.0;
        return GetComponentType(componentNode.component->metadata.type, [&](auto *typePtr) {
            using T = std::remove_pointer_t<decltype(typePtr)>;
            if constexpr (!ECS::IsComponent<T>() || Tecs::is_global_component<T>()) {
                Warnf("SignalExpression can't evaluate component type: %s", typeid(T).name());
                return 0.0;
            } else if constexpr (Tecs::is_read_allowed<T, ReadSignalsLock>()) {
                auto &component = ent.Get<const T>(ctx.lock);
                return ecs::ReadStructField(&component, componentNode.field);
            } else {
                auto tryLock = ctx.lock.TryLock<Read<T>>();
                if (tryLock) {
                    auto &component = ent.Get<const T>(*tryLock);
                    return ecs::ReadStructField(&component, componentNode.field);
                } else {
                    Warnf("SignalExpression can't evaluate component '%s' without lock: %s",
                        componentNode.field.name,
                        typeid(T).name());
                    return 0.0;
                }
            }
        });
    }

    double FocusCondition::Evaluate(const Context &ctx, const Node &node, size_t depth) {
        // ZoneScoped;
        auto &focusNode = std::get<FocusCondition>(node);
        if (!ctx.lock.Has<FocusLock>() || !ctx.lock.Get<FocusLock>().HasPrimaryFocus(focusNode.ifFocused)) {
            return 0.0;
        } else if (!focusNode.inputNode) {
            return 1.0;
        } else {
            DebugAssertf(focusNode.inputNode, "FocusCondition::Evaluate null input node: %s", node.text);
            return focusNode.inputFunc(ctx, *focusNode.inputNode, depth);
        }
    }

    double OneInputOperation::Evaluate(const Context &ctx, const Node &node, size_t depth) {
        // ZoneScoped;
        auto &opNode = std::get<OneInputOperation>(node);

        DebugAssertf(opNode.inputNode, "OneInputOperation::Evaluate null input node: %s", node.text);
        return opNode.evaluate(opNode.inputFunc(ctx, *opNode.inputNode, depth));
    }

    double TwoInputOperation::Evaluate(const Context &ctx, const Node &node, size_t depth) {
        // ZoneScoped;
        auto &opNode = std::get<TwoInputOperation>(node);

        // Argument execution order is undefined, so args must be evaluated before
        DebugAssertf(opNode.inputNodeA, "TwoInputOperation::Evaluate null input node a: %s", node.text);
        DebugAssertf(opNode.inputNodeB, "TwoInputOperation::Evaluate null input node b: %s", node.text);
        double inputA = opNode.inputFuncA(ctx, *opNode.inputNodeA, depth);
        double inputB = opNode.inputFuncB(ctx, *opNode.inputNodeB, depth);
        return opNode.evaluate(inputA, inputB);
    }

    double DeciderOperation::Evaluate(const Context &ctx, const Node &node, size_t depth) {
        // ZoneScoped;
        auto &opNode = std::get<DeciderOperation>(node);

        DebugAssertf(opNode.ifNode, "DeciderOperation::Evaluate null condition input node: %s", node.text);
        if (opNode.ifFunc(ctx, *opNode.ifNode, depth) >= 0.5) {
            DebugAssertf(opNode.trueNode, "DeciderOperation::Evaluate null true input node: %s", node.text);
            return opNode.trueFunc(ctx, *opNode.trueNode, depth);
        } else {
            DebugAssertf(opNode.falseNode, "DeciderOperation::Evaluate null false input node: %s", node.text);
            return opNode.falseFunc(ctx, *opNode.falseNode, depth);
        }
    }

    bool SignalExpression::CanEvaluate(const DynamicLock<ReadSignalsLock> &lock, size_t depth) const {
        if (!rootNode) return false;
        return rootNode->canEvaluate(lock, depth);
    }

    bool Node::canEvaluate(const DynamicLock<ReadSignalsLock> &lock, size_t depth) const {
        for (auto &node : childNodes) {
            if (std::holds_alternative<SignalNode>(*node)) {
                const SignalNode &signalNode = std::get<SignalNode>(*node);
                if (signalNode.signal.HasValue(lock)) return true;
                auto &binding = signalNode.signal.GetBinding(lock);
                if (depth >= MAX_SIGNAL_BINDING_DEPTH) {
                    Errorf("Max signal binding depth exceeded: %s -> %s", text, binding.expr);
                    return false;
                }
                return signalNode.signal.GetBinding(lock).CanEvaluate(lock, depth + 1);
            } else if (std::holds_alternative<ComponentNode>(*node)) {
                const ComponentNode &componentNode = std::get<ComponentNode>(*node);
                const ComponentBase *base = componentNode.component;
                if (!base) return true;
                Entity ent = componentNode.entity.Get(lock);
                if (!ent) return true;
                return GetComponentType(base->metadata.type, [&](auto *typePtr) {
                    using T = std::remove_pointer_t<decltype(typePtr)>;
                    if constexpr (!ECS::IsComponent<T>() || Tecs::is_global_component<T>()) {
                        return true;
                    } else {
                        return lock.TryLock<Read<T>>().has_value();
                    }
                });
            } else if (std::holds_alternative<FocusCondition>(*node)) {
                if (!lock.template Has<FocusLock>()) return false;
            }
        }
        return true;
    }

    SignalNodePtr Node::setScope(const EntityScope &scope) const {
        if (std::holds_alternative<SignalNode>(*this)) {
            auto &signalNode = std::get<SignalNode>(*this);
            SignalRef signalCopy = signalNode.signal;
            signalCopy.SetScope(scope);
            if (signalCopy != signalNode.signal) {
                SignalManager &manager = GetSignalManager();
                return manager.GetNode(Node(SignalNode(signalCopy), signalCopy.String()));
            }
        } else if (std::holds_alternative<ComponentNode>(*this)) {
            auto &componentNode = std::get<ComponentNode>(*this);
            EntityRef entityCopy = componentNode.entity;
            entityCopy.SetScope(scope);
            if (entityCopy != componentNode.entity) {
                SignalManager &manager = GetSignalManager();
                return manager.GetNode(
                    Node(ComponentNode(entityCopy, componentNode.component, componentNode.field, componentNode.path),
                        entityCopy.Name().String() + "#" + componentNode.path));
            }
        } else if (std::holds_alternative<FocusCondition>(*this)) {
            auto &focusNode = std::get<FocusCondition>(*this);
            if (!focusNode.inputNode) return nullptr;
            SignalNodePtr setNode = focusNode.inputNode->setScope(scope);
            if (setNode) {
                SignalManager &manager = GetSignalManager();
                return manager.GetNode(Node(FocusCondition(focusNode.ifFocused, setNode),
                    "if_focused( " + std::string(magic_enum::enum_name(focusNode.ifFocused)) + " , " + setNode->text +
                        " )"));
            }
        } else if (std::holds_alternative<OneInputOperation>(*this)) {
            auto &oneNode = std::get<OneInputOperation>(*this);
            if (!oneNode.inputNode) return nullptr;
            SignalNodePtr setNode = oneNode.inputNode->setScope(scope);
            if (setNode) {
                SignalManager &manager = GetSignalManager();
                return manager.GetNode(
                    Node(OneInputOperation(setNode, oneNode.evaluate, oneNode.prefixStr, oneNode.suffixStr),
                        oneNode.prefixStr + setNode->text + oneNode.suffixStr));
            }
        } else if (std::holds_alternative<TwoInputOperation>(*this)) {
            auto &twoNode = std::get<TwoInputOperation>(*this);
            SignalNodePtr setNodeA = twoNode.inputNodeA ? twoNode.inputNodeA->setScope(scope) : nullptr;
            SignalNodePtr setNodeB = twoNode.inputNodeB ? twoNode.inputNodeB->setScope(scope) : nullptr;
            if (setNodeA || setNodeB) {
                if (!setNodeA) setNodeA = twoNode.inputNodeA;
                if (!setNodeB) setNodeB = twoNode.inputNodeB;
                SignalManager &manager = GetSignalManager();
                return manager.GetNode(Node(TwoInputOperation(setNodeA,
                                                setNodeB,
                                                twoNode.evaluate,
                                                twoNode.prefixStr,
                                                twoNode.middleStr,
                                                twoNode.suffixStr),
                    twoNode.prefixStr + setNodeA->text + twoNode.middleStr + setNodeB->text + twoNode.suffixStr));
            }
        } else if (std::holds_alternative<DeciderOperation>(*this)) {
            auto &deciderNode = std::get<DeciderOperation>(*this);
            SignalNodePtr setNodeIf = deciderNode.ifNode ? deciderNode.ifNode->setScope(scope) : nullptr;
            SignalNodePtr setNodeTrue = deciderNode.trueNode ? deciderNode.trueNode->setScope(scope) : nullptr;
            SignalNodePtr setNodeFalse = deciderNode.falseNode ? deciderNode.falseNode->setScope(scope) : nullptr;
            if (setNodeIf || setNodeTrue || setNodeFalse) {
                if (!setNodeIf) setNodeIf = deciderNode.ifNode;
                if (!setNodeTrue) setNodeTrue = deciderNode.trueNode;
                if (!setNodeFalse) setNodeFalse = deciderNode.falseNode;
                SignalManager &manager = GetSignalManager();
                return manager.GetNode(Node(DeciderOperation(setNodeIf, setNodeTrue, setNodeFalse),
                    setNodeIf->text + " ? " + setNodeTrue->text + " : " + setNodeFalse->text));
            }
        }
        return nullptr;
    }

    double SignalExpression::Evaluate(const DynamicLock<ReadSignalsLock> &lock, size_t depth) const {
        // ZoneScoped;
        // ZoneStr(expr);
        if (!rootNode) return 0.0;
        Storage cache;
        Context ctx(lock, *this, cache, 0.0);
        return rootNode->evaluate(ctx, *rootNode, depth);
    }

    double SignalExpression::EvaluateEvent(const DynamicLock<ReadSignalsLock> &lock, const EventData &input) const {
        // ZoneScoped;
        // ZoneStr(expr);
        if (!rootNode) return 0.0;
        Storage cache;
        Context ctx(lock, *this, cache, input);
        return rootNode->evaluate(ctx, *rootNode, 0);
    }

    void SignalExpression::SetScope(const EntityScope &scope) {
        this->scope = scope;
        SignalNodePtr newRoot = rootNode->setScope(scope);
        if (newRoot) {
            newRoot->compile(*this, false);
            // Logf("Setting scope of expression (%s): %s -> %s", scope.String(), rootNode->text, newRoot->text);
            Assertf(newRoot->evaluate, "Failed to compile expression: %s", expr);
            this->rootNode = newRoot;
        }
    }

    template<>
    bool StructMetadata::Load<SignalExpression>(SignalExpression &dst, const picojson::value &src) {
        if (src.is<std::string>()) {
            dst = ecs::SignalExpression(src.get<std::string>());
            return dst.rootNode != nullptr;
        } else {
            Errorf("Invalid signal expression: %s", src.to_str());
            return false;
        }
    }

    template<>
    void StructMetadata::Save<SignalExpression>(const EntityScope &scope,
        picojson::value &dst,
        const SignalExpression &src,
        const SignalExpression *def) {
        if (src.scope != scope) {
            // TODO: Remap signal names to new scope instead of converting to fully qualified names
            // Warnf("Saving signal expression with missmatched scope: `%s`, scope '%s' != '%s'",
            //     src.expr,
            //     src.scope.String(),
            //     scope.String());
            Assertf(src.rootNode != nullptr, "Saving invalid signal expression: %s", src.expr);
            dst = picojson::value(src.rootNode->text);
        } else {
            dst = picojson::value(src.expr);
        }
    }

    template<>
    void StructMetadata::SetScope<SignalExpression>(SignalExpression &dst, const EntityScope &scope) {
        dst.SetScope(scope);
    }

    template<>
    void StructMetadata::DefineSchema<SignalExpression>(picojson::value &dst,
        sp::json::SchemaTypeReferences *references) {
        if (!dst.is<picojson::object>()) dst.set<picojson::object>({});
        auto &typeSchema = dst.get<picojson::object>();
        typeSchema["type"] = picojson::value("string");
        typeSchema["description"] = picojson::value(
            "A signal expression string, e.g: \"scene:entity/signal + (other_entity/signal2 + 1)\"");
    }
} // namespace ecs
