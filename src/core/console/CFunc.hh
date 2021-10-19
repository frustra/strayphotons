#pragma once

#include "console/CVar.hh"
#include "core/Common.hh"

#include <functional>

namespace sp {
    class RestOfLine : public string {
    public:
        friend std::istream &operator>>(std::istream &input, RestOfLine &str) {
            return getline(input, str);
        }
    };

    template<typename... ParamTypes>
    class CFunc : public CVarBase {
    public:
        typedef std::function<void(ParamTypes...)> Callback;

        CFunc(const string &name, const string &description, Callback callback)
            : CVarBase(name, description), callback(callback) {}

        CFunc(const string &name, Callback callback) : CFunc(name, "", callback) {}

        virtual ~CFunc(){};

        string StringValue() {
            return "CFunc:" + GetName();
        }

        void SetFromString(const string &newValue) {
            std::tuple<ParamTypes...> values = {};
            std::istringstream in(newValue);

            std::apply(
                [&](auto &&...args) {
                    (ParseArgument(args, in), ...);
                },
                values);

            std::apply(callback, values);
        }

        bool IsValueType() {
            return false;
        }

    private:
        template<typename T>
        void ParseArgument(T &value, std::istringstream &in) {
            in >> value;
        }

        void ParseArgument(string &value, std::istringstream &in) {
            in >> std::ws;
            if (in.peek() == '"') {
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
            : CVarBase(name, description), callback(callback) {}

        CFunc(const string &name, Callback callback) : CFunc(name, "", callback) {}

        virtual ~CFunc(){};

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
            : CVarBase(name, description), callback(callback) {}

        CFunc(const string &name, Callback callback) : CFunc(name, "", callback) {}

        virtual ~CFunc(){};

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
        void Register(const string &name, const string &description, CFunc<ParamTypes...>::Callback callback) {
            collection.push_back(make_shared<CFunc<ParamTypes...>>(name, description, callback));
        }

        void Register(const string &name, const string &description, std::function<void()> callback) {
            collection.push_back(make_shared<CFunc<void>>(name, description, callback));
        }

        template<typename ParamType, typename ThisType>
        void Register(ThisType *parent,
                      const string &name,
                      const string &description,
                      void (ThisType::*callback)(ParamType)) {
            auto cb = std::bind(callback, parent, std::placeholders::_1);
            collection.push_back(make_shared<CFunc<ParamType>>(name, description, cb));
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
