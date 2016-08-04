#pragma once

#include "Common.hh"
#include "StreamOverloads.hh"
#include <sstream>

namespace sp
{
	class CVarBase
	{
	public:
		CVarBase(const string &name, const string &description);

		const string &GetName() const
		{
			return name;
		}

		const string &GetDescription() const
		{
			return description;
		}

		virtual string StringValue() = 0;
		virtual void SetFromString(const string &newValue) = 0;

		bool Changed() const
		{
			return dirty;
		}

	protected:
		bool dirty = true;

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

		inline VarType Get(bool setClean = false)
		{
			if (setClean)
				dirty = false;

			return value;
		}

		void Set(VarType newValue)
		{
			value = newValue;
			dirty = true;
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
			dirty = true;
		}

	private:
		VarType value;
	};
}
