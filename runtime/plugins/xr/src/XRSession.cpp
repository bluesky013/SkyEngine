//
// Created by blues on 2024/3/3.
//

#include <xr/XRSession.h>
#include <xr/XRInstance.h>
#include <core/logger/Logger.h>

static const char* TAG = "XRSession";

namespace sky {
    XRSession::~XRSession()
    {
        if (space != XR_NULL_HANDLE) {
            xrDestroySpace(space);
        }

        if (session != XR_NULL_HANDLE) {
            xrDestroySession(session);
        }
    }

    bool XRSession::Init(const void *graphicsBinding)
    {
        XrInstance instHandle = instance.GetXrInstanceHandle();

        XrSessionCreateInfo createInfo{XR_TYPE_SESSION_CREATE_INFO};
        createInfo.next = graphicsBinding;
        createInfo.systemId = instance.GetXrSystemId();
        auto result = xrCreateSession(instHandle, &createInfo, &session);
        if (result != XR_SUCCESS) {
            return false;
        }

        uint32_t spaceCount;
        XR_CHECK_RESULT(xrEnumerateReferenceSpaces(session, 0, &spaceCount, nullptr));
        std::vector<XrReferenceSpaceType> spaces(spaceCount);
        XR_CHECK_RESULT(xrEnumerateReferenceSpaces(session, spaceCount, &spaceCount, spaces.data()));

        XrActionSetCreateInfo actionSetInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
        strcpy_s(actionSetInfo.actionSetName, "gameplay");
        strcpy_s(actionSetInfo.localizedActionSetName, "Gameplay");
        actionSetInfo.priority = 0;
        result = xrCreateActionSet(instHandle, &actionSetInfo, &inputState.actionSet);
        if (result != XR_SUCCESS) {
            return false;
        }

        XR_CHECK_RESULT(xrStringToPath(instHandle, "/user/hand/left", &inputState.handSubActionPath[static_cast<uint32_t>(XRSide::LEFT)]));
        XR_CHECK_RESULT(xrStringToPath(instHandle, "/user/hand/right", &inputState.handSubActionPath[static_cast<uint32_t>(XRSide::RIGHT)]));

        {
            // Create an input action for grabbing objects with the left and right hands.
            XrActionCreateInfo actionInfo{XR_TYPE_ACTION_CREATE_INFO};
            actionInfo.actionType = XR_ACTION_TYPE_FLOAT_INPUT;
            strcpy_s(actionInfo.actionName, "grab_object");
            strcpy_s(actionInfo.localizedActionName, "Grab Object");
            actionInfo.countSubactionPaths = uint32_t(inputState.handSubActionPath.size());
            actionInfo.subactionPaths = inputState.handSubActionPath.data();
            XR_CHECK_RESULT(xrCreateAction(inputState.actionSet, &actionInfo, &inputState.grabAction));

            actionInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
            strcpy_s(actionInfo.actionName, "hand_pose");
            strcpy_s(actionInfo.localizedActionName, "Hand Pose");
            actionInfo.countSubactionPaths = uint32_t(inputState.handSubActionPath.size());
            actionInfo.subactionPaths = inputState.handSubActionPath.data();
            XR_CHECK_RESULT(xrCreateAction(inputState.actionSet, &actionInfo, &inputState.poseAction));

            actionInfo.actionType = XR_ACTION_TYPE_VIBRATION_OUTPUT;
            strcpy_s(actionInfo.actionName, "vibrate_hand");
            strcpy_s(actionInfo.localizedActionName, "Vibrate Hand");
            actionInfo.countSubactionPaths = uint32_t(inputState.handSubActionPath.size());
            actionInfo.subactionPaths = inputState.handSubActionPath.data();
            XR_CHECK_RESULT(xrCreateAction(inputState.actionSet, &actionInfo, &inputState.vibrateAction));

            actionInfo.actionType = XR_ACTION_TYPE_BOOLEAN_INPUT;
            strcpy_s(actionInfo.actionName, "quit_session");
            strcpy_s(actionInfo.localizedActionName, "Quit Session");
            actionInfo.countSubactionPaths = 0;
            actionInfo.subactionPaths = nullptr;
            XR_CHECK_RESULT(xrCreateAction(inputState.actionSet, &actionInfo, &inputState.quitAction));
        }


        XrActionSpaceCreateInfo actionSpaceInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        actionSpaceInfo.action = inputState.poseAction;
        actionSpaceInfo.poseInActionSpace.orientation.w = 1.f;
        actionSpaceInfo.subactionPath = inputState.handSubActionPath[static_cast<uint32_t>(XRSide::LEFT)];
        XR_CHECK_RESULT(xrCreateActionSpace(session, &actionSpaceInfo, &inputState.handSpace[static_cast<uint32_t>(XRSide::LEFT)]));
        actionSpaceInfo.subactionPath = inputState.handSubActionPath[static_cast<uint32_t>(XRSide::RIGHT)];
        XR_CHECK_RESULT(xrCreateActionSpace(session, &actionSpaceInfo, &inputState.handSpace[static_cast<uint32_t>(XRSide::RIGHT)]));

        XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
        attachInfo.countActionSets = 1;
        attachInfo.actionSets = &inputState.actionSet;
        XR_CHECK_RESULT(xrAttachSessionActionSets(session, &attachInfo));


        // action space
        XrReferenceSpaceCreateInfo spaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        spaceInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        spaceInfo.poseInReferenceSpace.orientation.w = 1.f;

        result = xrCreateReferenceSpace(session, &spaceInfo, &space);
        if (result != XR_SUCCESS) {
            return false;
        }

        LOG_I(TAG, "Init XR Session Success.");
        return true;
    }

