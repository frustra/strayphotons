#pragma once

#include "core/Common.hh"
#include "core/StreamOverloads.hh"

#include <atomic>
#include <functional>
#include <mutex>
#include <sstream>

namespace sp {
    template<typename VarType>
    static inline void ToggleBetweenValues(VarType &var, const string *str_values, size_t count);

    class CVarBase {
    public:
        CVarBase(const string &name, const string &description);
        virtual ~CVarBase();

        const string &GetName() const {
            return name;
        }

        const string &GetNameLower() const {
            return nameLower;
        }

        const string &GetDescription() const {
            return description;
        }

        template<typename Callback>
        void EachCompletion(Callback callback) {
            std::lock_guard lock(completionMutex);
            for (const auto &str : completions) {
                callback(str);
            }
        }

        void RequestCompletion() {
            pendingCompletion = true;
        }

        bool PendingCompletion() const {
            return pendingCompletion;
        }

        template<typename Callback>
        void UpdateCompletions(Callback callback) {
            if (!pendingCompletion) return;

            std::lock_guard lock(completionMutex);
            completions.clear();
            callback(completions);
            pendingCompletion = false;
        }

        virtual string StringValue() = 0;
        virtual void SetFromString(const string &newValue) = 0;
        virtual void ToggleValue(const string *values, size_t count) {}
        virtual bool IsValueType() = 0;

        bool Changed() const {
            return dirty;
        }

    protected:
        bool dirty = true;

    private:
        string name, nameLower, description;

        vector<string> completions;
        std::atomic_bool pendingCompletion = false;
        std::mutex completionMutex;
    };

    template<typename VarType>
    class CVar : public CVarBase {
    public:
        CVar(const string &name, const VarType &initial, const string &description)
            : CVarBase(name, description), value(initial) {}

        inline const VarType &Get() const {
            return value;
        }

        inline const VarType &Get(bool setClean) {
            if (setClean) dirty = false;

            return value;
        }

        void Set(const VarType &newValue) {
            value = newValue;
            dirty = true;
        }

        string StringValue() {
            std::stringstream out;
            out << value;
            return out.str();
        }

        void SetFromString(const string &newValue) {
            if (newValue.size() == 0) return;

            std::stringstream in(newValue);
            in >> value;
            dirty = true;
        }

        void ToggleValue(const string *str_values, size_t count) {
            ToggleBetweenValues(value, str_values, count);
            dirty = true;
        }

        bool IsValueType() {
            return true;
        }

    private:
        VarType value;
    };

    template<>
    inline void CVar<string>::ToggleValue(const string *str_values, size_t count) {
        // Do nothing
    }

    /*
     * Toggle values between those given in the set.
     * If no values are given, a true/false toggle is used.
     */
    template<typename VarType>
    static inline void ToggleBetweenValues(VarType &var, const string *str_values, size_t count) {
        if (count == 0) {
            if (var == VarType()) {
                var = VarType(1);
            } else {
                var = VarType();
            }
        } else if (count == 1) {
            std::stringstream in(str_values[0]);
            VarType v;
            in >> v;
            if (var == v) {
                var = VarType();
            } else {
                var = v;
            }
        } else {
            std::vector<VarType> values(count);
            size_t target = count - 1;
            for (size_t i = 0; i < count && i <= target; i++) {
                std::stringstream in(str_values[i]);
                VarType v;
                in >> v;
                values[i] = v;
                if (var == values[i]) {
                    target = (i + 1) % count;
                }
            }
            var = values[target];
        }
    }
} // namespace sp
