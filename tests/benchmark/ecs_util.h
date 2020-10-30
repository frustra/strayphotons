#pragma once

#include <tuple>
#include <type_traits>

namespace benchmark {
	// is_type_in_set<T, Un...>::value is true if T is in the set Un
	template<typename T, typename... Un>
	struct is_type_in_set {};

	template<typename T, typename U>
	struct is_type_in_set<T, U> {
		static constexpr bool value = std::is_same<T, U>::value;
	};

	template<typename T, typename U, typename... Un>
	struct is_type_in_set<T, U, Un...> {
		static constexpr bool value = is_type_in_set<T, U>::value || is_type_in_set<T, Un...>::value;
	};

}; // namespace benchmark
