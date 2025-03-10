/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "SignalExpression.hh"

#include "assets/JsonHelpers.hh"
#include "common/Common.hh"
#include "common/Hashing.hh"
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
        SignalRef outputRef;
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
            return manager.GetConstantNode(0.0);
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
                    // parseNode will already have logged the problem in this path.
                    // Errorf("Failed to parse signal expression, invalid true expression for conditional: %s",
                    //     joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                } else if (tokenIndex >= tokens.size() || tokens[tokenIndex] != ":") {
                    Errorf("Failed to parse signal expression, conditional missing ':': %s",
                        joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                }

                tokenIndex++;
                SignalNodePtr falseNode = parseNode(tokenIndex, precedence);
                if (!falseNode) {
                    // parseNode will already have logged the problem in this path.
                    // Errorf("Failed to parse signal expression, invalid false expression for conditional: %s",
                    //     joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                }

                node = manager.GetNode(Node(DeciderOperation(),
                    ifNode->text + " ? " + trueNode->text + " : " + falseNode->text,
                    {ifNode, trueNode, falseNode}));
            } else if (token == "(") {
                if (node) {
                    Errorf("Failed to parse signal expression, unexpected expression '(': %s",
                        joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                }

                tokenIndex++;
                SignalNodePtr inputNode = parseNode(tokenIndex, ')');
                if (!inputNode) {
                    // parseNode will already have logged the problem in this path.
                    // Errorf("Failed to parse signal expression, invalid expression: %s",
                    //     joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                } else if (tokenIndex >= tokens.size() || tokens[tokenIndex] != ")") {
                    Errorf("Failed to parse signal expression, expression missing ')': %s",
                        joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                }

                tokenIndex++;
                node = manager.GetNode(Node(OneInputOperation{"( ", " )"}, "( " + inputNode->text + " )", {inputNode}));
            } else if (!node && (token == "-" || token == "!")) {
                tokenIndex++;
                SignalNodePtr inputNode = parseNode(tokenIndex, '_');
                if (!inputNode) {
                    // parseNode will already have logged the problem in this path.
                    // Errorf("Failed to parse signal expression, invalid expression: %s",
                    //     joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                }

                auto *constantNode = std::get_if<ConstantNode>(inputNode.get());
                if (token == "-") {
                    if (constantNode) {
                        double newVal = constantNode->value * -1;
                        node = manager.GetConstantNode(newVal);
                    } else {
                        node = manager.GetNode(Node(OneInputOperation{"-", ""}, "-" + inputNode->text, {inputNode}));
                    }
                } else if (token == "!") {
                    if (constantNode) {
                        double newVal = constantNode->value >= 0.5 ? 0.0 : 1.0;
                        node = manager.GetConstantNode(newVal);
                    } else {
                        node = manager.GetNode(Node(OneInputOperation{"!", ""}, "!" + inputNode->text, {inputNode}));
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
                    // parseNode will already have logged the problem in this path.
                    // Errorf("Failed to parse signal expression, invalid argument to function '%s': %s",
                    //     std::string(token),
                    //     joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                } else if (tokenIndex >= tokens.size() || tokens[tokenIndex] != ")") {
                    Errorf("Failed to parse signal expression, '%s' function missing close brace: %s",
                        std::string(token),
                        joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                }

                tokenIndex++;
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
                    node = manager.GetNode(Node(FocusCondition{focus}, "is_focused( " + inputNode->text + " )"));
                } else {
                    node = manager.GetNode(Node(OneInputOperation{std::string(token) + "( ", " )"},
                        std::string(token) + "( " + inputNode->text + " )",
                        {inputNode}));
                }
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
                    // parseNode will already have logged the problem in this path.
                    // Errorf("Failed to parse signal expression, invalid 1st argument to function: '%s': %s",
                    //     std::string(token),
                    //     joinTokens(nodeStart, tokenIndex));
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
                    // parseNode will already have logged the problem in this path.
                    // Errorf("Failed to parse signal expression, invalid 2nd argument to function '%s': %s",
                    //     std::string(token),
                    //     joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                } else if (tokenIndex >= tokens.size() || tokens[tokenIndex] != ")") {
                    Errorf("Failed to parse signal expression, '%s' function missing close brace: %s",
                        std::string(token),
                        joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                }

                tokenIndex++;
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
                    node = manager.GetNode(Node(FocusCondition{focus},
                        "if_focused( " + aNode->text + " , " + bNode->text + " )",
                        {bNode}));
                } else {
                    node = manager.GetNode(Node(TwoInputOperation{std::string(token) + "( ", " , ", " )"},
                        std::string(token) + "( " + aNode->text + " , " + bNode->text + " )",
                        {aNode, bNode}));
                }
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
                    // parseNode will already have logged the problem in this path.
                    // Errorf("Failed to parse signal expression, invalid 2nd argument to operator '%s': %s",
                    //     std::string(token),
                    //     joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                }

                node = manager.GetNode(Node(TwoInputOperation{"", " " + std::string(token) + " ", ""},
                    aNode->text + " " + std::string(token) + " " + bNode->text,
                    {aNode, bNode}));
            } else if (sp::is_float(token)) {
                if (node) {
                    Errorf("Failed to parse signal expression, unexpected constant '%s': %s",
                        std::string(token),
                        joinTokens(nodeStart, tokenIndex));
                    return nullptr;
                }

                double newVal = std::stod(std::string(token));
                node = manager.GetConstantNode(newVal);
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

                    node = manager.GetSignalNode(signalRef);
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
        this->rootNode = GetSignalManager().GetSignalNode(signal);
        rootNode->Compile();
        Assertf(rootNode->evaluate, "Failed to compile expression: %s", expr);
    }

    SignalExpression::SignalExpression(std::string_view expr, const Name &scope) : scope(scope), expr(expr) {
        Compile();
    }

    bool SignalExpression::Compile() {
        ZoneScoped;
        ZoneStr(expr);
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
        rootNode->Compile();
        Assertf(rootNode->evaluate, "Failed to compile expression: %s", expr);
        return true;
    }

    Context::Context(const DynamicLock<ReadSignalsLock> &lock, const SignalExpression &expr, const EventData &input)
        : lock(lock), expr(expr), input(input) {}

    const SignalNodePtr &Node::UpdateDependencies(const SignalNodePtr &node) {
        if (!node) return node;
        for (const auto &child : node->childNodes) {
            if (child->uncacheable) node->uncacheable = true;
            sp::erase_if(child->dependencies, [](auto &weakPtr) {
                return weakPtr.expired();
            });
            if (!sp::contains(child->dependencies, node)) child->dependencies.emplace_back(node);
        }
        return node;
    }

    void Node::PropagateUncacheable(bool newUncacheable) {
        bool oldUncacheable = uncacheable;
        uncacheable = newUncacheable;
        for (const auto &child : childNodes) {
            if (child->uncacheable) {
                uncacheable = true;
                break;
            }
        }
        if (uncacheable != oldUncacheable) {
            for (const auto &dep : dependencies) {
                auto dependency = dep.lock();
                if (dependency) dependency->PropagateUncacheable(uncacheable);
            }
        }
    }

    CompiledFunc Node::Compile() {
        for (const auto &child : childNodes) {
            if (child) child->Compile();
        }
        return std::visit(
            [&](auto &node) {
                if (evaluate) return evaluate;
                return evaluate = node.Compile();
            },
            (NodeVariant &)*this);
    }

    void Node::SubscribeToChildren(const Lock<Write<Signals>> &lock, const SignalRef &subscriber) const {
        if (auto *signalNode = std::get_if<SignalNode>((NodeVariant *)this)) {
            signalNode->signal.AddSubscriber(lock, subscriber);
        }
        for (const auto &child : childNodes) {
            if (child) child->SubscribeToChildren(lock, subscriber);
        }
    }

    double Node::Evaluate(const Context &ctx, size_t depth) const {
        DebugAssertf(evaluate, "Node::Evaluate null compiled function: %s", text);
        double result = this->evaluate(ctx, *this, depth);
        DebugAssertf(std::isfinite(result), "expression::Node::Evaluate() returned non-finite value: %f", result);
        return result;
    }

    CompiledFunc ConstantNode::Compile() const {
        return [](const Context &ctx, const Node &node, size_t depth) {
            // ZoneScoped;
            return std::get<ConstantNode>(node).value;
        };
    }

    CompiledFunc IdentifierNode::Compile() const {
        return [](const Context &ctx, const Node &node, size_t depth) {
            // ZoneScoped;
            auto &identNode = std::get<IdentifierNode>(node);
            if (identNode.field.type != typeid(EventData)) {
                Warnf("SignalExpression can't convert %s from %s to EventData", node.text, identNode.field.type.name());
                return 0.0;
            }
            return ecs::ReadStructField(&ctx.input, identNode.field);
        };
    }

    CompiledFunc SignalNode::Compile() const {
        return [](const Context &ctx, const Node &node, size_t depth) {
            // ZoneScoped;
            auto &signalNode = std::get<SignalNode>(node);
            if (depth >= MAX_SIGNAL_BINDING_DEPTH) {
                Errorf("Max signal binding depth exceeded: %s -> %s", ctx.expr.expr, signalNode.signal.String());
                return 0.0;
            }
            return signalNode.signal.GetSignal(ctx.lock, depth + 1);
        };
    }

    CompiledFunc ComponentNode::Compile() const {
        return [](const Context &ctx, const Node &node, size_t depth) {
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
                            componentNode.path,
                            typeid(T).name());
                        return 0.0;
                    }
                }
            });
        };
    }

    CompiledFunc FocusCondition::Compile() const {
        return [](const Context &ctx, const Node &node, size_t depth) {
            // ZoneScoped;
            auto &focusNode = std::get<FocusCondition>(node);
            if (!ctx.lock.Has<FocusLock>() || !ctx.lock.Get<FocusLock>().HasPrimaryFocus(focusNode.ifFocused)) {
                return 0.0;
            } else if (node.childNodes.empty()) {
                return 1.0;
            } else {
                DebugAssertf(node.childNodes.size() > 0, "FocusCondition::Compile null input node: %s", node.text);
                return node.childNodes[0]->Evaluate(ctx, depth);
            }
        };
    }

    CompiledFunc OneInputOperation::Compile() const {
        if (prefixStr == "( ") {
            return [](const Context &ctx, const Node &node, size_t depth) {
                // ZoneScoped;
                DebugAssertf(node.childNodes.size() > 0, "OneInputOperation::Compile null input node: %s", node.text);
                return node.childNodes[0]->Evaluate(ctx, depth);
            };
        } else if (prefixStr == "-") {
            return [](const Context &ctx, const Node &node, size_t depth) {
                // ZoneScoped;
                DebugAssertf(node.childNodes.size() > 0, "OneInputOperation::Compile null input node: %s", node.text);
                return -node.childNodes[0]->Evaluate(ctx, depth);
            };
        } else if (prefixStr == "!") {
            return [](const Context &ctx, const Node &node, size_t depth) {
                // ZoneScoped;
                DebugAssertf(node.childNodes.size() > 0, "OneInputOperation::Compile null input node: %s", node.text);
                return node.childNodes[0]->Evaluate(ctx, depth) >= 0.5 ? 0.0 : 1.0;
            };
        } else if (prefixStr == "sin( ") {
            return [](const Context &ctx, const Node &node, size_t depth) {
                // ZoneScoped;
                DebugAssertf(node.childNodes.size() > 0, "OneInputOperation::Compile null input node: %s", node.text);
                return std::sin(node.childNodes[0]->Evaluate(ctx, depth));
            };
        } else if (prefixStr == "cos( ") {
            return [](const Context &ctx, const Node &node, size_t depth) {
                // ZoneScoped;
                DebugAssertf(node.childNodes.size() > 0, "OneInputOperation::Compile null input node: %s", node.text);
                return std::cos(node.childNodes[0]->Evaluate(ctx, depth));
            };
        } else if (prefixStr == "tan( ") {
            return [](const Context &ctx, const Node &node, size_t depth) {
                // ZoneScoped;
                DebugAssertf(node.childNodes.size() > 0, "OneInputOperation::Compile null input node: %s", node.text);
                return std::tan(node.childNodes[0]->Evaluate(ctx, depth));
            };
        } else if (prefixStr == "floor( ") {
            return [](const Context &ctx, const Node &node, size_t depth) {
                // ZoneScoped;
                DebugAssertf(node.childNodes.size() > 0, "OneInputOperation::Compile null input node: %s", node.text);
                return std::floor(node.childNodes[0]->Evaluate(ctx, depth));
            };
        } else if (prefixStr == "ceil( ") {
            return [](const Context &ctx, const Node &node, size_t depth) {
                // ZoneScoped;
                DebugAssertf(node.childNodes.size() > 0, "OneInputOperation::Compile null input node: %s", node.text);
                return std::ceil(node.childNodes[0]->Evaluate(ctx, depth));
            };
        } else if (prefixStr == "abs( ") {
            return [](const Context &ctx, const Node &node, size_t depth) {
                // ZoneScoped;
                DebugAssertf(node.childNodes.size() > 0, "OneInputOperation::Compile null input node: %s", node.text);
                return std::abs(node.childNodes[0]->Evaluate(ctx, depth));
            };
        } else {
            Abortf("OneIpputOperation::Compile unknown operation: %s", prefixStr);
        }
    }

    CompiledFunc TwoInputOperation::Compile() const {
        if (prefixStr == "min( ") {
            return [](const Context &ctx, const Node &node, size_t depth) {
                // ZoneScoped;
                DebugAssertf(node.childNodes.size() > 1, "TwoInputOperation::Compile null input node: %s", node.text);
                double inputA = node.childNodes[0]->Evaluate(ctx, depth);
                double inputB = node.childNodes[1]->Evaluate(ctx, depth);
                return std::min(inputA, inputB);
            };
        } else if (prefixStr == "max( ") {
            return [](const Context &ctx, const Node &node, size_t depth) {
                // ZoneScoped;
                DebugAssertf(node.childNodes.size() > 1, "TwoInputOperation::Compile null input node: %s", node.text);
                double inputA = node.childNodes[0]->Evaluate(ctx, depth);
                double inputB = node.childNodes[1]->Evaluate(ctx, depth);
                return std::max(inputA, inputB);
            };
        } else if (prefixStr == "") {
            if (middleStr == " + ") {
                return [](const Context &ctx, const Node &node, size_t depth) {
                    // ZoneScoped;
                    DebugAssertf(node.childNodes.size() > 1,
                        "TwoInputOperation::Compile null input node: %s",
                        node.text);
                    double inputA = node.childNodes[0]->Evaluate(ctx, depth);
                    double inputB = node.childNodes[1]->Evaluate(ctx, depth);
                    double result = inputA + inputB;
                    if (!std::isfinite(result)) {
                        Warnf("Signal expression evaluation error: %f + %f = %f", inputA, inputB, result);
                        return 0.0;
                    }
                    return result;
                };
            } else if (middleStr == " - ") {
                return [](const Context &ctx, const Node &node, size_t depth) {
                    // ZoneScoped;
                    DebugAssertf(node.childNodes.size() > 1,
                        "TwoInputOperation::Compile null input node: %s",
                        node.text);
                    double inputA = node.childNodes[0]->Evaluate(ctx, depth);
                    double inputB = node.childNodes[1]->Evaluate(ctx, depth);
                    double result = inputA - inputB;
                    if (!std::isfinite(result)) {
                        Warnf("Signal expression evaluation error: %f - %f = %f", inputA, inputB, result);
                        return 0.0;
                    }
                    return result;
                };
            } else if (middleStr == " * ") {
                return [](const Context &ctx, const Node &node, size_t depth) {
                    // ZoneScoped;
                    DebugAssertf(node.childNodes.size() > 1,
                        "TwoInputOperation::Compile null input node: %s",
                        node.text);
                    double inputA = node.childNodes[0]->Evaluate(ctx, depth);
                    double inputB = node.childNodes[1]->Evaluate(ctx, depth);
                    double result = inputA * inputB;
                    if (!std::isfinite(result)) {
                        Warnf("Signal expression evaluation error: %f * %f = %f", inputA, inputB, result);
                        return 0.0;
                    }
                    return result;
                };
            } else if (middleStr == " / ") {
                return [](const Context &ctx, const Node &node, size_t depth) {
                    // ZoneScoped;
                    DebugAssertf(node.childNodes.size() > 1,
                        "TwoInputOperation::Compile null input node: %s",
                        node.text);
                    double inputA = node.childNodes[0]->Evaluate(ctx, depth);
                    double inputB = node.childNodes[1]->Evaluate(ctx, depth);
                    double result = inputA / inputB;
                    if (!std::isfinite(result)) {
                        Warnf("Signal expression evaluation error: %f / %f = %f", inputA, inputB, result);
                        return 0.0;
                    }
                    return result;
                };
            } else if (middleStr == " && ") {
                return [](const Context &ctx, const Node &node, size_t depth) {
                    // ZoneScoped;
                    DebugAssertf(node.childNodes.size() > 1,
                        "TwoInputOperation::Compile null input node: %s",
                        node.text);
                    double inputA = node.childNodes[0]->Evaluate(ctx, depth);
                    double inputB = node.childNodes[1]->Evaluate(ctx, depth);
                    return (double)(inputA >= 0.5 && inputB >= 0.5);
                };
            } else if (middleStr == " || ") {
                return [](const Context &ctx, const Node &node, size_t depth) {
                    // ZoneScoped;
                    DebugAssertf(node.childNodes.size() > 1,
                        "TwoInputOperation::Compile null input node: %s",
                        node.text);
                    double inputA = node.childNodes[0]->Evaluate(ctx, depth);
                    double inputB = node.childNodes[1]->Evaluate(ctx, depth);
                    return (double)(inputA >= 0.5 || inputB >= 0.5);
                };
            } else if (middleStr == " > ") {
                return [](const Context &ctx, const Node &node, size_t depth) {
                    // ZoneScoped;
                    DebugAssertf(node.childNodes.size() > 1,
                        "TwoInputOperation::Compile null input node: %s",
                        node.text);
                    double inputA = node.childNodes[0]->Evaluate(ctx, depth);
                    double inputB = node.childNodes[1]->Evaluate(ctx, depth);
                    return (double)(inputA > inputB);
                };
            } else if (middleStr == " >= ") {
                return [](const Context &ctx, const Node &node, size_t depth) {
                    // ZoneScoped;
                    DebugAssertf(node.childNodes.size() > 1,
                        "TwoInputOperation::Compile null input node: %s",
                        node.text);
                    double inputA = node.childNodes[0]->Evaluate(ctx, depth);
                    double inputB = node.childNodes[1]->Evaluate(ctx, depth);
                    return (double)(inputA >= inputB);
                };
            } else if (middleStr == " < ") {
                return [](const Context &ctx, const Node &node, size_t depth) {
                    // ZoneScoped;
                    DebugAssertf(node.childNodes.size() > 1,
                        "TwoInputOperation::Compile null input node: %s",
                        node.text);
                    double inputA = node.childNodes[0]->Evaluate(ctx, depth);
                    double inputB = node.childNodes[1]->Evaluate(ctx, depth);
                    return (double)(inputA < inputB);
                };
            } else if (middleStr == " <= ") {
                return [](const Context &ctx, const Node &node, size_t depth) {
                    // ZoneScoped;
                    DebugAssertf(node.childNodes.size() > 1,
                        "TwoInputOperation::Compile null input node: %s",
                        node.text);
                    double inputA = node.childNodes[0]->Evaluate(ctx, depth);
                    double inputB = node.childNodes[1]->Evaluate(ctx, depth);
                    return (double)(inputA <= inputB);
                };
            } else if (middleStr == " == ") {
                return [](const Context &ctx, const Node &node, size_t depth) {
                    // ZoneScoped;
                    DebugAssertf(node.childNodes.size() > 1,
                        "TwoInputOperation::Compile null input node: %s",
                        node.text);
                    double inputA = node.childNodes[0]->Evaluate(ctx, depth);
                    double inputB = node.childNodes[1]->Evaluate(ctx, depth);
                    return (double)(inputA == inputB);
                };
            } else if (middleStr == " != ") {
                return [](const Context &ctx, const Node &node, size_t depth) {
                    // ZoneScoped;
                    DebugAssertf(node.childNodes.size() > 1,
                        "TwoInputOperation::Compile null input node: %s",
                        node.text);
                    double inputA = node.childNodes[0]->Evaluate(ctx, depth);
                    double inputB = node.childNodes[1]->Evaluate(ctx, depth);
                    return (double)(inputA != inputB);
                };
            } else {
                Abortf("TwoInputOperation::Compile unknown operation: %s", middleStr);
            }
        } else {
            Abortf("TwoInputOperation::Compile unknown operation: %s", prefixStr);
        }
    }

    CompiledFunc DeciderOperation::Compile() const {
        return [](const Context &ctx, const Node &node, size_t depth) {
            // ZoneScoped;
            DebugAssertf(node.childNodes.size() > 2, "DeciderOperation::Compile null input node: %s", node.text);
            double condition = node.childNodes[0]->Evaluate(ctx, depth);
            if (condition >= 0.5) {
                return node.childNodes[1]->Evaluate(ctx, depth);
            } else {
                return node.childNodes[2]->Evaluate(ctx, depth);
            }
        };
    }

    bool SignalExpression::IsCacheable() const {
        if (!rootNode) return true;
        return !rootNode->uncacheable;
    }

    bool SignalExpression::CanEvaluate(const DynamicLock<ReadSignalsLock> &lock, size_t depth) const {
        if (!rootNode) return true;
        return rootNode->CanEvaluate(lock, depth);
    }

    bool Node::CanEvaluate(const DynamicLock<ReadSignalsLock> &lock, size_t depth) const {
        if (std::holds_alternative<SignalNode>(*this)) {
            const SignalNode &signalNode = std::get<SignalNode>(*this);
            if (signalNode.signal.HasValue(lock)) return true;
            auto &binding = signalNode.signal.GetBinding(lock);
            if (depth >= MAX_SIGNAL_BINDING_DEPTH) {
                Errorf("Max signal binding depth exceeded: %s -> %s", text, binding.expr);
                return false;
            }
            return binding.CanEvaluate(lock, depth + 1);
        } else if (std::holds_alternative<ComponentNode>(*this)) {
            const ComponentNode &componentNode = std::get<ComponentNode>(*this);
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
        } else if (std::holds_alternative<FocusCondition>(*this)) {
            if (!lock.template Has<FocusLock>()) return false;
        }
        for (auto &node : childNodes) {
            if (!node->CanEvaluate(lock, depth)) return false;
        }
        return true;
    }

    SignalNodePtr Node::SetScope(const EntityScope &scope) const {
        if (std::holds_alternative<SignalNode>(*this)) {
            auto &signalNode = std::get<SignalNode>(*this);
            SignalRef signalCopy = signalNode.signal;
            signalCopy.SetScope(scope);
            if (signalCopy != signalNode.signal) {
                SignalManager &manager = GetSignalManager();
                return manager.GetSignalNode(signalCopy);
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
            if (childNodes.empty()) return nullptr;
            SignalNodePtr setNode = childNodes[0]->SetScope(scope);
            if (setNode) {
                SignalManager &manager = GetSignalManager();
                return manager.GetNode(Node(FocusCondition(focusNode.ifFocused),
                    "if_focused( " + std::string(magic_enum::enum_name(focusNode.ifFocused)) + " , " + setNode->text +
                        " )",
                    {setNode}));
            }
        } else if (std::holds_alternative<OneInputOperation>(*this)) {
            auto &oneNode = std::get<OneInputOperation>(*this);
            if (childNodes.empty()) return nullptr;
            SignalNodePtr setNode = childNodes[0]->SetScope(scope);
            if (setNode) {
                SignalManager &manager = GetSignalManager();
                return manager.GetNode(Node(OneInputOperation(oneNode.prefixStr, oneNode.suffixStr),
                    oneNode.prefixStr + setNode->text + oneNode.suffixStr,
                    {setNode}));
            }
        } else if (std::holds_alternative<TwoInputOperation>(*this)) {
            auto &twoNode = std::get<TwoInputOperation>(*this);
            if (childNodes.size() < 2) return nullptr;
            SignalNodePtr setNodeA = childNodes[0]->SetScope(scope);
            SignalNodePtr setNodeB = childNodes[1]->SetScope(scope);
            if (setNodeA || setNodeB) {
                if (!setNodeA) setNodeA = childNodes[0];
                if (!setNodeB) setNodeB = childNodes[1];
                SignalManager &manager = GetSignalManager();
                return manager.GetNode(Node(TwoInputOperation(twoNode.prefixStr, twoNode.middleStr, twoNode.suffixStr),
                    twoNode.prefixStr + setNodeA->text + twoNode.middleStr + setNodeB->text + twoNode.suffixStr,
                    {setNodeA, setNodeB}));
            }
        } else if (std::holds_alternative<DeciderOperation>(*this)) {
            if (childNodes.size() < 3) return nullptr;
            SignalNodePtr setNodeIf = childNodes[0]->SetScope(scope);
            SignalNodePtr setNodeTrue = childNodes[1]->SetScope(scope);
            SignalNodePtr setNodeFalse = childNodes[2]->SetScope(scope);
            if (setNodeIf || setNodeTrue || setNodeFalse) {
                if (!setNodeIf) setNodeIf = childNodes[0];
                if (!setNodeTrue) setNodeTrue = childNodes[1];
                if (!setNodeFalse) setNodeFalse = childNodes[2];
                SignalManager &manager = GetSignalManager();
                return manager.GetNode(Node(DeciderOperation(),
                    setNodeIf->text + " ? " + setNodeTrue->text + " : " + setNodeFalse->text,
                    {setNodeIf, setNodeTrue, setNodeFalse}));
            }
        }
        return nullptr;
    }

    size_t Node::Hash() const {
        size_t hash = std::visit(
            [&](auto &node) {
                return node.Hash();
            },
            (NodeVariant &)*this);
        for (const auto &child : childNodes) {
            sp::hash_combine(hash, child->Hash());
        }
        return hash;
    }

    double SignalExpression::Evaluate(const DynamicLock<ReadSignalsLock> &lock, size_t depth) const {
        // ZoneScoped;
        // ZoneStr(expr);
        if (!rootNode) return 0.0;
        Context ctx(lock, *this, 0.0);
        return rootNode->Evaluate(ctx, depth);
    }

    double SignalExpression::EvaluateEvent(const DynamicLock<ReadSignalsLock> &lock, const EventData &input) const {
        // ZoneScoped;
        // ZoneStr(expr);
        if (!rootNode) return 0.0;
        Context ctx(lock, *this, input);
        return rootNode->Evaluate(ctx, 0);
    }

    void SignalExpression::SetScope(const EntityScope &scope) {
        this->scope = scope;
        SignalNodePtr newRoot = rootNode->SetScope(scope);
        if (newRoot) {
            newRoot->Compile();
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
