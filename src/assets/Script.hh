#pragma once

#include <Common.hh>

namespace sp
{
	class Asset;

	class Script : public NonCopyable
	{
	public:
		Script(const string &path, shared_ptr<Asset> asset, vector<string> &&lines);
		~Script() {}
        
        bool Exec();

		const string path;
	private:
		shared_ptr<Asset> asset;
		vector<string> lines;
	};
}
