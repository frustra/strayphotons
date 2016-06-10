#pragma once

#include "Common.hh"
#include <sstream>

namespace sp
{
	class CVarBase
	{
	public:
		CVarBase(const string &name, const string &description);

		const string &GetName()
		{
			return name;
		}

		const string &GetDescription()
		{
			return description;
		}

		virtual string StringValue() = 0;
		virtual void SetFromString(const string &newValue) = 0;

	private:
		string name, description;
	};

	template <typename VarType>
	class CVar : public CVarBase
	{
	public:
		CVar(const string &name, const VarType &initial, const string &description)
			: CVarBase(name, description), value(initial)
		{
		}

		VarType Get()
		{
			return value;
		}

		void Set(VarType newValue)
		{
			value = newValue;
		}

		string StringValue()
		{
			std::stringstream out;
			out << value;
			return out.str();
		}

		void SetFromString(const string &newValue)
		{
			std::stringstream in(newValue);
			in >> value;
		}

	private:
		VarType value;
	};
}
