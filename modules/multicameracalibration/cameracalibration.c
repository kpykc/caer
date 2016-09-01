#include "cameracalibration.h"
#include "calibration_settings.h"
#include "calibration_wrapper.h"
#include "base/mainloop.h"
#include "base/module.h"

struct MultiCalibrationState_struct {
	struct MultiCalibrationSettings_struct settings; // Struct containing all settings (shared)
	struct MultiCalibrarion *cpp_class; // Pointer to cpp_class_object
	uint64_t lastFrameTimestamp_cam0;
	uint64_t lastFrameTimestamp_cam1;
	uint32_t points_found;
	uint32_t last_points_found;
	size_t lastFoundPoints;
	bool calibrationLoaded;
};

typedef struct MultiCalibrationState_struct *MultiCalibrationState;

static bool caerMultiCalibrationInit(caerModuleData moduleData);
static void caerMultiCalibrationRun(caerModuleData moduleData,
		size_t argsNumber, va_list args);
static void caerMultiCalibrationConfig(caerModuleData moduleData);
static void caerMultiCalibrationExit(caerModuleData moduleData);
static void updateSettings(caerModuleData moduleData);

static struct caer_module_functions caerMultiCalibrationFunctions = {
		.moduleInit = &caerMultiCalibrationInit, .moduleRun =
				&caerMultiCalibrationRun, .moduleConfig =
				&caerMultiCalibrationConfig, .moduleExit =
				&caerMultiCalibrationExit };

void caerMultiCalibration(uint16_t moduleID, caerFrameEventPacket frame_0,
		caerFrameEventPacket frame_1) {
	caerModuleData moduleData = caerMainloopFindModule(moduleID,
			"MultiCameraCalibration", PROCESSOR);
	if (moduleData == NULL) {
		return;
	}

	caerModuleSM(&caerMultiCalibrationFunctions, moduleData,
			sizeof(struct MultiCalibrationState_struct), 2, frame_0, frame_1);
}

static bool caerMultiCalibrationInit(caerModuleData moduleData) {
	MultiCalibrationState state = moduleData->moduleState;

	// Create config settings.
	sshsNodePutBoolIfAbsent(moduleData->moduleNode, "doCalibration", false); // Do calibration using live images
	sshsNodePutStringIfAbsent(moduleData->moduleNode, "saveFileName_intrinsics",
			"intrinsics.xml");
	sshsNodePutStringIfAbsent(moduleData->moduleNode, "loadFileName_cam0",
			"camera_calib_0.xml"); // The name of the file from which to load the calibration
	sshsNodePutBoolIfAbsent(moduleData->moduleNode, "useFisheyeModel_cam0",
			false); // Use Fisheye camera model for calibration
	sshsNodePutStringIfAbsent(moduleData->moduleNode, "saveFileName_extrinsics",
			"extrinsics.xml");
	sshsNodePutStringIfAbsent(moduleData->moduleNode, "loadFileName_cam1",
			"camera_calib_1.xml"); // The name of the file from which to load the calibration
	sshsNodePutBoolIfAbsent(moduleData->moduleNode, "useFisheyeModel_cam1",
			false); // Use Fisheye camera model for calibration
	sshsNodePutIntIfAbsent(moduleData->moduleNode, "verbose", 0);
	sshsNodePutIntIfAbsent(moduleData->moduleNode, "boardWidth", 9); // The size of the board (width)
	sshsNodePutIntIfAbsent(moduleData->moduleNode, "boardHeigth", 5); // The size of the board (height)
	sshsNodePutIntIfAbsent(moduleData->moduleNode, "captureDelay", 50000);
	sshsNodePutIntIfAbsent(moduleData->moduleNode, "numPairsImagesBeforCalib",20);
	sshsNodePutFloatIfAbsent(moduleData->moduleNode, "boardSquareSize", 1.0f); // The size of a square in your defined unit (point, millimeter, etc.)
	sshsNodePutFloatIfAbsent(moduleData->moduleNode, "acceptableAvrEpipolarErr", 200.0f);
	sshsNodePutFloatIfAbsent(moduleData->moduleNode, "acceptableRMSErr", 200.0f);
	sshsNodePutBoolIfAbsent(moduleData->moduleNode, "doDisparity", false); // Do calibration using live images


	// Update all settings.
	updateSettings(moduleData);

	// Initialize C++ class for OpenCV integration.
	state->cpp_class = multicalibration_init(&state->settings);
	if (state->cpp_class == NULL) {
		return (false);
	}

	// Add config listeners last, to avoid having them dangling if Init doesn't succeed.
	sshsNodeAddAttributeListener(moduleData->moduleNode, moduleData,
			&caerModuleConfigDefaultListener);
	//not loaded at the init
	state->calibrationLoaded = false;
	state->points_found = 0;
	state->last_points_found = 0;

	return (true);
}

