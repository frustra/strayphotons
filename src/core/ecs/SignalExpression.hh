/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

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
    class SignalManager;

    static const char *DocsDescriptionSignalExpression = R"(
Signal expressions allow math and logic to be performed using input from almost any entity property.  
Expressions are defined as strings and automatically parsed and compiled for fast game logic evaluation.

A basic signal expression might look like this:  
`"(entity/input_value + 1) > 10"`

The above will evaluate to `1.0` if the `input_value` signal on `entity` is greater than 9, or `0.0` otherwise.

> [!NOTE]
> All expressions are evaluated using double (64-bit) floating point numbers.  
> Whitespace is required before operators and function names.

Signal expressions support the following operations and functions:

- **Arithmetic operators**:
  - `a + b`: Addition
  - `a - b`: Subtraction
  - `a * b`: Multiplication
  - `a / b`: Division (Divide by zero returns 0.0)
  - `-a`: Sign Inverse
- **Boolean operators**: (Inputs are true if >= 0.5, output is `0.0` or `1.0`)
  - `a && b`: Logical AND
  - `a || b`: Logical OR
  - `!a`: Logical NOT
- **Comparison operators**: (Output is `0.0` or `1.0`)
  - `a > b`: Greater Than
  - `a >= b`: Greater Than or Equal
  - `a < b`: Less Than
  - `a <= b`: Less Than or Equal
  - `a == b`: Equal
  - `a != b`: Not Equal
- **Math functions**:
  - `sin(x)`, `cos(x)`, `tan(x)` (Input in radians)
  - `floor(x)`, `ceil(x)`, `abs(x)`
  - `min(a, b)`, `max(a, b)`
- **Focus functions**: (Possible focus layers: `Game`, `Menu`, `Overlay`)
  - `is_focused(FocusLayer)`: Returns `1.0` if the layer is active, else `0.0`.
  - `if_focused(FocusLayer, x)`: Returns `x` if the layer is active, else `0.0`.
- **Entity signal access**:
  - `<entity_name>/<signal_name>`: Read a signal on a specific entity. If the signal or entity is missing, returns `0.0`.
- **Component field access**:  
  - `"<entity_name>#<component_name>.<field_name>"`: Read a component value on a specific entity.  
    For example: `light#renderable.emissive` will return the `emissive` value from the `light` entity's `renderable` component.  
    Vector fields such as position or color can be accessed as `pos.x` or `color.r`.  
    **Note**: Only number-convertible fields can be referenced. Not all components are accessible from within the physics thread.
)";

    namespace expression {
        static const size_t MAX_SIGNAL_EXPRESSION_NODES = 256;

        struct Context {
            const DynamicLock<ReadSignalsLock> &lock;
            const SignalExpression &expr;
            const EventData &input;

            Context(const DynamicLock<ReadSignalsLock> &lock, const SignalExpression &expr, const EventData &input);
        };

        struct Node;
    } // namespace expression

    class SignalExpression {
    public:
        SignalExpression() {}
        SignalExpression(const SignalRef &signal);
        SignalExpression(std::string_view expr, const Name &scope = Name());

        SignalExpression(const SignalExpression &other)
            : scope(other.scope), expr(other.expr), rootNode(other.rootNode) {}

        // Called automatically by constructor. Should be called when expression string is changed.
        bool Compile();

        template<typename LockType>
        bool CanEvaluate(const LockType &lock, size_t depth = 0) const {
            if constexpr (LockType::template has_permissions<ReadAll>()) {
                return true;
            } else if constexpr (LockType::template has_permissions<ReadSignalsLock>()) {
                return CanEvaluate((const DynamicLock<ReadSignalsLock> &)lock, depth);
            } else {
                return false;
            }
        }

        bool IsCacheable() const;
        bool CanEvaluate(const DynamicLock<ReadSignalsLock> &lock, size_t depth) const;
        double Evaluate(const DynamicLock<ReadSignalsLock> &lock, size_t depth = 0) const;
        double EvaluateEvent(const DynamicLock<ReadSignalsLock> &lock, const EventData &input) const;

        bool operator==(const SignalExpression &other) const {
            return expr == other.expr && scope == other.scope;
        }

        // True if the expression is valid and can be evaluated.
        // An empty expression is valid and evaluates to 0.
        explicit operator bool() const {
            return rootNode != nullptr;
        }

        // Returns true if this expression is default constructed.
        // Both empty expressions and invalid expressions are considered set (not null).
        bool IsNull() const {
            return expr.empty() && rootNode == nullptr;
        }

        void SetScope(const EntityScope &scope);

        EntityScope scope;
        std::string expr;
        std::shared_ptr<expression::Node> rootNode;
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
