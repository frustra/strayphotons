/*
 * Stray Photons - Copyright (C) 2026 Jacob Wirth & Justine Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#pragma once

#include "magic_enum.hpp"
#include "strayphotons/StreamOverloads.hh"
#include "strayphotons/Utility.hh"

#include <atomic>
#include <mutex>
#include <sstream>
#include <type_traits>
#include <vector>

namespace sp {
    template<typename VarType>
    static inline void ToggleBetweenValues(VarType &var, const std::string *str_values, size_t count);

    class CVarBase {
    public:
        CVarBase(const std::string &name, const std::string &description);
        virtual ~CVarBase() {}

        void Register();
        void UnRegister();

        const std::string &GetName() const {
            return name;
        }

        const std::string &GetNameLower() const {
            return nameLower;
        }

        const std::string &GetDescription() const {
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

        virtual std::string StringValue() = 0;
        virtual void SetFromString(const std::string &newValue) = 0;
        virtual void ToggleValue(const std::string *values, size_t count) {}
        virtual bool IsValueType() = 0;

        bool Changed() const {
            return dirty;
        }

    protected:
        bool dirty = true;

    private:
        std::string name, nameLower, description;

        std::vector<std::string> completions;
        std::atomic_bool pendingCompletion = false;
        std::mutex completionMutex;
    };

    template<typename VarType>
    class CVar : public CVarBase {
    private:
        static inline std::string appendEnumDescription(const std::string &description) {
            if constexpr (std::is_enum<VarType>()) {
                std::stringstream ss;
                bool first = true;
                ss << " (";
                for (const VarType &v : magic_enum::enum_values<VarType>()) {
                    if (first) {
                        first = false;
                    } else {
                        ss << ", ";
                    }
                    ss << magic_enum::enum_underlying(v) << ": " << magic_enum::enum_name(v);
                }
                ss << ")";
                return description + ss.str();
            }
            return description;
        }

    public:
        CVar(const std::string &name, const VarType &initial, const std::string &description)
            : CVarBase(name, appendEnumDescription(description)), value(initial) {
            this->Register();
        }

        virtual ~CVar() {
            this->UnRegister();
        }

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

        std::string StringValue() {
            std::stringstream out;
            if constexpr (std::is_enum<VarType>()) {
                out << static_cast<std::underlying_type_t<VarType>>(value);
                out << " (" << magic_enum::enum_name(value) << ")";
            } else {
                out << value;
            }
            return out.str();
        }

        static inline VarType FromString(const std::string &strValue) {
            if constexpr (std::is_enum<VarType>()) {
                std::string lowerInput = to_lower_copy(strValue);
                for (const auto &[enumValue, enumName] : magic_enum::enum_entries<VarType>()) {
                    if (lowerInput == to_lower_copy(enumName)) {
                        return enumValue;
                    }
                }
                std::underlying_type_t<VarType> v;
                std::stringstream in(strValue);
                in >> v;
                auto optValue = magic_enum::enum_cast<VarType>(v);
                if (optValue) return *optValue;
                return {};
            } else {
                VarType v;
                std::stringstream in(strValue);
                in >> v;
                return v;
            }
        }

        void SetFromString(const std::string &newValue) {
            if (newValue.size() == 0) return;

            value = FromString(newValue);
            dirty = true;
        }

        void ToggleValue(const std::string *str_values, size_t count) {
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
    inline void CVar<std::string>::ToggleValue(const std::string *str_values, size_t count) {
        // Do nothing
    }

    /*
     * Toggle values between those given in the set.
     * If no values are given, a true/false toggle is used.
     */
    template<typename VarType>
    static inline void ToggleBetweenValues(VarType &var, const std::string *str_values, size_t count) {
        if (count == 0) {
            if (var == VarType()) {
                var = VarType(1);
            } else {
                var = VarType();
            }
        } else if (count == 1) {
            VarType v = CVar<VarType>::FromString(str_values[0]);
            if (var == v) {
                var = VarType();
            } else {
                var = v;
            }
        } else {
            std::vector<VarType> values(count);
            size_t target = count - 1;
            for (size_t i = 0; i < count && i <= target; i++) {
                values[i] = CVar<VarType>::FromString(str_values[i]);
                if (var == values[i]) {
                    target = (i + 1) % count;
                }
            }
            var = values[target];
        }
    }
} // namespace sp
