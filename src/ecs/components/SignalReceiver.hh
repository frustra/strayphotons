#pragma once

#include "Common.hh"

#include <Ecs.hh>
#include <limits>
#include <map>

namespace ecs
{
	class SignalReceiver
	{
	public:
		struct Input
		{
			Subscription sub;
			float signal;
		};

		/**
		 * Each signaller can only be attached once.
		 */
		void AttachSignal(Entity signaller, float startSig = 0);
		void DetachSignal(Entity signaller);
		float GetSignal() const;
		bool IsTriggered() const;
		void SetAmplifier(float amp);
		void SetOffset(float offs);

	private:
		float amplifier = 1.0f;
		float offset = 0.0f;
		std::map<Entity::Id, Input> signallers;
		
		static const float TRIGGER_TOLERANCE;
	};
}