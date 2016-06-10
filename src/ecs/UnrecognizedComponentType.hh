#pragma once

#include <typeindex>
#include <sstream>
#include <stdexcept>

class UnrecognizedComponentType : public std::invalid_argument
{
public:
	UnrecognizedComponentType(std::type_index typeIndex)
		: invalid_argument(""), typeIndex(typeIndex) {}
	virtual const char* what() const throw()
	{
		std::stringstream ss;
		ss << "component type " << string(typeIndex.name()) << " is not recognized. "
		   << "Make sure you register it with EntityManager::RegisterComponentType.";

		return ss.str().c_str();
	}

private:
	std::type_index typeIndex;
};