Script Example Definition
=========================

```c++
    // Registered script objects are instantiated for the lifetime of the entity it is attached to.
    // Each instance is stored in the ScriptState::scriptData field.
    struct Example {
        // Input parameters
        std::string a = "default value";
        glm::vec3 b = glm::vec3(1, 2, 3);
        ecs::EntityRef c;
        SignalExpression expr;

        // Internal script state
        bool init = false;

        void OnLogicTick(ScriptState &state, Lock<Read<Name>> lock, Entity ent, chrono_clock::duration interval) {
            if (!init) {
                // First run only
                Logf("Example script added to %s", ToString(lock, ent));
                init = true;
            }
        }

        void OnEvent(ScriptState &state, Lock<Write<Signals>> &lock, Entity ent, Event event) {
            if (event.name != "/script/event") return;

            // Process event
        }
    };

    // Script input and output parameters are defined the same as Component fields and will be saved and loaded automatically.
    // Any serializable type can be used as an input parameter, including vectors and entity references.
    StructMetadata MetadataExample(typeid(Example),
        "Example",
        "", // no description
        StructField::New("field_a", &Example::a),
        StructField::New("field_b", &Example::b),
        StructField::New("field_c", &Example::c),
        StructField::New("input_expression", &Example::expr));

    // Scripts self-registered using an instance of the LogicScript template.
    // A script's input events are defined here, as well as if OnLogicTick should be called every logic frame, or only if events are available.
    BuiltInScript<Example> example("example", MetadataExample, "/script/event" /* ... more events go here ... */);
```