static void updateSettings(caerModuleData moduleData) {
	MultiCalibrationState state = moduleData->moduleState;

	state->settings.doCalibration = sshsNodeGetBool(moduleData->moduleNode,
			"doCalibration");
	state->settings.saveFileName_extrinsics = sshsNodeGetString(
			moduleData->moduleNode, "saveFileName_extrinsics");
	state->settings.loadFileName_cam0 = sshsNodeGetString(
			moduleData->moduleNode, "loadFileName_cam0");
	state->settings.useFisheyeModel_cam0 = sshsNodeGetBool(
			moduleData->moduleNode, "useFisheyeModel_cam0");
	state->settings.saveFileName_intrinsics = sshsNodeGetString(
			moduleData->moduleNode, "saveFileName_intrinsics");
	state->settings.loadFileName_cam1 = sshsNodeGetString(
			moduleData->moduleNode, "loadFileName_cam1");
	state->settings.useFisheyeModel_cam1 = sshsNodeGetBool(
			moduleData->moduleNode, "useFisheyeModel_cam1");
	state->settings.verbose = sshsNodeGetInt(moduleData->moduleNode, "verbose");
	state->settings.boardWidth = U32T(
			sshsNodeGetInt(moduleData->moduleNode, "boardWidth"));
	state->settings.boardHeigth = U32T(
			sshsNodeGetInt(moduleData->moduleNode, "boardHeigth"));
	state->settings.captureDelay = sshsNodeGetInt(moduleData->moduleNode,
			"captureDelay");
	state->settings.numPairsImagesBeforCalib = sshsNodeGetInt(
			moduleData->moduleNode, "numPairsImagesBeforCalib");
	state->settings.boardSquareSize = sshsNodeGetFloat(moduleData->moduleNode,
			"boardSquareSize");
	state->settings.doDisparity = sshsNodeGetBool(moduleData->moduleNode,
				"doDisparity");
	state->settings.acceptableAvrEpipolarErr = sshsNodeGetFloat(moduleData->moduleNode,
			"acceptableAvrEpipolarErr");
	state->settings.acceptableRMSErr = sshsNodeGetFloat(moduleData->moduleNode,
				"acceptableRMSErr");

}

static void caerMultiCalibrationConfig(caerModuleData moduleData) {
	caerModuleConfigUpdateReset(moduleData);

	MultiCalibrationState state = moduleData->moduleState;

}

static void caerMultiCalibrationExit(caerModuleData moduleData) {
	// Remove listener, which can reference invalid memory in userData.
	sshsNodeRemoveAttributeListener(moduleData->moduleNode, moduleData,
			&caerModuleConfigDefaultListener);

	MultiCalibrationState state = moduleData->moduleState;

	//Multicalibration_destroy(state->cpp_class);

	free(state->settings.saveFileName_intrinsics);
	free(state->settings.saveFileName_extrinsics);
	free(state->settings.loadFileName_cam0);
	free(state->settings.loadFileName_cam1);

}

