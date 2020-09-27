#pragma once

namespace sp {
	/**
	 * ActionValue to allow storage of generic action value T
	 */
	class ActionValueBase {
	public:
		ActionValueBase() {}
		virtual ~ActionValueBase(){};

		template<typename T>
		const T *Get();
	};

	template<typename T>
	class ActionValue : public ActionValueBase {
	public:
		ActionValue(const T &value) : ActionValueBase(), value(value) {}
		~ActionValue() override {}

		inline const T &Get() {
			return value;
		}

	private:
		const T value;
	};

	template<typename T>
	inline const T *ActionValueBase::Get() {
		auto value = dynamic_cast<ActionValue<T> *>(this);
		if (value != nullptr) {
			return &value->Get();
		}
		return nullptr;
	}
} // namespace sp
