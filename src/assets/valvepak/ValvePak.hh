
#pragma once

#include "Common.hh"
#include "Vpk.hh"

namespace sp
{
	class Asset;

	class ValvePak : public NonCopyable
	{
	public:
		ValvePak(const string &name, shared_ptr<Asset> asset);
		~ValvePak() {}

		const string name;

        const VpkHeader *header;
        const VpkHeaderV2 *header2;
        vector<VpkEntry> entries;
	private:
		shared_ptr<Asset> asset;

        void readEntries(const uint8_t *buffer, int buffer_size);
	};
}
