#pragma once

#include "Common.hh"
#include "core/CVar.hh"
#include <functional>

namespace sp
{
	template <typename ParamType>
	class CFunc : public CVarBase
	{
	public:
		typedef std::function<void(ParamType)> Callback;

		CFunc(const string &name, const string &description, Callback callback)
			: CVarBase(name, description), callback(callback) { }

		CFunc(const string &name, Callback callback) : CFunc(name, "", callback) { }

		virtual ~CFunc() { };

		string StringValue()
		{
			return "CFunc:" + GetName();
		}

		void SetFromString(const string &newValue)
		{
			ParamType value;
			std::stringstream in(newValue);
			in >> value;

			callback(value);
		}

		bool IsValueType()
		{
			return false;
		}

	private:
		Callback callback;
	};

	template <>
	class CFunc<void> : public CVarBase
	{
	public:
		typedef std::function<void()> Callback;

		CFunc(const string &name, const string &description, Callback callback)
			: CVarBase(name, description), callback(callback) { }

		CFunc(const string &name, Callback callback) : CFunc(name, "", callback) { }

		virtual ~CFunc() { };

		string StringValue()
		{
			return "CFunc:" + GetName();
		}

		void SetFromString(const string &newValue)
		{
			callback();
		}

		bool IsValueType()
		{
			return false;
		}

	private:
		Callback callback;
	};

	class CFuncCollection
	{
	public:
		template <typename ParamType>
		void Register(const string &name, const string &description, std::function<void(ParamType)> callback)
		{
			collection.push_back(make_shared<CFunc<ParamType>>(name, description, callback));
		}

		void Register(const string &name, const string &description, std::function<void()> callback)
		{
			collection.push_back(make_shared<CFunc<void>>(name, description, callback));
		}

		template <typename ParamType, typename ThisType>
		void RegisterMember(ThisType *parent, const string &name, const string &description, void(ThisType::*callback)(ParamType))
		{
			auto cb = std::bind(callback, parent, std::placeholders::_1);
			collection.push_back(make_shared<CFunc<ParamType>>(name, description, cb));
		}

		template <typename ThisType>
		void RegisterMember(ThisType *parent, const string &name, const string &description, void(ThisType::*callback)())
		{
			auto cb = std::bind(callback, parent);
			collection.push_back(make_shared<CFunc<void>>(name, description, cb));
		}

	private:
		vector<shared_ptr<CVarBase>> collection;
	};
}
