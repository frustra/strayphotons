/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "common/Common.hh"
#include "game/Game.hh"

#ifdef SP_AUDIO_SUPPORT
    #include "audio/AudioManager.hh"
#endif

namespace sp {
    void InitAudioManager(Game &game) {
#ifdef SP_AUDIO_SUPPORT
        game.audio = make_shared<AudioManager>();
#endif
    }
} // namespace sp
