/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "console/CVar.hh"
#include "core/Common.hh"
#include "core/Logging.hh"
#include "ecs/EntityRef.hh"

#include <functional>
#include <magic_enum.hpp>

namespace sp {
    template<typename... ParamTypes>
    class CFunc : public CVarBase {
    public:
        typedef std::function<void(ParamTypes...)> Callback;

        CFunc(const string &name, const string &description, Callback callback)
            : CVarBase(name, description), callback(callback) {
            this->Register();
        }

        CFunc(const string &name, Callback callback) : CFunc(name, "", callback) {}

        virtual ~CFunc() {
            this->UnRegister();
        };

        string StringValue() {
            return "CFunc:" + GetName();
        }

        void SetFromString(const string &newValue) {
            std::tuple<ParamTypes...> values = {};
            std::istringstream in(newValue);
            size_t parsedCount = 0;

            std::apply(
                [&](auto &&...args) {
                    (ParseArgument(args, in, ++parsedCount == sizeof...(ParamTypes)), ...);
                },
                values);

            std::apply(callback, values);
        }

        bool IsValueType() {
            return false;
        }

    private:
        template<typename T>
        void ParseArgument(T &value, std::istringstream &in, bool last) {
            if constexpr (std::is_enum<T>()) {
                std::string enumName;
                in >> enumName;

                auto opt = magic_enum::enum_cast<T>(enumName);
                if (opt) {
                    value = *opt;
                } else {
                    Errorf("Unknown enum value specified for %s: %s", typeid(T).name(), enumName);
                }
            } else {
                in >> value;
            }
        }

        void ParseArgument(ecs::EntityRef &value, std::istringstream &in, bool last) {
            std::string entityName;
            in >> entityName;

            value = ecs::Name(entityName, ecs::Name());
        }

        void ParseArgument(string &value, std::istringstream &in, bool last) {
            in >> std::ws;
            if (last) {
                std::getline(in, value);
                if (value.size() >= 2 && value[0] == '"' && value.back() == '"') {
                    value = value.substr(1, value.size() - 2);
                }
            } else if (in.peek() == '"') {
                in.seekg(1, std::ios_base::cur);
                std::getline(in, value, '"');
            } else {
                in >> value;
            }
        }

        Callback callback;
    };

    template<>
    class CFunc<string> : public CVarBase {
    public:
        typedef std::function<void(string)> Callback;

        CFunc(const string &name, const string &description, Callback callback)
            : CVarBase(name, description), callback(callback) {
            this->Register();
        }

        CFunc(const string &name, Callback callback) : CFunc(name, "", callback) {}

        virtual ~CFunc() {
            this->UnRegister();
        };

        string StringValue() {
            return "CFunc:" + GetName();
        }

        void SetFromString(const string &newValue) {
            callback(newValue);
        }

        bool IsValueType() {
            return false;
        }

    private:
        Callback callback;
    };

    template<>
    class CFunc<void> : public CVarBase {
    public:
        typedef std::function<void()> Callback;

        CFunc(const string &name, const string &description, Callback callback)
            : CVarBase(name, description), callback(callback) {
            this->Register();
        }

        CFunc(const string &name, Callback callback) : CFunc(name, "", callback) {}

        virtual ~CFunc() {
            this->UnRegister();
        };

        string StringValue() {
            return "CFunc:" + GetName();
        }

        void SetFromString(const string &newValue) {
            callback();
        }

        bool IsValueType() {
            return false;
        }

    private:
        Callback callback;
    };

    class CFuncCollection {
    public:
        template<typename... ParamTypes>
        void Register(const string &name, const string &description, typename CFunc<ParamTypes...>::Callback callback) {
            collection.push_back(make_shared<CFunc<ParamTypes...>>(name, description, callback));
        }

        void Register(const string &name, const string &description, std::function<void()> callback) {
            collection.push_back(make_shared<CFunc<void>>(name, description, callback));
        }

        template<typename ThisType, typename... ParamTypes>
        void Register(ThisType *parent,
            const string &name,
            const string &description,
            void (ThisType::*callback)(ParamTypes...)) {
            auto cb = [parent, callback](ParamTypes... args) {
                (parent->*callback)(args...);
            };
            collection.push_back(make_shared<CFunc<ParamTypes...>>(name, description, cb));
        }

        template<typename ThisType>
        void Register(ThisType *parent, const string &name, const string &description, void (ThisType::*callback)()) {
            auto cb = std::bind(callback, parent);
            collection.push_back(make_shared<CFunc<void>>(name, description, cb));
        }

    private:
        vector<shared_ptr<CVarBase>> collection;
    };
} // namespace sp