static void caerMultiCalibrationRun(caerModuleData moduleData,
		size_t argsNumber, va_list args) {
	UNUSED_ARGUMENT(argsNumber);

	// Interpret variable arguments (same as above in main function).
	caerFrameEventPacket frame_0 = va_arg(args, caerFrameEventPacket);
	caerFrameEventPacket frame_1 = va_arg(args, caerFrameEventPacket);

	MultiCalibrationState state = moduleData->moduleState;

	// At this point we always try to load the calibration settings for undistorsion.
	// Maybe they just got created or exist from a previous run.
	if (!state->calibrationLoaded) {
		state->calibrationLoaded = multicalibration_loadCalibrationFile(
				state->cpp_class, &state->settings);
	}


	// Multi Camera calibration is done only using frames.
	if (state->settings.doCalibration && frame_0 != NULL && frame_1 != NULL) {

		bool frame_0_pattern = false;
		uint64_t frame_0_ts = NULL;
		bool frame_1_pattern = false;
		uint64_t frame_1_ts = NULL;
		void * foundPoint_cam1 = NULL;
		void * foundPoint_cam0 = NULL;

		CAER_FRAME_ITERATOR_VALID_START(frame_0)
		// Only work on new frames if enough time has passed between this and the last used one.
			uint64_t currTimestamp_0 = U64T(
					caerFrameEventGetTSStartOfFrame64(caerFrameIteratorElement,
							frame_0));
			// If enough time has passed, try to add a new point set.
			if ((currTimestamp_0 - state->lastFrameTimestamp_cam0)
					>= state->settings.captureDelay) {
				state->lastFrameTimestamp_cam0 = currTimestamp_0;

				foundPoint_cam0 = multicalibration_findNewPoints(
						state->cpp_class, caerFrameIteratorElement, 0);
				if (foundPoint_cam0 != NULL) {
					caerLog(CAER_LOG_WARNING, moduleData->moduleSubSystemString,
							"Found calibration pattern cam0");
					frame_0_ts = currTimestamp_0;
					frame_0_pattern = true;
				}
			}CAER_FRAME_ITERATOR_VALID_END

		CAER_FRAME_ITERATOR_VALID_START(frame_1)
		// Only work on new frames if enough time has passed between this and the last used one.
			uint64_t currTimestamp_0 = U64T(
					caerFrameEventGetTSStartOfFrame64(caerFrameIteratorElement,
							frame_1));
			// If enough time has passed, try to add a new point set.
			if ((currTimestamp_0 - state->lastFrameTimestamp_cam1)
					>= state->settings.captureDelay) {
				state->lastFrameTimestamp_cam1 = currTimestamp_0;

				foundPoint_cam1 = multicalibration_findNewPoints(
						state->cpp_class, caerFrameIteratorElement, 1);
				if (foundPoint_cam1 != NULL) {
					caerLog(CAER_LOG_WARNING, moduleData->moduleSubSystemString,
							"Found calibration pattern cam1");
					frame_1_ts = currTimestamp_0;
					frame_1_pattern = true;
				}
			}CAER_FRAME_ITERATOR_VALID_END

		if (frame_1_pattern && frame_0_pattern) {
			//check Timestamp difference of last two found frames
			if (!(abs(frame_1_ts - frame_0_ts) < state->settings.captureDelay)) {
				caerLog(CAER_LOG_WARNING, moduleData->moduleSubSystemString,
						"Both camera have seen the calibration pattern... adding valid points");
				//increment number of points
				state->points_found += 1;
				multicalibration_addStereoCalibVec(state->cpp_class,
						foundPoint_cam0, foundPoint_cam1);
				caerLog(CAER_LOG_WARNING, moduleData->moduleSubSystemString,
						"Pairs have been successfully detected");
			}

		}

		multicalibration_freeStereoVec(foundPoint_cam0, foundPoint_cam1);

		if ((state->points_found >= state->settings.numPairsImagesBeforCalib) && (state->last_points_found < state->points_found) ) {
			caerLog(CAER_LOG_WARNING, moduleData->moduleSubSystemString,
					"Running stereo calibration ...");
			bool calib_done = multicalibration_stereoCalibrate(state->cpp_class, &state->settings);
			state->last_points_found = state->points_found; // make sure we do not re-calibrate if we did not add new points
			if(calib_done)
				sshsNodePutBool(moduleData->moduleNode, "doCalibration", false); // calibration done
			else
				caerLog(CAER_LOG_WARNING, moduleData->moduleSubSystemString,
								"Keep acquiring images, error not acceptable ...");
		}

	}

	// update settings
	updateSettings(moduleData);

}
