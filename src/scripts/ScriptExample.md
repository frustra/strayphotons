Script Example Definition
=========================

```c++
    // Registered script objects are instantiated for the lifetime of the entity it is attached to.
    // Each instance is stored in the ScriptState::userData field.
    struct Example {
        // Input parameters
        std::string a = "default value";
        glm::vec3 b;
        ecs::EntityRef c;
        SignalExpression expr;

        // Internal script state
        bool init = false;

        void OnTick(ScriptState &state, Lock<WriteAll> lock, Entity ent, chrono_clock::duration interval) {
            if (!init) {
                // First run only
                Logf("Example script added to %s", ToString(lock, ent));
                init = true;
            }

            Event event;
            while (EventInput::Poll(lock, state.eventQueue, event)) {
                if (event.name != "/script/event") continue;

                // Process event
            }
        }
    };

    // Script input and output parameters are defined the same as Component fields and will be saved and loaded automatically.
    // Any serializable type can be used as an input parameter, including vectors and entity references.
    StructMetadata MetadataExample(typeid(Example),
        StructField::New("field_a", &Example::a),
        StructField::New("field_b", &Example::b),
        StructField::New("field_c", &Example::c),
        StructField::New("input_expression", &Example::expr));

    // Scripts self-registered using an instance of the InternalScript2 template.
    // A script's input events are defined here, as well as if OnTick should be called every logic frame, or only if events are available.
    InternalScript2<Example> example("example", MetadataExample, true /* filterOnEvent */, "/script/event" /* ... more events go here ... */);
```
