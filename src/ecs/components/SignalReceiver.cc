#include "ecs/components/SignalReceiver.hh"
#include "ecs/events/SignalChange.hh"

#include <picojson/picojson.h>
#include <assets/AssetHelpers.hh>

namespace ecs
{
	template<>
	bool Component<SignalReceiver>::LoadEntity(Entity &dst, picojson::value &src)
	{
		auto receiver = dst.Assign<SignalReceiver>();

		for (auto param : src.get<picojson::object>())
		{
			if (param.first == "amplifier")
			{
				receiver->SetAmplifier(param.second.get<double>());
			}
			else if (param.first == "offset")
			{
				receiver->SetOffset(param.second.get<double>());
			}
		}
		return true;
	}

	void SignalReceiver::AttachSignal(Entity signaller, float startSig)
	{
		Entity::Id eId = signaller.GetId();

		if (this->signallers.count(eId) > 0)
		{
			return;
		}

		auto handler = [&](Entity e, const SignalChange & sig)
		{
			this->signallers.at(e.GetId()).signal = sig.signal;
		};

		this->signallers[eId] = {signaller.Subscribe<SignalChange>(handler), startSig};
	}

	void SignalReceiver::DetachSignal(Entity signaller)
	{
		Entity::Id eId = signaller.GetId();
		if (this->signallers.count(eId) <= 0)
		{
			return;
		}

		this->signallers[eId].sub.Unsubscribe();
		this->signallers.erase(eId);
	}

	float SignalReceiver::GetSignal() const
	{
		float signal = 0;
		for (auto &kv : this->signallers)
		{
			const SignalReceiver::Input &inputSig = kv.second;
			signal += inputSig.signal;
		}

		return this->amplifier * signal + this->offset;
	}

	bool SignalReceiver::IsTriggered() const
	{
		return this->GetSignal()
			   > (1.0f - SignalReceiver::TRIGGER_TOLERANCE);
	}

	void SignalReceiver::SetAmplifier(float amp)
	{
		this->amplifier = amp;
	}

	void SignalReceiver::SetOffset(float offs)
	{
		this->offset = offs;
	}

	const float SignalReceiver::TRIGGER_TOLERANCE =
		10 * std::numeric_limits<float>::epsilon();
}