/*
 * Stray Photons - Copyright (C) 2023 Jacob Wirth & Justin Li
 *
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
 * If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include "EventHandler.hh"

#include "common/Logging.hh"
#include "ecs/EcsImpl.hh"
#include "xr/openvr/OpenVrSystem.hh"

#include <openvr.h>

namespace sp::xr {
    EventHandler::EventHandler(const OpenVrSystem &vrSystem) : vrSystem(vrSystem) {}

    void EventHandler::Frame() {
        /* TODO:
         * Determine which events we should follow:
         * Pause rendering when headset isn't active
         * Controller add/remove handling
         * Overlay integrations
         * SteamVR shutdown events
         */
        auto vr = vrSystem.vrSystem;

        vr::VREvent_t event;
        while (vr && vr->PollNextEvent(&event, sizeof(event))) {
            /*std::string str;

            switch (event.eventType) {
            case vr::VREvent_TrackedDeviceActivated:
                Debugf("[OVREvent] Controller activated at %f: %d", event.eventAgeSeconds, event.trackedDeviceIndex);
                break;
            case vr::VREvent_TrackedDeviceDeactivated:
                Debugf("[OVREvent] Controller deactivated at %f: %d", event.eventAgeSeconds, event.trackedDeviceIndex);
                break;
            case vr::VREvent_TrackedDeviceUpdated:
                Debugf("[OVREvent] Controller updated at %f: %d", event.eventAgeSeconds, event.trackedDeviceIndex);
                break;
            case vr::VREvent_InputFocusCaptured:
            case vr::VREvent_InputFocusChanged:
            case vr::VREvent_InputFocusReleased:
                Debugf("[OVREvent] Input focus changed at %f: (%d) %d -> %d",
                    event.eventAgeSeconds,
                    event.eventType,
                    event.data.process.oldPid,
                    event.data.process.pid);
                break;
            case vr::VREvent_SceneApplicationChanged:
                Debugf("[OVREvent] Scene application changed at %f: %d -> %d",
                    event.eventAgeSeconds,
                    event.data.process.oldPid,
                    event.data.process.pid);
                break;
            case vr::VREvent_SceneApplicationStateChanged:
                switch (vr::VRApplications()->GetSceneApplicationState()) {
                case vr::EVRSceneApplicationState_None:
                    str = "None";
                    break;
                case vr::EVRSceneApplicationState_Starting:
                    str = "Starting";
                    break;
                case vr::EVRSceneApplicationState_Quitting:
                    str = "Quitting";
                    break;
                case vr::EVRSceneApplicationState_Running:
                    str = "Running";
                    break;
                case vr::EVRSceneApplicationState_Waiting:
                    str = "Waiting";
                    break;
                }
                Debugf("[OVREvent] Scene application state changed at %f: %s", event.eventAgeSeconds, str);
                break;
            case vr::VREvent_Compositor_ApplicationResumed:
                Debugf("[OVREvent] Application resumed at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_StatusUpdate:
                switch (event.data.status.statusState) {
                case vr::VRState_Off:
                    str = "Off";
                    break;
                case vr::VRState_Searching:
                    str = "Searching";
                    break;
                case vr::VRState_Searching_Alert:
                    str = "Searching_Alert";
                    break;
                case vr::VRState_Ready:
                    str = "Ready";
                    break;
                case vr::VRState_Ready_Alert:
                    str = "Ready_Alert";
                    break;
                case vr::VRState_NotReady:
                    str = "NotReady";
                    break;
                case vr::VRState_Standby:
                    str = "Standby";
                    break;
                case vr::VRState_Ready_Alert_Low:
                    str = "Ready_Alert_Low";
                    break;
                default:
                    str = "Undefined";
                    break;
                }
                Debugf("[OVREvent] Status updated at %f: %s", event.eventAgeSeconds, str);
                break;
            case vr::VREvent_EnterStandbyMode:
                Debugf("[OVREvent] Entered standby mode at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_LeaveStandbyMode:
                Debugf("[OVREvent] Left standby mode at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_DashboardActivated:
                Debugf("[OVREvent] Dashboard activated at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_DashboardDeactivated:
                Debugf("[OVREvent] Dashboard deactivated at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_TrackedDeviceUserInteractionStarted:
                Debugf("[OVREvent] User interaction started at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_TrackedDeviceUserInteractionEnded:
                Debugf("[OVREvent] User interaction ended at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_TrackedDeviceRoleChanged: {
                Debugf("[OVREvent] Device role changed at %f: (%d) %s",
                    event.eventAgeSeconds,
                    event.trackedDeviceIndex,
                    str);
                vr::ETrackedControllerRole role = vr::TrackedControllerRole_Max;
                for (vr::TrackedDeviceIndex_t i = 0; i < vr::k_unMaxTrackedDeviceCount; i++) {
                    role = vr->GetControllerRoleForTrackedDeviceIndex(i);
                    switch (role) {
                    case vr::TrackedControllerRole_Invalid:
                        str = "";
                        break;
                    case vr::TrackedControllerRole_LeftHand:
                        str = "LeftHand";
                        break;
                    case vr::TrackedControllerRole_RightHand:
                        str = "RightHand";
                        break;
                    case vr::TrackedControllerRole_OptOut:
                        str = "OptOut";
                        break;
                    case vr::TrackedControllerRole_Treadmill:
                        str = "Treadmill";
                        break;
                    case vr::TrackedControllerRole_Stylus:
                        str = "Stylus";
                        break;
                    }
                    if (!str.empty()) { Debugf("[OVREvent]   Device role for (%u): %s", i, str); }
                }
            } break;
            case vr::VREvent_PropertyChanged:
                // Debugf("[OVREvent] Property changed at %f: (%d) %d",
                //        event.eventAgeSeconds,
                //        event.trackedDeviceIndex,
                //        event.data.property.prop);
                break;
            case vr::VREvent_ChaperoneFlushCache:
                Debugf("[OVREvent] Chaperone cache refresh at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_ChaperoneUniverseHasChanged:
                Debugf("[OVREvent] Chaperone updated at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_Compositor_ChaperoneBoundsShown:
                Debugf("[OVREvent] Chaperone bounds shown at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_Compositor_ChaperoneBoundsHidden:
                Debugf("[OVREvent] Chaperone bounds hidden at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_IpdChanged:
                // Debugf("[OVREvent] IPD changed at %f: %f", event.eventAgeSeconds, event.data.ipd.ipdMeters);
                break;
            case vr::VREvent_OtherSectionSettingChanged:
                Debugf("[OVREvent] Settings: Other section changed at %f: %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_ApplicationListUpdated:
                Debugf("[OVREvent] Applications list updated at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_Input_ActionManifestReloaded:
                Debugf("[OVREvent] Action manifest reloaded at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_ActionBindingReloaded:
                Debugf("[OVREvent] Action bindings reloaded at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_Input_BindingLoadFailed:
                Errorf("[OVREvent] Binding load failed at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_Input_BindingLoadSuccessful:
                Debugf("[OVREvent] Binding load succeeded at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_Input_BindingsUpdated:
                Debugf("[OVREvent] Bindings updated at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_DesktopViewUpdating:
                Debugf("[OVREvent] Desktop view updating at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_DesktopViewReady:
                Debugf("[OVREvent] Desktop view ready at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_ProcessConnected:
                Debugf("[OVREvent] Process connected at %f: %u", event.eventAgeSeconds, event.data.process.pid);
                break;
            case vr::VREvent_ProcessDisconnected:
                Debugf("[OVREvent] Process disconnected at %f: %u", event.eventAgeSeconds, event.data.process.pid);
                break;
            case vr::VREvent_Quit:
                Logf("[OVREvent] OpenVR quiting at %f", event.eventAgeSeconds);
                break;
            case vr::VREvent_ProcessQuit:
                Debugf("[OVREvent] OpenVR process quitting at %f: %u", event.eventAgeSeconds, event.data.process.pid);
                break;
            case vr::VREvent_Compositor_ApplicationNotResponding:
                Logf("[OVREvent] OpenVR Compositor not responding at %f", event.eventAgeSeconds);
                break;
            default:
                Debugf("[OVREvent] Unknown OpenVR event: %d", event.eventType);
            } */
        }
    }
} // namespace sp::xr
