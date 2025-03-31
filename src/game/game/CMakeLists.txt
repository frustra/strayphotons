#
# Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
#
# This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
# If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#

target_sources(${PROJECT_GAME_LIB} PRIVATE
    CGameContext.cc
    Game.cc
    GameLogic.cc
    Scene.cc
    SceneManager.cc
    SceneSaving.cc
)