    void XRSession::SessionStateHandler(const XrEventDataSessionStateChanged& stateChangedEvent)
    {
        if ((stateChangedEvent.session != XR_NULL_HANDLE) && (stateChangedEvent.session != session)) {
            return;
        }

        sessionState = stateChangedEvent.state;
        switch (sessionState) {
            case XR_SESSION_STATE_READY: {
                XrSessionBeginInfo sessionBeginInfo{XR_TYPE_SESSION_BEGIN_INFO};
                sessionBeginInfo.primaryViewConfigurationType = instance.GetOptions().viewCfgType;
                XR_CHECK_RESULT(xrBeginSession(session, &sessionBeginInfo));
                running = true;
                break;
            }
            case XR_SESSION_STATE_STOPPING: {
                XR_CHECK_RESULT(xrEndSession(session))
                running = false;
                break;
            }
            case XR_SESSION_STATE_EXITING:
            case XR_SESSION_STATE_LOSS_PENDING:
            default:
                break;
        }
    }

    void XRSession::BeginFrame()
    {
        if (running) {
            XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
            XR_CHECK_RESULT(xrWaitFrame(session, &frameWaitInfo, &frameState));

            XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
            XR_CHECK_RESULT(xrBeginFrame(session, &frameBeginInfo));
        }
    }

    void XRSession::EndFrame(const XrFrameEndInfo &endInfo)
    {
        if (running) {
            XR_CHECK_RESULT(xrEndFrame(session, &endInfo));
        }
    }

    void XRSession::PollActions()
    {
        inputState.handActive = {XR_FALSE, XR_FALSE};

        const XrActiveActionSet activeActionSet{inputState.actionSet, XR_NULL_PATH};
        XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO};
        syncInfo.countActiveActionSets = 1;
        syncInfo.activeActionSets = &activeActionSet;
        XR_CHECK_RESULT(xrSyncActions(session, &syncInfo));

        for (auto hand : {XRSide::LEFT, XRSide::RIGHT}) {
            auto handIdx = static_cast<uint32_t>(hand);

            XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO};
            getInfo.action = inputState.grabAction;
            getInfo.subactionPath = inputState.handSubActionPath[handIdx];

            XrActionStateFloat grabValue{XR_TYPE_ACTION_STATE_FLOAT};
            XR_CHECK_RESULT(xrGetActionStateFloat(session, &getInfo, &grabValue));
            if (grabValue.isActive == XR_TRUE) {
                inputState.handScale[handIdx] = 1.0f - 0.5f * grabValue.currentState;
                if (grabValue.currentState > 0.9f) {
                    XrHapticVibration vibration{XR_TYPE_HAPTIC_VIBRATION};
                    vibration.amplitude = 0.5;
                    vibration.duration = XR_MIN_HAPTIC_DURATION;
                    vibration.frequency = XR_FREQUENCY_UNSPECIFIED;

                    XrHapticActionInfo hapticActionInfo{XR_TYPE_HAPTIC_ACTION_INFO};
                    hapticActionInfo.action = inputState.vibrateAction;
                    hapticActionInfo.subactionPath = inputState.handSubActionPath[handIdx];
                    XR_CHECK_RESULT(xrApplyHapticFeedback(session, &hapticActionInfo, (XrHapticBaseHeader*)&vibration));
                }
            }

            getInfo.action = inputState.poseAction;
            XrActionStatePose poseState{XR_TYPE_ACTION_STATE_POSE};
            XR_CHECK_RESULT(xrGetActionStatePose(session, &getInfo, &poseState));
            inputState.handActive[handIdx] = poseState.isActive;
        }

        XrActionStateGetInfo getInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr, inputState.quitAction, XR_NULL_PATH};
        XrActionStateBoolean quitValue{XR_TYPE_ACTION_STATE_BOOLEAN};
        XR_CHECK_RESULT(xrGetActionStateBoolean(session, &getInfo, &quitValue));
        if ((quitValue.isActive == XR_TRUE) && (quitValue.changedSinceLastSync == XR_TRUE) && (quitValue.currentState == XR_TRUE)) {
            XR_CHECK_RESULT(xrRequestExitSession(session));
        }
    }

} // namespace sky