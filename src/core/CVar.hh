#pragma once

#include "Common.hh"
#include "StreamOverloads.hh"
#include <sstream>
#include <functional>

namespace sp
{
	class CVarBase
	{
	public:
		CVarBase(const string &name, const string &description);
		virtual ~CVarBase();

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
		virtual bool IsValueType() = 0;

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

		inline VarType Get()
		{
			return value;
		}

		inline VarType Get(bool setClean)
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
			if (newValue.size() == 0)
				return;

			std::stringstream in(newValue);
			in >> value;
			dirty = true;
		}

		bool IsValueType()
		{
			return true;
		}

	private:
		VarType value;
	};

	class CFunc : public CVarBase
	{
	public:
		typedef std::function<void(const string &)> Callback;

		CFunc(const string &name, const string &description, Callback callback)
			: CVarBase(name, description), callback(callback)
		{
		}

		CFunc(const string &name, Callback callback) : CFunc(name, "", callback)
		{
		}

		string StringValue()
		{
			return "CFunc:" + GetName();
		}

		void SetFromString(const string &newValue)
		{
			callback(newValue);
		}

		bool IsValueType()
		{
			return false;
		}

	private:
		Callback callback;
	};
}
