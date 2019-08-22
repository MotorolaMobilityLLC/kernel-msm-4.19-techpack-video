// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/jiffies.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <soc/qcom/subsystem_restart.h>
#include <asm/div64.h>
#include "msm_vidc_common.h"
#include "vidc_hfi_api.h"
#include "vidc_hfi.h"
#include "msm_vidc_debug.h"
#include "msm_vidc_clocks.h"
#include "msm_cvp_external.h"
#include "msm_cvp_internal.h"
#include "msm_vidc_buffer_calculations.h"

#define IS_ALREADY_IN_STATE(__p, __d) (\
	(__p >= __d)\
)

#define V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT \
		V4L2_EVENT_MSM_VIDC_PORT_SETTINGS_CHANGED_INSUFFICIENT
#define V4L2_EVENT_RELEASE_BUFFER_REFERENCE \
		V4L2_EVENT_MSM_VIDC_RELEASE_BUFFER_REFERENCE

static void handle_session_error(enum hal_command_response cmd, void *data);
static void msm_vidc_print_running_insts(struct msm_vidc_core *core);

#define V4L2_H264_LEVEL_UNKNOWN V4L2_MPEG_VIDEO_H264_LEVEL_UNKNOWN
#define V4L2_HEVC_LEVEL_UNKNOWN V4L2_MPEG_VIDEO_HEVC_LEVEL_UNKNOWN
#define V4L2_VP9_LEVEL_61 V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_61

int msm_comm_g_ctrl_for_id(struct msm_vidc_inst *inst, int id)
{
	struct v4l2_ctrl *ctrl;

	ctrl = get_ctrl(inst, id);
	return ctrl->val;
}

int msm_comm_hfi_to_v4l2(int id, int value)
{
	switch (id) {
		/* H264 */
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		switch (value) {
		case HFI_H264_PROFILE_BASELINE:
			return V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE;
		case HFI_H264_PROFILE_CONSTRAINED_BASE:
			return
			V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_BASELINE;
		case HFI_H264_PROFILE_MAIN:
			return V4L2_MPEG_VIDEO_H264_PROFILE_MAIN;
		case HFI_H264_PROFILE_HIGH:
			return V4L2_MPEG_VIDEO_H264_PROFILE_HIGH;
		case HFI_H264_PROFILE_STEREO_HIGH:
			return V4L2_MPEG_VIDEO_H264_PROFILE_STEREO_HIGH;
		case HFI_H264_PROFILE_MULTIVIEW_HIGH:
			return V4L2_MPEG_VIDEO_H264_PROFILE_MULTIVIEW_HIGH;
		case HFI_H264_PROFILE_CONSTRAINED_HIGH:
			return V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_HIGH;
		default:
			goto unknown_value;
		}
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		switch (value) {
		case HFI_H264_LEVEL_1:
			return V4L2_MPEG_VIDEO_H264_LEVEL_1_0;
		case HFI_H264_LEVEL_1b:
			return V4L2_MPEG_VIDEO_H264_LEVEL_1B;
		case HFI_H264_LEVEL_11:
			return V4L2_MPEG_VIDEO_H264_LEVEL_1_1;
		case HFI_H264_LEVEL_12:
			return V4L2_MPEG_VIDEO_H264_LEVEL_1_2;
		case HFI_H264_LEVEL_13:
			return V4L2_MPEG_VIDEO_H264_LEVEL_1_3;
		case HFI_H264_LEVEL_2:
			return V4L2_MPEG_VIDEO_H264_LEVEL_2_0;
		case HFI_H264_LEVEL_21:
			return V4L2_MPEG_VIDEO_H264_LEVEL_2_1;
		case HFI_H264_LEVEL_22:
			return V4L2_MPEG_VIDEO_H264_LEVEL_2_2;
		case HFI_H264_LEVEL_3:
			return V4L2_MPEG_VIDEO_H264_LEVEL_3_0;
		case HFI_H264_LEVEL_31:
			return V4L2_MPEG_VIDEO_H264_LEVEL_3_1;
		case HFI_H264_LEVEL_32:
			return V4L2_MPEG_VIDEO_H264_LEVEL_3_2;
		case HFI_H264_LEVEL_4:
			return V4L2_MPEG_VIDEO_H264_LEVEL_4_0;
		case HFI_H264_LEVEL_41:
			return V4L2_MPEG_VIDEO_H264_LEVEL_4_1;
		case HFI_H264_LEVEL_42:
			return V4L2_MPEG_VIDEO_H264_LEVEL_4_2;
		case HFI_H264_LEVEL_5:
			return V4L2_MPEG_VIDEO_H264_LEVEL_5_0;
		case HFI_H264_LEVEL_51:
			return V4L2_MPEG_VIDEO_H264_LEVEL_5_1;
		case HFI_H264_LEVEL_52:
			return V4L2_MPEG_VIDEO_H264_LEVEL_5_2;
		case HFI_H264_LEVEL_6:
			return V4L2_MPEG_VIDEO_H264_LEVEL_6_0;
		case HFI_H264_LEVEL_61:
			return V4L2_MPEG_VIDEO_H264_LEVEL_6_1;
		case HFI_H264_LEVEL_62:
			return V4L2_MPEG_VIDEO_H264_LEVEL_6_2;
		default:
			goto unknown_value;
		}

	case V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE:
		switch (value) {
		case HFI_H264_ENTROPY_CAVLC:
			return V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CAVLC;
		case HFI_H264_ENTROPY_CABAC:
			return V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC;
		default:
			goto unknown_value;
		}
	case V4L2_CID_MPEG_VIDEO_HEVC_PROFILE:
		switch (value) {
		case HFI_HEVC_PROFILE_MAIN:
			return V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN;
		case HFI_HEVC_PROFILE_MAIN10:
			return V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_10;
		case HFI_HEVC_PROFILE_MAIN_STILL_PIC:
			return V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_STILL_PICTURE;
		default:
			goto unknown_value;
		}
	case V4L2_CID_MPEG_VIDEO_HEVC_LEVEL:
	switch (value) {
	case HFI_HEVC_LEVEL_1:
		return V4L2_MPEG_VIDEO_HEVC_LEVEL_1;
	case HFI_HEVC_LEVEL_2:
		return V4L2_MPEG_VIDEO_HEVC_LEVEL_2;
	case HFI_HEVC_LEVEL_21:
		return V4L2_MPEG_VIDEO_HEVC_LEVEL_2_1;
	case HFI_HEVC_LEVEL_3:
		return V4L2_MPEG_VIDEO_HEVC_LEVEL_3;
	case HFI_HEVC_LEVEL_31:
		return V4L2_MPEG_VIDEO_HEVC_LEVEL_3_1;
	case HFI_HEVC_LEVEL_4:
		return V4L2_MPEG_VIDEO_HEVC_LEVEL_4;
	case HFI_HEVC_LEVEL_41:
		return V4L2_MPEG_VIDEO_HEVC_LEVEL_4_1;
	case HFI_HEVC_LEVEL_5:
		return V4L2_MPEG_VIDEO_HEVC_LEVEL_5;
	case HFI_HEVC_LEVEL_51:
		return V4L2_MPEG_VIDEO_HEVC_LEVEL_5_1;
	case HFI_HEVC_LEVEL_52:
		return V4L2_MPEG_VIDEO_HEVC_LEVEL_5_2;
	case HFI_HEVC_LEVEL_6:
		return V4L2_MPEG_VIDEO_HEVC_LEVEL_6;
	case HFI_HEVC_LEVEL_61:
		return V4L2_MPEG_VIDEO_HEVC_LEVEL_6_1;
	case HFI_HEVC_LEVEL_62:
		return V4L2_MPEG_VIDEO_HEVC_LEVEL_6_2;
	case HFI_LEVEL_UNKNOWN:
		return V4L2_MPEG_VIDEO_HEVC_LEVEL_UNKNOWN;
	default:
		goto unknown_value;
	}
	case V4L2_CID_MPEG_VIDC_VIDEO_VP8_PROFILE_LEVEL:
		switch (value) {
		case HFI_VP8_LEVEL_VERSION_0:
			return V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_0;
		case HFI_VP8_LEVEL_VERSION_1:
			return V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_1;
		case HFI_VP8_LEVEL_VERSION_2:
			return V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_2;
		case HFI_VP8_LEVEL_VERSION_3:
			return V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_3;
		case HFI_LEVEL_UNKNOWN:
			return V4L2_MPEG_VIDC_VIDEO_VP8_UNUSED;
		default:
			goto unknown_value;
		}
	case V4L2_CID_MPEG_VIDEO_VP9_PROFILE:
		switch (value) {
		case HFI_VP9_PROFILE_P0:
			return V4L2_MPEG_VIDEO_VP9_PROFILE_0;
		case HFI_VP9_PROFILE_P2_10B:
			return V4L2_MPEG_VIDEO_VP9_PROFILE_2;
		default:
			goto unknown_value;
		}
	case V4L2_CID_MPEG_VIDC_VIDEO_VP9_LEVEL:
		switch (value) {
		case HFI_VP9_LEVEL_1:
			return V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_1;
		case HFI_VP9_LEVEL_11:
			return V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_11;
		case HFI_VP9_LEVEL_2:
			return V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_2;
		case HFI_VP9_LEVEL_21:
			return V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_21;
		case HFI_VP9_LEVEL_3:
			return V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_3;
		case HFI_VP9_LEVEL_31:
			return V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_31;
		case HFI_VP9_LEVEL_4:
			return V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_4;
		case HFI_VP9_LEVEL_41:
			return V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_41;
		case HFI_VP9_LEVEL_5:
			return V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_5;
		case HFI_VP9_LEVEL_51:
			return V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_51;
		case HFI_VP9_LEVEL_6:
			return V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_6;
		case HFI_VP9_LEVEL_61:
			return V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_61;
		case HFI_LEVEL_UNKNOWN:
			return V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_UNUSED;
		default:
			goto unknown_value;
		}
	case V4L2_CID_MPEG_VIDC_VIDEO_MPEG2_PROFILE:
		switch (value) {
		case HFI_MPEG2_PROFILE_SIMPLE:
			return V4L2_MPEG_VIDC_VIDEO_MPEG2_PROFILE_SIMPLE;
		case HFI_MPEG2_PROFILE_MAIN:
			return V4L2_MPEG_VIDC_VIDEO_MPEG2_PROFILE_MAIN;
		default:
			goto unknown_value;
		}
	case V4L2_CID_MPEG_VIDC_VIDEO_MPEG2_LEVEL:
		/* This mapping is not defined properly in V4L2 */
		switch (value) {
		case HFI_MPEG2_LEVEL_LL:
			return V4L2_MPEG_VIDC_VIDEO_MPEG2_LEVEL_0;
		case HFI_MPEG2_LEVEL_ML:
			return V4L2_MPEG_VIDC_VIDEO_MPEG2_LEVEL_1;
		case HFI_MPEG2_LEVEL_HL:
			return V4L2_MPEG_VIDC_VIDEO_MPEG2_LEVEL_2;
		default:
			goto unknown_value;
		}
	}

unknown_value:
	dprintk(VIDC_ERR, "Unknown control (%x, %d)\n", id, value);
	return -EINVAL;
}

static int h264_level_v4l2_to_hfi(int value)
{
	switch (value) {
	case V4L2_MPEG_VIDEO_H264_LEVEL_1_0:
		return HFI_H264_LEVEL_1;
	case V4L2_MPEG_VIDEO_H264_LEVEL_1B:
		return HFI_H264_LEVEL_1b;
	case V4L2_MPEG_VIDEO_H264_LEVEL_1_1:
		return HFI_H264_LEVEL_11;
	case V4L2_MPEG_VIDEO_H264_LEVEL_1_2:
		return HFI_H264_LEVEL_12;
	case V4L2_MPEG_VIDEO_H264_LEVEL_1_3:
		return HFI_H264_LEVEL_13;
	case V4L2_MPEG_VIDEO_H264_LEVEL_2_0:
		return HFI_H264_LEVEL_2;
	case V4L2_MPEG_VIDEO_H264_LEVEL_2_1:
		return HFI_H264_LEVEL_21;
	case V4L2_MPEG_VIDEO_H264_LEVEL_2_2:
		return HFI_H264_LEVEL_22;
	case V4L2_MPEG_VIDEO_H264_LEVEL_3_0:
		return HFI_H264_LEVEL_3;
	case V4L2_MPEG_VIDEO_H264_LEVEL_3_1:
		return HFI_H264_LEVEL_31;
	case V4L2_MPEG_VIDEO_H264_LEVEL_3_2:
		return HFI_H264_LEVEL_32;
	case V4L2_MPEG_VIDEO_H264_LEVEL_4_0:
		return HFI_H264_LEVEL_4;
	case V4L2_MPEG_VIDEO_H264_LEVEL_4_1:
		return HFI_H264_LEVEL_41;
	case V4L2_MPEG_VIDEO_H264_LEVEL_4_2:
		return HFI_H264_LEVEL_42;
	case V4L2_MPEG_VIDEO_H264_LEVEL_5_0:
		return HFI_H264_LEVEL_5;
	case V4L2_MPEG_VIDEO_H264_LEVEL_5_1:
		return HFI_H264_LEVEL_51;
	case V4L2_MPEG_VIDEO_H264_LEVEL_5_2:
		return HFI_H264_LEVEL_52;
	case V4L2_MPEG_VIDEO_H264_LEVEL_6_0:
		return HFI_H264_LEVEL_6;
	case V4L2_MPEG_VIDEO_H264_LEVEL_6_1:
		return HFI_H264_LEVEL_61;
	case V4L2_MPEG_VIDEO_H264_LEVEL_6_2:
		return HFI_H264_LEVEL_62;
	case V4L2_MPEG_VIDEO_H264_LEVEL_UNKNOWN:
		return HFI_LEVEL_UNKNOWN;
	default:
		goto unknown_value;
	}

unknown_value:
	dprintk(VIDC_ERR, "Unknown level (%d)\n", value);
	return -EINVAL;
}

static int hevc_level_v4l2_to_hfi(int value)
{
	switch (value) {
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_1:
		return HFI_HEVC_LEVEL_1;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_2:
		return HFI_HEVC_LEVEL_2;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_2_1:
		return HFI_HEVC_LEVEL_21;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_3:
		return HFI_HEVC_LEVEL_3;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_3_1:
		return HFI_HEVC_LEVEL_31;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_4:
		return HFI_HEVC_LEVEL_4;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_4_1:
		return HFI_HEVC_LEVEL_41;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_5:
		return HFI_HEVC_LEVEL_5;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_5_1:
		return HFI_HEVC_LEVEL_51;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_5_2:
		return HFI_HEVC_LEVEL_52;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_6:
		return HFI_HEVC_LEVEL_6;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_6_1:
		return HFI_HEVC_LEVEL_61;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_6_2:
		return HFI_HEVC_LEVEL_62;
	case V4L2_MPEG_VIDEO_HEVC_LEVEL_UNKNOWN:
		return HFI_LEVEL_UNKNOWN;
	default:
		goto unknown_value;
	}

unknown_value:
	dprintk(VIDC_ERR, "Unknown level (%d)\n", value);
	return -EINVAL;
}

static int vp9_level_v4l2_to_hfi(int value)
{
	switch (value) {
	case V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_1:
		return HFI_VP9_LEVEL_1;
	case V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_11:
		return HFI_VP9_LEVEL_11;
	case V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_2:
		return HFI_VP9_LEVEL_2;
	case V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_21:
		return HFI_VP9_LEVEL_21;
	case V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_3:
		return HFI_VP9_LEVEL_3;
	case V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_31:
		return HFI_VP9_LEVEL_31;
	case V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_4:
		return HFI_VP9_LEVEL_4;
	case V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_41:
		return HFI_VP9_LEVEL_41;
	case V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_5:
		return HFI_VP9_LEVEL_5;
	case V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_51:
		return HFI_VP9_LEVEL_51;
	case V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_6:
		return HFI_VP9_LEVEL_6;
	case V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_61:
		return HFI_VP9_LEVEL_61;
	case V4L2_MPEG_VIDC_VIDEO_VP9_LEVEL_UNUSED:
		return HFI_LEVEL_UNKNOWN;
	default:
		goto unknown_value;
	}

unknown_value:
	dprintk(VIDC_ERR, "Unknown level (%d)\n", value);
	return -EINVAL;

}
int msm_comm_v4l2_to_hfi(int id, int value)
{
	switch (id) {
	/* H264 */
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		switch (value) {
		case V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE:
			return HFI_H264_PROFILE_BASELINE;
		case V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_BASELINE:
			return HFI_H264_PROFILE_CONSTRAINED_BASE;
		case V4L2_MPEG_VIDEO_H264_PROFILE_MAIN:
			return HFI_H264_PROFILE_MAIN;
		case V4L2_MPEG_VIDEO_H264_PROFILE_HIGH:
			return HFI_H264_PROFILE_HIGH;
		case V4L2_MPEG_VIDEO_H264_PROFILE_STEREO_HIGH:
			return HFI_H264_PROFILE_STEREO_HIGH;
		case V4L2_MPEG_VIDEO_H264_PROFILE_MULTIVIEW_HIGH:
			return HFI_H264_PROFILE_MULTIVIEW_HIGH;
		case V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_HIGH:
			return HFI_H264_PROFILE_CONSTRAINED_HIGH;
		default:
			return HFI_H264_PROFILE_HIGH;
		}
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		return h264_level_v4l2_to_hfi(value);
	case V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE:
		switch (value) {
		case V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CAVLC:
			return HFI_H264_ENTROPY_CAVLC;
		case V4L2_MPEG_VIDEO_H264_ENTROPY_MODE_CABAC:
			return HFI_H264_ENTROPY_CABAC;
		default:
			return HFI_H264_ENTROPY_CABAC;
		}
	case V4L2_CID_MPEG_VIDEO_VP8_PROFILE:
		switch (value) {
		case V4L2_MPEG_VIDEO_VP8_PROFILE_0:
			return HFI_VP8_PROFILE_MAIN;
		default:
			return HFI_VP8_PROFILE_MAIN;
		}
	case V4L2_CID_MPEG_VIDC_VIDEO_VP8_PROFILE_LEVEL:
		switch (value) {
		case V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_0:
			return HFI_VP8_LEVEL_VERSION_0;
		case V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_1:
			return HFI_VP8_LEVEL_VERSION_1;
		case V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_2:
			return HFI_VP8_LEVEL_VERSION_2;
		case V4L2_MPEG_VIDC_VIDEO_VP8_VERSION_3:
			return HFI_VP8_LEVEL_VERSION_3;
		case V4L2_MPEG_VIDC_VIDEO_VP8_UNUSED:
			return HFI_LEVEL_UNKNOWN;
		default:
			return HFI_LEVEL_UNKNOWN;
		}
	case V4L2_CID_MPEG_VIDEO_VP9_PROFILE:
		switch (value) {
		case V4L2_MPEG_VIDEO_VP9_PROFILE_0:
			return HFI_VP9_PROFILE_P0;
		case V4L2_MPEG_VIDEO_VP9_PROFILE_2:
			return HFI_VP9_PROFILE_P2_10B;
		default:
			return HFI_VP9_PROFILE_P0;
		}
	case V4L2_CID_MPEG_VIDC_VIDEO_VP9_LEVEL:
		return vp9_level_v4l2_to_hfi(value);
	case V4L2_CID_MPEG_VIDEO_HEVC_PROFILE:
		switch (value) {
		case V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN:
			return HFI_HEVC_PROFILE_MAIN;
		case V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_10:
			return HFI_HEVC_PROFILE_MAIN10;
		case V4L2_MPEG_VIDEO_HEVC_PROFILE_MAIN_STILL_PICTURE:
			return HFI_HEVC_PROFILE_MAIN_STILL_PIC;
		default:
			return HFI_HEVC_PROFILE_MAIN;
		}
	case V4L2_CID_MPEG_VIDEO_HEVC_LEVEL:
		return hevc_level_v4l2_to_hfi(value);
	case V4L2_CID_MPEG_VIDEO_HEVC_TIER:
		switch (value) {
		case V4L2_MPEG_VIDEO_HEVC_TIER_MAIN:
			return HFI_HEVC_TIER_MAIN;
		case V4L2_MPEG_VIDEO_HEVC_TIER_HIGH:
			return HFI_HEVC_TIER_HIGH;
		default:
			return HFI_HEVC_TIER_HIGH;
		}
	case V4L2_CID_MPEG_VIDC_VIDEO_MPEG2_PROFILE:
		switch (value) {
		case V4L2_MPEG_VIDC_VIDEO_MPEG2_PROFILE_SIMPLE:
			return HFI_MPEG2_PROFILE_SIMPLE;
		case V4L2_MPEG_VIDC_VIDEO_MPEG2_PROFILE_MAIN:
			return HFI_MPEG2_PROFILE_MAIN;
		default:
			return HFI_MPEG2_PROFILE_MAIN;
		}
	case V4L2_CID_MPEG_VIDC_VIDEO_MPEG2_LEVEL:
		/* This mapping is not defined properly in V4L2 */
		switch (value) {
		case V4L2_MPEG_VIDC_VIDEO_MPEG2_LEVEL_0:
			return HFI_MPEG2_LEVEL_LL;
		case V4L2_MPEG_VIDC_VIDEO_MPEG2_LEVEL_1:
			return HFI_MPEG2_LEVEL_ML;
		case V4L2_MPEG_VIDC_VIDEO_MPEG2_LEVEL_2:
			return HFI_MPEG2_LEVEL_HL;
		default:
			return HFI_MPEG2_LEVEL_HL;
		}
	case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE:
		switch (value) {
		case V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_DISABLED:
			return HFI_H264_DB_MODE_DISABLE;
		case V4L2_MPEG_VIDEO_H264_LOOP_FILTER_MODE_ENABLED:
			return HFI_H264_DB_MODE_ALL_BOUNDARY;
		case DB_DISABLE_SLICE_BOUNDARY:
			return HFI_H264_DB_MODE_SKIP_SLICE_BOUNDARY;
		default:
			return HFI_H264_DB_MODE_ALL_BOUNDARY;
		}
	}
	dprintk(VIDC_ERR, "Unknown control (%x, %d)\n", id, value);
	return -EINVAL;
}

int msm_comm_get_v4l2_profile(int fourcc, int profile)
{
	switch (fourcc) {
	case V4L2_PIX_FMT_H264:
		return msm_comm_hfi_to_v4l2(
			V4L2_CID_MPEG_VIDEO_H264_PROFILE,
			profile);
	case V4L2_PIX_FMT_HEVC:
		return msm_comm_hfi_to_v4l2(
			V4L2_CID_MPEG_VIDEO_HEVC_PROFILE,
			profile);
	case V4L2_PIX_FMT_VP8:
	case V4L2_PIX_FMT_VP9:
	case V4L2_PIX_FMT_MPEG2:
		return 0;
	default:
		dprintk(VIDC_ERR, "Unknown codec id %x\n", fourcc);
		return 0;
	}
}

int msm_comm_get_v4l2_level(int fourcc, int level)
{
	switch (fourcc) {
	case V4L2_PIX_FMT_H264:
		return msm_comm_hfi_to_v4l2(
			V4L2_CID_MPEG_VIDEO_H264_LEVEL,
			level);
	case V4L2_PIX_FMT_HEVC:
		level &= ~(0xF << 28);
		return msm_comm_hfi_to_v4l2(
			V4L2_CID_MPEG_VIDEO_HEVC_LEVEL,
			level);
	case V4L2_PIX_FMT_VP8:
		return msm_comm_hfi_to_v4l2(
			V4L2_CID_MPEG_VIDC_VIDEO_VP8_PROFILE_LEVEL,
			level);
	case V4L2_PIX_FMT_VP9:
	case V4L2_PIX_FMT_MPEG2:
		return 0;
	default:
		dprintk(VIDC_ERR, "Unknown codec id %x\n", fourcc);
		return 0;
	}
}

int msm_comm_ctrl_init(struct msm_vidc_inst *inst,
		struct msm_vidc_ctrl *drv_ctrls, u32 num_ctrls,
		const struct v4l2_ctrl_ops *ctrl_ops)
{
	int idx = 0;
	struct v4l2_ctrl_config ctrl_cfg = {0};
	int ret_val = 0;

	if (!inst || !drv_ctrls || !ctrl_ops || !num_ctrls) {
		dprintk(VIDC_ERR, "%s - invalid input\n", __func__);
		return -EINVAL;
	}

	inst->ctrls = kcalloc(num_ctrls, sizeof(struct v4l2_ctrl *),
				GFP_KERNEL);
	if (!inst->ctrls) {
		dprintk(VIDC_ERR, "%s - failed to allocate ctrl\n", __func__);
		return -ENOMEM;
	}

	ret_val = v4l2_ctrl_handler_init(&inst->ctrl_handler, num_ctrls);

	if (ret_val) {
		dprintk(VIDC_ERR, "CTRL ERR: Control handler init failed, %d\n",
				inst->ctrl_handler.error);
		return ret_val;
	}

	for (; idx < (int) num_ctrls; idx++) {
		struct v4l2_ctrl *ctrl = NULL;

		if (IS_PRIV_CTRL(drv_ctrls[idx].id)) {
			/*add private control*/
			ctrl_cfg.def = drv_ctrls[idx].default_value;
			ctrl_cfg.flags = 0;
			ctrl_cfg.id = drv_ctrls[idx].id;
			ctrl_cfg.max = drv_ctrls[idx].maximum;
			ctrl_cfg.min = drv_ctrls[idx].minimum;
			ctrl_cfg.menu_skip_mask =
				drv_ctrls[idx].menu_skip_mask;
			ctrl_cfg.name = drv_ctrls[idx].name;
			ctrl_cfg.ops = ctrl_ops;
			ctrl_cfg.step = drv_ctrls[idx].step;
			ctrl_cfg.type = drv_ctrls[idx].type;
			ctrl_cfg.qmenu = drv_ctrls[idx].qmenu;

			ctrl = v4l2_ctrl_new_custom(&inst->ctrl_handler,
					&ctrl_cfg, NULL);
		} else {
			if (drv_ctrls[idx].type == V4L2_CTRL_TYPE_MENU) {
				ctrl = v4l2_ctrl_new_std_menu(
					&inst->ctrl_handler,
					ctrl_ops,
					drv_ctrls[idx].id,
					(u8) drv_ctrls[idx].maximum,
					drv_ctrls[idx].menu_skip_mask,
					(u8) drv_ctrls[idx].default_value);
			} else {
				ctrl = v4l2_ctrl_new_std(&inst->ctrl_handler,
					ctrl_ops,
					drv_ctrls[idx].id,
					drv_ctrls[idx].minimum,
					drv_ctrls[idx].maximum,
					drv_ctrls[idx].step,
					drv_ctrls[idx].default_value);
			}
		}

		if (!ctrl) {
			dprintk(VIDC_ERR, "%s - invalid ctrl %s\n", __func__,
				 drv_ctrls[idx].name);
			return -EINVAL;
		}

		ret_val = inst->ctrl_handler.error;
		if (ret_val) {
			dprintk(VIDC_ERR,
				"Error adding ctrl (%s) to ctrl handle, %d\n",
				drv_ctrls[idx].name, inst->ctrl_handler.error);
			return ret_val;
		}

		ctrl->flags |= drv_ctrls[idx].flags;
		inst->ctrls[idx] = ctrl;
	}
	inst->num_ctrls = num_ctrls;

	return ret_val;
}

int msm_comm_ctrl_deinit(struct msm_vidc_inst *inst)
{
	if (!inst) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	kfree(inst->ctrls);
	v4l2_ctrl_handler_free(&inst->ctrl_handler);

	return 0;
}

int msm_comm_set_stream_output_mode(struct msm_vidc_inst *inst,
		enum multi_stream mode)
{
	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	if (!is_decode_session(inst)) {
		dprintk(VIDC_HIGH, "%s: not a decode session %x\n",
			__func__, hash32_ptr(inst->session));
		return -EINVAL;
	}

	if (mode == HAL_VIDEO_DECODER_SECONDARY)
		inst->stream_output_mode = HAL_VIDEO_DECODER_SECONDARY;
	else
		inst->stream_output_mode = HAL_VIDEO_DECODER_PRIMARY;

	return 0;
}

enum multi_stream msm_comm_get_stream_output_mode(struct msm_vidc_inst *inst)
{
	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid params, return default mode\n",
			__func__);
		return HAL_VIDEO_DECODER_PRIMARY;
	}

	if (!is_decode_session(inst))
		return HAL_VIDEO_DECODER_PRIMARY;

	if (inst->stream_output_mode == HAL_VIDEO_DECODER_SECONDARY)
		return HAL_VIDEO_DECODER_SECONDARY;
	else
		return HAL_VIDEO_DECODER_PRIMARY;
}

bool is_single_session(struct msm_vidc_inst *inst, u32 ignore_flags)
{
	bool single = true;
	struct msm_vidc_core *core;
	struct msm_vidc_inst *temp;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return false;
	}
	core = inst->core;

	mutex_lock(&core->lock);
	list_for_each_entry(temp, &core->instances, list) {
		/* ignore invalid session */
		if (temp->state == MSM_VIDC_CORE_INVALID)
			continue;
		if ((ignore_flags & VIDC_THUMBNAIL) &&
			is_thumbnail_session(temp))
			continue;
		if (temp != inst) {
			single = false;
			break;
		}
	}
	mutex_unlock(&core->lock);

	return single;
}

int msm_comm_get_num_perf_sessions(struct msm_vidc_inst *inst)
{
	int count = 0;
	struct msm_vidc_core *core;
	struct msm_vidc_inst *temp;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		goto exit;
	}
	core = inst->core;
	mutex_lock(&core->lock);
	list_for_each_entry(temp, &core->instances, list) {
		if (temp->is_perf_eligible_session)
			count++;
	}
	mutex_unlock(&core->lock);
exit:
	return count;
}

static int msm_comm_get_mbs_per_sec(struct msm_vidc_inst *inst,
					enum load_calc_quirks quirks)
{
	int input_port_mbs, output_port_mbs;
	int fps;
	struct v4l2_format *f;

	f = &inst->fmts[INPUT_PORT].v4l2_fmt;
	input_port_mbs = NUM_MBS_PER_FRAME(f->fmt.pix_mp.width,
		f->fmt.pix_mp.height);

	f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
	output_port_mbs = NUM_MBS_PER_FRAME(f->fmt.pix_mp.width,
		f->fmt.pix_mp.height);

	fps = inst->clk_data.frame_rate;

	/* For admission control operating rate is ignored */
	if (quirks == LOAD_POWER)
		fps = max(inst->clk_data.operating_rate,
				  inst->clk_data.frame_rate);

	/* In case of fps < 1 we assume 1 */
	fps = max(fps >> 16, 1);

	return max(input_port_mbs, output_port_mbs) * fps;
}

int msm_comm_get_inst_load(struct msm_vidc_inst *inst,
		enum load_calc_quirks quirks)
{
	int load = 0;

	mutex_lock(&inst->lock);

	if (!(inst->state >= MSM_VIDC_OPEN_DONE &&
		inst->state < MSM_VIDC_STOP_DONE))
		goto exit;

	/*  Clock and Load calculations for REALTIME/NON-REALTIME
	 *  Operating rate will either Default or Client value.
	 *  Session admission control will be based on Load.
	 *  Power requests based of calculated Clock/Freq.
	 * ----------------|----------------------------|
	 * REALTIME        | Admission Control Load =   |
	 *                 |          res * fps         |
	 *                 | Power Request Load =       |
	 *                 |          res * max(op, fps)|
	 * ----------------|----------------------------|
	 * NON-REALTIME/   | Admission Control Load = 0	|
	 *  THUMBNAIL      | Power Request Load =       |
	 *                 |          res * max(op, fps)|
	 * ----------------|----------------------------|
	 */

	if ((is_thumbnail_session(inst) ||
		 !is_realtime_session(inst)) &&
		quirks == LOAD_ADMISSION_CONTROL) {
		load = 0;
	} else {
		load = msm_comm_get_mbs_per_sec(inst, quirks);
	}

exit:
	mutex_unlock(&inst->lock);
	return load;
}

int msm_comm_get_inst_load_per_core(struct msm_vidc_inst *inst,
		enum load_calc_quirks quirks)
{
	int load = msm_comm_get_inst_load(inst, quirks);

	if (inst->clk_data.core_id == VIDC_CORE_ID_3)
		load = load / 2;

	return load;
}

int msm_comm_get_device_load(struct msm_vidc_core *core,
	enum session_type type, enum load_calc_quirks quirks)
{
	struct msm_vidc_inst *inst = NULL;
	int num_mbs_per_sec = 0;

	if (!core) {
		dprintk(VIDC_ERR, "Invalid args: %pK\n", core);
		return -EINVAL;
	}

	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {
		if (inst->session_type != type)
			continue;

		num_mbs_per_sec += msm_comm_get_inst_load(inst, quirks);
	}
	mutex_unlock(&core->lock);

	return num_mbs_per_sec;
}

enum hal_domain get_hal_domain(int session_type)
{
	enum hal_domain domain;

	switch (session_type) {
	case MSM_VIDC_ENCODER:
		domain = HAL_VIDEO_DOMAIN_ENCODER;
		break;
	case MSM_VIDC_DECODER:
		domain = HAL_VIDEO_DOMAIN_DECODER;
		break;
	case MSM_VIDC_CVP:
		domain = HAL_VIDEO_DOMAIN_CVP;
		break;
	default:
		dprintk(VIDC_ERR, "Wrong domain %d\n", session_type);
		domain = HAL_UNUSED_DOMAIN;
		break;
	}

	return domain;
}

enum hal_video_codec get_hal_codec(int fourcc)
{
	enum hal_video_codec codec;

	switch (fourcc) {
	case V4L2_PIX_FMT_H264:
	case V4L2_PIX_FMT_H264_NO_SC:
		codec = HAL_VIDEO_CODEC_H264;
		break;
	case V4L2_PIX_FMT_H264_MVC:
		codec = HAL_VIDEO_CODEC_MVC;
		break;
	case V4L2_PIX_FMT_MPEG1:
		codec = HAL_VIDEO_CODEC_MPEG1;
		break;
	case V4L2_PIX_FMT_MPEG2:
		codec = HAL_VIDEO_CODEC_MPEG2;
		break;
	case V4L2_PIX_FMT_VP8:
		codec = HAL_VIDEO_CODEC_VP8;
		break;
	case V4L2_PIX_FMT_VP9:
		codec = HAL_VIDEO_CODEC_VP9;
		break;
	case V4L2_PIX_FMT_HEVC:
		codec = HAL_VIDEO_CODEC_HEVC;
		break;
	case V4L2_PIX_FMT_TME:
		codec = HAL_VIDEO_CODEC_TME;
		break;
	case V4L2_PIX_FMT_CVP:
		codec = HAL_VIDEO_CODEC_CVP;
		break;
	default:
		dprintk(VIDC_ERR, "Wrong codec: %#x\n", fourcc);
		codec = HAL_UNUSED_CODEC;
		break;
	}

	return codec;
}

enum hal_uncompressed_format msm_comm_get_hal_uncompressed(int fourcc)
{
	enum hal_uncompressed_format format = HAL_UNUSED_COLOR;

	switch (fourcc) {
	case V4L2_PIX_FMT_NV12:
		format = HAL_COLOR_FORMAT_NV12;
		break;
	case V4L2_PIX_FMT_NV12_512:
		format = HAL_COLOR_FORMAT_NV12_512;
		break;
	case V4L2_PIX_FMT_NV21:
		format = HAL_COLOR_FORMAT_NV21;
		break;
	case V4L2_PIX_FMT_NV12_UBWC:
		format = HAL_COLOR_FORMAT_NV12_UBWC;
		break;
	case V4L2_PIX_FMT_NV12_TP10_UBWC:
		format = HAL_COLOR_FORMAT_NV12_TP10_UBWC;
		break;
	case V4L2_PIX_FMT_SDE_Y_CBCR_H2V2_P010_VENUS:
		format = HAL_COLOR_FORMAT_P010;
		break;
	default:
		format = HAL_UNUSED_COLOR;
		break;
	}

	return format;
}

u32 msm_comm_get_hfi_uncompressed(int fourcc)
{
	u32 format;

	switch (fourcc) {
	case V4L2_PIX_FMT_NV12:
		format = HFI_COLOR_FORMAT_NV12;
		break;
	case V4L2_PIX_FMT_NV12_512:
		format = HFI_COLOR_FORMAT_NV12;
		break;
	case V4L2_PIX_FMT_NV21:
		format = HFI_COLOR_FORMAT_NV21;
		break;
	case V4L2_PIX_FMT_NV12_UBWC:
		format = HFI_COLOR_FORMAT_NV12_UBWC;
		break;
	case V4L2_PIX_FMT_NV12_TP10_UBWC:
		format = HFI_COLOR_FORMAT_YUV420_TP10_UBWC;
		break;
	case V4L2_PIX_FMT_SDE_Y_CBCR_H2V2_P010_VENUS:
		format = HFI_COLOR_FORMAT_P010;
		break;
	default:
		format = HFI_COLOR_FORMAT_NV12_UBWC;
		dprintk(VIDC_ERR, "Invalid format, defaulting to UBWC");
		break;
	}

	return format;
}
struct msm_vidc_core *get_vidc_core(int core_id)
{
	struct msm_vidc_core *core;
	int found = 0;

	if (core_id > MSM_VIDC_CORES_MAX) {
		dprintk(VIDC_ERR, "Core id = %d is greater than max = %d\n",
			core_id, MSM_VIDC_CORES_MAX);
		return NULL;
	}
	mutex_lock(&vidc_driver->lock);
	list_for_each_entry(core, &vidc_driver->cores, list) {
		if (core->id == core_id) {
			found = 1;
			break;
		}
	}
	mutex_unlock(&vidc_driver->lock);
	if (found)
		return core;
	return NULL;
}

const struct msm_vidc_format_desc *msm_comm_get_pixel_fmt_index(
	const struct msm_vidc_format_desc fmt[], int size, int index)
{
	int i, k = 0;

	if (!fmt || index < 0) {
		dprintk(VIDC_ERR, "Invalid inputs, fmt = %pK, index = %d\n",
						fmt, index);
		return NULL;
	}
	for (i = 0; i < size; i++) {
		if (k == index)
			break;
		k++;
	}
	if (i == size) {
		dprintk(VIDC_HIGH, "Format not found\n");
		return NULL;
	}
	return &fmt[i];
}
struct msm_vidc_format_desc *msm_comm_get_pixel_fmt_fourcc(
	struct msm_vidc_format_desc fmt[], int size, int fourcc)
{
	int i;

	if (!fmt) {
		dprintk(VIDC_ERR, "Invalid inputs, fmt = %pK\n", fmt);
		return NULL;
	}
	for (i = 0; i < size; i++) {
		if (fmt[i].fourcc == fourcc)
			break;
	}
	if (i == size) {
		dprintk(VIDC_HIGH, "Format not found\n");
		return NULL;
	}
	return &fmt[i];
}

struct msm_vidc_format_constraint *msm_comm_get_pixel_fmt_constraints(
	struct msm_vidc_format_constraint fmt[], int size, int fourcc)
{
	int i;

	if (!fmt) {
		dprintk(VIDC_ERR, "Invalid inputs, fmt = %pK\n", fmt);
		return NULL;
	}
	for (i = 0; i < size; i++) {
		if (fmt[i].fourcc == fourcc)
			break;
	}
	if (i == size) {
		dprintk(VIDC_HIGH, "Format constraint not found.\n");
		return NULL;
	}
	return &fmt[i];
}

struct buf_queue *msm_comm_get_vb2q(
		struct msm_vidc_inst *inst, enum v4l2_buf_type type)
{
	if (type == OUTPUT_MPLANE)
		return &inst->bufq[OUTPUT_PORT];
	if (type == INPUT_MPLANE)
		return &inst->bufq[INPUT_PORT];
	return NULL;
}

static void update_capability(struct msm_vidc_codec_capability *in,
		struct msm_vidc_capability *capability)
{
	if (!in || !capability) {
		dprintk(VIDC_ERR, "%s Invalid params\n", __func__);
		return;
	}

	if (in->capability_type < CAP_MAX) {
		capability->cap[in->capability_type].capability_type =
				in->capability_type;
		capability->cap[in->capability_type].min = in->min;
		capability->cap[in->capability_type].max = in->max;
		capability->cap[in->capability_type].step_size = in->step_size;
		capability->cap[in->capability_type].default_value =
				in->default_value;
	} else {
		dprintk(VIDC_ERR, "%s: invalid capability_type %d\n",
			__func__, in->capability_type);
	}
}

static int msm_vidc_capabilities(struct msm_vidc_core *core)
{
	int rc = 0;
	struct msm_vidc_codec_capability *platform_caps;
	int i, j, num_platform_caps;

	if (!core || !core->capabilities) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	platform_caps = core->resources.codec_caps;
	num_platform_caps = core->resources.codec_caps_count;

	dprintk(VIDC_HIGH, "%s: num caps %d\n", __func__, num_platform_caps);
	/* loop over each platform capability */
	for (i = 0; i < num_platform_caps; i++) {
		/* select matching core codec and update it */
		for (j = 0; j < core->resources.codecs_count; j++) {
			if ((platform_caps[i].domains &
				core->capabilities[j].domain) &&
				(platform_caps[i].codecs &
				core->capabilities[j].codec)) {
				/* update core capability */
				update_capability(&platform_caps[i],
					&core->capabilities[j]);
			}
		}
	}

	return rc;
}

static void handle_sys_init_done(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_core *core;

	if (!IS_HAL_SYS_CMD(cmd)) {
		dprintk(VIDC_ERR, "%s - invalid cmd\n", __func__);
		return;
	}
	if (!response) {
		dprintk(VIDC_ERR,
			"Failed to get valid response for sys init\n");
		return;
	}
	core = get_vidc_core(response->device_id);
	if (!core) {
		dprintk(VIDC_ERR, "Wrong device_id received\n");
		return;
	}
	dprintk(VIDC_HIGH, "%s: core %pK\n", __func__, core);
	complete(&(core->completions[SYS_MSG_INDEX(cmd)]));
}

static void put_inst_helper(struct kref *kref)
{
	struct msm_vidc_inst *inst = container_of(kref,
			struct msm_vidc_inst, kref);

	msm_vidc_destroy(inst);
}

void put_inst(struct msm_vidc_inst *inst)
{
	if (!inst)
		return;

	kref_put(&inst->kref, put_inst_helper);
}

struct msm_vidc_inst *get_inst(struct msm_vidc_core *core,
		void *session_id)
{
	struct msm_vidc_inst *inst = NULL;
	bool matches = false;

	if (!core || !session_id)
		return NULL;

	mutex_lock(&core->lock);
	/*
	 * This is as good as !list_empty(!inst->list), but at this point
	 * we don't really know if inst was kfree'd via close syscall before
	 * hardware could respond.  So manually walk thru the list of active
	 * sessions
	 */
	list_for_each_entry(inst, &core->instances, list) {
		if (inst == session_id) {
			/*
			 * Even if the instance is valid, we really shouldn't
			 * be receiving or handling callbacks when we've deleted
			 * our session with HFI
			 */
			matches = !!inst->session;
			break;
		}
	}

	/*
	 * kref_* is atomic_int backed, so no need for inst->lock.  But we can
	 * always acquire inst->lock and release it in put_inst for a stronger
	 * locking system.
	 */
	inst = (matches && kref_get_unless_zero(&inst->kref)) ? inst : NULL;
	mutex_unlock(&core->lock);

	return inst;
}

static void handle_session_release_buf_done(enum hal_command_response cmd,
	void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;
	struct internal_buf *buf;
	struct list_head *ptr, *next;
	struct hal_buffer_info *buffer;
	u32 buf_found = false;
	u32 address;

	if (!response) {
		dprintk(VIDC_ERR, "Invalid release_buf_done response\n");
		return;
	}

	inst = get_inst(get_vidc_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(VIDC_ERR, "Got a response for an inactive session\n");
		return;
	}

	buffer = &response->data.buffer_info;
	address = buffer->buffer_addr;

	mutex_lock(&inst->scratchbufs.lock);
	list_for_each_safe(ptr, next, &inst->scratchbufs.list) {
		buf = list_entry(ptr, struct internal_buf, list);
		if (address == buf->smem.device_addr) {
			dprintk(VIDC_HIGH, "releasing scratch: %x\n",
					buf->smem.device_addr);
			buf_found = true;
		}
	}
	mutex_unlock(&inst->scratchbufs.lock);

	mutex_lock(&inst->persistbufs.lock);
	list_for_each_safe(ptr, next, &inst->persistbufs.list) {
		buf = list_entry(ptr, struct internal_buf, list);
		if (address == buf->smem.device_addr) {
			dprintk(VIDC_HIGH, "releasing persist: %x\n",
					buf->smem.device_addr);
			buf_found = true;
		}
	}
	mutex_unlock(&inst->persistbufs.lock);

	if (!buf_found)
		dprintk(VIDC_ERR, "invalid buffer received from firmware");
	if (IS_HAL_SESSION_CMD(cmd))
		complete(&inst->completions[SESSION_MSG_INDEX(cmd)]);
	else
		dprintk(VIDC_ERR, "Invalid inst cmd response: %d\n", cmd);

	put_inst(inst);
}

static void handle_sys_release_res_done(
		enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_core *core;

	if (!response) {
		dprintk(VIDC_ERR,
			"Failed to get valid response for sys init\n");
		return;
	}
	core = get_vidc_core(response->device_id);
	if (!core) {
		dprintk(VIDC_ERR, "Wrong device_id received\n");
		return;
	}
	complete(&core->completions[
			SYS_MSG_INDEX(HAL_SYS_RELEASE_RESOURCE_DONE)]);
}

void change_inst_state(struct msm_vidc_inst *inst, enum instance_state state)
{
	if (!inst) {
		dprintk(VIDC_ERR, "Invalid parameter %s\n", __func__);
		return;
	}
	mutex_lock(&inst->lock);
	if (inst->state == MSM_VIDC_CORE_INVALID) {
		dprintk(VIDC_HIGH,
			"Inst: %pK is in bad state can't change state to %d\n",
			inst, state);
		goto exit;
	}
	dprintk(VIDC_HIGH, "Moved inst: %pK from state: %d to state: %d\n",
		   inst, inst->state, state);
	inst->state = state;
exit:
	mutex_unlock(&inst->lock);
}

static int signal_session_msg_receipt(enum hal_command_response cmd,
		struct msm_vidc_inst *inst)
{
	if (!inst) {
		dprintk(VIDC_ERR, "Invalid(%pK) instance id\n", inst);
		return -EINVAL;
	}
	if (IS_HAL_SESSION_CMD(cmd)) {
		complete(&inst->completions[SESSION_MSG_INDEX(cmd)]);
	} else {
		dprintk(VIDC_ERR, "Invalid inst cmd response: %d\n", cmd);
		return -EINVAL;
	}
	return 0;
}

static int wait_for_sess_signal_receipt(struct msm_vidc_inst *inst,
	enum hal_command_response cmd)
{
	int rc = 0;
	struct hfi_device *hdev;

	if (!IS_HAL_SESSION_CMD(cmd)) {
		dprintk(VIDC_ERR, "Invalid inst cmd response: %d\n", cmd);
		return -EINVAL;
	}
	hdev = (struct hfi_device *)(inst->core->device);
	rc = wait_for_completion_timeout(
		&inst->completions[SESSION_MSG_INDEX(cmd)],
		msecs_to_jiffies(
			inst->core->resources.msm_vidc_hw_rsp_timeout));
	if (!rc) {
		dprintk(VIDC_ERR, "Wait interrupted or timed out: %d\n",
				SESSION_MSG_INDEX(cmd));
		msm_comm_kill_session(inst);
		rc = -EIO;
	} else {
		rc = 0;
	}
	return rc;
}

static int wait_for_state(struct msm_vidc_inst *inst,
	enum instance_state flipped_state,
	enum instance_state desired_state,
	enum hal_command_response hal_cmd)
{
	int rc = 0;

	if (IS_ALREADY_IN_STATE(flipped_state, desired_state)) {
		dprintk(VIDC_HIGH, "inst: %pK is already in state: %d\n",
						inst, inst->state);
		goto err_same_state;
	}
	dprintk(VIDC_HIGH, "Waiting for hal_cmd: %d\n", hal_cmd);
	rc = wait_for_sess_signal_receipt(inst, hal_cmd);
	if (!rc)
		change_inst_state(inst, desired_state);
err_same_state:
	return rc;
}

void msm_vidc_queue_v4l2_event(struct msm_vidc_inst *inst, int event_type)
{
	struct v4l2_event event = {.id = 0, .type = event_type};

	v4l2_event_queue_fh(&inst->event_handler, &event);
}

static void msm_comm_generate_max_clients_error(struct msm_vidc_inst *inst)
{
	enum hal_command_response cmd = HAL_SESSION_ERROR;
	struct msm_vidc_cb_cmd_done response = {0};

	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid input parameters\n", __func__);
		return;
	}
	dprintk(VIDC_ERR, "%s: Too many clients\n", __func__);
	response.session_id = inst;
	response.status = VIDC_ERR_MAX_CLIENTS;
	handle_session_error(cmd, (void *)&response);
}

static void print_cap(const char *type,
		struct hal_capability_supported *cap)
{
	dprintk(VIDC_HIGH,
		"%-24s: %-10d %-10d %-10d %-10d\n",
		type, cap->min, cap->max, cap->step_size, cap->default_value);
}

static int msm_vidc_comm_update_ctrl(struct msm_vidc_inst *inst,
	u32 id, struct hal_capability_supported *cap)
{
	struct v4l2_ctrl *ctrl = NULL;
	int rc = 0;

	ctrl = v4l2_ctrl_find(&inst->ctrl_handler, id);
	if (!ctrl) {
		dprintk(VIDC_ERR,
			"%s: Conrol id %d not found\n", __func__, id);
		return -EINVAL;
	}

	rc = v4l2_ctrl_modify_range(ctrl, cap->min, cap->max,
			cap->step_size, cap->default_value);
	if (rc) {
		dprintk(VIDC_ERR,
			"%s: failed: control name %s, min %d, max %d, step %d, default_value %d\n",
			__func__, ctrl->name, cap->min, cap->max,
			cap->step_size, cap->default_value);
		goto error;
	}
	/*
	 * v4l2_ctrl_modify_range() is not updating default_value,
	 * so use v4l2_ctrl_s_ctrl() to update it.
	 */
	rc = v4l2_ctrl_s_ctrl(ctrl, cap->default_value);
	if (rc) {
		dprintk(VIDC_ERR,
			"%s: failed s_ctrl: %s with value %d\n",
			__func__, ctrl->name, cap->default_value);
		goto error;
	}
	dprintk(VIDC_HIGH,
		"Updated control: %s: min %lld, max %lld, step %lld, default value = %lld\n",
		ctrl->name, ctrl->minimum, ctrl->maximum,
		ctrl->step, ctrl->default_value);

error:
	return rc;
}

static void msm_vidc_comm_update_ctrl_limits(struct msm_vidc_inst *inst)
{
	struct v4l2_format *f;

	if (inst->session_type == MSM_VIDC_ENCODER) {
		f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
		if (get_hal_codec(f->fmt.pix_mp.pixelformat) ==
			HAL_VIDEO_CODEC_TME)
			return;
		msm_vidc_comm_update_ctrl(inst, V4L2_CID_MPEG_VIDEO_BITRATE,
				&inst->capability.cap[CAP_BITRATE]);
		msm_vidc_comm_update_ctrl(inst,
				V4L2_CID_MPEG_VIDC_VIDEO_LTRCOUNT,
				&inst->capability.cap[CAP_LTR_COUNT]);
		msm_vidc_comm_update_ctrl(inst,
				V4L2_CID_MPEG_VIDEO_B_FRAMES,
				&inst->capability.cap[CAP_BFRAME]);
	}
}

static void handle_session_init_done(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst = NULL;
	struct msm_vidc_capability *capability = NULL;
	struct msm_vidc_core *core;
	u32 i, codec;

	if (!response) {
		dprintk(VIDC_ERR,
			"Failed to get valid response for session init\n");
		return;
	}

	inst = get_inst(get_vidc_core(response->device_id),
		response->session_id);

	if (!inst) {
		dprintk(VIDC_ERR, "Got a response for an inactive session\n");
		return;
	}

	if (response->status) {
		dprintk(VIDC_ERR,
			"Session init response from FW : %#x\n",
			response->status);
		goto error;
	}

	if (inst->session_type == MSM_VIDC_CVP) {
		dprintk(VIDC_HIGH, "%s: cvp session %#x\n",
			__func__, hash32_ptr(inst->session));
		signal_session_msg_receipt(cmd, inst);
		put_inst(inst);
		return;
	}

	core = inst->core;
	codec = get_v4l2_codec(inst);

	for (i = 0; i < core->resources.codecs_count; i++) {
		if (core->capabilities[i].codec ==
				get_hal_codec(codec) &&
			core->capabilities[i].domain ==
				get_hal_domain(inst->session_type)) {
			capability = &core->capabilities[i];
			break;
		}
	}
	if (!capability) {
		dprintk(VIDC_ERR,
			"%s: capabilities not found for domain %#x codec %#x\n",
			__func__, get_hal_domain(inst->session_type),
			get_hal_codec(codec));
		goto error;
	}

	dprintk(VIDC_HIGH,
		"%s: capabilities for domain %#x codec %#x\n",
		__func__, capability->domain, capability->codec);
	memcpy(&inst->capability, capability,
			sizeof(struct msm_vidc_capability));

	dprintk(VIDC_HIGH,
		"Capability type :         min        max        step_size  default_value\n");
	print_cap("width", &inst->capability.cap[CAP_FRAME_WIDTH]);
	print_cap("height", &inst->capability.cap[CAP_FRAME_HEIGHT]);
	print_cap("mbs_per_frame", &inst->capability.cap[CAP_MBS_PER_FRAME]);
	print_cap("mbs_per_sec", &inst->capability.cap[CAP_MBS_PER_SECOND]);
	print_cap("frame_rate", &inst->capability.cap[CAP_FRAMERATE]);
	print_cap("bitrate", &inst->capability.cap[CAP_BITRATE]);
	print_cap("scale_x", &inst->capability.cap[CAP_SCALE_X]);
	print_cap("scale_y", &inst->capability.cap[CAP_SCALE_Y]);
	print_cap("hier_p", &inst->capability.cap[CAP_HIER_P_NUM_ENH_LAYERS]);
	print_cap("ltr_count", &inst->capability.cap[CAP_LTR_COUNT]);
	print_cap("bframe", &inst->capability.cap[CAP_BFRAME]);
	print_cap("mbs_per_sec_low_power",
		&inst->capability.cap[CAP_MBS_PER_SECOND_POWER_SAVE]);
	print_cap("i_qp", &inst->capability.cap[CAP_I_FRAME_QP]);
	print_cap("p_qp", &inst->capability.cap[CAP_P_FRAME_QP]);
	print_cap("b_qp", &inst->capability.cap[CAP_B_FRAME_QP]);
	print_cap("slice_bytes", &inst->capability.cap[CAP_SLICE_BYTE]);
	print_cap("slice_mbs", &inst->capability.cap[CAP_SLICE_MB]);
	print_cap("max_videocores", &inst->capability.cap[CAP_MAX_VIDEOCORES]);
	/* Secure usecase specific */
	print_cap("secure_width",
		&inst->capability.cap[CAP_SECURE_FRAME_WIDTH]);
	print_cap("secure_height",
		&inst->capability.cap[CAP_SECURE_FRAME_HEIGHT]);
	print_cap("secure_mbs_per_frame",
		&inst->capability.cap[CAP_SECURE_MBS_PER_FRAME]);
	print_cap("secure_bitrate", &inst->capability.cap[CAP_SECURE_BITRATE]);
	/* Batch Mode Decode */
	print_cap("batch_mbs_per_frame",
		&inst->capability.cap[CAP_BATCH_MAX_MB_PER_FRAME]);
	print_cap("batch_frame_rate", &inst->capability.cap[CAP_BATCH_MAX_FPS]);
	/* Lossless encoding usecase specific */
	print_cap("lossless_width",
		&inst->capability.cap[CAP_LOSSLESS_FRAME_WIDTH]);
	print_cap("lossless_height",
		&inst->capability.cap[CAP_LOSSLESS_FRAME_HEIGHT]);
	print_cap("lossless_mbs_per_frame",
		&inst->capability.cap[CAP_LOSSLESS_MBS_PER_FRAME]);
	/* All intra encoding usecase specific */
	print_cap("all_intra_frame_rate",
		&inst->capability.cap[CAP_ALLINTRA_MAX_FPS]);

	msm_vidc_comm_update_ctrl_limits(inst);

	signal_session_msg_receipt(cmd, inst);
	put_inst(inst);
	return;

error:
	if (response->status == VIDC_ERR_MAX_CLIENTS)
		msm_comm_generate_max_clients_error(inst);
	else
		msm_comm_generate_session_error(inst);

	signal_session_msg_receipt(cmd, inst);
	put_inst(inst);
}

static void msm_vidc_queue_rbr_event(struct msm_vidc_inst *inst,
		int fd, u32 offset)
{
	struct v4l2_event buf_event = {0};
	u32 *ptr;

	buf_event.type = V4L2_EVENT_RELEASE_BUFFER_REFERENCE;
	ptr = (u32 *)buf_event.u.data;
	ptr[0] = fd;
	ptr[1] = offset;

	v4l2_event_queue_fh(&inst->event_handler, &buf_event);
}

static void handle_event_change(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_inst *inst = NULL;
	struct msm_vidc_cb_event *event_notify = data;
	int event = V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT;
	struct v4l2_event seq_changed_event = {0};
	int rc = 0;
	struct hfi_device *hdev;
	u32 *ptr = NULL;
	struct msm_vidc_format *fmt;
	struct v4l2_format *f;
	int extra_buff_count = 0;
	u32 codec;

	if (!event_notify) {
		dprintk(VIDC_ERR, "Got an empty event from hfi\n");
		return;
	}

	inst = get_inst(get_vidc_core(event_notify->device_id),
			event_notify->session_id);
	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "Got a response for an inactive session\n");
		goto err_bad_event;
	}
	hdev = inst->core->device;

	switch (event_notify->hal_event_type) {
	case HAL_EVENT_SEQ_CHANGED_SUFFICIENT_RESOURCES:
	{
		/*
		 * Check if there is some parameter has changed
		 * If there is no change then no need to notify client
		 * If there is a change, then raise an insufficient event
		 */
		bool event_fields_changed = false;

		dprintk(VIDC_HIGH, "V4L2_EVENT_SEQ_CHANGED_SUFFICIENT\n");
		dprintk(VIDC_HIGH,
				"event_notify->height = %d event_notify->width = %d\n",
				event_notify->height,
				event_notify->width);
		event_fields_changed |= (inst->bit_depth !=
			event_notify->bit_depth);
		/* Check for change from hdr->non-hdr and vice versa */
		if ((event_notify->colour_space == MSM_VIDC_BT2020 &&
			inst->colour_space != MSM_VIDC_BT2020) ||
			(event_notify->colour_space != MSM_VIDC_BT2020 &&
			inst->colour_space == MSM_VIDC_BT2020))
			event_fields_changed = true;

		f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
		event_fields_changed |=
			(f->fmt.pix_mp.height != event_notify->height);
		event_fields_changed |=
			(f->fmt.pix_mp.width != event_notify->width);

		if (event_fields_changed) {
			event = V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT;
		} else {
			dprintk(VIDC_HIGH,
				"No parameter change continue session\n");
			rc = call_hfi_op(hdev, session_continue,
						 (void *)inst->session);
			if (rc) {
				dprintk(VIDC_ERR,
					"failed to send session_continue\n");
			}
			goto err_bad_event;
		}
		break;
	}
	case HAL_EVENT_SEQ_CHANGED_INSUFFICIENT_RESOURCES:
		event = V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT;
		break;
	case HAL_EVENT_RELEASE_BUFFER_REFERENCE:
	{
		struct msm_vidc_buffer *mbuf;
		u32 planes[VIDEO_MAX_PLANES] = {0};

		dprintk(VIDC_LOW,
			"%s: inst: %pK data_buffer: %x extradata_buffer: %x\n",
			__func__, inst, event_notify->packet_buffer,
			event_notify->extra_data_buffer);

		planes[0] = event_notify->packet_buffer;
		planes[1] = event_notify->extra_data_buffer;
		mbuf = msm_comm_get_buffer_using_device_planes(inst,
				OUTPUT_MPLANE, planes);
		if (!mbuf || !kref_get_mbuf(inst, mbuf)) {
			dprintk(VIDC_ERR,
				"%s: data_addr %x, extradata_addr %x not found\n",
				__func__, planes[0], planes[1]);
		} else {
			handle_release_buffer_reference(inst, mbuf);
			kref_put_mbuf(mbuf);
		}
		goto err_bad_event;
	}
	default:
		break;
	}

	/* Bit depth and pic struct changed event are combined into a single
	 * event (insufficient event) for the userspace. Currently bitdepth
	 * changes is only for HEVC and interlaced support is for all
	 * codecs except HEVC
	 * event data is now as follows:
	 * u32 *ptr = seq_changed_event.u.data;
	 * ptr[0] = height
	 * ptr[1] = width
	 * ptr[2] = bit depth
	 * ptr[3] = pic struct (progressive or interlaced)
	 * ptr[4] = colour space
	 * ptr[5] = crop_data(top)
	 * ptr[6] = crop_data(left)
	 * ptr[7] = crop_data(height)
	 * ptr[8] = crop_data(width)
	 * ptr[9] = profile
	 * ptr[10] = level
	 */

	inst->profile = event_notify->profile;
	inst->level = event_notify->level;
	inst->entropy_mode = event_notify->entropy_mode;
	inst->prop.crop_info.left =
		event_notify->crop_data.left;
	inst->prop.crop_info.top =
		event_notify->crop_data.top;
	inst->prop.crop_info.height =
		event_notify->crop_data.height;
	inst->prop.crop_info.width =
		event_notify->crop_data.width;
	/* HW returns progressive_only flag in pic_struct. */
	inst->pic_struct =
		event_notify->pic_struct ?
		MSM_VIDC_PIC_STRUCT_PROGRESSIVE :
		MSM_VIDC_PIC_STRUCT_MAYBE_INTERLACED;
	inst->colour_space = event_notify->colour_space;

	ptr = (u32 *)seq_changed_event.u.data;
	ptr[0] = event_notify->height;
	ptr[1] = event_notify->width;
	ptr[2] = event_notify->bit_depth;
	ptr[3] = event_notify->pic_struct;
	ptr[4] = event_notify->colour_space;
	ptr[5] = event_notify->crop_data.top;
	ptr[6] = event_notify->crop_data.left;
	ptr[7] = event_notify->crop_data.height;
	ptr[8] = event_notify->crop_data.width;
	codec = get_v4l2_codec(inst);
	ptr[9] = msm_comm_get_v4l2_profile(codec,
		event_notify->profile);
	ptr[10] = msm_comm_get_v4l2_level(codec,
		event_notify->level);

	dprintk(VIDC_HIGH,
		"Event payload: height = %u width = %u profile = %u level = %u\n",
			event_notify->height, event_notify->width,
			ptr[9], ptr[10]);

	dprintk(VIDC_HIGH,
		"Event payload: bit_depth = %u pic_struct = %u colour_space = %u\n",
		event_notify->bit_depth, event_notify->pic_struct,
			event_notify->colour_space);

	dprintk(VIDC_HIGH,
		"Event payload: CROP top = %u left = %u Height = %u Width = %u\n",
			event_notify->crop_data.top,
			event_notify->crop_data.left,
			event_notify->crop_data.height,
			event_notify->crop_data.width);

	mutex_lock(&inst->lock);
	inst->in_reconfig = true;
	fmt = &inst->fmts[INPUT_PORT];
	fmt->v4l2_fmt.fmt.pix_mp.height = event_notify->height;
	fmt->v4l2_fmt.fmt.pix_mp.width = event_notify->width;
	inst->bit_depth = event_notify->bit_depth;

	fmt = &inst->fmts[OUTPUT_PORT];
	fmt->v4l2_fmt.fmt.pix_mp.height = event_notify->height;
	fmt->v4l2_fmt.fmt.pix_mp.width = event_notify->width;
	mutex_unlock(&inst->lock);

	if (event == V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT) {
		dprintk(VIDC_HIGH, "V4L2_EVENT_SEQ_CHANGED_INSUFFICIENT\n");

		/* decide batching as configuration changed */
		if (inst->batch.enable)
			inst->batch.enable = is_batching_allowed(inst);
		dprintk(VIDC_HIGH|VIDC_PERF, "%s: %x : batching %s\n",
			__func__, hash32_ptr(inst->session),
			inst->batch.enable ? "enabled" : "disabled");
		msm_dcvs_try_enable(inst);
		extra_buff_count = msm_vidc_get_extra_buff_count(inst,
				HAL_BUFFER_OUTPUT);
		fmt->count_min = event_notify->capture_buf_count;
		fmt->count_min_host = fmt->count_min + extra_buff_count;
		dprintk(VIDC_HIGH,
			"%s: %x : hal buffer[%d] count: min %d min_host %d\n",
			__func__, hash32_ptr(inst->session),
			HAL_BUFFER_OUTPUT, fmt->count_min,
			fmt->count_min_host);
	}

	rc = msm_vidc_check_session_supported(inst);
	if (!rc) {
		seq_changed_event.type = event;
		v4l2_event_queue_fh(&inst->event_handler, &seq_changed_event);
	} else if (rc == -ENOTSUPP) {
		msm_vidc_queue_v4l2_event(inst,
				V4L2_EVENT_MSM_VIDC_HW_UNSUPPORTED);
	} else if (rc == -EBUSY) {
		msm_vidc_queue_v4l2_event(inst,
				V4L2_EVENT_MSM_VIDC_HW_OVERLOAD);
	}

err_bad_event:
	put_inst(inst);
}

static void handle_session_prop_info(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct getprop_buf *getprop;
	struct msm_vidc_inst *inst;

	if (!response) {
		dprintk(VIDC_ERR,
			"Failed to get valid response for prop info\n");
		return;
	}

	inst = get_inst(get_vidc_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(VIDC_ERR, "Got a response for an inactive session\n");
		return;
	}

	getprop = kzalloc(sizeof(*getprop), GFP_KERNEL);
	if (!getprop) {
		dprintk(VIDC_ERR, "%s: getprop kzalloc failed\n", __func__);
		goto err_prop_info;
	}

	getprop->data = kmemdup((void *) (&response->data.property),
			sizeof(union hal_get_property), GFP_KERNEL);
	if (!getprop->data) {
		dprintk(VIDC_ERR, "%s: kmemdup failed\n", __func__);
		kfree(getprop);
		goto err_prop_info;
	}

	mutex_lock(&inst->pending_getpropq.lock);
	list_add_tail(&getprop->list, &inst->pending_getpropq.list);
	mutex_unlock(&inst->pending_getpropq.lock);

	signal_session_msg_receipt(cmd, inst);
err_prop_info:
	put_inst(inst);
}

static void handle_load_resource_done(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;

	if (!response) {
		dprintk(VIDC_ERR,
			"Failed to get valid response for load resource\n");
		return;
	}

	inst = get_inst(get_vidc_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(VIDC_ERR, "Got a response for an inactive session\n");
		return;
	}

	if (response->status) {
		dprintk(VIDC_ERR,
				"Load resource response from FW : %#x\n",
				response->status);
		msm_comm_generate_session_error(inst);
	}

	put_inst(inst);
}

static void handle_start_done(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;

	if (!response) {
		dprintk(VIDC_ERR, "Failed to get valid response for start\n");
		return;
	}

	inst = get_inst(get_vidc_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(VIDC_ERR, "Got a response for an inactive session\n");
		return;
	}

	signal_session_msg_receipt(cmd, inst);
	put_inst(inst);
}

static void handle_stop_done(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;

	if (!response) {
		dprintk(VIDC_ERR, "Failed to get valid response for stop\n");
		return;
	}

	inst = get_inst(get_vidc_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(VIDC_ERR, "Got a response for an inactive session\n");
		return;
	}

	signal_session_msg_receipt(cmd, inst);
	put_inst(inst);
}

static void handle_release_res_done(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;

	if (!response) {
		dprintk(VIDC_ERR,
			"Failed to get valid response for release resource\n");
		return;
	}

	inst = get_inst(get_vidc_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(VIDC_ERR, "Got a response for an inactive session\n");
		return;
	}

	signal_session_msg_receipt(cmd, inst);
	put_inst(inst);
}

void msm_comm_validate_output_buffers(struct msm_vidc_inst *inst)
{
	struct internal_buf *binfo;
	u32 buffers_owned_by_driver = 0;
	struct msm_vidc_format *fmt;

	fmt = &inst->fmts[OUTPUT_PORT];

	mutex_lock(&inst->outputbufs.lock);
	if (list_empty(&inst->outputbufs.list)) {
		dprintk(VIDC_HIGH, "%s: no OUTPUT buffers allocated\n",
			__func__);
		mutex_unlock(&inst->outputbufs.lock);
		return;
	}
	list_for_each_entry(binfo, &inst->outputbufs.list, list) {
		if (binfo->buffer_ownership != DRIVER) {
			dprintk(VIDC_HIGH,
				"This buffer is with FW %x\n",
				binfo->smem.device_addr);
			continue;
		}
		buffers_owned_by_driver++;
	}
	mutex_unlock(&inst->outputbufs.lock);

	/* Only minimum number of DPBs are allocated */
	if (buffers_owned_by_driver != fmt->count_min) {
		dprintk(VIDC_ERR,
			"OUTPUT Buffer count mismatch %d of %d\n",
			buffers_owned_by_driver,
			fmt->count_min);
		msm_vidc_handle_hw_error(inst->core);
	}
}

int msm_comm_queue_dpb_only_buffers(struct msm_vidc_inst *inst)
{
	struct internal_buf *binfo, *extra_info;
	struct hfi_device *hdev;
	struct vidc_frame_data frame_data = {0};
	int rc = 0;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	hdev = inst->core->device;

	extra_info = inst->dpb_extra_binfo;
	mutex_lock(&inst->outputbufs.lock);
	list_for_each_entry(binfo, &inst->outputbufs.list, list) {
		if (binfo->buffer_ownership != DRIVER)
			continue;
		if (binfo->mark_remove)
			continue;
		frame_data.alloc_len = binfo->smem.size;
		frame_data.filled_len = 0;
		frame_data.offset = 0;
		frame_data.device_addr = binfo->smem.device_addr;
		frame_data.flags = 0;
		frame_data.extradata_addr =
			extra_info ? extra_info->smem.device_addr : 0;
		frame_data.buffer_type = HAL_BUFFER_OUTPUT;
		frame_data.extradata_size =
			extra_info ? extra_info->smem.size : 0;
		rc = call_hfi_op(hdev, session_ftb,
			(void *) inst->session, &frame_data);
		binfo->buffer_ownership = FIRMWARE;
	}
	mutex_unlock(&inst->outputbufs.lock);

	return rc;
}

static void handle_session_flush(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;
	struct v4l2_event flush_event = {0};
	u32 *ptr = NULL;
	enum hal_flush flush_type;
	int rc;

	if (!response) {
		dprintk(VIDC_ERR, "Failed to get valid response for flush\n");
		return;
	}

	inst = get_inst(get_vidc_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(VIDC_ERR, "Got a response for an inactive session\n");
		return;
	}

	mutex_lock(&inst->flush_lock);
	if (msm_comm_get_stream_output_mode(inst) ==
			HAL_VIDEO_DECODER_SECONDARY) {

		if (!(get_v4l2_codec(inst) == V4L2_PIX_FMT_VP9 &&
				inst->in_reconfig))
			msm_comm_validate_output_buffers(inst);

		if (!inst->in_reconfig) {
			rc = msm_comm_queue_dpb_only_buffers(inst);
			if (rc) {
				dprintk(VIDC_ERR,
						"Failed to queue output buffers: %d\n",
						rc);
			}
		}
	}
	flush_event.type = V4L2_EVENT_MSM_VIDC_FLUSH_DONE;
	ptr = (u32 *)flush_event.u.data;

	flush_type = response->data.flush_type;
	switch (flush_type) {
	case HAL_FLUSH_INPUT:
		inst->in_flush = false;
		ptr[0] = V4L2_CMD_FLUSH_OUTPUT;
		break;
	case HAL_FLUSH_OUTPUT:
		inst->out_flush = false;
		ptr[0] = V4L2_CMD_FLUSH_CAPTURE;
		break;
	case HAL_FLUSH_ALL:
		inst->in_flush = false;
		inst->out_flush = false;
		ptr[0] |= V4L2_CMD_FLUSH_CAPTURE;
		ptr[0] |= V4L2_CMD_FLUSH_OUTPUT;
		break;
	default:
		dprintk(VIDC_ERR, "Invalid flush type received!");
		goto exit;
	}

	if (flush_type == HAL_FLUSH_ALL) {
		msm_comm_clear_window_data(inst);
		msm_comm_release_client_data(inst, false);
		inst->clk_data.buffer_counter = 0;
	}

	dprintk(VIDC_HIGH,
		"Notify flush complete, flush_type: %x\n", flush_type);
	v4l2_event_queue_fh(&inst->event_handler, &flush_event);

exit:
	mutex_unlock(&inst->flush_lock);
	put_inst(inst);
}

static void handle_session_error(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct hfi_device *hdev = NULL;
	struct msm_vidc_inst *inst = NULL;
	int event = V4L2_EVENT_MSM_VIDC_SYS_ERROR;

	if (!response) {
		dprintk(VIDC_ERR,
			"Failed to get valid response for session error\n");
		return;
	}

	inst = get_inst(get_vidc_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(VIDC_ERR, "Got a response for an inactive session\n");
		return;
	}

	hdev = inst->core->device;
	dprintk(VIDC_ERR, "Session error received for inst %pK session %x\n",
		inst, hash32_ptr(inst->session));

	if (response->status == VIDC_ERR_MAX_CLIENTS) {
		dprintk(VIDC_ERR, "Too many clients, rejecting %pK", inst);
		event = V4L2_EVENT_MSM_VIDC_MAX_CLIENTS;

		/*
		 * Clean the HFI session now. Since inst->state is moved to
		 * INVALID, forward thread doesn't know FW has valid session
		 * or not. This is the last place driver knows that there is
		 * no session in FW. Hence clean HFI session now.
		 */

		msm_comm_session_clean(inst);
	} else if (response->status == VIDC_ERR_NOT_SUPPORTED) {
		dprintk(VIDC_ERR, "Unsupported bitstream in %pK", inst);
		event = V4L2_EVENT_MSM_VIDC_HW_UNSUPPORTED;
	} else {
		dprintk(VIDC_ERR, "Unknown session error (%d) for %pK\n",
				response->status, inst);
		event = V4L2_EVENT_MSM_VIDC_SYS_ERROR;
	}

	/* change state before sending error to client */
	change_inst_state(inst, MSM_VIDC_CORE_INVALID);
	msm_vidc_queue_v4l2_event(inst, event);
	put_inst(inst);
}

static void msm_comm_clean_notify_client(struct msm_vidc_core *core)
{
	struct msm_vidc_inst *inst = NULL;

	if (!core) {
		dprintk(VIDC_ERR, "%s: Invalid params\n", __func__);
		return;
	}

	dprintk(VIDC_ERR, "%s: Core %pK\n", __func__, core);
	mutex_lock(&core->lock);

	list_for_each_entry(inst, &core->instances, list) {
		mutex_lock(&inst->lock);
		inst->state = MSM_VIDC_CORE_INVALID;
		mutex_unlock(&inst->lock);
		dprintk(VIDC_ERR,
			"%s Send sys error for inst %pK\n", __func__, inst);
		msm_vidc_queue_v4l2_event(inst,
				V4L2_EVENT_MSM_VIDC_SYS_ERROR);
	}
	mutex_unlock(&core->lock);
}

static void handle_sys_error(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_core *core = NULL;
	struct hfi_device *hdev = NULL;
	struct msm_vidc_inst *inst = NULL;
	int rc = 0;

	subsystem_crashed("venus");
	if (!response) {
		dprintk(VIDC_ERR,
			"Failed to get valid response for sys error\n");
		return;
	}

	core = get_vidc_core(response->device_id);
	if (!core) {
		dprintk(VIDC_ERR,
				"Got SYS_ERR but unable to identify core\n");
		return;
	}
	hdev = core->device;

	mutex_lock(&core->lock);
	if (core->state == VIDC_CORE_UNINIT) {
		dprintk(VIDC_ERR,
			"%s: Core %pK already moved to state %d\n",
			 __func__, core, core->state);
		mutex_unlock(&core->lock);
		return;
	}

	dprintk(VIDC_ERR, "SYS_ERROR received for core %pK\n", core);
	msm_vidc_noc_error_info(core);
	call_hfi_op(hdev, flush_debug_queue, hdev->hfi_device_data);
	list_for_each_entry(inst, &core->instances, list) {
		dprintk(VIDC_ERR,
			"%s: Send sys error for inst %pK\n", __func__, inst);
		change_inst_state(inst, MSM_VIDC_CORE_INVALID);
		msm_vidc_queue_v4l2_event(inst, V4L2_EVENT_MSM_VIDC_SYS_ERROR);
		if (!core->trigger_ssr)
			msm_comm_print_inst_info(inst);
	}

	/* handle the hw error before core released to get full debug info */
	msm_vidc_handle_hw_error(core);
	if (response->status == VIDC_ERR_NOC_ERROR) {
		dprintk(VIDC_ERR, "Got NOC error");
		MSM_VIDC_ERROR(true);
	}

	dprintk(VIDC_ERR, "Calling core_release\n");
	rc = call_hfi_op(hdev, core_release, hdev->hfi_device_data);
	if (rc) {
		dprintk(VIDC_ERR, "core_release failed\n");
		mutex_unlock(&core->lock);
		return;
	}
	core->state = VIDC_CORE_UNINIT;
	mutex_unlock(&core->lock);

	dprintk(VIDC_ERR, "SYS_ERROR handled.\n");
}

void msm_comm_session_clean(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev = NULL;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid params\n", __func__);
		return;
	}
	if (!inst->session) {
		dprintk(VIDC_HIGH, "%s: inst %pK session already cleaned\n",
			__func__, inst);
		return;
	}

	hdev = inst->core->device;
	mutex_lock(&inst->lock);
	dprintk(VIDC_HIGH, "%s: inst %pK\n", __func__, inst);
	rc = call_hfi_op(hdev, session_clean,
			(void *)inst->session);
	if (rc) {
		dprintk(VIDC_ERR,
			"Session clean failed :%pK\n", inst);
	}
	inst->session = NULL;
	mutex_unlock(&inst->lock);
}

static void handle_session_close(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_cmd_done *response = data;
	struct msm_vidc_inst *inst;

	if (!response) {
		dprintk(VIDC_ERR,
			"Failed to get valid response for session close\n");
		return;
	}

	inst = get_inst(get_vidc_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(VIDC_ERR, "Got a response for an inactive session\n");
		return;
	}

	signal_session_msg_receipt(cmd, inst);
	show_stats(inst);
	put_inst(inst);
}

struct vb2_buffer *msm_comm_get_vb_using_vidc_buffer(
		struct msm_vidc_inst *inst, struct msm_vidc_buffer *mbuf)
{
	u32 port = 0;
	struct vb2_buffer *vb = NULL;
	struct vb2_queue *q = NULL;
	bool found = false;

	if (mbuf->vvb.vb2_buf.type == OUTPUT_MPLANE) {
		port = OUTPUT_PORT;
	} else if (mbuf->vvb.vb2_buf.type == INPUT_MPLANE) {
		port = INPUT_PORT;
	} else {
		dprintk(VIDC_ERR, "%s: invalid type %d\n",
			__func__, mbuf->vvb.vb2_buf.type);
		return NULL;
	}

	mutex_lock(&inst->bufq[port].lock);
	found = false;
	q = &inst->bufq[port].vb2_bufq;
	if (!q->streaming) {
		dprintk(VIDC_ERR, "port %d is not streaming", port);
		goto unlock;
	}
	list_for_each_entry(vb, &q->queued_list, queued_entry) {
		if (vb->state != VB2_BUF_STATE_ACTIVE)
			continue;
		if (msm_comm_compare_vb2_planes(inst, mbuf, vb)) {
			found = true;
			break;
		}
	}
unlock:
	mutex_unlock(&inst->bufq[port].lock);
	if (!found) {
		print_vidc_buffer(VIDC_ERR, "vb2 not found for", inst, mbuf);
		return NULL;
	}

	return vb;
}

int msm_comm_vb2_buffer_done(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf)
{
	struct vb2_buffer *vb2;
	struct vb2_v4l2_buffer *vbuf;
	u32 i, port;

	if (!inst || !mbuf) {
		dprintk(VIDC_ERR, "%s: invalid params %pK %pK\n",
			__func__, inst, mbuf);
		return -EINVAL;
	}

	if (mbuf->vvb.vb2_buf.type == OUTPUT_MPLANE)
		port = OUTPUT_PORT;
	else if (mbuf->vvb.vb2_buf.type == INPUT_MPLANE)
		port = INPUT_PORT;
	else
		return -EINVAL;

	vb2 = msm_comm_get_vb_using_vidc_buffer(inst, mbuf);
	if (!vb2)
		return -EINVAL;

	/*
	 * access vb2 buffer under q->lock and if streaming only to
	 * ensure the buffer was not free'd by vb2 framework while
	 * we are accessing it here.
	 */
	mutex_lock(&inst->bufq[port].lock);
	if (inst->bufq[port].vb2_bufq.streaming) {
		vbuf = to_vb2_v4l2_buffer(vb2);
		vbuf->flags = mbuf->vvb.flags;
		vb2->timestamp = mbuf->vvb.vb2_buf.timestamp;
		for (i = 0; i < mbuf->vvb.vb2_buf.num_planes; i++) {
			vb2->planes[i].bytesused =
				mbuf->vvb.vb2_buf.planes[i].bytesused;
			vb2->planes[i].data_offset =
				mbuf->vvb.vb2_buf.planes[i].data_offset;
		}
		vb2_buffer_done(vb2, VB2_BUF_STATE_DONE);
	} else {
		dprintk(VIDC_ERR, "%s: port %d is not streaming\n",
			__func__, port);
	}
	mutex_unlock(&inst->bufq[port].lock);

	return 0;
}

static bool is_eos_buffer(struct msm_vidc_inst *inst, u32 device_addr)
{
	struct eos_buf *temp, *next;
	bool found = false;

	mutex_lock(&inst->eosbufs.lock);
	list_for_each_entry_safe(temp, next, &inst->eosbufs.list, list) {
		if (temp->smem.device_addr == device_addr) {
			found = true;
			list_del(&temp->list);
			msm_comm_smem_free(inst, &temp->smem);
			kfree(temp);
			break;
		}
	}
	mutex_unlock(&inst->eosbufs.lock);

	return found;
}

static void handle_ebd(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_data_done *response = data;
	struct msm_vidc_buffer *mbuf;
	struct vb2_buffer *vb;
	struct msm_vidc_inst *inst;
	struct vidc_hal_ebd *empty_buf_done;
	u32 planes[VIDEO_MAX_PLANES] = {0};
	struct v4l2_format *f;
	struct v4l2_ctrl *ctrl;

	if (!response) {
		dprintk(VIDC_ERR, "Invalid response from vidc_hal\n");
		return;
	}

	inst = get_inst(get_vidc_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(VIDC_ERR, "Got a response for an inactive session\n");
		return;
	}

	empty_buf_done = (struct vidc_hal_ebd *)&response->input_done;
	/* If this is internal EOS buffer, handle it in driver */
	if (is_eos_buffer(inst, empty_buf_done->packet_buffer)) {
		dprintk(VIDC_HIGH, "Received EOS buffer 0x%x\n",
			empty_buf_done->packet_buffer);
		goto exit;
	}

	planes[0] = empty_buf_done->packet_buffer;
	planes[1] = empty_buf_done->extra_data_buffer;

	mbuf = msm_comm_get_buffer_using_device_planes(inst,
			INPUT_MPLANE, planes);
	if (!mbuf || !kref_get_mbuf(inst, mbuf)) {
		dprintk(VIDC_ERR,
			"%s: data_addr %x, extradata_addr %x not found\n",
			__func__, planes[0], planes[1]);
		goto exit;
	}
	vb = &mbuf->vvb.vb2_buf;

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_SUPERFRAME);
	if (ctrl->val && empty_buf_done->offset +
		empty_buf_done->filled_len < vb->planes[0].length) {
		dprintk(VIDC_HIGH,
			"%s: %x : addr (%#x): offset (%d) + filled_len (%d) < length (%d)\n",
			__func__, hash32_ptr(inst->session),
			empty_buf_done->packet_buffer,
			empty_buf_done->offset,
			empty_buf_done->filled_len,
			vb->planes[0].length);
		kref_put_mbuf(mbuf);
		goto exit;
	}

	mbuf->flags &= ~MSM_VIDC_FLAG_QUEUED;
	vb->planes[0].bytesused = response->input_done.filled_len;
	if (vb->planes[0].bytesused > vb->planes[0].length)
		dprintk(VIDC_LOW, "bytesused overflow length\n");

	vb->planes[0].data_offset = response->input_done.offset;
	if (vb->planes[0].data_offset > vb->planes[0].length)
		dprintk(VIDC_LOW, "data_offset overflow length\n");

	if (empty_buf_done->status == VIDC_ERR_NOT_SUPPORTED) {
		dprintk(VIDC_LOW, "Failed : Unsupported input stream\n");
		mbuf->vvb.flags |= V4L2_BUF_INPUT_UNSUPPORTED;
	}
	if (empty_buf_done->status == VIDC_ERR_BITSTREAM_ERR) {
		dprintk(VIDC_LOW, "Failed : Corrupted input stream\n");
		mbuf->vvb.flags |= V4L2_BUF_FLAG_DATA_CORRUPT;
	}

	f = &inst->fmts[INPUT_PORT].v4l2_fmt;
	if (f->fmt.pix_mp.num_planes > 1)
		vb->planes[1].bytesused = vb->planes[1].length;

	update_recon_stats(inst, &empty_buf_done->recon_stats);
	msm_vidc_clear_freq_entry(inst, mbuf->smem[0].device_addr);
	/*
	 * dma cache operations need to be performed before dma_unmap
	 * which is done inside msm_comm_put_vidc_buffer()
	 */
	msm_comm_dqbuf_cache_operations(inst, mbuf);
	/*
	 * put_buffer should be done before vb2_buffer_done else
	 * client might queue the same buffer before it is unmapped
	 * in put_buffer.
	 */
	msm_comm_put_vidc_buffer(inst, mbuf);
	msm_comm_vb2_buffer_done(inst, mbuf);
	msm_vidc_debugfs_update(inst, MSM_VIDC_DEBUGFS_EVENT_EBD);
	kref_put_mbuf(mbuf);
exit:
	put_inst(inst);
}

static int handle_multi_stream_buffers(struct msm_vidc_inst *inst,
		u32 dev_addr)
{
	struct internal_buf *binfo;
	struct msm_smem *smem;
	bool found = false;

	mutex_lock(&inst->outputbufs.lock);
	list_for_each_entry(binfo, &inst->outputbufs.list, list) {
		smem = &binfo->smem;
		if (smem && dev_addr == smem->device_addr) {
			if (binfo->buffer_ownership == DRIVER) {
				dprintk(VIDC_ERR,
					"FW returned same buffer: %x\n",
					dev_addr);
				break;
			}
			binfo->buffer_ownership = DRIVER;
			found = true;
			break;
		}
	}
	mutex_unlock(&inst->outputbufs.lock);

	if (!found) {
		dprintk(VIDC_ERR,
			"Failed to find output buffer in queued list: %x\n",
			dev_addr);
	}

	return 0;
}

enum hal_buffer msm_comm_get_hal_output_buffer(struct msm_vidc_inst *inst)
{
	if (msm_comm_get_stream_output_mode(inst) ==
		HAL_VIDEO_DECODER_SECONDARY)
		return HAL_BUFFER_OUTPUT2;
	else
		return HAL_BUFFER_OUTPUT;
}

static void handle_fbd(enum hal_command_response cmd, void *data)
{
	struct msm_vidc_cb_data_done *response = data;
	struct msm_vidc_buffer *mbuf;
	struct msm_vidc_inst *inst;
	struct vb2_buffer *vb;
	struct vidc_hal_fbd *fill_buf_done;
	enum hal_buffer buffer_type;
	u64 time_usec = 0;
	u32 planes[VIDEO_MAX_PLANES] = {0};
	struct v4l2_format *f;

	if (!response) {
		dprintk(VIDC_ERR, "Invalid response from vidc_hal\n");
		return;
	}

	inst = get_inst(get_vidc_core(response->device_id),
			response->session_id);
	if (!inst) {
		dprintk(VIDC_ERR, "Got a response for an inactive session\n");
		return;
	}

	fill_buf_done = (struct vidc_hal_fbd *)&response->output_done;
	planes[0] = fill_buf_done->packet_buffer1;
	planes[1] = fill_buf_done->extra_data_buffer;

	buffer_type = msm_comm_get_hal_output_buffer(inst);
	if (fill_buf_done->buffer_type == buffer_type) {
		mbuf = msm_comm_get_buffer_using_device_planes(inst,
				OUTPUT_MPLANE, planes);
		if (!mbuf || !kref_get_mbuf(inst, mbuf)) {
			dprintk(VIDC_ERR,
				"%s: data_addr %x, extradata_addr %x not found\n",
				__func__, planes[0], planes[1]);
			goto exit;
		}
	} else {
		if (handle_multi_stream_buffers(inst,
				fill_buf_done->packet_buffer1))
			dprintk(VIDC_ERR,
				"Failed : Output buffer not found %pa\n",
				&fill_buf_done->packet_buffer1);
		goto exit;
	}
	mbuf->flags &= ~MSM_VIDC_FLAG_QUEUED;
	vb = &mbuf->vvb.vb2_buf;

	if (fill_buf_done->buffer_type == HAL_BUFFER_OUTPUT2 &&
		fill_buf_done->flags1 & HAL_BUFFERFLAG_READONLY) {
		dprintk(VIDC_ERR,
			"%s: Read only buffer not allowed for OPB\n", __func__);
		goto exit;
	}

	if (fill_buf_done->flags1 & HAL_BUFFERFLAG_DROP_FRAME)
		fill_buf_done->filled_len1 = 0;
	vb->planes[0].bytesused = fill_buf_done->filled_len1;
	if (vb->planes[0].bytesused > vb->planes[0].length)
		dprintk(VIDC_LOW,
			"fbd:Overflow bytesused = %d; length = %d\n",
			vb->planes[0].bytesused,
			vb->planes[0].length);
	vb->planes[0].data_offset = fill_buf_done->offset1;
	if (vb->planes[0].data_offset > vb->planes[0].length)
		dprintk(VIDC_LOW,
			"fbd:Overflow data_offset = %d; length = %d\n",
			vb->planes[0].data_offset,
			vb->planes[0].length);

	time_usec = fill_buf_done->timestamp_hi;
	time_usec = (time_usec << 32) | fill_buf_done->timestamp_lo;

	vb->timestamp = (time_usec * NSEC_PER_USEC);

	msm_comm_store_input_tag(&inst->fbd_data, vb->index,
		fill_buf_done->input_tag, fill_buf_done->input_tag2);

	if (inst->session_type == MSM_VIDC_ENCODER) {
		msm_comm_store_filled_length(&inst->fbd_data, vb->index,
			fill_buf_done->filled_len1);
	}

	f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
	if (f->fmt.pix_mp.num_planes > 1)
		vb->planes[1].bytesused = vb->planes[1].length;

	mbuf->vvb.flags = 0;
	if (fill_buf_done->flags1 & HAL_BUFFERFLAG_READONLY)
		mbuf->vvb.flags |= V4L2_BUF_FLAG_READONLY;
	if (fill_buf_done->flags1 & HAL_BUFFERFLAG_EOS)
		mbuf->vvb.flags |= V4L2_BUF_FLAG_EOS;
	if (fill_buf_done->flags1 & HAL_BUFFERFLAG_CODECCONFIG)
		mbuf->vvb.flags |= V4L2_BUF_FLAG_CODECCONFIG;
	if (fill_buf_done->flags1 & HAL_BUFFERFLAG_SYNCFRAME)
		mbuf->vvb.flags |= V4L2_BUF_FLAG_KEYFRAME;
	if (fill_buf_done->flags1 & HAL_BUFFERFLAG_DATACORRUPT)
		mbuf->vvb.flags |= V4L2_BUF_FLAG_DATA_CORRUPT;
	if (fill_buf_done->flags1 & HAL_BUFFERFLAG_ENDOFSUBFRAME)
		mbuf->vvb.flags |= V4L2_BUF_FLAG_END_OF_SUBFRAME;
	switch (fill_buf_done->picture_type) {
	case HFI_PICTURE_TYPE_P:
		mbuf->vvb.flags |= V4L2_BUF_FLAG_PFRAME;
		break;
	case HFI_PICTURE_TYPE_B:
		mbuf->vvb.flags |= V4L2_BUF_FLAG_BFRAME;
		break;
	case HFI_FRAME_NOTCODED:
	case HFI_UNUSED_PICT:
		/* Do we need to care about these? */
	case HFI_FRAME_YUV:
		break;
	default:
		break;
	}

	/*
	 * dma cache operations need to be performed before dma_unmap
	 * which is done inside msm_comm_put_vidc_buffer()
	 */
	msm_comm_dqbuf_cache_operations(inst, mbuf);
	/*
	 * put_buffer should be done before vb2_buffer_done else
	 * client might queue the same buffer before it is unmapped
	 * in put_buffer.
	 */
	msm_comm_put_vidc_buffer(inst, mbuf);
	msm_comm_vb2_buffer_done(inst, mbuf);
	msm_vidc_debugfs_update(inst, MSM_VIDC_DEBUGFS_EVENT_FBD);
	kref_put_mbuf(mbuf);

exit:
	put_inst(inst);
}

void handle_cmd_response(enum hal_command_response cmd, void *data)
{
	dprintk(VIDC_LOW, "Command response = %d\n", cmd);
	switch (cmd) {
	case HAL_SYS_INIT_DONE:
		handle_sys_init_done(cmd, data);
		break;
	case HAL_SYS_RELEASE_RESOURCE_DONE:
		handle_sys_release_res_done(cmd, data);
		break;
	case HAL_SESSION_INIT_DONE:
		handle_session_init_done(cmd, data);
		break;
	case HAL_SESSION_PROPERTY_INFO:
		handle_session_prop_info(cmd, data);
		break;
	case HAL_SESSION_LOAD_RESOURCE_DONE:
		handle_load_resource_done(cmd, data);
		break;
	case HAL_SESSION_START_DONE:
		handle_start_done(cmd, data);
		break;
	case HAL_SESSION_ETB_DONE:
		handle_ebd(cmd, data);
		break;
	case HAL_SESSION_FTB_DONE:
		handle_fbd(cmd, data);
		break;
	case HAL_SESSION_STOP_DONE:
		handle_stop_done(cmd, data);
		break;
	case HAL_SESSION_RELEASE_RESOURCE_DONE:
		handle_release_res_done(cmd, data);
		break;
	case HAL_SESSION_END_DONE:
	case HAL_SESSION_ABORT_DONE:
		handle_session_close(cmd, data);
		break;
	case HAL_SESSION_EVENT_CHANGE:
		handle_event_change(cmd, data);
		break;
	case HAL_SESSION_FLUSH_DONE:
		handle_session_flush(cmd, data);
		break;
	case HAL_SYS_WATCHDOG_TIMEOUT:
	case HAL_SYS_ERROR:
		handle_sys_error(cmd, data);
		break;
	case HAL_SESSION_ERROR:
		handle_session_error(cmd, data);
		break;
	case HAL_SESSION_RELEASE_BUFFER_DONE:
		handle_session_release_buf_done(cmd, data);
		break;
	case HAL_SESSION_REGISTER_BUFFER_DONE:
		handle_session_register_buffer_done(cmd, data);
		break;
	case HAL_SESSION_UNREGISTER_BUFFER_DONE:
		handle_session_unregister_buffer_done(cmd, data);
		break;
	default:
		dprintk(VIDC_LOW, "response unhandled: %d\n", cmd);
		break;
	}
}

static inline enum msm_vidc_thermal_level msm_comm_vidc_thermal_level(int level)
{
	switch (level) {
	case 0:
		return VIDC_THERMAL_NORMAL;
	case 1:
		return VIDC_THERMAL_LOW;
	case 2:
		return VIDC_THERMAL_HIGH;
	default:
		return VIDC_THERMAL_CRITICAL;
	}
}

static bool is_core_turbo(struct msm_vidc_core *core, unsigned long freq)
{
	unsigned int i = 0;
	struct allowed_clock_rates_table *allowed_clks_tbl = NULL;
	u32 max_freq = 0;

	allowed_clks_tbl = core->resources.allowed_clks_tbl;
	for (i = 0; i < core->resources.allowed_clks_tbl_size; i++) {
		if (max_freq < allowed_clks_tbl[i].clock_rate)
			max_freq = allowed_clks_tbl[i].clock_rate;
	}
	return freq >= max_freq;
}

static bool is_thermal_permissible(struct msm_vidc_core *core)
{
	enum msm_vidc_thermal_level tl;
	unsigned long freq = 0;
	bool is_turbo = false;

	if (!core->resources.thermal_mitigable)
		return true;

	if (msm_vidc_thermal_mitigation_disabled) {
		dprintk(VIDC_HIGH,
			"Thermal mitigation not enabled. debugfs %d\n",
			msm_vidc_thermal_mitigation_disabled);
		return true;
	}

	tl = msm_comm_vidc_thermal_level(vidc_driver->thermal_level);
	freq = core->curr_freq;

	is_turbo = is_core_turbo(core, freq);
	dprintk(VIDC_HIGH,
		"Core freq %ld Thermal level %d Turbo mode %d\n",
		freq, tl, is_turbo);

	if (is_turbo && tl >= VIDC_THERMAL_LOW) {
		dprintk(VIDC_ERR,
			"Video session not allowed. Turbo mode %d Thermal level %d\n",
			is_turbo, tl);
		return false;
	}
	return true;
}

bool is_batching_allowed(struct msm_vidc_inst *inst)
{
	u32 op_pixelformat, fps, maxmbs, maxfps;
	u32 ignore_flags = VIDC_THUMBNAIL;

	if (!inst || !inst->core)
		return false;

	/* Enable decode batching based on below conditions */
	op_pixelformat =
		inst->fmts[OUTPUT_PORT].v4l2_fmt.fmt.pix_mp.pixelformat;
	fps = inst->clk_data.frame_rate >> 16;
	maxmbs = inst->capability.cap[CAP_BATCH_MAX_MB_PER_FRAME].max;
	maxfps = inst->capability.cap[CAP_BATCH_MAX_FPS].max;

	return (is_single_session(inst, ignore_flags) &&
		is_decode_session(inst) &&
		!is_thumbnail_session(inst) &&
		!inst->clk_data.low_latency_mode &&
		(op_pixelformat == V4L2_PIX_FMT_NV12_UBWC ||
		 op_pixelformat	== V4L2_PIX_FMT_NV12_TP10_UBWC) &&
		fps <= maxfps &&
		msm_vidc_get_mbs_per_frame(inst) <= maxmbs);
}

static int msm_comm_session_abort(struct msm_vidc_inst *inst)
{
	int rc = 0, abort_completion = 0;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid params\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;
	abort_completion = SESSION_MSG_INDEX(HAL_SESSION_ABORT_DONE);

	dprintk(VIDC_ERR, "%s: inst %pK session %x\n", __func__,
		inst, hash32_ptr(inst->session));
	rc = call_hfi_op(hdev, session_abort, (void *)inst->session);
	if (rc) {
		dprintk(VIDC_ERR,
			"%s session_abort failed rc: %d\n", __func__, rc);
		goto exit;
	}
	rc = wait_for_completion_timeout(
			&inst->completions[abort_completion],
			msecs_to_jiffies(
				inst->core->resources.msm_vidc_hw_rsp_timeout));
	if (!rc) {
		dprintk(VIDC_ERR, "%s: inst %pK session %x abort timed out\n",
				__func__, inst, hash32_ptr(inst->session));
		msm_comm_generate_sys_error(inst);
		rc = -EBUSY;
	} else {
		rc = 0;
	}
exit:
	return rc;
}

static void handle_thermal_event(struct msm_vidc_core *core)
{
	int rc = 0;
	struct msm_vidc_inst *inst;

	if (!core || !core->device) {
		dprintk(VIDC_ERR, "%s Invalid params\n", __func__);
		return;
	}
	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {
		if (!inst->session)
			continue;

		mutex_unlock(&core->lock);
		if (inst->state >= MSM_VIDC_OPEN_DONE &&
			inst->state < MSM_VIDC_CLOSE_DONE) {
			dprintk(VIDC_ERR, "%s: abort inst %pK\n",
				__func__, inst);
			rc = msm_comm_session_abort(inst);
			if (rc) {
				dprintk(VIDC_ERR,
					"%s session_abort failed rc: %d\n",
					__func__, rc);
				goto err_sess_abort;
			}
			change_inst_state(inst, MSM_VIDC_CORE_INVALID);
			dprintk(VIDC_ERR,
				"%s Send sys error for inst %pK\n",
				__func__, inst);
			msm_vidc_queue_v4l2_event(inst,
					V4L2_EVENT_MSM_VIDC_SYS_ERROR);
		} else {
			msm_comm_generate_session_error(inst);
		}
		mutex_lock(&core->lock);
	}
	mutex_unlock(&core->lock);
	return;

err_sess_abort:
	msm_comm_clean_notify_client(core);
}

void msm_comm_handle_thermal_event(void)
{
	struct msm_vidc_core *core;

	list_for_each_entry(core, &vidc_driver->cores, list) {
		if (!is_thermal_permissible(core)) {
			dprintk(VIDC_ERR,
				"Thermal level critical, stop all active sessions!\n");
			handle_thermal_event(core);
		}
	}
}

int msm_comm_check_core_init(struct msm_vidc_core *core)
{
	int rc = 0;

	mutex_lock(&core->lock);
	if (core->state >= VIDC_CORE_INIT_DONE) {
		dprintk(VIDC_HIGH, "Video core: %d is already in state: %d\n",
				core->id, core->state);
		goto exit;
	}
	dprintk(VIDC_HIGH, "Waiting for SYS_INIT_DONE\n");
	rc = wait_for_completion_timeout(
		&core->completions[SYS_MSG_INDEX(HAL_SYS_INIT_DONE)],
		msecs_to_jiffies(core->resources.msm_vidc_hw_rsp_timeout));
	if (!rc) {
		dprintk(VIDC_ERR, "%s: Wait interrupted or timed out: %d\n",
				__func__, SYS_MSG_INDEX(HAL_SYS_INIT_DONE));
		rc = -EIO;
		goto exit;
	} else {
		core->state = VIDC_CORE_INIT_DONE;
		rc = 0;
	}
	dprintk(VIDC_HIGH, "SYS_INIT_DONE!!!\n");
exit:
	mutex_unlock(&core->lock);
	return rc;
}

static int msm_comm_init_core_done(struct msm_vidc_inst *inst)
{
	int rc = 0;

	rc = msm_comm_check_core_init(inst->core);
	if (rc) {
		dprintk(VIDC_ERR, "%s - failed to initialize core\n", __func__);
		msm_comm_generate_sys_error(inst);
		return rc;
	}
	change_inst_state(inst, MSM_VIDC_CORE_INIT_DONE);
	return rc;
}

static int msm_comm_init_core(struct msm_vidc_inst *inst)
{
	int rc, i;
	struct hfi_device *hdev;
	struct msm_vidc_core *core;

	if (!inst || !inst->core || !inst->core->device)
		return -EINVAL;

	core = inst->core;
	hdev = core->device;
	mutex_lock(&core->lock);
	if (core->state >= VIDC_CORE_INIT) {
		dprintk(VIDC_HIGH, "Video core: %d is already in state: %d\n",
				core->id, core->state);
		goto core_already_inited;
	}
	dprintk(VIDC_HIGH, "%s: core %pK\n", __func__, core);
	rc = call_hfi_op(hdev, core_init, hdev->hfi_device_data);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to init core, id = %d\n",
				core->id);
		goto fail_core_init;
	}

	/* initialize core while firmware processing SYS_INIT cmd */
	core->state = VIDC_CORE_INIT;
	core->smmu_fault_handled = false;
	core->trigger_ssr = false;
	core->resources.max_inst_count = MAX_SUPPORTED_INSTANCES;
	core->resources.max_secure_inst_count =
		core->resources.max_secure_inst_count ?
		core->resources.max_secure_inst_count :
		core->resources.max_inst_count;
	dprintk(VIDC_HIGH, "%s: codecs count %d, max inst count %d\n",
		__func__, core->resources.codecs_count,
		core->resources.max_inst_count);
	if (!core->resources.codecs || !core->resources.codecs_count) {
		dprintk(VIDC_ERR, "%s: invalid codecs\n", __func__);
		rc = -EINVAL;
		goto fail_core_init;
	}
	if (!core->capabilities) {
		core->capabilities = kcalloc(core->resources.codecs_count,
			sizeof(struct msm_vidc_capability), GFP_KERNEL);
		if (!core->capabilities) {
			dprintk(VIDC_ERR,
				"%s: failed to allocate capabilities\n",
				__func__);
			rc = -ENOMEM;
			goto fail_core_init;
		}
	} else {
		dprintk(VIDC_ERR,
			"%s: capabilities memory is expected to be freed\n",
			__func__);
	}
	for (i = 0; i < core->resources.codecs_count; i++) {
		core->capabilities[i].domain =
				core->resources.codecs[i].domain;
		core->capabilities[i].codec =
				core->resources.codecs[i].codec;
	}
	rc = msm_vidc_capabilities(core);
	if (rc) {
		dprintk(VIDC_ERR,
			 "%s: default capabilities failed\n", __func__);
		kfree(core->capabilities);
		core->capabilities = NULL;
		goto fail_core_init;
	}
	dprintk(VIDC_HIGH, "%s: done\n", __func__);
core_already_inited:
	change_inst_state(inst, MSM_VIDC_CORE_INIT);
	mutex_unlock(&core->lock);

	rc = msm_comm_scale_clocks_and_bus(inst, 1);
	return rc;

fail_core_init:
	core->state = VIDC_CORE_UNINIT;
	mutex_unlock(&core->lock);
	return rc;
}

static int msm_vidc_deinit_core(struct msm_vidc_inst *inst)
{
	struct msm_vidc_core *core;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	core = inst->core;
	hdev = core->device;

	mutex_lock(&core->lock);
	if (core->state == VIDC_CORE_UNINIT) {
		dprintk(VIDC_HIGH, "Video core: %d is already in state: %d\n",
				core->id, core->state);
		goto core_already_uninited;
	}
	mutex_unlock(&core->lock);

	msm_comm_scale_clocks_and_bus(inst, 1);

	mutex_lock(&core->lock);

	if (!core->resources.never_unload_fw) {
		cancel_delayed_work(&core->fw_unload_work);

		/*
		 * Delay unloading of firmware. This is useful
		 * in avoiding firmware download delays in cases where we
		 * will have a burst of back to back video playback sessions
		 * e.g. thumbnail generation.
		 */
		schedule_delayed_work(&core->fw_unload_work,
			msecs_to_jiffies(core->state == VIDC_CORE_INIT_DONE ?
			core->resources.msm_vidc_firmware_unload_delay : 0));

		dprintk(VIDC_HIGH, "firmware unload delayed by %u ms\n",
			core->state == VIDC_CORE_INIT_DONE ?
			core->resources.msm_vidc_firmware_unload_delay : 0);
	}

core_already_uninited:
	change_inst_state(inst, MSM_VIDC_CORE_UNINIT);
	mutex_unlock(&core->lock);
	return 0;
}

int msm_comm_force_cleanup(struct msm_vidc_inst *inst)
{
	msm_comm_kill_session(inst);
	return msm_vidc_deinit_core(inst);
}

static int msm_comm_session_init_done(int flipped_state,
	struct msm_vidc_inst *inst)
{
	int rc;

	dprintk(VIDC_HIGH, "inst %pK: waiting for session init done\n", inst);
	rc = wait_for_state(inst, flipped_state, MSM_VIDC_OPEN_DONE,
			HAL_SESSION_INIT_DONE);
	if (rc) {
		dprintk(VIDC_ERR, "Session init failed for inst %pK\n", inst);
		msm_comm_generate_sys_error(inst);
		return rc;
	}

	return rc;
}

static int msm_comm_session_init(int flipped_state,
	struct msm_vidc_inst *inst)
{
	int rc = 0;
	int fourcc = 0;
	struct hfi_device *hdev;
	struct v4l2_format *f;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	if (IS_ALREADY_IN_STATE(flipped_state, MSM_VIDC_OPEN)) {
		dprintk(VIDC_HIGH, "inst: %pK is already in state: %d\n",
						inst, inst->state);
		goto exit;
	}
	if (inst->session_type == MSM_VIDC_DECODER) {
		f = &inst->fmts[INPUT_PORT].v4l2_fmt;
		fourcc = f->fmt.pix_mp.pixelformat;
	} else if (inst->session_type == MSM_VIDC_ENCODER) {
		f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
		fourcc = f->fmt.pix_mp.pixelformat;
	} else if (inst->session_type == MSM_VIDC_CVP) {
		fourcc = V4L2_PIX_FMT_CVP;
	} else {
		dprintk(VIDC_ERR, "Invalid session\n");
		return -EINVAL;
	}

	rc = msm_comm_init_clocks_and_bus_data(inst);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to initialize clocks and bus data\n");
		goto exit;
	}

	dprintk(VIDC_HIGH, "%s: inst %pK\n", __func__, inst);
	rc = call_hfi_op(hdev, session_init, hdev->hfi_device_data,
			inst, get_hal_domain(inst->session_type),
			get_hal_codec(fourcc),
			&inst->session);

	if (rc || !inst->session) {
		dprintk(VIDC_ERR,
			"Failed to call session init for: %pK, %pK, %d, %d\n",
			inst->core->device, inst,
			inst->session_type, fourcc);
		rc = -EINVAL;
		goto exit;
	}

	rc = msm_vidc_calculate_buffer_counts(inst);
	if (rc) {
		dprintk(VIDC_ERR, "Failed to initialize buff counts\n");
		goto exit;
	}
	change_inst_state(inst, MSM_VIDC_OPEN);

exit:
	return rc;
}

static void msm_vidc_print_running_insts(struct msm_vidc_core *core)
{
	struct msm_vidc_inst *temp;
	int op_rate = 0;
	struct v4l2_format *out_f;
	struct v4l2_format *inp_f;

	dprintk(VIDC_ERR, "Running instances:\n");
	dprintk(VIDC_ERR, "%4s|%4s|%4s|%4s|%4s|%4s\n",
			"type", "w", "h", "fps", "opr", "prop");

	mutex_lock(&core->lock);
	list_for_each_entry(temp, &core->instances, list) {
		out_f = &temp->fmts[OUTPUT_PORT].v4l2_fmt;
		inp_f = &temp->fmts[INPUT_PORT].v4l2_fmt;
		if (temp->state >= MSM_VIDC_OPEN_DONE &&
				temp->state < MSM_VIDC_STOP_DONE) {
			char properties[4] = "";

			if (is_thumbnail_session(temp))
				strlcat(properties, "N", sizeof(properties));

			if (is_turbo_session(temp))
				strlcat(properties, "T", sizeof(properties));

			if (is_realtime_session(temp))
				strlcat(properties, "R", sizeof(properties));

			if (temp->clk_data.operating_rate)
				op_rate = temp->clk_data.operating_rate >> 16;
			else
				op_rate = temp->clk_data.frame_rate >> 16;

			dprintk(VIDC_ERR, "%4d|%4d|%4d|%4d|%4d|%4s\n",
					temp->session_type,
					max(out_f->fmt.pix_mp.width,
						inp_f->fmt.pix_mp.width),
					max(out_f->fmt.pix_mp.height,
						inp_f->fmt.pix_mp.height),
					temp->clk_data.frame_rate >> 16,
					op_rate, properties);
		}
	}
	mutex_unlock(&core->lock);
}

static int msm_vidc_load_resources(int flipped_state,
	struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;
	int num_mbs_per_sec = 0, max_load_adj = 0;
	struct msm_vidc_core *core;
	enum load_calc_quirks quirks = LOAD_ADMISSION_CONTROL;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	if (inst->state == MSM_VIDC_CORE_INVALID) {
		dprintk(VIDC_ERR,
			"%s: inst %pK is in invalid state\n", __func__, inst);
		return -EINVAL;
	}
	if (IS_ALREADY_IN_STATE(flipped_state, MSM_VIDC_LOAD_RESOURCES)) {
		dprintk(VIDC_HIGH, "inst: %pK is already in state: %d\n",
						inst, inst->state);
		goto exit;
	}
	core = inst->core;

	num_mbs_per_sec =
		msm_comm_get_device_load(core, MSM_VIDC_DECODER, quirks) +
		msm_comm_get_device_load(core, MSM_VIDC_ENCODER, quirks);

	max_load_adj = core->resources.max_load +
		inst->capability.cap[CAP_MBS_PER_FRAME].max;

	if (num_mbs_per_sec > max_load_adj) {
		dprintk(VIDC_ERR, "HW is overloaded, needed: %d max: %d\n",
			num_mbs_per_sec, max_load_adj);
		msm_vidc_print_running_insts(core);
		msm_comm_kill_session(inst);
		return -EBUSY;
	}

	hdev = core->device;
	dprintk(VIDC_HIGH, "%s: inst %pK\n", __func__, inst);
	rc = call_hfi_op(hdev, session_load_res, (void *) inst->session);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to send load resources\n");
		goto exit;
	}
	change_inst_state(inst, MSM_VIDC_LOAD_RESOURCES);
exit:
	return rc;
}

static int msm_vidc_start(int flipped_state, struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	if (inst->state == MSM_VIDC_CORE_INVALID) {
		dprintk(VIDC_ERR,
			"%s: inst %pK is in invalid\n", __func__, inst);
		return -EINVAL;
	}
	if (IS_ALREADY_IN_STATE(flipped_state, MSM_VIDC_START)) {
		dprintk(VIDC_HIGH,
			"inst: %pK is already in state: %d\n",
			inst, inst->state);
		goto exit;
	}
	hdev = inst->core->device;
	dprintk(VIDC_HIGH, "%s: inst %pK\n", __func__, inst);
	rc = call_hfi_op(hdev, session_start, (void *) inst->session);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to send start\n");
		goto exit;
	}
	change_inst_state(inst, MSM_VIDC_START);
exit:
	return rc;
}

static int msm_vidc_stop(int flipped_state, struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	if (inst->state == MSM_VIDC_CORE_INVALID) {
		dprintk(VIDC_ERR,
			"%s: inst %pK is in invalid state\n", __func__, inst);
		return -EINVAL;
	}
	if (IS_ALREADY_IN_STATE(flipped_state, MSM_VIDC_STOP)) {
		dprintk(VIDC_HIGH,
			"inst: %pK is already in state: %d\n",
			inst, inst->state);
		goto exit;
	}
	hdev = inst->core->device;
	dprintk(VIDC_HIGH, "%s: inst %pK\n", __func__, inst);
	rc = call_hfi_op(hdev, session_stop, (void *) inst->session);
	if (rc) {
		dprintk(VIDC_ERR, "%s: inst %pK session_stop failed\n",
				__func__, inst);
		goto exit;
	}
	change_inst_state(inst, MSM_VIDC_STOP);
exit:
	return rc;
}

static int msm_vidc_release_res(int flipped_state, struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	if (inst->state == MSM_VIDC_CORE_INVALID) {
		dprintk(VIDC_ERR,
			"%s: inst %pK is in invalid state\n", __func__, inst);
		return -EINVAL;
	}
	if (IS_ALREADY_IN_STATE(flipped_state, MSM_VIDC_RELEASE_RESOURCES)) {
		dprintk(VIDC_HIGH,
			"inst: %pK is already in state: %d\n",
			inst, inst->state);
		goto exit;
	}
	hdev = inst->core->device;
	dprintk(VIDC_HIGH, "%s: inst %pK\n", __func__, inst);
	rc = call_hfi_op(hdev, session_release_res, (void *) inst->session);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to send release resources\n");
		goto exit;
	}
	change_inst_state(inst, MSM_VIDC_RELEASE_RESOURCES);
exit:
	return rc;
}

static int msm_comm_session_close(int flipped_state,
			struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid params\n", __func__);
		return -EINVAL;
	}
	if (IS_ALREADY_IN_STATE(flipped_state, MSM_VIDC_CLOSE)) {
		dprintk(VIDC_HIGH,
			"inst: %pK is already in state: %d\n",
						inst, inst->state);
		goto exit;
	}
	hdev = inst->core->device;
	dprintk(VIDC_HIGH, "%s: inst %pK\n", __func__, inst);
	rc = call_hfi_op(hdev, session_end, (void *) inst->session);
	if (rc) {
		dprintk(VIDC_ERR,
			"Failed to send close\n");
		goto exit;
	}
	change_inst_state(inst, MSM_VIDC_CLOSE);
exit:
	return rc;
}

int msm_comm_suspend(int core_id)
{
	struct hfi_device *hdev;
	struct msm_vidc_core *core;
	int rc = 0;

	core = get_vidc_core(core_id);
	if (!core) {
		dprintk(VIDC_ERR,
			"%s: Failed to find core for core_id = %d\n",
			__func__, core_id);
		return -EINVAL;
	}

	hdev = (struct hfi_device *)core->device;
	if (!hdev) {
		dprintk(VIDC_ERR, "%s Invalid device handle\n", __func__);
		return -EINVAL;
	}

	rc = call_hfi_op(hdev, suspend, hdev->hfi_device_data);
	if (rc)
		dprintk(VIDC_ERR, "Failed to suspend\n");

	return rc;
}

static int get_flipped_state(int present_state,
	int desired_state)
{
	int flipped_state = present_state;

	if (flipped_state < MSM_VIDC_STOP
			&& desired_state > MSM_VIDC_STOP) {
		flipped_state = MSM_VIDC_STOP + (MSM_VIDC_STOP - flipped_state);
		flipped_state &= 0xFFFE;
		flipped_state = flipped_state - 1;
	} else if (flipped_state > MSM_VIDC_STOP
			&& desired_state < MSM_VIDC_STOP) {
		flipped_state = MSM_VIDC_STOP -
			(flipped_state - MSM_VIDC_STOP + 1);
		flipped_state &= 0xFFFE;
		flipped_state = flipped_state - 1;
	}
	return flipped_state;
}

int msm_comm_reset_bufreqs(struct msm_vidc_inst *inst, enum hal_buffer buf_type)
{
	struct hal_buffer_requirements *bufreqs;

	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	bufreqs = get_buff_req_buffer(inst, buf_type);
	if (!bufreqs) {
		dprintk(VIDC_ERR, "%s: invalid buf type %d\n",
			__func__, buf_type);
		return -EINVAL;
	}
	bufreqs->buffer_size = bufreqs->buffer_region_size =
	bufreqs->buffer_count_min = bufreqs->buffer_count_min_host =
	bufreqs->buffer_count_actual = bufreqs->contiguous =
	bufreqs->buffer_alignment = 0;

	return 0;
}

struct hal_buffer_requirements *get_buff_req_buffer(
		struct msm_vidc_inst *inst, enum hal_buffer buffer_type)
{
	int i;

	for (i = 0; i < HAL_BUFFER_MAX; i++) {
		if (inst->buff_req.buffer[i].buffer_type == buffer_type)
			return &inst->buff_req.buffer[i];
	}
	dprintk(VIDC_ERR, "Failed to get buff req for : %x", buffer_type);
	return NULL;
}

u32 msm_comm_convert_color_fmt(u32 v4l2_fmt)
{
	switch (v4l2_fmt) {
	case V4L2_PIX_FMT_NV12:
		return COLOR_FMT_NV12;
	case V4L2_PIX_FMT_NV21:
		return COLOR_FMT_NV21;
	case V4L2_PIX_FMT_NV12_512:
		return COLOR_FMT_NV12_512;
	case V4L2_PIX_FMT_SDE_Y_CBCR_H2V2_P010_VENUS:
		return COLOR_FMT_P010;
	case V4L2_PIX_FMT_NV12_UBWC:
		return COLOR_FMT_NV12_UBWC;
	case V4L2_PIX_FMT_NV12_TP10_UBWC:
		return COLOR_FMT_NV12_BPP10_UBWC;
	default:
		dprintk(VIDC_ERR,
			"Invalid v4l2 color fmt FMT : %x, Set default(NV12)",
			v4l2_fmt);
		return COLOR_FMT_NV12;
	}
}

static u32 get_hfi_buffer(int hal_buffer)
{
	u32 buffer;

	switch (hal_buffer) {
	case HAL_BUFFER_INPUT:
		buffer = HFI_BUFFER_INPUT;
		break;
	case HAL_BUFFER_OUTPUT:
		buffer = HFI_BUFFER_OUTPUT;
		break;
	case HAL_BUFFER_OUTPUT2:
		buffer = HFI_BUFFER_OUTPUT2;
		break;
	case HAL_BUFFER_EXTRADATA_INPUT:
		buffer = HFI_BUFFER_EXTRADATA_INPUT;
		break;
	case HAL_BUFFER_EXTRADATA_OUTPUT:
		buffer = HFI_BUFFER_EXTRADATA_OUTPUT;
		break;
	case HAL_BUFFER_EXTRADATA_OUTPUT2:
		buffer = HFI_BUFFER_EXTRADATA_OUTPUT2;
		break;
	case HAL_BUFFER_INTERNAL_SCRATCH:
		buffer = HFI_BUFFER_COMMON_INTERNAL_SCRATCH;
		break;
	case HAL_BUFFER_INTERNAL_SCRATCH_1:
		buffer = HFI_BUFFER_COMMON_INTERNAL_SCRATCH_1;
		break;
	case HAL_BUFFER_INTERNAL_SCRATCH_2:
		buffer = HFI_BUFFER_COMMON_INTERNAL_SCRATCH_2;
		break;
	case HAL_BUFFER_INTERNAL_PERSIST:
		buffer = HFI_BUFFER_INTERNAL_PERSIST;
		break;
	case HAL_BUFFER_INTERNAL_PERSIST_1:
		buffer = HFI_BUFFER_INTERNAL_PERSIST_1;
		break;
	default:
		dprintk(VIDC_ERR, "Invalid buffer: %#x\n",
			hal_buffer);
		buffer = 0;
		break;
	}
	return buffer;
}

static int set_dpb_only_buffers(struct msm_vidc_inst *inst,
	enum hal_buffer buffer_type)
{
	int rc = 0;
	struct internal_buf *binfo = NULL;
	u32 smem_flags = SMEM_UNCACHED, buffer_size, num_buffers, hfi_fmt;
	struct msm_vidc_format *fmt;
	unsigned int i;
	struct hfi_device *hdev;
	struct hfi_buffer_size_minimum b;
	struct v4l2_format *f;

	hdev = inst->core->device;

	fmt = &inst->fmts[OUTPUT_PORT];

	/* For DPB buffers, Always use min count */
	num_buffers = fmt->count_min;
	hfi_fmt = msm_comm_convert_color_fmt(inst->clk_data.dpb_fourcc);
	f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
	buffer_size = VENUS_BUFFER_SIZE(hfi_fmt, f->fmt.pix_mp.width,
			f->fmt.pix_mp.height);
	dprintk(VIDC_HIGH,
		"output: num = %d, size = %d\n",
		num_buffers,
		buffer_size);

	b.buffer_type = get_hfi_buffer(buffer_type);
	if (!b.buffer_type)
		return -EINVAL;
	b.buffer_size = buffer_size;
	rc = call_hfi_op(hdev, session_set_property,
		inst->session, HFI_PROPERTY_PARAM_BUFFER_SIZE_MINIMUM,
		&b, sizeof(b));

	if (f->fmt.pix_mp.num_planes == 1 ||
		!f->fmt.pix_mp.plane_fmt[1].sizeimage) {
		dprintk(VIDC_HIGH,
			"This extradata buffer not required, buffer_type: %x\n",
			buffer_type);
	} else {
		dprintk(VIDC_HIGH,
			"extradata: num = 1, size = %d\n",
			f->fmt.pix_mp.plane_fmt[1].sizeimage);
		inst->dpb_extra_binfo = NULL;
		inst->dpb_extra_binfo = kzalloc(sizeof(*binfo), GFP_KERNEL);
		if (!inst->dpb_extra_binfo) {
			dprintk(VIDC_ERR, "Out of memory\n");
			rc = -ENOMEM;
			goto fail_kzalloc;
		}
		rc = msm_comm_smem_alloc(inst,
			f->fmt.pix_mp.plane_fmt[1].sizeimage, 1, smem_flags,
			buffer_type, 0, &inst->dpb_extra_binfo->smem);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to allocate output memory\n");
			goto err_no_mem;
		}
	}

	if (inst->flags & VIDC_SECURE)
		smem_flags |= SMEM_SECURE;

	if (buffer_size) {
		for (i = 0; i < num_buffers; i++) {
			binfo = kzalloc(sizeof(*binfo), GFP_KERNEL);
			if (!binfo) {
				dprintk(VIDC_ERR, "Out of memory\n");
				rc = -ENOMEM;
				goto fail_kzalloc;
			}
			rc = msm_comm_smem_alloc(inst,
					buffer_size, 1, smem_flags,
					buffer_type, 0, &binfo->smem);
			if (rc) {
				dprintk(VIDC_ERR,
					"Failed to allocate output memory\n");
				goto err_no_mem;
			}
			binfo->buffer_type = buffer_type;
			binfo->buffer_ownership = DRIVER;
			dprintk(VIDC_HIGH, "Output buffer address: %#x\n",
					binfo->smem.device_addr);

			if (inst->buffer_mode_set[OUTPUT_PORT] ==
				HAL_BUFFER_MODE_STATIC) {
				struct vidc_buffer_addr_info buffer_info = {0};

				buffer_info.buffer_size = buffer_size;
				buffer_info.buffer_type = buffer_type;
				buffer_info.num_buffers = 1;
				buffer_info.align_device_addr =
					binfo->smem.device_addr;
				buffer_info.extradata_addr =
				inst->dpb_extra_binfo->smem.device_addr;
				buffer_info.extradata_size =
					inst->dpb_extra_binfo->smem.size;
				rc = call_hfi_op(hdev, session_set_buffers,
					(void *) inst->session, &buffer_info);
				if (rc) {
					dprintk(VIDC_ERR,
						"%s : session_set_buffers failed\n",
						__func__);
					goto fail_set_buffers;
				}
			}
			mutex_lock(&inst->outputbufs.lock);
			list_add_tail(&binfo->list, &inst->outputbufs.list);
			mutex_unlock(&inst->outputbufs.lock);
		}
	}
	return rc;
fail_set_buffers:
	msm_comm_smem_free(inst, &binfo->smem);
err_no_mem:
	kfree(binfo);
fail_kzalloc:
	return rc;
}

static inline char *get_buffer_name(enum hal_buffer buffer_type)
{
	switch (buffer_type) {
	case HAL_BUFFER_INPUT: return "input";
	case HAL_BUFFER_OUTPUT: return "output";
	case HAL_BUFFER_OUTPUT2: return "output_2";
	case HAL_BUFFER_EXTRADATA_INPUT: return "input_extra";
	case HAL_BUFFER_EXTRADATA_OUTPUT: return "output_extra";
	case HAL_BUFFER_EXTRADATA_OUTPUT2: return "output2_extra";
	case HAL_BUFFER_INTERNAL_SCRATCH: return "scratch";
	case HAL_BUFFER_INTERNAL_SCRATCH_1: return "scratch_1";
	case HAL_BUFFER_INTERNAL_SCRATCH_2: return "scratch_2";
	case HAL_BUFFER_INTERNAL_PERSIST: return "persist";
	case HAL_BUFFER_INTERNAL_PERSIST_1: return "persist_1";
	case HAL_BUFFER_INTERNAL_CMD_QUEUE: return "queue";
	default: return "????";
	}
}

static int set_internal_buf_on_fw(struct msm_vidc_inst *inst,
				enum hal_buffer buffer_type,
				struct msm_smem *handle, bool reuse)
{
	struct vidc_buffer_addr_info buffer_info;
	struct hfi_device *hdev;
	int rc = 0;

	if (!inst || !inst->core || !inst->core->device || !handle) {
		dprintk(VIDC_ERR, "%s - invalid params\n", __func__);
		return -EINVAL;
	}

	hdev = inst->core->device;

	buffer_info.buffer_size = handle->size;
	buffer_info.buffer_type = buffer_type;
	buffer_info.num_buffers = 1;
	buffer_info.align_device_addr = handle->device_addr;
	dprintk(VIDC_HIGH, "%s %s buffer : %x\n",
				reuse ? "Reusing" : "Allocated",
				get_buffer_name(buffer_type),
				buffer_info.align_device_addr);

	rc = call_hfi_op(hdev, session_set_buffers,
		(void *) inst->session, &buffer_info);
	if (rc) {
		dprintk(VIDC_ERR,
			"vidc_hal_session_set_buffers failed\n");
		return rc;
	}
	return 0;
}

static bool reuse_internal_buffers(struct msm_vidc_inst *inst,
		enum hal_buffer buffer_type, struct msm_vidc_list *buf_list)
{
	struct internal_buf *buf;
	int rc = 0;
	bool reused = false;

	if (!inst || !buf_list) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return false;
	}

	mutex_lock(&buf_list->lock);
	list_for_each_entry(buf, &buf_list->list, list) {
		if (buf->buffer_type != buffer_type)
			continue;

		/*
		 * Persist buffer size won't change with resolution. If they
		 * are in queue means that they are already allocated and
		 * given to HW. HW can use them without reallocation. These
		 * buffers are not released as part of port reconfig. So
		 * driver no need to set them again.
		 */

		if (buffer_type != HAL_BUFFER_INTERNAL_PERSIST
			&& buffer_type != HAL_BUFFER_INTERNAL_PERSIST_1) {

			rc = set_internal_buf_on_fw(inst, buffer_type,
					&buf->smem, true);
			if (rc) {
				dprintk(VIDC_ERR,
					"%s: session_set_buffers failed\n",
					__func__);
				reused = false;
				break;
			}
		}
		reused = true;
		dprintk(VIDC_HIGH,
			"Re-using internal buffer type : %d\n", buffer_type);
	}
	mutex_unlock(&buf_list->lock);
	return reused;
}

static int allocate_and_set_internal_bufs(struct msm_vidc_inst *inst,
			struct hal_buffer_requirements *internal_bufreq,
			struct msm_vidc_list *buf_list)
{
	struct internal_buf *binfo;
	u32 smem_flags = SMEM_UNCACHED;
	int rc = 0;
	unsigned int i = 0;

	if (!inst || !internal_bufreq || !buf_list)
		return -EINVAL;

	if (!internal_bufreq->buffer_size)
		return 0;

	if (inst->flags & VIDC_SECURE)
		smem_flags |= SMEM_SECURE;

	for (i = 0; i < internal_bufreq->buffer_count_actual; i++) {
		binfo = kzalloc(sizeof(*binfo), GFP_KERNEL);
		if (!binfo) {
			dprintk(VIDC_ERR, "Out of memory\n");
			rc = -ENOMEM;
			goto fail_kzalloc;
		}
		rc = msm_comm_smem_alloc(inst, internal_bufreq->buffer_size,
				1, smem_flags, internal_bufreq->buffer_type,
				0, &binfo->smem);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to allocate scratch memory\n");
			goto err_no_mem;
		}

		binfo->buffer_type = internal_bufreq->buffer_type;

		rc = set_internal_buf_on_fw(inst, internal_bufreq->buffer_type,
				&binfo->smem, false);
		if (rc)
			goto fail_set_buffers;

		mutex_lock(&buf_list->lock);
		list_add_tail(&binfo->list, &buf_list->list);
		mutex_unlock(&buf_list->lock);
	}
	return rc;

fail_set_buffers:
	msm_comm_smem_free(inst, &binfo->smem);
err_no_mem:
	kfree(binfo);
fail_kzalloc:
	return rc;

}

static int set_internal_buffers(struct msm_vidc_inst *inst,
	enum hal_buffer buffer_type, struct msm_vidc_list *buf_list)
{
	struct hal_buffer_requirements *internal_buf;

	internal_buf = get_buff_req_buffer(inst, buffer_type);
	if (!internal_buf) {
		dprintk(VIDC_HIGH,
			"This internal buffer not required, buffer_type: %x\n",
			buffer_type);
		return 0;
	}

	dprintk(VIDC_HIGH, "Buffer type %s: num = %d, size = %d\n",
		get_buffer_name(buffer_type),
		internal_buf->buffer_count_actual, internal_buf->buffer_size);

	/*
	 * Try reusing existing internal buffers first.
	 * If it's not possible to reuse, allocate new buffers.
	 */
	if (reuse_internal_buffers(inst, buffer_type, buf_list))
		return 0;

	return allocate_and_set_internal_bufs(inst, internal_buf,
				buf_list);
}

int msm_comm_try_state(struct msm_vidc_inst *inst, int state)
{
	int rc = 0;
	int flipped_state;

	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid params %pK", __func__, inst);
		return -EINVAL;
	}
	dprintk(VIDC_HIGH,
		"Trying to move inst: %pK (%#x) from: %#x to %#x\n",
		inst, hash32_ptr(inst->session), inst->state, state);

	mutex_lock(&inst->sync_lock);
	if (inst->state == MSM_VIDC_CORE_INVALID) {
		dprintk(VIDC_ERR, "%s: inst %pK is in invalid\n",
			__func__, inst);
		rc = -EINVAL;
		goto exit;
	}

	flipped_state = get_flipped_state(inst->state, state);
	dprintk(VIDC_HIGH,
		"inst: %pK (%#x) flipped_state = %#x\n",
		inst, hash32_ptr(inst->session), flipped_state);
	switch (flipped_state) {
	case MSM_VIDC_CORE_UNINIT_DONE:
	case MSM_VIDC_CORE_INIT:
		rc = msm_comm_init_core(inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_VIDC_CORE_INIT_DONE:
		rc = msm_comm_init_core_done(inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_VIDC_OPEN:
		rc = msm_comm_session_init(flipped_state, inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_VIDC_OPEN_DONE:
		rc = msm_comm_session_init_done(flipped_state, inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_VIDC_LOAD_RESOURCES:
		rc = msm_vidc_load_resources(flipped_state, inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_VIDC_LOAD_RESOURCES_DONE:
	case MSM_VIDC_START:
		rc = msm_vidc_start(flipped_state, inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_VIDC_START_DONE:
		rc = wait_for_state(inst, flipped_state, MSM_VIDC_START_DONE,
				HAL_SESSION_START_DONE);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_VIDC_STOP:
		rc = msm_vidc_stop(flipped_state, inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_VIDC_STOP_DONE:
		rc = wait_for_state(inst, flipped_state, MSM_VIDC_STOP_DONE,
				HAL_SESSION_STOP_DONE);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
		dprintk(VIDC_HIGH, "Moving to Stop Done state\n");
	case MSM_VIDC_RELEASE_RESOURCES:
		rc = msm_vidc_release_res(flipped_state, inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_VIDC_RELEASE_RESOURCES_DONE:
		rc = wait_for_state(inst, flipped_state,
			MSM_VIDC_RELEASE_RESOURCES_DONE,
			HAL_SESSION_RELEASE_RESOURCE_DONE);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
		dprintk(VIDC_HIGH,
				"Moving to release resources done state\n");
	case MSM_VIDC_CLOSE:
		rc = msm_comm_session_close(flipped_state, inst);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
	case MSM_VIDC_CLOSE_DONE:
		rc = wait_for_state(inst, flipped_state, MSM_VIDC_CLOSE_DONE,
				HAL_SESSION_END_DONE);
		if (rc || state <= get_flipped_state(inst->state, state))
			break;
		msm_comm_session_clean(inst);
	case MSM_VIDC_CORE_UNINIT:
	case MSM_VIDC_CORE_INVALID:
		dprintk(VIDC_HIGH, "Sending core uninit\n");
		rc = msm_vidc_deinit_core(inst);
		if (rc || state == get_flipped_state(inst->state, state))
			break;
	default:
		dprintk(VIDC_ERR, "State not recognized\n");
		rc = -EINVAL;
		break;
	}

exit:
	mutex_unlock(&inst->sync_lock);

	if (rc) {
		dprintk(VIDC_ERR,
				"Failed to move from state: %d to %d\n",
				inst->state, state);
		msm_comm_kill_session(inst);
	} else {
		trace_msm_vidc_common_state_change((void *)inst,
				inst->state, state);
	}
	return rc;
}

int msm_vidc_send_pending_eos_buffers(struct msm_vidc_inst *inst)
{
	struct vidc_frame_data data = {0};
	struct hfi_device *hdev;
	struct eos_buf *binfo = NULL, *temp = NULL;
	int rc = 0;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s: Invalid arguments\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&inst->eosbufs.lock);
	list_for_each_entry_safe(binfo, temp, &inst->eosbufs.list, list) {
		data.alloc_len = binfo->smem.size;
		data.device_addr = binfo->smem.device_addr;
		data.input_tag = 0;
		data.buffer_type = HAL_BUFFER_INPUT;
		data.filled_len = 0;
		data.offset = 0;
		data.flags = HAL_BUFFERFLAG_EOS;
		data.timestamp = 0;
		data.extradata_addr = data.device_addr;
		data.extradata_size = 0;
		dprintk(VIDC_HIGH, "Queueing EOS buffer 0x%x\n",
				data.device_addr);
		hdev = inst->core->device;

		rc = call_hfi_op(hdev, session_etb, inst->session,
				&data);
	}
	mutex_unlock(&inst->eosbufs.lock);

	return rc;
}

int msm_vidc_comm_cmd(void *instance, union msm_v4l2_cmd *cmd)
{
	struct msm_vidc_inst *inst = instance;
	struct v4l2_decoder_cmd *dec = NULL;
	struct v4l2_encoder_cmd *enc = NULL;
	struct msm_vidc_core *core;
	int which_cmd = 0, flags = 0, rc = 0;

	if (!inst || !inst->core || !cmd) {
		dprintk(VIDC_ERR, "%s invalid params\n", __func__);
		return -EINVAL;
	}
	core = inst->core;
	if (inst->session_type == MSM_VIDC_ENCODER) {
		enc = (struct v4l2_encoder_cmd *)cmd;
		which_cmd = enc->cmd;
		flags = enc->flags;
	} else if (inst->session_type == MSM_VIDC_DECODER) {
		dec = (struct v4l2_decoder_cmd *)cmd;
		which_cmd = dec->cmd;
		flags = dec->flags;
	}


	switch (which_cmd) {
	case V4L2_CMD_FLUSH:
		rc = msm_comm_flush(inst, flags);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed to flush buffers: %d\n", rc);
		}
		break;
	case V4L2_CMD_SESSION_CONTINUE:
	{
		rc = msm_comm_session_continue(inst);
		break;
	}
	/* This case also for V4L2_ENC_CMD_STOP */
	case V4L2_DEC_CMD_STOP:
	{
		struct eos_buf *binfo = NULL;
		u32 smem_flags = SMEM_UNCACHED;

		if (inst->state != MSM_VIDC_START_DONE) {
			dprintk(VIDC_HIGH,
				"Inst = %pK is not ready for EOS\n", inst);
			break;
		}

		binfo = kzalloc(sizeof(*binfo), GFP_KERNEL);
		if (!binfo) {
			dprintk(VIDC_ERR, "%s: Out of memory\n", __func__);
			rc = -ENOMEM;
			break;
		}

		if (inst->flags & VIDC_SECURE)
			smem_flags |= SMEM_SECURE;

		rc = msm_comm_smem_alloc(inst,
				SZ_4K, 1, smem_flags,
				HAL_BUFFER_INPUT, 0, &binfo->smem);
		if (rc) {
			kfree(binfo);
			dprintk(VIDC_ERR,
				"Failed to allocate output memory\n");
			rc = -ENOMEM;
			break;
		}

		mutex_lock(&inst->eosbufs.lock);
		list_add_tail(&binfo->list, &inst->eosbufs.list);
		mutex_unlock(&inst->eosbufs.lock);

		rc = msm_vidc_send_pending_eos_buffers(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed pending_eos_buffers sending\n");
			list_del(&binfo->list);
			kfree(binfo);
			break;
		}
		break;
	}
	default:
		dprintk(VIDC_ERR, "Unknown Command %d\n", which_cmd);
		rc = -ENOTSUPP;
		break;
	}
	return rc;
}

static int msm_comm_preprocess(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf)
{
	int rc = 0;

	if (!inst || !mbuf) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}

	/* preprocessing is allowed for encoder input buffer only */
	if (!is_encode_session(inst) || mbuf->vvb.vb2_buf.type != INPUT_MPLANE)
		return 0;

	/* preprocessing is done using CVP module only */
	if (!is_vidc_cvp_enabled(inst))
		return 0;

	rc = msm_vidc_cvp_preprocess(inst, mbuf);
	if (rc) {
		dprintk(VIDC_ERR, "%s: cvp preprocess failed\n",
			__func__);
		return rc;
	}

	return rc;
}

static void populate_frame_data(struct vidc_frame_data *data,
		struct msm_vidc_buffer *mbuf, struct msm_vidc_inst *inst)
{
	u64 time_usec;
	struct v4l2_format *f = NULL;
	struct vb2_buffer *vb;
	struct vb2_v4l2_buffer *vbuf;
	u32 itag = 0, itag2 = 0;

	if (!inst || !mbuf || !data) {
		dprintk(VIDC_ERR, "%s: invalid params %pK %pK %pK\n",
			__func__, inst, mbuf, data);
		return;
	}

	vb = &mbuf->vvb.vb2_buf;
	vbuf = to_vb2_v4l2_buffer(vb);

	time_usec = vb->timestamp;
	do_div(time_usec, NSEC_PER_USEC);

	data->alloc_len = vb->planes[0].length;
	data->device_addr = mbuf->smem[0].device_addr;
	data->timestamp = time_usec;
	data->flags = 0;
	data->input_tag = 0;

	if (vb->type == INPUT_MPLANE) {
		data->buffer_type = HAL_BUFFER_INPUT;
		data->filled_len = vb->planes[0].bytesused;
		data->offset = vb->planes[0].data_offset;

		if (vbuf->flags & V4L2_BUF_FLAG_EOS)
			data->flags |= HAL_BUFFERFLAG_EOS;

		if (vbuf->flags & V4L2_BUF_FLAG_CODECCONFIG)
			data->flags |= HAL_BUFFERFLAG_CODECCONFIG;

		if(msm_vidc_cvp_usage && (vbuf->flags & V4L2_BUF_FLAG_CVPMETADATA_SKIP))
			data->flags |= HAL_BUFFERFLAG_CVPMETADATA_SKIP;

		msm_comm_fetch_input_tag(&inst->etb_data, vb->index,
			&itag, &itag2);
		data->input_tag = itag;

		f = &inst->fmts[INPUT_PORT].v4l2_fmt;
	} else if (vb->type == OUTPUT_MPLANE) {
		data->buffer_type = msm_comm_get_hal_output_buffer(inst);
		f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
	}

	if (f && f->fmt.pix_mp.num_planes > 1) {
		data->extradata_addr = mbuf->smem[1].device_addr;
		data->extradata_size = vb->planes[1].length;
		data->flags |= HAL_BUFFERFLAG_EXTRADATA;
	}
}

enum hal_buffer get_hal_buffer_type(unsigned int type,
		unsigned int plane_num)
{
	if (type == INPUT_MPLANE) {
		if (plane_num == 0)
			return HAL_BUFFER_INPUT;
		else
			return HAL_BUFFER_EXTRADATA_INPUT;
	} else if (type == OUTPUT_MPLANE) {
		if (plane_num == 0)
			return HAL_BUFFER_OUTPUT;
		else
			return HAL_BUFFER_EXTRADATA_OUTPUT;
	} else {
		return -EINVAL;
	}
}

int msm_comm_num_queued_bufs(struct msm_vidc_inst *inst, u32 type)
{
	int count = 0;
	struct msm_vidc_buffer *mbuf;

	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return 0;
	}

	mutex_lock(&inst->registeredbufs.lock);
	list_for_each_entry(mbuf, &inst->registeredbufs.list, list) {
		if (mbuf->vvb.vb2_buf.type != type)
			continue;
		if (!(mbuf->flags & MSM_VIDC_FLAG_QUEUED))
			continue;
		count++;
	}
	mutex_unlock(&inst->registeredbufs.lock);

	return count;
}

static int num_pending_qbufs(struct msm_vidc_inst *inst, u32 type)
{
	int count = 0;
	struct msm_vidc_buffer *mbuf;

	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return 0;
	}

	mutex_lock(&inst->registeredbufs.lock);
	list_for_each_entry(mbuf, &inst->registeredbufs.list, list) {
		if (mbuf->vvb.vb2_buf.type != type)
			continue;
		/* Count only deferred buffers */
		if (!(mbuf->flags & MSM_VIDC_FLAG_DEFERRED))
			continue;
		count++;
	}
	mutex_unlock(&inst->registeredbufs.lock);

	return count;
}

static int msm_comm_qbuf_to_hfi(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf)
{
	int rc = 0;
	struct hfi_device *hdev;
	enum msm_vidc_debugfs_event e;
	struct vidc_frame_data frame_data = {0};

	if (!inst || !inst->core || !inst->core->device || !mbuf) {
		dprintk(VIDC_ERR, "%s: Invalid arguments\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	populate_frame_data(&frame_data, mbuf, inst);
	/* mbuf is not deferred anymore */
	mbuf->flags &= ~MSM_VIDC_FLAG_DEFERRED;

	if (mbuf->vvb.vb2_buf.type == INPUT_MPLANE) {
		e = MSM_VIDC_DEBUGFS_EVENT_ETB;
		rc = call_hfi_op(hdev, session_etb, inst->session, &frame_data);
	} else if (mbuf->vvb.vb2_buf.type == OUTPUT_MPLANE) {
		e = MSM_VIDC_DEBUGFS_EVENT_FTB;
		rc = call_hfi_op(hdev, session_ftb, inst->session, &frame_data);
	} else {
		dprintk(VIDC_ERR, "%s: invalid qbuf type %d:\n", __func__,
			mbuf->vvb.vb2_buf.type);
		rc = -EINVAL;
	}
	if (rc) {
		dprintk(VIDC_ERR, "%s: Failed to qbuf: %d\n", __func__, rc);
		goto err_bad_input;
	}
	mbuf->flags |= MSM_VIDC_FLAG_QUEUED;
	msm_vidc_debugfs_update(inst, e);

	if (mbuf->vvb.vb2_buf.type == INPUT_MPLANE &&
			is_decode_session(inst))
		rc = msm_comm_check_window_bitrate(inst, &frame_data);

err_bad_input:
	return rc;
}

void msm_vidc_batch_handler(struct work_struct *work)
{
	int rc = 0;
	struct msm_vidc_inst *inst;

	inst = container_of(work, struct msm_vidc_inst, batch_work.work);

	inst = get_inst(get_vidc_core(MSM_VIDC_CORE_VENUS), inst);
	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return;
	}

	if (inst->state == MSM_VIDC_CORE_INVALID) {
		dprintk(VIDC_ERR, "%s: invalid state\n", __func__);
		goto exit;
	}

	dprintk(VIDC_HIGH, "%s: %x: queue pending batch buffers\n",
		__func__, hash32_ptr(inst->session));

	rc = msm_comm_qbufs_batch(inst, NULL);
	if (rc)
		dprintk(VIDC_ERR, "%s: batch qbufs failed\n", __func__);

exit:
	put_inst(inst);
}

static int msm_comm_qbuf_superframe_to_hfi(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf)
{
	int rc, i;
	struct hfi_device *hdev;
	struct v4l2_format *f;
	struct v4l2_ctrl *ctrl;
	u64 ts_delta_us;
	struct vidc_frame_data *frames;
	u32 num_etbs, superframe_count, frame_size, hfi_fmt;
	bool skip_allowed = false;

	if (!inst || !inst->core || !inst->core->device || !mbuf) {
		dprintk(VIDC_ERR, "%s: Invalid arguments\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;
	frames = inst->superframe_data;

	if (!is_input_buffer(mbuf))
		return msm_comm_qbuf_to_hfi(inst, mbuf);

	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_SUPERFRAME);
	superframe_count = ctrl->val;
	if (superframe_count > VIDC_SUPERFRAME_MAX) {
		dprintk(VIDC_ERR, "%s: wrong superframe count %d, max %d\n",
			__func__, superframe_count, VIDC_SUPERFRAME_MAX);
		return -EINVAL;
	}

	ts_delta_us = 1000000 / (inst->clk_data.frame_rate >> 16);
	f = &inst->fmts[INPUT_PORT].v4l2_fmt;
	hfi_fmt = msm_comm_convert_color_fmt(f->fmt.pix_mp.pixelformat);
	frame_size = VENUS_BUFFER_SIZE(hfi_fmt, f->fmt.pix_mp.width,
			f->fmt.pix_mp.height);
	if (frame_size * superframe_count !=
		mbuf->vvb.vb2_buf.planes[0].length) {
		dprintk(VIDC_ERR,
			"%s: %#x : invalid superframe length, pxlfmt %#x wxh %dx%d framesize %d count %d length %d\n",
			__func__, hash32_ptr(inst->session),
			f->fmt.pix_mp.pixelformat, f->fmt.pix_mp.width,
			f->fmt.pix_mp.height, frame_size, superframe_count,
			mbuf->vvb.vb2_buf.planes[0].length);
		return -EINVAL;
	}

	num_etbs = 0;
	populate_frame_data(&frames[0], mbuf, inst);
	/* prepare superframe buffers */
	frames[0].filled_len = frame_size;
	/*
	 * superframe logic updates extradata, cvpmetadata_skip and eos flags only,
	 * so ensure no other flags are populated in populate_frame_data()
	 */
	frames[0].flags &= ~HAL_BUFFERFLAG_EXTRADATA;
	frames[0].flags &= ~HAL_BUFFERFLAG_EOS;
	frames[0].flags &= ~HAL_BUFFERFLAG_CVPMETADATA_SKIP;
	if (frames[0].flags)
		dprintk(VIDC_ERR, "%s: invalid flags %#x\n",
			__func__, frames[0].flags);
	frames[0].flags = 0;

	/* Add skip flag only if CVP metadata is enabled */
	if (inst->prop.extradata_ctrls & EXTRADATA_ENC_INPUT_CVP) {
		skip_allowed = true;
		frames[0].flags |= HAL_BUFFERFLAG_CVPMETADATA_SKIP;
	}

	for (i = 0; i < superframe_count; i++) {
		if (i)
			memcpy(&frames[i], &frames[0],
				sizeof(struct vidc_frame_data));
		frames[i].offset += i * frame_size;
		frames[i].timestamp += i * ts_delta_us;
		if (!i) {
			/* first frame */
			if (frames[0].extradata_addr)
				frames[0].flags |= HAL_BUFFERFLAG_EXTRADATA;
		} else if (i == superframe_count - 1) {
			/* last frame */
			if (mbuf->vvb.flags & V4L2_BUF_FLAG_EOS)
				frames[i].flags |= HAL_BUFFERFLAG_EOS;
		}
		num_etbs++;
	}

	/* If cvp metadata is enabled and metadata is available,
	 * do not add skip flag for only first frame */
	if (skip_allowed && !(mbuf->vvb.flags & V4L2_BUF_FLAG_CVPMETADATA_SKIP))
		frames[0].flags &= ~HAL_BUFFERFLAG_CVPMETADATA_SKIP;

	rc = call_hfi_op(hdev, session_process_batch, inst->session,
			num_etbs, frames, 0, NULL);
	if (rc) {
		dprintk(VIDC_ERR, "%s: Failed to qbuf: %d\n", __func__, rc);
		return rc;
	}
	/* update mbuf flags */
	mbuf->flags |= MSM_VIDC_FLAG_QUEUED;
	mbuf->flags &= ~MSM_VIDC_FLAG_DEFERRED;
	msm_vidc_debugfs_update(inst, MSM_VIDC_DEBUGFS_EVENT_ETB);

	return 0;
}

static int msm_comm_qbuf_in_rbr(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf)
{
	int rc = 0;

	if (!inst || !mbuf) {
		dprintk(VIDC_ERR, "%s: Invalid arguments\n", __func__);
		return -EINVAL;
	}

	if (inst->state == MSM_VIDC_CORE_INVALID) {
		dprintk(VIDC_ERR, "%s: inst is in bad state\n", __func__);
		return -EINVAL;
	}

	rc = msm_comm_scale_clocks_and_bus(inst, 0);
	if (rc)
		dprintk(VIDC_ERR, "%s: scale clock failed\n", __func__);

	print_vidc_buffer(VIDC_HIGH|VIDC_PERF, "qbuf in rbr", inst, mbuf);
	rc = msm_comm_qbuf_to_hfi(inst, mbuf);
	if (rc)
		dprintk(VIDC_ERR, "%s: Failed qbuf to hfi: %d\n", __func__, rc);

	return rc;
}

int msm_comm_qbuf(struct msm_vidc_inst *inst, struct msm_vidc_buffer *mbuf)
{
	int rc = 0;
	struct v4l2_ctrl *ctrl;
	int do_bw_calc = 0;

	if (!inst || !mbuf) {
		dprintk(VIDC_ERR, "%s: Invalid arguments\n", __func__);
		return -EINVAL;
	}

	if (inst->state == MSM_VIDC_CORE_INVALID) {
		dprintk(VIDC_ERR, "%s: inst is in bad state\n", __func__);
		return -EINVAL;
	}

	if (inst->state != MSM_VIDC_START_DONE) {
		mbuf->flags |= MSM_VIDC_FLAG_DEFERRED;
		print_vidc_buffer(VIDC_HIGH, "qbuf deferred", inst, mbuf);
		return 0;
	}

	rc = msm_comm_preprocess(inst, mbuf);
	if (rc)
		return rc;

	do_bw_calc = mbuf->vvb.vb2_buf.type == INPUT_MPLANE;
	rc = msm_comm_scale_clocks_and_bus(inst, do_bw_calc);
	if (rc)
		dprintk(VIDC_ERR, "%s: scale clock & bw failed\n", __func__);

	print_vidc_buffer(VIDC_HIGH|VIDC_PERF, "qbuf", inst, mbuf);
	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_SUPERFRAME);
	if (ctrl->val)
		rc = msm_comm_qbuf_superframe_to_hfi(inst, mbuf);
	else
		rc = msm_comm_qbuf_to_hfi(inst, mbuf);
	if (rc)
		dprintk(VIDC_ERR, "%s: Failed qbuf to hfi: %d\n", __func__, rc);

	return rc;
}

int msm_comm_qbufs(struct msm_vidc_inst *inst)
{
	int rc = 0;
	struct msm_vidc_buffer *mbuf;
	bool found;

	if (!inst) {
		dprintk(VIDC_ERR, "%s: Invalid arguments\n", __func__);
		return -EINVAL;
	}

	if (inst->state != MSM_VIDC_START_DONE) {
		dprintk(VIDC_HIGH, "%s: inst not in start state: %d\n",
			__func__, inst->state);
		return 0;
	}

	do {
		mutex_lock(&inst->registeredbufs.lock);
		found = false;
		list_for_each_entry(mbuf, &inst->registeredbufs.list, list) {
			/* Queue only deferred buffers */
			if (mbuf->flags & MSM_VIDC_FLAG_DEFERRED) {
				found = true;
				break;
			}
		}
		mutex_unlock(&inst->registeredbufs.lock);
		if (!found) {
			dprintk(VIDC_HIGH,
				"%s: no more deferred qbufs\n", __func__);
			break;
		}

		/* do not call msm_comm_qbuf() under registerbufs lock */
		if (!kref_get_mbuf(inst, mbuf)) {
			dprintk(VIDC_ERR, "%s: mbuf not found\n", __func__);
			rc = -EINVAL;
			break;
		}
		rc = msm_comm_qbuf(inst, mbuf);
		kref_put_mbuf(mbuf);
		if (rc) {
			dprintk(VIDC_ERR, "%s: failed qbuf\n", __func__);
			break;
		}
	} while (found);

	return rc;
}

int msm_comm_qbufs_batch(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf)
{
	int rc = 0;
	struct msm_vidc_buffer *buf;
	int do_bw_calc = 0;

	do_bw_calc = mbuf ? mbuf->vvb.vb2_buf.type == INPUT_MPLANE : 0;
	rc = msm_comm_scale_clocks_and_bus(inst, do_bw_calc);
	if (rc)
		dprintk(VIDC_ERR, "%s: scale clock & bw failed\n", __func__);

	mutex_lock(&inst->registeredbufs.lock);
	list_for_each_entry(buf, &inst->registeredbufs.list, list) {
		/* Don't queue if buffer is not OUTPUT_MPLANE */
		if (buf->vvb.vb2_buf.type != OUTPUT_MPLANE)
			goto loop_end;
		/* Don't queue if buffer is not a deferred buffer */
		if (!(buf->flags & MSM_VIDC_FLAG_DEFERRED))
			goto loop_end;
		/* Don't queue if RBR event is pending on this buffer */
		if (buf->flags & MSM_VIDC_FLAG_RBR_PENDING)
			goto loop_end;

		print_vidc_buffer(VIDC_HIGH|VIDC_PERF, "batch-qbuf", inst, buf);
		rc = msm_comm_qbuf_to_hfi(inst, buf);
		if (rc) {
			dprintk(VIDC_ERR, "%s: Failed batch qbuf to hfi: %d\n",
				__func__, rc);
			break;
		}
loop_end:
		/* Queue pending buffers till the current buffer only */
		if (buf == mbuf)
			break;
	}
	mutex_unlock(&inst->registeredbufs.lock);

	return rc;
}

/*
 * msm_comm_qbuf_decode_batch - count the buffers which are not queued to
 *              firmware yet (count includes rbr pending buffers too) and
 *              queue the buffers at once if full batch count reached.
 *              Don't queue rbr pending buffers as they would be queued
 *              when rbr event arrived from firmware.
 */
int msm_comm_qbuf_decode_batch(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf)
{
	int rc = 0;
	u32 count = 0;

	if (!inst || !inst->core || !mbuf) {
		dprintk(VIDC_ERR, "%s: Invalid arguments\n", __func__);
		return -EINVAL;
	}

	if (inst->state == MSM_VIDC_CORE_INVALID) {
		dprintk(VIDC_ERR, "%s: inst is in bad state\n", __func__);
		return -EINVAL;
	}

	if (inst->state != MSM_VIDC_START_DONE) {
		mbuf->flags |= MSM_VIDC_FLAG_DEFERRED;
		print_vidc_buffer(VIDC_HIGH|VIDC_PERF,
					"qbuf deferred", inst, mbuf);
		return 0;
	}

	/*
	 * Don't defer buffers initially to avoid startup latency increase
	 * due to batching
	 */
	if (inst->clk_data.buffer_counter > SKIP_BATCH_WINDOW) {
		count = num_pending_qbufs(inst, OUTPUT_MPLANE);
		if (count < inst->batch.size) {
			print_vidc_buffer(VIDC_HIGH,
				"batch-qbuf deferred", inst, mbuf);
			schedule_batch_work(inst);
			return 0;
		}

		/*
		 * Batch completed - queing bufs to firmware.
		 * so cancel pending work if any.
		 */
		cancel_batch_work(inst);
	}

	rc = msm_comm_qbufs_batch(inst, mbuf);
	if (rc)
		dprintk(VIDC_ERR, "%s: Failed qbuf to hfi: %d\n",
			__func__, rc);

	return rc;
}

int schedule_batch_work(struct msm_vidc_inst *inst)
{
	struct msm_vidc_core *core;
	struct msm_vidc_platform_resources *res;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: Invalid arguments\n", __func__);
		return -EINVAL;
	}
	core = inst->core;
	res = &core->resources;

	cancel_delayed_work(&inst->batch_work);
	queue_delayed_work(core->vidc_core_workq, &inst->batch_work,
		msecs_to_jiffies(res->batch_timeout));

	return 0;
}

int cancel_batch_work(struct msm_vidc_inst *inst)
{
	if (!inst) {
		dprintk(VIDC_ERR, "%s: Invalid arguments\n", __func__);
		return -EINVAL;
	}
	cancel_delayed_work(&inst->batch_work);

	return 0;
}

int msm_comm_try_get_bufreqs(struct msm_vidc_inst *inst)
{
	int rc = -EINVAL, i = 0;
	union hal_get_property hprop;
	enum hal_buffer int_buf[] = {
			HAL_BUFFER_INTERNAL_SCRATCH,
			HAL_BUFFER_INTERNAL_SCRATCH_1,
			HAL_BUFFER_INTERNAL_SCRATCH_2,
			HAL_BUFFER_INTERNAL_PERSIST,
			HAL_BUFFER_INTERNAL_PERSIST_1,
			HAL_BUFFER_INTERNAL_RECON,
	};

	memset(&hprop, 0x0, sizeof(hprop));
	/*
	 * First check if we can calculate bufffer sizes.
	 * If we can calculate then we do it within the driver.
	 * If we cannot then we get buffer requirements from firmware.
	 */
	if (inst->buffer_size_calculators) {
		rc = inst->buffer_size_calculators(inst);
		if (rc)
			dprintk(VIDC_ERR,
			"Failed calculating internal buffer sizes: %d", rc);
	}

	/*
	 * Fallback to get buffreq from firmware if internal calculation
	 * is not done or if it fails
	 */
	if (rc) {
		rc = msm_comm_try_get_buff_req(inst, &hprop);
		if (rc) {
			dprintk(VIDC_ERR,
				"Failed getting buffer requirements: %d", rc);
			return rc;
		}

		/* reset internal buffers */
		for (i = 0; i < ARRAY_SIZE(int_buf); i++)
			msm_comm_reset_bufreqs(inst, int_buf[i]);

		for (i = 0; i < HAL_BUFFER_MAX; i++) {
			struct hal_buffer_requirements req;
			struct hal_buffer_requirements *curr_req;

			req = hprop.buf_req.buffer[i];
			/*
			 * Firmware buffer requirements are needed for internal
			 * buffers only and all other buffer requirements are
			 * calculated in driver.
			 */
			curr_req = get_buff_req_buffer(inst, req.buffer_type);
			if (!curr_req)
				return -EINVAL;

			if (req.buffer_type == HAL_BUFFER_INTERNAL_SCRATCH ||
				req.buffer_type ==
					HAL_BUFFER_INTERNAL_SCRATCH_1 ||
				req.buffer_type ==
					HAL_BUFFER_INTERNAL_SCRATCH_2 ||
				req.buffer_type ==
					HAL_BUFFER_INTERNAL_PERSIST ||
				req.buffer_type ==
					HAL_BUFFER_INTERNAL_PERSIST_1 ||
				req.buffer_type == HAL_BUFFER_INTERNAL_RECON) {
				memcpy(curr_req, &req,
					sizeof(struct hal_buffer_requirements));
			}
		}
	}

	dprintk(VIDC_HIGH, "Buffer requirements :\n");
	dprintk(VIDC_HIGH, "%15s %8s %8s %8s %8s %8s\n",
		"buffer type", "count", "mincount_host", "mincount_fw", "size",
		"alignment");
	for (i = 0; i < HAL_BUFFER_MAX; i++) {
		struct hal_buffer_requirements req = inst->buff_req.buffer[i];

		if (req.buffer_type != HAL_BUFFER_NONE) {
			dprintk(VIDC_HIGH, "%15s %8d %8d %8d %8d %8d\n",
				get_buffer_name(req.buffer_type),
				req.buffer_count_actual,
				req.buffer_count_min_host,
				req.buffer_count_min, req.buffer_size,
				req.buffer_alignment);
		}
	}
	return rc;
}

int msm_comm_try_get_buff_req(struct msm_vidc_inst *inst,
				union hal_get_property *hprop)
{
	int rc = 0;
	struct hfi_device *hdev;
	struct getprop_buf *buf;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	hdev = inst->core->device;
	mutex_lock(&inst->sync_lock);
	if (inst->state < MSM_VIDC_OPEN_DONE ||
			inst->state >= MSM_VIDC_CLOSE) {

		/* No need to check inst->state == MSM_VIDC_INVALID since
		 * INVALID is > CLOSE_DONE. When core went to INVALID state,
		 * we put all the active instances in INVALID. So > CLOSE_DONE
		 * is enough check to have.
		 */

		dprintk(VIDC_ERR,
			"In Wrong state to call Buf Req: Inst %pK or Core %pK\n",
				inst, inst->core);
		rc = -EAGAIN;
		mutex_unlock(&inst->sync_lock);
		goto exit;
	}
	mutex_unlock(&inst->sync_lock);

	rc = call_hfi_op(hdev, session_get_buf_req, inst->session);
	if (rc) {
		dprintk(VIDC_ERR, "Can't query hardware for property: %d\n",
				rc);
		goto exit;
	}

	rc = wait_for_completion_timeout(&inst->completions[
			SESSION_MSG_INDEX(HAL_SESSION_PROPERTY_INFO)],
		msecs_to_jiffies(
			inst->core->resources.msm_vidc_hw_rsp_timeout));
	if (!rc) {
		dprintk(VIDC_ERR,
			"%s: Wait interrupted or timed out [%pK]: %d\n",
			__func__, inst,
			SESSION_MSG_INDEX(HAL_SESSION_PROPERTY_INFO));
		msm_comm_kill_session(inst);
		rc = -ETIMEDOUT;
		goto exit;
	} else {
		/* wait_for_completion_timeout returns jiffies before expiry */
		rc = 0;
	}

	mutex_lock(&inst->pending_getpropq.lock);
	if (!list_empty(&inst->pending_getpropq.list)) {
		buf = list_first_entry(&inst->pending_getpropq.list,
					struct getprop_buf, list);
		*hprop = *(union hal_get_property *)buf->data;
		kfree(buf->data);
		list_del(&buf->list);
		kfree(buf);
	} else {
		dprintk(VIDC_ERR, "%s getprop list empty\n", __func__);
		rc = -EINVAL;
	}
	mutex_unlock(&inst->pending_getpropq.lock);
exit:
	return rc;
}

int msm_comm_release_dpb_only_buffers(struct msm_vidc_inst *inst,
	bool force_release)
{
	struct msm_smem *handle;
	struct internal_buf *buf, *dummy;
	struct vidc_buffer_addr_info buffer_info;
	int rc = 0;
	struct msm_vidc_core *core;
	struct hfi_device *hdev;

	if (!inst) {
		dprintk(VIDC_ERR,
				"Invalid instance pointer = %pK\n", inst);
		return -EINVAL;
	}
	mutex_lock(&inst->outputbufs.lock);
	if (list_empty(&inst->outputbufs.list)) {
		dprintk(VIDC_HIGH, "%s - No OUTPUT buffers allocated\n",
			__func__);
		mutex_unlock(&inst->outputbufs.lock);
		return 0;
	}
	mutex_unlock(&inst->outputbufs.lock);

	core = inst->core;
	if (!core) {
		dprintk(VIDC_ERR,
				"Invalid core pointer = %pK\n", core);
		return -EINVAL;
	}
	hdev = core->device;
	if (!hdev) {
		dprintk(VIDC_ERR, "Invalid device pointer = %pK\n", hdev);
		return -EINVAL;
	}
	mutex_lock(&inst->outputbufs.lock);
	list_for_each_entry_safe(buf, dummy, &inst->outputbufs.list, list) {
		handle = &buf->smem;

		if ((buf->buffer_ownership == FIRMWARE) && !force_release) {
			dprintk(VIDC_HIGH, "DPB is with f/w. Can't free it\n");
			/*
			 * mark this buffer to avoid sending it to video h/w
			 * again, this buffer belongs to old resolution and
			 * it will be removed when video h/w returns it.
			 */
			buf->mark_remove = true;
			continue;
		}

		buffer_info.buffer_size = handle->size;
		buffer_info.buffer_type = buf->buffer_type;
		buffer_info.num_buffers = 1;
		buffer_info.align_device_addr = handle->device_addr;
		if (inst->buffer_mode_set[OUTPUT_PORT] ==
				HAL_BUFFER_MODE_STATIC) {
			buffer_info.response_required = false;
			rc = call_hfi_op(hdev, session_release_buffers,
				(void *)inst->session, &buffer_info);
			if (rc) {
				dprintk(VIDC_ERR,
					"Rel output buf fail:%x, %d\n",
					buffer_info.align_device_addr,
					buffer_info.buffer_size);
			}
		}

		list_del(&buf->list);
		msm_comm_smem_free(inst, &buf->smem);
		kfree(buf);
	}

	if (inst->dpb_extra_binfo) {
		msm_comm_smem_free(inst, &inst->dpb_extra_binfo->smem);
		kfree(inst->dpb_extra_binfo);
		inst->dpb_extra_binfo = NULL;
	}

	mutex_unlock(&inst->outputbufs.lock);
	return rc;
}

static enum hal_buffer scratch_buf_sufficient(struct msm_vidc_inst *inst,
				enum hal_buffer buffer_type)
{
	struct hal_buffer_requirements *bufreq = NULL;
	struct internal_buf *buf;
	int count = 0;

	if (!inst) {
		dprintk(VIDC_ERR, "%s - invalid param\n", __func__);
		goto not_sufficient;
	}

	bufreq = get_buff_req_buffer(inst, buffer_type);
	if (!bufreq)
		goto not_sufficient;

	/* Check if current scratch buffers are sufficient */
	mutex_lock(&inst->scratchbufs.lock);

	list_for_each_entry(buf, &inst->scratchbufs.list, list) {
		if (buf->buffer_type == buffer_type &&
			buf->smem.size >= bufreq->buffer_size)
			count++;
	}
	mutex_unlock(&inst->scratchbufs.lock);

	if (count != bufreq->buffer_count_actual)
		goto not_sufficient;

	dprintk(VIDC_HIGH,
		"Existing scratch buffer is sufficient for buffer type %#x\n",
		buffer_type);

	return buffer_type;

not_sufficient:
	return HAL_BUFFER_NONE;
}

int msm_comm_release_scratch_buffers(struct msm_vidc_inst *inst,
					bool check_for_reuse)
{
	struct msm_smem *handle;
	struct internal_buf *buf, *dummy;
	struct vidc_buffer_addr_info buffer_info;
	int rc = 0;
	struct msm_vidc_core *core;
	struct hfi_device *hdev;
	enum hal_buffer sufficiency = HAL_BUFFER_NONE;

	if (!inst) {
		dprintk(VIDC_ERR,
				"Invalid instance pointer = %pK\n", inst);
		return -EINVAL;
	}
	core = inst->core;
	if (!core) {
		dprintk(VIDC_ERR,
				"Invalid core pointer = %pK\n", core);
		return -EINVAL;
	}
	hdev = core->device;
	if (!hdev) {
		dprintk(VIDC_ERR, "Invalid device pointer = %pK\n", hdev);
		return -EINVAL;
	}

	if (check_for_reuse) {
		sufficiency |= scratch_buf_sufficient(inst,
					HAL_BUFFER_INTERNAL_SCRATCH);

		sufficiency |= scratch_buf_sufficient(inst,
					HAL_BUFFER_INTERNAL_SCRATCH_1);

		sufficiency |= scratch_buf_sufficient(inst,
					HAL_BUFFER_INTERNAL_SCRATCH_2);
	}

	mutex_lock(&inst->scratchbufs.lock);
	list_for_each_entry_safe(buf, dummy, &inst->scratchbufs.list, list) {
		handle = &buf->smem;
		buffer_info.buffer_size = handle->size;
		buffer_info.buffer_type = buf->buffer_type;
		buffer_info.num_buffers = 1;
		buffer_info.align_device_addr = handle->device_addr;
		buffer_info.response_required = true;
		rc = call_hfi_op(hdev, session_release_buffers,
				(void *)inst->session, &buffer_info);
		if (!rc) {
			mutex_unlock(&inst->scratchbufs.lock);
			rc = wait_for_sess_signal_receipt(inst,
				HAL_SESSION_RELEASE_BUFFER_DONE);
			if (rc)
				dprintk(VIDC_ERR,
					"%s: wait for signal failed, rc %d\n",
					__func__, rc);
			mutex_lock(&inst->scratchbufs.lock);
		} else {
			dprintk(VIDC_ERR,
				"Rel scrtch buf fail:%x, %d\n",
				buffer_info.align_device_addr,
				buffer_info.buffer_size);
		}

		/*If scratch buffers can be reused, do not free the buffers*/
		if (sufficiency & buf->buffer_type)
			continue;

		list_del(&buf->list);
		msm_comm_smem_free(inst, handle);
		kfree(buf);
	}

	mutex_unlock(&inst->scratchbufs.lock);
	return rc;
}

void msm_comm_release_eos_buffers(struct msm_vidc_inst *inst)
{
	struct eos_buf *buf, *next;

	if (!inst) {
		dprintk(VIDC_ERR,
			"Invalid instance pointer = %pK\n", inst);
		return;
	}

	mutex_lock(&inst->eosbufs.lock);
	list_for_each_entry_safe(buf, next, &inst->eosbufs.list, list) {
		list_del(&buf->list);
		msm_comm_smem_free(inst, &buf->smem);
		kfree(buf);
	}
	INIT_LIST_HEAD(&inst->eosbufs.list);
	mutex_unlock(&inst->eosbufs.lock);
}


int msm_comm_release_recon_buffers(struct msm_vidc_inst *inst)
{
	struct recon_buf *buf, *next;

	if (!inst) {
		dprintk(VIDC_ERR,
			"Invalid instance pointer = %pK\n", inst);
		return -EINVAL;
	}

	mutex_lock(&inst->refbufs.lock);
	list_for_each_entry_safe(buf, next, &inst->refbufs.list, list) {
		list_del(&buf->list);
		kfree(buf);
	}
	INIT_LIST_HEAD(&inst->refbufs.list);
	mutex_unlock(&inst->refbufs.lock);

	return 0;
}

int msm_comm_release_persist_buffers(struct msm_vidc_inst *inst)
{
	struct msm_smem *handle;
	struct list_head *ptr, *next;
	struct internal_buf *buf;
	struct vidc_buffer_addr_info buffer_info;
	int rc = 0;
	struct msm_vidc_core *core;
	struct hfi_device *hdev;

	if (!inst) {
		dprintk(VIDC_ERR,
				"Invalid instance pointer = %pK\n", inst);
		return -EINVAL;
	}
	core = inst->core;
	if (!core) {
		dprintk(VIDC_ERR,
				"Invalid core pointer = %pK\n", core);
		return -EINVAL;
	}
	hdev = core->device;
	if (!hdev) {
		dprintk(VIDC_ERR, "Invalid device pointer = %pK\n", hdev);
		return -EINVAL;
	}

	mutex_lock(&inst->persistbufs.lock);
	list_for_each_safe(ptr, next, &inst->persistbufs.list) {
		buf = list_entry(ptr, struct internal_buf, list);
		handle = &buf->smem;
		buffer_info.buffer_size = handle->size;
		buffer_info.buffer_type = buf->buffer_type;
		buffer_info.num_buffers = 1;
		buffer_info.align_device_addr = handle->device_addr;
		buffer_info.response_required = true;
		rc = call_hfi_op(hdev, session_release_buffers,
				(void *)inst->session, &buffer_info);
		if (!rc) {
			mutex_unlock(&inst->persistbufs.lock);
			rc = wait_for_sess_signal_receipt(inst,
				HAL_SESSION_RELEASE_BUFFER_DONE);
			if (rc)
				dprintk(VIDC_ERR,
					"%s: wait for signal failed, rc %d\n",
					__func__, rc);
			mutex_lock(&inst->persistbufs.lock);
		} else {
			dprintk(VIDC_ERR,
				"Rel prst buf fail:%x, %d\n",
				buffer_info.align_device_addr,
				buffer_info.buffer_size);
		}
		list_del(&buf->list);
		msm_comm_smem_free(inst, handle);
		kfree(buf);
	}
	mutex_unlock(&inst->persistbufs.lock);
	return rc;
}

int msm_comm_set_buffer_count(struct msm_vidc_inst *inst,
	int host_count, int act_count, enum hal_buffer type)
{
	int rc = 0;
	struct v4l2_ctrl *ctrl;
	struct hfi_device *hdev;
	struct hfi_buffer_count_actual buf_count;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}
	hdev = inst->core->device;

	buf_count.buffer_type = get_hfi_buffer(type);
	buf_count.buffer_count_actual = act_count;
	buf_count.buffer_count_min_host = host_count;
	/* set total superframe buffers count */
	ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_SUPERFRAME);
	if (ctrl->val)
		buf_count.buffer_count_actual = act_count * ctrl->val;
	dprintk(VIDC_HIGH, "%s: %x : hal_buffer %d min_host %d actual %d\n",
		__func__, hash32_ptr(inst->session), type,
		host_count, act_count);
	rc = call_hfi_op(hdev, session_set_property,
		inst->session, HFI_PROPERTY_PARAM_BUFFER_COUNT_ACTUAL,
		&buf_count, sizeof(buf_count));
	if (rc)
		dprintk(VIDC_ERR,
			"Failed to set actual buffer count %d for buffer type %d\n",
			act_count, type);
	return rc;
}

int msm_comm_set_dpb_only_buffers(struct msm_vidc_inst *inst)
{
	int rc = 0;
	bool force_release = true;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	if (get_v4l2_codec(inst) == V4L2_PIX_FMT_VP9)
		force_release = false;

	if (msm_comm_release_dpb_only_buffers(inst, force_release))
		dprintk(VIDC_ERR, "Failed to release output buffers\n");

	rc = set_dpb_only_buffers(inst, HAL_BUFFER_OUTPUT);
	if (rc)
		goto error;
	return rc;
error:
	msm_comm_release_dpb_only_buffers(inst, true);
	return rc;
}

int msm_comm_set_scratch_buffers(struct msm_vidc_inst *inst)
{
	int rc = 0;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	if (msm_comm_release_scratch_buffers(inst, true))
		dprintk(VIDC_ERR, "Failed to release scratch buffers\n");

	rc = set_internal_buffers(inst, HAL_BUFFER_INTERNAL_SCRATCH,
		&inst->scratchbufs);
	if (rc)
		goto error;

	rc = set_internal_buffers(inst, HAL_BUFFER_INTERNAL_SCRATCH_1,
		&inst->scratchbufs);
	if (rc)
		goto error;

	rc = set_internal_buffers(inst, HAL_BUFFER_INTERNAL_SCRATCH_2,
		&inst->scratchbufs);
	if (rc)
		goto error;

	return rc;
error:
	msm_comm_release_scratch_buffers(inst, false);
	return rc;
}

int msm_comm_set_recon_buffers(struct msm_vidc_inst *inst)
{
	int rc = 0;
	unsigned int i = 0, bufcount = 0;
	struct recon_buf *binfo;
	struct msm_vidc_list *buf_list = &inst->refbufs;

	if (!inst) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	if (inst->session_type != MSM_VIDC_ENCODER &&
		inst->session_type != MSM_VIDC_DECODER) {
		dprintk(VIDC_HIGH, "Recon buffs not req for cvp\n");
		return 0;
	}

	bufcount = inst->fmts[OUTPUT_PORT].count_actual;

	msm_comm_release_recon_buffers(inst);

	for (i = 0; i < bufcount; i++) {
		binfo = kzalloc(sizeof(*binfo), GFP_KERNEL);
		if (!binfo) {
			dprintk(VIDC_ERR, "Out of memory\n");
			rc = -ENOMEM;
			goto fail_kzalloc;
		}

		binfo->buffer_index = i;
		mutex_lock(&buf_list->lock);
		list_add_tail(&binfo->list, &buf_list->list);
		mutex_unlock(&buf_list->lock);
	}

fail_kzalloc:
	return rc;
}

int msm_comm_set_persist_buffers(struct msm_vidc_inst *inst)
{
	int rc = 0;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s invalid parameters\n", __func__);
		return -EINVAL;
	}

	rc = set_internal_buffers(inst, HAL_BUFFER_INTERNAL_PERSIST,
		&inst->persistbufs);
	if (rc)
		goto error;

	rc = set_internal_buffers(inst, HAL_BUFFER_INTERNAL_PERSIST_1,
		&inst->persistbufs);
	if (rc)
		goto error;
	return rc;
error:
	msm_comm_release_persist_buffers(inst);
	return rc;
}

static void msm_comm_flush_in_invalid_state(struct msm_vidc_inst *inst)
{
	struct list_head *ptr, *next;
	enum vidc_ports ports[] = {INPUT_PORT, OUTPUT_PORT};
	int c = 0;

	/* before flush ensure venus released all buffers */
	msm_comm_try_state(inst, MSM_VIDC_RELEASE_RESOURCES_DONE);

	for (c = 0; c < ARRAY_SIZE(ports); ++c) {
		enum vidc_ports port = ports[c];

		mutex_lock(&inst->bufq[port].lock);
		list_for_each_safe(ptr, next,
				&inst->bufq[port].vb2_bufq.queued_list) {
			struct vb2_buffer *vb = container_of(ptr,
					struct vb2_buffer, queued_entry);
			if (vb->state == VB2_BUF_STATE_ACTIVE) {
				vb->planes[0].bytesused = 0;
				print_vb2_buffer(VIDC_ERR, "flush in invalid",
					inst, vb);
				vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
			} else {
				dprintk(VIDC_ERR,
					"%s VB is in state %d not in ACTIVE state\n"
					, __func__, vb->state);
			}
		}
		mutex_unlock(&inst->bufq[port].lock);
	}
	msm_vidc_queue_v4l2_event(inst, V4L2_EVENT_MSM_VIDC_FLUSH_DONE);
}

int msm_comm_flush(struct msm_vidc_inst *inst, u32 flags)
{
	unsigned int i = 0;
	int rc =  0;
	bool ip_flush = false;
	bool op_flush = false;
	struct msm_vidc_buffer *mbuf, *next;
	struct msm_vidc_core *core;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR,
				"Invalid params, inst %pK\n", inst);
		return -EINVAL;
	}

	if (inst->state < MSM_VIDC_OPEN_DONE) {
		dprintk(VIDC_ERR,
			"Invalid state to call flush, inst %pK, state %#x\n",
			inst, inst->state);
		return -EINVAL;
	}

	core = inst->core;
	hdev = core->device;

	ip_flush = !!(flags & V4L2_CMD_FLUSH_OUTPUT);
	op_flush = !!(flags & V4L2_CMD_FLUSH_CAPTURE);

	if (ip_flush && !op_flush) {
		dprintk(VIDC_ERR,
			"Input only flush not supported, making it flush all\n");
		op_flush = true;
		goto exit;
	}

	if ((inst->in_flush && ip_flush) || (inst->out_flush && op_flush)) {
		dprintk(VIDC_ERR, "%s: %x : Already in flush\n",
			__func__, hash32_ptr(inst->session));
		goto exit;
	}

	msm_clock_data_reset(inst);

	cancel_batch_work(inst);
	if (inst->state == MSM_VIDC_CORE_INVALID) {
		dprintk(VIDC_ERR,
				"Core %pK and inst %pK are in bad state\n",
					core, inst);
		msm_comm_flush_in_invalid_state(inst);
		goto exit;
	}

	mutex_lock(&inst->flush_lock);
	/* enable in flush */
	inst->in_flush = ip_flush;
	inst->out_flush = op_flush;

	mutex_lock(&inst->registeredbufs.lock);
	list_for_each_entry_safe(mbuf, next, &inst->registeredbufs.list, list) {
		/* don't flush input buffers if input flush is not requested */
		if (!ip_flush && mbuf->vvb.vb2_buf.type == INPUT_MPLANE)
			continue;

		/* flush only deferred or rbr pending buffers */
		if (!(mbuf->flags & MSM_VIDC_FLAG_DEFERRED ||
			mbuf->flags & MSM_VIDC_FLAG_RBR_PENDING))
			continue;

		/*
		 * flush buffers which are queued by client already,
		 * the refcount will be two or more for those buffers.
		 */
		if (!(mbuf->smem[0].refcount >= 2))
			continue;

		print_vidc_buffer(VIDC_HIGH, "flush buf", inst, mbuf);
		msm_comm_flush_vidc_buffer(inst, mbuf);

		for (i = 0; i < mbuf->vvb.vb2_buf.num_planes; i++) {
			if (inst->smem_ops->smem_unmap_dma_buf(inst,
				&mbuf->smem[i]))
				print_vidc_buffer(VIDC_ERR,
					"dqbuf: unmap failed.", inst, mbuf);
			if (inst->smem_ops->smem_unmap_dma_buf(inst,
				&mbuf->smem[i]))
				print_vidc_buffer(VIDC_ERR,
					"dqbuf: unmap failed..", inst, mbuf);
		}
		if (!mbuf->smem[0].refcount) {
			list_del(&mbuf->list);
			kref_put_mbuf(mbuf);
		} else {
			/* buffer is no more a deferred buffer */
			mbuf->flags &= ~MSM_VIDC_FLAG_DEFERRED;
		}
	}
	mutex_unlock(&inst->registeredbufs.lock);

	hdev = inst->core->device;
	if (ip_flush) {
		dprintk(VIDC_HIGH, "Send flush on all ports to firmware\n");
		rc = call_hfi_op(hdev, session_flush, inst->session,
			HAL_FLUSH_ALL);
	} else {
		dprintk(VIDC_HIGH, "Send flush on output port to firmware\n");
		rc = call_hfi_op(hdev, session_flush, inst->session,
			HAL_FLUSH_OUTPUT);
	}
	mutex_unlock(&inst->flush_lock);
	if (rc) {
		dprintk(VIDC_ERR,
			"Sending flush to firmware failed, flush out all buffers\n");
		msm_comm_flush_in_invalid_state(inst);
		/* disable in_flush & out_flush */
		inst->in_flush = false;
		inst->out_flush = false;
	}

exit:
	return rc;
}

int msm_vidc_noc_error_info(struct msm_vidc_core *core)
{
	struct hfi_device *hdev;

	if (!core || !core->device) {
		dprintk(VIDC_ERR, "%s: Invalid parameters: %pK\n",
			__func__, core);
		return -EINVAL;
	}

	if (!core->resources.non_fatal_pagefaults)
		return 0;

	if (!core->smmu_fault_handled)
		return 0;

	hdev = core->device;
	call_hfi_op(hdev, noc_error_info, hdev->hfi_device_data);

	return 0;
}

int msm_vidc_trigger_ssr(struct msm_vidc_core *core,
	enum hal_ssr_trigger_type type)
{
	if (!core) {
		dprintk(VIDC_ERR, "%s: Invalid parameters\n", __func__);
		return -EINVAL;
	}
	core->ssr_type = type;
	schedule_work(&core->ssr_work);
	return 0;
}

void msm_vidc_ssr_handler(struct work_struct *work)
{
	int rc;
	struct msm_vidc_core *core;
	struct hfi_device *hdev;

	core = container_of(work, struct msm_vidc_core, ssr_work);
	if (!core || !core->device) {
		dprintk(VIDC_ERR, "%s: Invalid params\n", __func__);
		return;
	}
	hdev = core->device;

	mutex_lock(&core->lock);
	if (core->state == VIDC_CORE_INIT_DONE) {
		dprintk(VIDC_ERR, "%s: ssr type %d\n", __func__,
			core->ssr_type);
		/*
		 * In current implementation user-initiated SSR triggers
		 * a fatal error from hardware. However, there is no way
		 * to know if fatal error is due to SSR or not. Handle
		 * user SSR as non-fatal.
		 */
		core->trigger_ssr = true;
		rc = call_hfi_op(hdev, core_trigger_ssr,
				hdev->hfi_device_data, core->ssr_type);
		if (rc) {
			dprintk(VIDC_ERR, "%s: trigger_ssr failed\n",
				__func__);
			core->trigger_ssr = false;
		}
	} else {
		dprintk(VIDC_ERR, "%s: video core %pK not initialized\n",
			__func__, core);
	}
	mutex_unlock(&core->lock);
}

static int msm_vidc_check_mbpf_supported(struct msm_vidc_inst *inst)
{
	u32 mbpf = 0;
	struct msm_vidc_core *core;
	struct msm_vidc_inst *temp;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return -EINVAL;
	}
	core = inst->core;

	if (!core->resources.max_mbpf) {
		dprintk(VIDC_HIGH, "%s: max mbpf not available\n",
			__func__);
		return 0;
	}

	mutex_lock(&core->lock);
	list_for_each_entry(temp, &core->instances, list) {
		/* ignore invalid session */
		if (temp->state == MSM_VIDC_CORE_INVALID)
			continue;
		/* ignore thumbnail session */
		if (is_thumbnail_session(temp))
			continue;
		/* ignore HEIF sessions */
		if (is_image_session(temp))
			continue;
		mbpf += NUM_MBS_PER_FRAME(
			temp->fmts[INPUT_PORT].v4l2_fmt.fmt.pix_mp.height,
			temp->fmts[INPUT_PORT].v4l2_fmt.fmt.pix_mp.width);
	}
	mutex_unlock(&core->lock);

	if (mbpf > core->resources.max_mbpf) {
		msm_vidc_print_running_insts(inst->core);
		return -EBUSY;
	}

	return 0;
}

static int msm_vidc_check_mbps_supported(struct msm_vidc_inst *inst)
{
	int num_mbs_per_sec = 0, max_load_adj = 0;
	enum load_calc_quirks quirks = LOAD_ADMISSION_CONTROL;

	if (inst->state == MSM_VIDC_OPEN_DONE) {
		max_load_adj = inst->core->resources.max_load;
		num_mbs_per_sec = msm_comm_get_device_load(inst->core,
					MSM_VIDC_DECODER, quirks);
		num_mbs_per_sec += msm_comm_get_device_load(inst->core,
					MSM_VIDC_ENCODER, quirks);
		if (num_mbs_per_sec > max_load_adj) {
			dprintk(VIDC_ERR,
				"H/W is overloaded. needed: %d max: %d\n",
				num_mbs_per_sec,
				max_load_adj);
			msm_vidc_print_running_insts(inst->core);
			return -EBUSY;
		}
	}
	return 0;
}

int msm_vidc_check_scaling_supported(struct msm_vidc_inst *inst)
{
	u32 x_min, x_max, y_min, y_max;
	u32 input_height, input_width, output_height, output_width;
	struct v4l2_format *f;
	struct v4l2_ctrl *ctrl = NULL;

	/* Grid get_ctrl allowed for encode session only */
	if (is_image_session(inst)) {
		ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_IMG_GRID_SIZE);
		if (ctrl->val > 0) {
			dprintk(VIDC_HIGH, "Skip scaling check for HEIC\n");
			return 0;
		}
	}

	f = &inst->fmts[INPUT_PORT].v4l2_fmt;
	input_height = f->fmt.pix_mp.height;
	input_width = f->fmt.pix_mp.width;
	f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
	output_height = f->fmt.pix_mp.height;
	output_width = f->fmt.pix_mp.width;

	if (!input_height || !input_width || !output_height || !output_width) {
		dprintk(VIDC_ERR,
			"Invalid : Input height = %d width = %d",
			input_height, input_width);
		dprintk(VIDC_ERR,
			" output height = %d width = %d\n",
			output_height, output_width);
		return -ENOTSUPP;
	}

	if (!inst->capability.cap[CAP_SCALE_X].min ||
		!inst->capability.cap[CAP_SCALE_X].max ||
		!inst->capability.cap[CAP_SCALE_Y].min ||
		!inst->capability.cap[CAP_SCALE_Y].max) {

		if (input_width * input_height !=
			output_width * output_height) {
			dprintk(VIDC_ERR,
				"%s: scaling is not supported (%dx%d != %dx%d)\n",
				__func__, input_width, input_height,
				output_width, output_height);
			return -ENOTSUPP;
		}

		dprintk(VIDC_HIGH, "%s: supported WxH = %dx%d\n",
			__func__, input_width, input_height);
		return 0;
	}

	x_min = (1<<16)/inst->capability.cap[CAP_SCALE_X].min;
	y_min = (1<<16)/inst->capability.cap[CAP_SCALE_Y].min;
	x_max = inst->capability.cap[CAP_SCALE_X].max >> 16;
	y_max = inst->capability.cap[CAP_SCALE_Y].max >> 16;

	if (input_height > output_height) {
		if (input_height > x_min * output_height) {
			dprintk(VIDC_ERR,
				"Unsupported height min height %d vs %d\n",
				input_height / x_min, output_height);
			return -ENOTSUPP;
		}
	} else {
		if (output_height > x_max * input_height) {
			dprintk(VIDC_ERR,
				"Unsupported height max height %d vs %d\n",
				x_max * input_height, output_height);
			return -ENOTSUPP;
		}
	}
	if (input_width > output_width) {
		if (input_width > y_min * output_width) {
			dprintk(VIDC_ERR,
				"Unsupported width min width %d vs %d\n",
				input_width / y_min, output_width);
			return -ENOTSUPP;
		}
	} else {
		if (output_width > y_max * input_width) {
			dprintk(VIDC_ERR,
				"Unsupported width max width %d vs %d\n",
				y_max * input_width, output_width);
			return -ENOTSUPP;
		}
	}
	return 0;
}

int msm_vidc_check_session_supported(struct msm_vidc_inst *inst)
{
	struct msm_vidc_capability *capability;
	int rc = 0;
	struct hfi_device *hdev;
	struct msm_vidc_core *core;
	u32 output_height, output_width, input_height, input_width;
	u32 width_min, width_max, height_min, height_max;
	u32 mbpf_max;
	struct v4l2_format *f;
	struct v4l2_ctrl *ctrl = NULL;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s: Invalid parameter\n", __func__);
		return -EINVAL;
	}
	capability = &inst->capability;
	hdev = inst->core->device;
	core = inst->core;
	rc = msm_vidc_check_mbps_supported(inst);
	if (rc) {
		dprintk(VIDC_ERR,
			"%s: Hardware is overloaded\n", __func__);
		return rc;
	}

	rc = msm_vidc_check_mbpf_supported(inst);
	if (rc)
		return rc;

	if (!is_thermal_permissible(core)) {
		dprintk(VIDC_ERR,
			"Thermal level critical, stop all active sessions!\n");
		return -ENOTSUPP;
	}

	if (is_secure_session(inst)) {
		width_min = capability->cap[CAP_SECURE_FRAME_WIDTH].min;
		width_max = capability->cap[CAP_SECURE_FRAME_WIDTH].max;
		height_min = capability->cap[CAP_SECURE_FRAME_HEIGHT].min;
		height_max = capability->cap[CAP_SECURE_FRAME_HEIGHT].max;
		mbpf_max = capability->cap[CAP_SECURE_MBS_PER_FRAME].max;
	} else {
		width_min = capability->cap[CAP_FRAME_WIDTH].min;
		width_max = capability->cap[CAP_FRAME_WIDTH].max;
		height_min = capability->cap[CAP_FRAME_HEIGHT].min;
		height_max = capability->cap[CAP_FRAME_HEIGHT].max;
		mbpf_max = capability->cap[CAP_MBS_PER_FRAME].max;
	}

	if (inst->session_type == MSM_VIDC_ENCODER &&
		inst->rc_type == RATE_CONTROL_LOSSLESS) {
		width_min = capability->cap[CAP_LOSSLESS_FRAME_WIDTH].min;
		width_max = capability->cap[CAP_LOSSLESS_FRAME_WIDTH].max;
		height_min = capability->cap[CAP_LOSSLESS_FRAME_HEIGHT].min;
		height_max = capability->cap[CAP_LOSSLESS_FRAME_HEIGHT].max;
		mbpf_max = capability->cap[CAP_LOSSLESS_MBS_PER_FRAME].max;
	}

	f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
	output_height = f->fmt.pix_mp.height;
	output_width = f->fmt.pix_mp.width;
	f = &inst->fmts[INPUT_PORT].v4l2_fmt;
	input_height = f->fmt.pix_mp.height;
	input_width = f->fmt.pix_mp.width;

	if (is_image_session(inst)) {
		if (is_secure_session(inst)) {
			dprintk(VIDC_ERR,
				"Secure image encode isn't supported!\n");
			return -ENOTSUPP;
		}

		ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_IMG_GRID_SIZE);
		if (ctrl->val > 0) {
			if (inst->fmts[INPUT_PORT].v4l2_fmt.fmt.pix_mp.pixelformat !=
				V4L2_PIX_FMT_NV12 &&
				inst->fmts[INPUT_PORT].v4l2_fmt.fmt.pix_mp.pixelformat !=
				V4L2_PIX_FMT_NV12_512)
				return -ENOTSUPP;

			width_min =
				capability->cap[CAP_HEIC_IMAGE_FRAME_WIDTH].min;
			width_max =
				capability->cap[CAP_HEIC_IMAGE_FRAME_WIDTH].max;
			height_min =
				capability->cap[CAP_HEIC_IMAGE_FRAME_HEIGHT].min;
			height_max =
				capability->cap[CAP_HEIC_IMAGE_FRAME_HEIGHT].max;
			mbpf_max = capability->cap[CAP_MBS_PER_FRAME].max;

			input_height = ALIGN(input_height, 512);
			input_width = ALIGN(input_width, 512);
			output_height = input_height;
			output_width = input_width;
		} else {
			width_min =
				capability->cap[CAP_HEVC_IMAGE_FRAME_WIDTH].min;
			width_max =
				capability->cap[CAP_HEVC_IMAGE_FRAME_WIDTH].max;
			height_min =
				capability->cap[CAP_HEVC_IMAGE_FRAME_HEIGHT].min;
			height_max =
				capability->cap[CAP_HEVC_IMAGE_FRAME_HEIGHT].max;
			mbpf_max = capability->cap[CAP_MBS_PER_FRAME].max;
		}
	}

	if (inst->session_type == MSM_VIDC_ENCODER && (input_width % 2 != 0 ||
			input_height % 2 != 0 || output_width % 2 != 0 ||
			output_height % 2 != 0)) {
		dprintk(VIDC_ERR,
			"Height and Width should be even numbers for NV12\n");
		dprintk(VIDC_ERR,
			"Input WxH = (%u)x(%u), Output WxH = (%u)x(%u)\n",
			input_width, input_height,
			output_width, output_height);
		rc = -ENOTSUPP;
	}

	output_height = ALIGN(output_height, 16);
	output_width = ALIGN(output_width, 16);

	if (!rc) {
		if (output_width < width_min ||
			output_height < height_min) {
			dprintk(VIDC_ERR,
				"Unsupported WxH = (%u)x(%u), min supported is - (%u)x(%u)\n",
				output_width, output_height,
				width_min, height_min);
			rc = -ENOTSUPP;
		}
		if (!rc && output_width > width_max) {
			dprintk(VIDC_ERR,
				"Unsupported width = %u supported max width = %u\n",
				output_width, width_max);
				rc = -ENOTSUPP;
		}

		if (!rc && output_height * output_width >
			width_max * height_max) {
			dprintk(VIDC_ERR,
			"Unsupported WxH = (%u)x(%u), max supported is - (%u)x(%u)\n",
			output_width, output_height,
			width_max, height_max);
			rc = -ENOTSUPP;
		}
		if (!rc && NUM_MBS_PER_FRAME(input_width, input_height) >
			mbpf_max) {
			dprintk(VIDC_ERR, "Unsupported mbpf %d, max %d\n",
				NUM_MBS_PER_FRAME(input_width, input_height),
				mbpf_max);
			rc = -ENOTSUPP;
		}
		if (!rc && inst->pic_struct !=
			MSM_VIDC_PIC_STRUCT_PROGRESSIVE &&
			(output_width > INTERLACE_WIDTH_MAX ||
			output_height > INTERLACE_HEIGHT_MAX ||
			(NUM_MBS_PER_FRAME(output_height, output_width) >
			INTERLACE_MB_PER_FRAME_MAX))) {
			dprintk(VIDC_ERR,
				"Unsupported interlace WxH = (%u)x(%u), max supported is - (%u)x(%u)\n",
				output_width, output_height,
				INTERLACE_WIDTH_MAX, INTERLACE_HEIGHT_MAX);
			rc = -ENOTSUPP;
		}
	}
	if (rc) {
		dprintk(VIDC_ERR,
			"%s: Resolution unsupported\n", __func__);
	}
	return rc;
}

void msm_comm_generate_session_error(struct msm_vidc_inst *inst)
{
	enum hal_command_response cmd = HAL_SESSION_ERROR;
	struct msm_vidc_cb_cmd_done response = {0};

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid input parameters\n", __func__);
		return;
	}
	dprintk(VIDC_ERR, "%s: inst %pK\n", __func__, inst);
	response.session_id = inst;
	response.status = VIDC_ERR_FAIL;
	handle_session_error(cmd, (void *)&response);
}

void msm_comm_generate_sys_error(struct msm_vidc_inst *inst)
{
	struct msm_vidc_core *core;
	enum hal_command_response cmd = HAL_SYS_ERROR;
	struct msm_vidc_cb_cmd_done response  = {0};

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid input parameters\n", __func__);
		return;
	}
	dprintk(VIDC_ERR, "%s: inst %pK\n", __func__, inst);
	core = inst->core;
	response.device_id = (u32) core->id;
	handle_sys_error(cmd, (void *) &response);

}

int msm_comm_kill_session(struct msm_vidc_inst *inst)
{
	int rc = 0;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s: invalid input parameters\n", __func__);
		return -EINVAL;
	} else if (!inst->session) {
		dprintk(VIDC_ERR, "%s: no session to kill for inst %pK\n",
			__func__, inst);
		return 0;
	}

	dprintk(VIDC_ERR, "%s: inst %pK, session %x state %d\n", __func__,
		inst, hash32_ptr(inst->session), inst->state);
	/*
	 * We're internally forcibly killing the session, if fw is aware of
	 * the session send session_abort to firmware to clean up and release
	 * the session, else just kill the session inside the driver.
	 */
	if ((inst->state >= MSM_VIDC_OPEN_DONE &&
			inst->state < MSM_VIDC_CLOSE_DONE) ||
			inst->state == MSM_VIDC_CORE_INVALID) {
		rc = msm_comm_session_abort(inst);
		if (rc) {
			dprintk(VIDC_ERR,
				"%s: inst %pK session %x abort failed\n",
				__func__, inst, hash32_ptr(inst->session));
			change_inst_state(inst, MSM_VIDC_CORE_INVALID);
		}
	}

	change_inst_state(inst, MSM_VIDC_CLOSE_DONE);
	msm_comm_session_clean(inst);

	dprintk(VIDC_ERR, "%s: inst %pK session %x handled\n", __func__,
		inst, hash32_ptr(inst->session));
	return rc;
}

int msm_comm_smem_alloc(struct msm_vidc_inst *inst,
		size_t size, u32 align, u32 flags, enum hal_buffer buffer_type,
		int map_kernel, struct msm_smem *smem)
{
	int rc = 0;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s: invalid inst: %pK\n", __func__, inst);
		return -EINVAL;
	}
	rc = msm_smem_alloc(size, align, flags, buffer_type, map_kernel,
				&(inst->core->resources), inst->session_type,
				smem);
	return rc;
}

void msm_comm_smem_free(struct msm_vidc_inst *inst, struct msm_smem *mem)
{
	if (!inst || !inst->core || !mem) {
		dprintk(VIDC_ERR,
			"%s: invalid params: %pK %pK\n", __func__, inst, mem);
		return;
	}
	msm_smem_free(mem);
}

void msm_vidc_fw_unload_handler(struct work_struct *work)
{
	struct msm_vidc_core *core = NULL;
	struct hfi_device *hdev = NULL;
	int rc = 0;

	core = container_of(work, struct msm_vidc_core, fw_unload_work.work);
	if (!core || !core->device) {
		dprintk(VIDC_ERR, "%s - invalid work or core handle\n",
				__func__);
		return;
	}

	hdev = core->device;

	mutex_lock(&core->lock);
	if (list_empty(&core->instances) &&
		core->state != VIDC_CORE_UNINIT) {
		if (core->state > VIDC_CORE_INIT) {
			dprintk(VIDC_HIGH, "Calling vidc_hal_core_release\n");
			rc = call_hfi_op(hdev, core_release,
					hdev->hfi_device_data);
			if (rc) {
				dprintk(VIDC_ERR,
					"Failed to release core, id = %d\n",
					core->id);
				mutex_unlock(&core->lock);
				return;
			}
		}
		core->state = VIDC_CORE_UNINIT;
		kfree(core->capabilities);
		core->capabilities = NULL;
	}
	mutex_unlock(&core->lock);
}

int msm_comm_set_color_format(struct msm_vidc_inst *inst,
		enum hal_buffer buffer_type, int fourcc)
{
	struct hfi_uncompressed_format_select hfi_fmt = {0};
	u32 format = HFI_COLOR_FORMAT_NV12_UBWC;
	int rc = 0;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s - invalid param\n", __func__);
		return -EINVAL;
	}

	hdev = inst->core->device;

	format = msm_comm_get_hfi_uncompressed(fourcc);
	hfi_fmt.buffer_type = get_hfi_buffer(buffer_type);
	hfi_fmt.format = format;

	rc = call_hfi_op(hdev, session_set_property, inst->session,
		HFI_PROPERTY_PARAM_UNCOMPRESSED_FORMAT_SELECT, &hfi_fmt,
		sizeof(hfi_fmt));
	if (rc)
		dprintk(VIDC_ERR,
			"Failed to set input color format\n");
	else
		dprintk(VIDC_HIGH, "Setting uncompressed colorformat to %#x\n",
				format);

	return rc;
}

void msm_comm_print_inst_info(struct msm_vidc_inst *inst)
{
	struct msm_vidc_buffer *mbuf;
	struct internal_buf *buf;
	bool is_decode = false;
	enum vidc_ports port;
	bool is_secure = false;
	struct v4l2_format *f;

	if (!inst) {
		dprintk(VIDC_ERR, "%s - invalid param %pK\n",
			__func__, inst);
		return;
	}

	is_decode = inst->session_type == MSM_VIDC_DECODER;
	port = is_decode ? INPUT_PORT : OUTPUT_PORT;
	is_secure = inst->flags & VIDC_SECURE;
	f = &inst->fmts[port].v4l2_fmt;
	dprintk(VIDC_ERR,
			"%s session, %s, Codec type: %s HxW: %d x %d fps: %d bitrate: %d bit-depth: %s\n",
			is_decode ? "Decode" : "Encode",
			is_secure ? "Secure" : "Non-Secure",
			inst->fmts[port].name,
			f->fmt.pix_mp.height, f->fmt.pix_mp.width,
			inst->clk_data.frame_rate >> 16, inst->prop.bitrate,
			!inst->bit_depth ? "8" : "10");

	dprintk(VIDC_ERR,
			"---Buffer details for inst: %pK of type: %d---\n",
			inst, inst->session_type);
	mutex_lock(&inst->registeredbufs.lock);
	dprintk(VIDC_ERR, "registered buffer list:\n");
	list_for_each_entry(mbuf, &inst->registeredbufs.list, list)
		print_vidc_buffer(VIDC_ERR, "buf", inst, mbuf);
	mutex_unlock(&inst->registeredbufs.lock);

	mutex_lock(&inst->scratchbufs.lock);
	dprintk(VIDC_ERR, "scratch buffer list:\n");
	list_for_each_entry(buf, &inst->scratchbufs.list, list)
		dprintk(VIDC_ERR, "type: %d addr: %x size: %u\n",
				buf->buffer_type, buf->smem.device_addr,
				buf->smem.size);
	mutex_unlock(&inst->scratchbufs.lock);

	mutex_lock(&inst->persistbufs.lock);
	dprintk(VIDC_ERR, "persist buffer list:\n");
	list_for_each_entry(buf, &inst->persistbufs.list, list)
		dprintk(VIDC_ERR, "type: %d addr: %x size: %u\n",
				buf->buffer_type, buf->smem.device_addr,
				buf->smem.size);
	mutex_unlock(&inst->persistbufs.lock);

	mutex_lock(&inst->outputbufs.lock);
	dprintk(VIDC_ERR, "dpb buffer list:\n");
	list_for_each_entry(buf, &inst->outputbufs.list, list)
		dprintk(VIDC_ERR, "type: %d addr: %x size: %u\n",
				buf->buffer_type, buf->smem.device_addr,
				buf->smem.size);
	mutex_unlock(&inst->outputbufs.lock);
}

int msm_comm_session_continue(void *instance)
{
	struct msm_vidc_inst *inst = instance;
	int rc = 0;
	struct hfi_device *hdev;

	if (!inst || !inst->core || !inst->core->device)
		return -EINVAL;
	hdev = inst->core->device;
	mutex_lock(&inst->lock);
	if (inst->state >= MSM_VIDC_RELEASE_RESOURCES_DONE ||
			inst->state < MSM_VIDC_START_DONE) {
		dprintk(VIDC_HIGH,
			"Inst %pK : Not in valid state to call %s\n",
				inst, __func__);
		goto sess_continue_fail;
	}
	if (inst->session_type == MSM_VIDC_DECODER && inst->in_reconfig) {
		dprintk(VIDC_HIGH, "send session_continue\n");
		rc = call_hfi_op(hdev, session_continue,
						 (void *)inst->session);
		if (rc) {
			dprintk(VIDC_ERR,
					"failed to send session_continue\n");
			rc = -EINVAL;
			goto sess_continue_fail;
		}
		inst->in_reconfig = false;

		if (msm_comm_get_stream_output_mode(inst) ==
			HAL_VIDEO_DECODER_SECONDARY) {
			rc = msm_comm_queue_dpb_only_buffers(inst);
			if (rc) {
				dprintk(VIDC_ERR,
						"Failed to queue output buffers: %d\n",
						rc);
				goto sess_continue_fail;
			}
		}
	} else if (inst->session_type == MSM_VIDC_ENCODER) {
		dprintk(VIDC_HIGH,
				"session_continue not supported for encoder");
	} else {
		dprintk(VIDC_ERR,
				"session_continue called in wrong state for decoder");
	}

sess_continue_fail:
	mutex_unlock(&inst->lock);
	return rc;
}

void print_vidc_buffer(u32 tag, const char *str, struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf)
{
	struct vb2_buffer *vb2 = NULL;

	if (!(tag & msm_vidc_debug) || !inst || !mbuf)
		return;

	vb2 = &mbuf->vvb.vb2_buf;

	if (vb2->num_planes == 1)
		dprintk(tag,
			"%s: %s: %x : idx %2d fd %d off %d daddr %x size %d filled %d flags 0x%x ts %lld refcnt %d mflags 0x%x\n",
			str, vb2->type == INPUT_MPLANE ?
			"OUTPUT" : "CAPTURE", hash32_ptr(inst->session),
			vb2->index, vb2->planes[0].m.fd,
			vb2->planes[0].data_offset, mbuf->smem[0].device_addr,
			vb2->planes[0].length, vb2->planes[0].bytesused,
			mbuf->vvb.flags, mbuf->vvb.vb2_buf.timestamp,
			mbuf->smem[0].refcount, mbuf->flags);
	else
		dprintk(tag,
			"%s: %s: %x : idx %2d fd %d off %d daddr %x size %d filled %d flags 0x%x ts %lld refcnt %d mflags 0x%x, extradata: fd %d off %d daddr %x size %d filled %d refcnt %d\n",
			str, vb2->type == INPUT_MPLANE ?
			"OUTPUT" : "CAPTURE", hash32_ptr(inst->session),
			vb2->index, vb2->planes[0].m.fd,
			vb2->planes[0].data_offset, mbuf->smem[0].device_addr,
			vb2->planes[0].length, vb2->planes[0].bytesused,
			mbuf->vvb.flags, mbuf->vvb.vb2_buf.timestamp,
			mbuf->smem[0].refcount, mbuf->flags,
			vb2->planes[1].m.fd, vb2->planes[1].data_offset,
			mbuf->smem[1].device_addr, vb2->planes[1].length,
			vb2->planes[1].bytesused, mbuf->smem[1].refcount);
}

void print_vb2_buffer(u32 tag, const char *str, struct msm_vidc_inst *inst,
		struct vb2_buffer *vb2)
{
	if (!(tag & msm_vidc_debug) || !inst || !vb2)
		return;
	if ((tag & VIDC_PERF) &&
		!(tag & VIDC_HIGH) &&
		(inst->session_type != MSM_VIDC_DECODER))
		return;

	if (vb2->num_planes == 1)
		dprintk(tag,
			"%s: %s: %x : idx %2d fd %d off %d size %d filled %d\n",
			str, vb2->type == INPUT_MPLANE ?
			"OUTPUT" : "CAPTURE", hash32_ptr(inst->session),
			vb2->index, vb2->planes[0].m.fd,
			vb2->planes[0].data_offset, vb2->planes[0].length,
			vb2->planes[0].bytesused);
	else
		dprintk(tag,
			"%s: %s: %x : idx %2d fd %d off %d size %d filled %d, extradata: fd %d off %d size %d filled %d\n",
			str, vb2->type == INPUT_MPLANE ?
			"OUTPUT" : "CAPTURE", hash32_ptr(inst->session),
			vb2->index, vb2->planes[0].m.fd,
			vb2->planes[0].data_offset, vb2->planes[0].length,
			vb2->planes[0].bytesused, vb2->planes[1].m.fd,
			vb2->planes[1].data_offset, vb2->planes[1].length,
			vb2->planes[1].bytesused);
}

void print_v4l2_buffer(u32 tag, const char *str, struct msm_vidc_inst *inst,
		struct v4l2_buffer *v4l2)
{
	if (!(tag & msm_vidc_debug) || !inst || !v4l2)
		return;

	if (v4l2->length == 1)
		dprintk(tag,
			"%s: %s: %x : idx %2d fd %d off %d size %d filled %d\n",
			str, v4l2->type == INPUT_MPLANE ?
			"OUTPUT" : "CAPTURE", hash32_ptr(inst->session),
			v4l2->index, v4l2->m.planes[0].m.fd,
			v4l2->m.planes[0].data_offset,
			v4l2->m.planes[0].length,
			v4l2->m.planes[0].bytesused);
	else
		dprintk(tag,
			"%s: %s: %x : idx %2d fd %d off %d size %d filled %d, extradata: fd %d off %d size %d filled %d\n",
			str, v4l2->type == INPUT_MPLANE ?
			"OUTPUT" : "CAPTURE", hash32_ptr(inst->session),
			v4l2->index, v4l2->m.planes[0].m.fd,
			v4l2->m.planes[0].data_offset,
			v4l2->m.planes[0].length,
			v4l2->m.planes[0].bytesused,
			v4l2->m.planes[1].m.fd,
			v4l2->m.planes[1].data_offset,
			v4l2->m.planes[1].length,
			v4l2->m.planes[1].bytesused);
}

bool msm_comm_compare_vb2_plane(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf, struct vb2_buffer *vb2, u32 i)
{
	struct vb2_buffer *vb;

	if (!inst || !mbuf || !vb2) {
		dprintk(VIDC_ERR, "%s: invalid params, %pK %pK %pK\n",
			__func__, inst, mbuf, vb2);
		return false;
	}

	vb = &mbuf->vvb.vb2_buf;
	if (vb->planes[i].m.fd == vb2->planes[i].m.fd &&
		vb->planes[i].length == vb2->planes[i].length) {
		return true;
	}

	return false;
}

bool msm_comm_compare_vb2_planes(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf, struct vb2_buffer *vb2)
{
	unsigned int i = 0;
	struct vb2_buffer *vb;

	if (!inst || !mbuf || !vb2) {
		dprintk(VIDC_ERR, "%s: invalid params, %pK %pK %pK\n",
			__func__, inst, mbuf, vb2);
		return false;
	}

	vb = &mbuf->vvb.vb2_buf;

	if (vb->num_planes != vb2->num_planes)
		return false;

	for (i = 0; i < vb->num_planes; i++) {
		if (!msm_comm_compare_vb2_plane(inst, mbuf, vb2, i))
			return false;
	}

	return true;
}

bool msm_comm_compare_dma_plane(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf, unsigned long *dma_planes, u32 i)
{
	if (!inst || !mbuf || !dma_planes) {
		dprintk(VIDC_ERR, "%s: invalid params, %pK %pK %pK\n",
			__func__, inst, mbuf, dma_planes);
		return false;
	}

	if ((unsigned long)mbuf->smem[i].dma_buf == dma_planes[i])
		return true;

	return false;
}

bool msm_comm_compare_dma_planes(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf, unsigned long *dma_planes)
{
	unsigned int i = 0;
	struct vb2_buffer *vb;

	if (!inst || !mbuf || !dma_planes) {
		dprintk(VIDC_ERR, "%s: invalid params, %pK %pK %pK\n",
			__func__, inst, mbuf, dma_planes);
		return false;
	}

	vb = &mbuf->vvb.vb2_buf;
	for (i = 0; i < vb->num_planes; i++) {
		if (!msm_comm_compare_dma_plane(inst, mbuf, dma_planes, i))
			return false;
	}

	return true;
}


bool msm_comm_compare_device_plane(struct msm_vidc_buffer *mbuf,
		u32 type, u32 *planes, u32 i)
{
	if (!mbuf || !planes) {
		dprintk(VIDC_ERR, "%s: invalid params, %pK %pK\n",
			__func__, mbuf, planes);
		return false;
	}

	if (mbuf->vvb.vb2_buf.type == type &&
		mbuf->smem[i].device_addr == planes[i])
		return true;

	return false;
}

bool msm_comm_compare_device_planes(struct msm_vidc_buffer *mbuf,
		u32 type, u32 *planes)
{
	unsigned int i = 0;

	if (!mbuf || !planes)
		return false;

	for (i = 0; i < mbuf->vvb.vb2_buf.num_planes; i++) {
		if (!msm_comm_compare_device_plane(mbuf, type, planes, i))
			return false;
	}

	return true;
}

struct msm_vidc_buffer *msm_comm_get_buffer_using_device_planes(
		struct msm_vidc_inst *inst, u32 type, u32 *planes)
{
	struct msm_vidc_buffer *mbuf;
	bool found = false;

	mutex_lock(&inst->registeredbufs.lock);
	found = false;
	list_for_each_entry(mbuf, &inst->registeredbufs.list, list) {
		if (msm_comm_compare_device_planes(mbuf, type, planes)) {
			found = true;
			break;
		}
	}
	mutex_unlock(&inst->registeredbufs.lock);
	if (!found) {
		dprintk(VIDC_ERR,
			"%s: data_addr %x, extradata_addr %x not found\n",
			__func__, planes[0], planes[1]);
		mbuf = NULL;
	}

	return mbuf;
}

int msm_comm_flush_vidc_buffer(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf)
{
	struct vb2_buffer *vb;
	u32 port;

	if (!inst || !mbuf) {
		dprintk(VIDC_ERR, "%s: invalid params %pK %pK\n",
			__func__, inst, mbuf);
		return -EINVAL;
	}

	vb = msm_comm_get_vb_using_vidc_buffer(inst, mbuf);
	if (!vb) {
		print_vidc_buffer(VIDC_ERR,
			"vb not found for buf", inst, mbuf);
		return -EINVAL;
	}

	if (mbuf->vvb.vb2_buf.type == OUTPUT_MPLANE)
		port = OUTPUT_PORT;
	else if (mbuf->vvb.vb2_buf.type == INPUT_MPLANE)
		port = INPUT_PORT;
	else
		return -EINVAL;

	mutex_lock(&inst->bufq[port].lock);
	if (inst->bufq[port].vb2_bufq.streaming) {
		vb->planes[0].bytesused = 0;
		vb2_buffer_done(vb, VB2_BUF_STATE_DONE);
	} else {
		dprintk(VIDC_ERR, "%s: port %d is not streaming\n",
			__func__, port);
	}
	mutex_unlock(&inst->bufq[port].lock);

	return 0;
}

int msm_comm_qbuf_cache_operations(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf)
{
	int rc = 0;
	unsigned int i;
	struct vb2_buffer *vb;
	bool skip;

	if (!inst || !mbuf) {
		dprintk(VIDC_ERR, "%s: invalid params %pK %pK\n",
			__func__, inst, mbuf);
		return -EINVAL;
	}
	vb = &mbuf->vvb.vb2_buf;

	for (i = 0; i < vb->num_planes; i++) {
		unsigned long offset, size;
		enum smem_cache_ops cache_op;

		skip = true;
		if (inst->session_type == MSM_VIDC_DECODER) {
			if (vb->type == INPUT_MPLANE) {
				if (!i) { /* bitstream */
					skip = false;
					offset = vb->planes[i].data_offset;
					size = vb->planes[i].bytesused;
					cache_op = SMEM_CACHE_CLEAN_INVALIDATE;
				}
			} else if (vb->type == OUTPUT_MPLANE) {
				if (!i) { /* yuv */
					skip = false;
					offset = 0;
					size = vb->planes[i].length;
					cache_op = SMEM_CACHE_INVALIDATE;
				}
			}
		} else if (inst->session_type == MSM_VIDC_ENCODER) {
			if (vb->type == INPUT_MPLANE) {
				if (!i) { /* yuv */
					skip = false;
					offset = vb->planes[i].data_offset;
					size = vb->planes[i].bytesused;
					cache_op = SMEM_CACHE_CLEAN_INVALIDATE;
				}
			} else if (vb->type == OUTPUT_MPLANE) {
				if (!i) { /* bitstream */
					u32 size_u32;
					skip = false;
					offset = 0;
					size_u32 = vb->planes[i].length;
					msm_comm_fetch_filled_length(
						&inst->fbd_data, vb->index,
						&size_u32);
					size = size_u32;
					cache_op = SMEM_CACHE_INVALIDATE;
				}
			}
		}

		if (!skip) {
			rc = msm_smem_cache_operations(mbuf->smem[i].dma_buf,
					cache_op, offset, size);
			if (rc)
				print_vidc_buffer(VIDC_ERR,
					"qbuf cache ops failed", inst, mbuf);
		}
	}

	return rc;
}

int msm_comm_dqbuf_cache_operations(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf)
{
	int rc = 0;
	unsigned int i;
	struct vb2_buffer *vb;
	bool skip;

	if (!inst || !mbuf) {
		dprintk(VIDC_ERR, "%s: invalid params %pK %pK\n",
			__func__, inst, mbuf);
		return -EINVAL;
	}
	vb = &mbuf->vvb.vb2_buf;

	for (i = 0; i < vb->num_planes; i++) {
		unsigned long offset, size;
		enum smem_cache_ops cache_op;

		skip = true;
		if (inst->session_type == MSM_VIDC_DECODER) {
			if (vb->type == INPUT_MPLANE) {
				/* bitstream and extradata */
				/* we do not need cache operations */
			} else if (vb->type == OUTPUT_MPLANE) {
				if (!i) { /* yuv */
					skip = false;
					offset = vb->planes[i].data_offset;
					size = vb->planes[i].bytesused;
					cache_op = SMEM_CACHE_INVALIDATE;
				}
			}
		} else if (inst->session_type == MSM_VIDC_ENCODER) {
			if (vb->type == INPUT_MPLANE) {
				/* yuv and extradata */
				/* we do not need cache operations */
			} else if (vb->type == OUTPUT_MPLANE) {
				if (!i) { /* bitstream */
					skip = false;
					/*
					 * Include vp8e header bytes as well
					 * by making offset equal to zero
					 */
					offset = 0;
					size = vb->planes[i].bytesused +
						vb->planes[i].data_offset;
					cache_op = SMEM_CACHE_INVALIDATE;
				}
			}
		}

		if (!skip) {
			rc = msm_smem_cache_operations(mbuf->smem[i].dma_buf,
					cache_op, offset, size);
			if (rc)
				print_vidc_buffer(VIDC_ERR,
					"dqbuf cache ops failed", inst, mbuf);
		}
	}

	return rc;
}

struct msm_vidc_buffer *msm_comm_get_vidc_buffer(struct msm_vidc_inst *inst,
		struct vb2_buffer *vb2)
{
	int rc = 0;
	struct vb2_v4l2_buffer *vbuf;
	struct vb2_buffer *vb;
	unsigned long dma_planes[VB2_MAX_PLANES] = {0};
	struct msm_vidc_buffer *mbuf;
	bool found = false;
	unsigned int i;

	if (!inst || !vb2) {
		dprintk(VIDC_ERR, "%s: invalid params\n", __func__);
		return NULL;
	}

	for (i = 0; i < vb2->num_planes; i++) {
		/*
		 * always compare dma_buf addresses which is guaranteed
		 * to be same across the processes (duplicate fds).
		 */
		dma_planes[i] = (unsigned long)msm_smem_get_dma_buf(
				vb2->planes[i].m.fd);
		if (!dma_planes[i])
			return NULL;
		msm_smem_put_dma_buf((struct dma_buf *)dma_planes[i]);
	}

	mutex_lock(&inst->registeredbufs.lock);
	/*
	 * for encoder input, client may queue the same buffer with different
	 * fd before driver returned old buffer to the client. This buffer
	 * should be treated as new buffer Search the list with fd so that
	 * it will be treated as new msm_vidc_buffer.
	 */
	if (is_encode_session(inst) && vb2->type == INPUT_MPLANE) {
		list_for_each_entry(mbuf, &inst->registeredbufs.list, list) {
			if (msm_comm_compare_vb2_planes(inst, mbuf, vb2)) {
				found = true;
				break;
			}
		}
	} else {
		list_for_each_entry(mbuf, &inst->registeredbufs.list, list) {
			if (msm_comm_compare_dma_planes(inst, mbuf,
					dma_planes)) {
				found = true;
				break;
			}
		}
	}

	if (!found) {
		/* this is new vb2_buffer */
		mbuf = kzalloc(sizeof(struct msm_vidc_buffer), GFP_KERNEL);
		if (!mbuf) {
			dprintk(VIDC_ERR, "%s: alloc msm_vidc_buffer failed\n",
				__func__);
			rc = -ENOMEM;
			goto exit;
		}
		kref_init(&mbuf->kref);
	}

	/* Initially assume all the buffer are going to be deferred */
	mbuf->flags |= MSM_VIDC_FLAG_DEFERRED;

	vbuf = to_vb2_v4l2_buffer(vb2);
	memcpy(&mbuf->vvb, vbuf, sizeof(struct vb2_v4l2_buffer));
	vb = &mbuf->vvb.vb2_buf;

	for (i = 0; i < vb->num_planes; i++) {
		mbuf->smem[i].buffer_type = get_hal_buffer_type(vb->type, i);
		mbuf->smem[i].fd = vb->planes[i].m.fd;
		mbuf->smem[i].offset = vb->planes[i].data_offset;
		mbuf->smem[i].size = vb->planes[i].length;
		rc = inst->smem_ops->smem_map_dma_buf(inst, &mbuf->smem[i]);
		if (rc) {
			dprintk(VIDC_ERR, "%s: map failed.\n", __func__);
			goto exit;
		}
		/* increase refcount as we get both fbd and rbr */
		rc = inst->smem_ops->smem_map_dma_buf(inst, &mbuf->smem[i]);
		if (rc) {
			dprintk(VIDC_ERR, "%s: map failed..\n", __func__);
			goto exit;
		}
	}
	/* dma cache operations need to be performed after dma_map */
	msm_comm_qbuf_cache_operations(inst, mbuf);

	/* special handling for decoder */
	if (inst->session_type == MSM_VIDC_DECODER) {
		if (found) {
			rc = -EEXIST;
		} else {
			bool found_plane0 = false;
			struct msm_vidc_buffer *temp;
			/*
			 * client might have queued same plane[0] but different
			 * plane[1] search plane[0] and if found don't queue the
			 * buffer, the buffer will be queued when rbr event
			 * arrived.
			 */
			list_for_each_entry(temp, &inst->registeredbufs.list,
						list) {
				if (msm_comm_compare_dma_plane(inst, temp,
						dma_planes, 0)) {
					found_plane0 = true;
					break;
				}
			}
			if (found_plane0)
				rc = -EEXIST;
		}
		if (rc == -EEXIST) {
			print_vidc_buffer(VIDC_HIGH,
				"existing qbuf", inst, mbuf);
			/* enable RBR pending */
			mbuf->flags |= MSM_VIDC_FLAG_RBR_PENDING;
		}
	}

	/* add the new buffer to list */
	if (!found)
		list_add_tail(&mbuf->list, &inst->registeredbufs.list);

	mutex_unlock(&inst->registeredbufs.lock);

	/*
	 * Return mbuf if decode batching is enabled as this buffer
	 * may trigger queuing full batch to firmware, also this buffer
	 * will not be queued to firmware while full batch queuing,
	 * it will be queued when rbr event arrived from firmware.
	 */
	if (rc == -EEXIST && !inst->batch.enable)
		return ERR_PTR(rc);

	return mbuf;

exit:
	dprintk(VIDC_ERR, "%s: rc %d\n", __func__, rc);
	msm_comm_unmap_vidc_buffer(inst, mbuf);
	if (!found)
		kref_put_mbuf(mbuf);
	mutex_unlock(&inst->registeredbufs.lock);

	return ERR_PTR(rc);
}

void msm_comm_put_vidc_buffer(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf)
{
	struct msm_vidc_buffer *temp;
	bool found = false;
	unsigned int i = 0;

	if (!inst || !mbuf) {
		dprintk(VIDC_ERR, "%s: invalid params %pK %pK\n",
			__func__, inst, mbuf);
		return;
	}

	mutex_lock(&inst->registeredbufs.lock);
	/* check if mbuf was not removed by any chance */
	list_for_each_entry(temp, &inst->registeredbufs.list, list) {
		if (msm_comm_compare_vb2_planes(inst, mbuf,
				&temp->vvb.vb2_buf)) {
			found = true;
			break;
		}
	}
	if (!found) {
		print_vidc_buffer(VIDC_ERR, "buf was removed", inst, mbuf);
		goto unlock;
	}

	print_vidc_buffer(VIDC_HIGH, "dqbuf", inst, mbuf);
	for (i = 0; i < mbuf->vvb.vb2_buf.num_planes; i++) {
		if (inst->smem_ops->smem_unmap_dma_buf(inst, &mbuf->smem[i]))
			print_vidc_buffer(VIDC_ERR,
				"dqbuf: unmap failed.", inst, mbuf);

		if (!(mbuf->vvb.flags & V4L2_BUF_FLAG_READONLY)) {
			/* rbr won't come for this buffer */
			if (inst->smem_ops->smem_unmap_dma_buf(inst,
				&mbuf->smem[i]))
				print_vidc_buffer(VIDC_ERR,
					"dqbuf: unmap failed..", inst, mbuf);
		} else {
			/* RBR event expected */
			mbuf->flags |= MSM_VIDC_FLAG_RBR_PENDING;
		}
	}
	/*
	 * remove the entry if plane[0].refcount is zero else
	 * don't remove as client queued same buffer that's why
	 * plane[0].refcount is not zero
	 */
	if (!mbuf->smem[0].refcount) {
		list_del(&mbuf->list);
		kref_put_mbuf(mbuf);
	}
unlock:
	mutex_unlock(&inst->registeredbufs.lock);
}

void handle_release_buffer_reference(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf)
{
	int rc = 0;
	struct msm_vidc_buffer *temp;
	bool found = false;
	unsigned int i = 0;
	u32 planes[VIDEO_MAX_PLANES] = {0};

	mutex_lock(&inst->flush_lock);
	mutex_lock(&inst->registeredbufs.lock);
	found = false;
	/* check if mbuf was not removed by any chance */
	list_for_each_entry(temp, &inst->registeredbufs.list, list) {
		if (msm_comm_compare_vb2_planes(inst, mbuf,
				&temp->vvb.vb2_buf)) {
			found = true;
			break;
		}
	}
	if (found) {
		/* save device_addr */
		for (i = 0; i < mbuf->vvb.vb2_buf.num_planes; i++)
			planes[i] = mbuf->smem[i].device_addr;

		/* send RBR event to client */
		msm_vidc_queue_rbr_event(inst,
			mbuf->vvb.vb2_buf.planes[0].m.fd,
			mbuf->vvb.vb2_buf.planes[0].data_offset);

		/* clear RBR_PENDING flag */
		mbuf->flags &= ~MSM_VIDC_FLAG_RBR_PENDING;

		for (i = 0; i < mbuf->vvb.vb2_buf.num_planes; i++) {
			if (inst->smem_ops->smem_unmap_dma_buf(inst,
				&mbuf->smem[i]))
				print_vidc_buffer(VIDC_ERR,
					"rbr unmap failed.", inst, mbuf);
		}
		/* refcount is not zero if client queued the same buffer */
		if (!mbuf->smem[0].refcount) {
			list_del(&mbuf->list);
			kref_put_mbuf(mbuf);
			mbuf = NULL;
		}
	} else {
		print_vidc_buffer(VIDC_ERR, "mbuf not found", inst, mbuf);
		goto unlock;
	}

	/*
	 * 1. client might have pushed same planes in which case mbuf will be
	 *    same and refcounts are positive and buffer wouldn't have been
	 *    removed from the registeredbufs list.
	 * 2. client might have pushed same planes[0] but different planes[1]
	 *    in which case mbuf will be different.
	 * 3. in either case we can search mbuf->smem[0].device_addr in the list
	 *    and if found queue it to video hw (if not flushing).
	 */
	found = false;
	list_for_each_entry(temp, &inst->registeredbufs.list, list) {
		if (msm_comm_compare_device_plane(temp,
			OUTPUT_MPLANE, planes, 0)) {
			mbuf = temp;
			found = true;
			break;
		}
	}
	if (!found)
		goto unlock;

	/* buffer found means client queued the buffer already */
	if (inst->in_reconfig || inst->out_flush) {
		print_vidc_buffer(VIDC_HIGH, "rbr flush buf", inst, mbuf);
		msm_comm_flush_vidc_buffer(inst, mbuf);
		msm_comm_unmap_vidc_buffer(inst, mbuf);
		/* remove from list */
		list_del(&mbuf->list);
		kref_put_mbuf(mbuf);

		/* don't queue the buffer */
		found = false;
	}
	/* clear required flags as the buffer is going to be queued */
	if (found) {
		mbuf->flags &= ~MSM_VIDC_FLAG_DEFERRED;
		mbuf->flags &= ~MSM_VIDC_FLAG_RBR_PENDING;
	}

unlock:
	mutex_unlock(&inst->registeredbufs.lock);

	if (found) {
		rc = msm_comm_qbuf_in_rbr(inst, mbuf);
		if (rc)
			print_vidc_buffer(VIDC_ERR,
				"rbr qbuf failed", inst, mbuf);
	}
	mutex_unlock(&inst->flush_lock);
}

int msm_comm_unmap_vidc_buffer(struct msm_vidc_inst *inst,
		struct msm_vidc_buffer *mbuf)
{
	int rc = 0;
	unsigned int i;

	if (!inst || !mbuf) {
		dprintk(VIDC_ERR, "%s: invalid params %pK %pK\n",
			__func__, inst, mbuf);
		return -EINVAL;
	}
	if (mbuf->vvb.vb2_buf.num_planes > VIDEO_MAX_PLANES) {
		dprintk(VIDC_ERR, "%s: invalid num_planes %d\n", __func__,
			mbuf->vvb.vb2_buf.num_planes);
		return -EINVAL;
	}

	for (i = 0; i < mbuf->vvb.vb2_buf.num_planes; i++) {
		u32 refcount = mbuf->smem[i].refcount;

		while (refcount) {
			if (inst->smem_ops->smem_unmap_dma_buf(inst,
				&mbuf->smem[i]))
				print_vidc_buffer(VIDC_ERR,
					"unmap failed for buf", inst, mbuf);
			refcount--;
		}
	}

	return rc;
}

static void kref_free_mbuf(struct kref *kref)
{
	struct msm_vidc_buffer *mbuf = container_of(kref,
			struct msm_vidc_buffer, kref);

	kfree(mbuf);
}

void kref_put_mbuf(struct msm_vidc_buffer *mbuf)
{
	if (!mbuf)
		return;

	kref_put(&mbuf->kref, kref_free_mbuf);
}

bool kref_get_mbuf(struct msm_vidc_inst *inst, struct msm_vidc_buffer *mbuf)
{
	struct msm_vidc_buffer *temp;
	bool matches = false;
	bool ret = false;

	if (!inst || !mbuf)
		return false;

	mutex_lock(&inst->registeredbufs.lock);
	list_for_each_entry(temp, &inst->registeredbufs.list, list) {
		if (temp == mbuf) {
			matches = true;
			break;
		}
	}
	ret = (matches && kref_get_unless_zero(&mbuf->kref)) ? true : false;
	mutex_unlock(&inst->registeredbufs.lock);

	return ret;
}

struct msm_vidc_client_data *msm_comm_store_client_data(
	struct msm_vidc_inst *inst, u32 itag)
{
	struct msm_vidc_client_data *data = NULL, *temp = NULL;

	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid params %pK %un",
			__func__, inst, itag);
		return NULL;
	}

	mutex_lock(&inst->client_data.lock);
	list_for_each_entry(temp, &inst->client_data.list, list) {
		if (!temp->id) {
			data = temp;
			break;
		}
	}
	if (!data) {
		data = kzalloc(sizeof(*data), GFP_KERNEL);
		if (!data) {
			dprintk(VIDC_ERR, "No memory avilable - tag data");
			goto exit;
		}
		INIT_LIST_HEAD(&data->list);
		list_add_tail(&data->list, &inst->client_data.list);
	}

	/**
	 * Special handling, if etb_counter reaches to 2^32 - 1,
	 * then start next value from 1 not 0.
	 */
	if (!inst->etb_counter)
		inst->etb_counter = 1;

	data->id =  inst->etb_counter++;
	data->input_tag = itag;

exit:
	mutex_unlock(&inst->client_data.lock);

	return data;
}

void msm_comm_fetch_client_data(struct msm_vidc_inst *inst, bool remove,
	u32 itag, u32 itag2, u32 *otag, u32 *otag2)
{
	struct msm_vidc_client_data *temp, *next;
	bool found_itag = false, found_itag2 = false;

	if (!inst || !otag || !otag2) {
		dprintk(VIDC_ERR, "%s: invalid params %pK %x %x\n",
			__func__, inst, otag, otag2);
		return;
	}
	/**
	 * Some interlace clips, both BF & TF is available in single ETB buffer.
	 * In that case, firmware copies same input_tag value to both input_tag
	 * and input_tag2 at FBD.
	 */
	if (!itag2 || itag == itag2)
		found_itag2 = true;
	mutex_lock(&inst->client_data.lock);
	list_for_each_entry_safe(temp, next, &inst->client_data.list, list) {
		if (temp->id == itag) {
			*otag = temp->input_tag;
			found_itag = true;
			if (remove)
				temp->id = 0;
		} else if (!found_itag2 && temp->id == itag2) {
			*otag2 = temp->input_tag;
			found_itag2 = true;
			if (remove)
				temp->id = 0;
		}
		if (found_itag && found_itag2)
			break;
	}
	mutex_unlock(&inst->client_data.lock);

	if (!found_itag || !found_itag2) {
		dprintk(VIDC_ERR, "%s: %x: client data not found - %u, %u\n",
			__func__, hash32_ptr(inst->session), itag, itag2);
	}
}

void msm_comm_release_client_data(struct msm_vidc_inst *inst, bool remove)
{
	struct msm_vidc_client_data *temp, *next;

	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid params %pK\n",
			__func__, inst);
		return;
	}

	mutex_lock(&inst->client_data.lock);
	list_for_each_entry_safe(temp, next, &inst->client_data.list, list) {
		temp->id = 0;
		if (remove) {
			list_del(&temp->list);
			kfree(temp);
		}
	}
	mutex_unlock(&inst->client_data.lock);
}

void msm_comm_store_filled_length(struct msm_vidc_list *data_list,
		u32 index, u32 filled_length)
{
	struct msm_vidc_buf_data *pdata = NULL;
	bool found = false;

	if (!data_list) {
		dprintk(VIDC_ERR, "%s: invalid params %pK\n",
			__func__, data_list);
		return;
	}

	mutex_lock(&data_list->lock);
	list_for_each_entry(pdata, &data_list->list, list) {
		if (pdata->index == index) {
			pdata->filled_length = filled_length;
			found = true;
			break;
		}
	}

	if (!found) {
		pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
		if (!pdata)  {
			dprintk(VIDC_ERR, "%s: malloc failure.\n", __func__);
			goto exit;
		}
		pdata->index = index;
		pdata->filled_length = filled_length;
		list_add_tail(&pdata->list, &data_list->list);
	}

exit:
	mutex_unlock(&data_list->lock);
}

void msm_comm_fetch_filled_length(struct msm_vidc_list *data_list,
		u32 index, u32 *filled_length)
{
	struct msm_vidc_buf_data *pdata = NULL;

	if (!data_list || !filled_length) {
		dprintk(VIDC_ERR, "%s: invalid params %pK %pK\n",
			__func__, data_list, filled_length);
		return;
	}

	mutex_lock(&data_list->lock);
	list_for_each_entry(pdata, &data_list->list, list) {
		if (pdata->index == index) {
			*filled_length = pdata->filled_length;
			break;
		}
	}
	mutex_unlock(&data_list->lock);
}

void msm_comm_store_input_tag(struct msm_vidc_list *data_list,
		u32 index, u32 itag, u32 itag2)
{
	struct msm_vidc_buf_data *pdata = NULL;
	bool found = false;

	if (!data_list) {
		dprintk(VIDC_ERR, "%s: invalid params %pK\n",
			__func__, data_list);
		return;
	}

	mutex_lock(&data_list->lock);
	list_for_each_entry(pdata, &data_list->list, list) {
		if (pdata->index == index) {
			pdata->input_tag = itag;
			pdata->input_tag2 = itag2;
			found = true;
			break;
		}
	}

	if (!found) {
		pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
		if (!pdata)  {
			dprintk(VIDC_ERR, "%s: malloc failure.\n", __func__);
			goto exit;
		}
		pdata->index = index;
		pdata->input_tag = itag;
		pdata->input_tag2 = itag2;
		list_add_tail(&pdata->list, &data_list->list);
	}

exit:
	mutex_unlock(&data_list->lock);
}

int msm_comm_fetch_input_tag(struct msm_vidc_list *data_list,
		u32 index, u32 *itag, u32 *itag2)
{
	struct msm_vidc_buf_data *pdata = NULL;
	int rc = 0;

	if (!data_list || !itag || !itag2) {
		dprintk(VIDC_ERR, "%s: invalid params %pK %pK %pK\n",
			__func__, data_list, itag, itag2);
		return -EINVAL;
	}

	*itag = *itag2 = 0;
	mutex_lock(&data_list->lock);
	list_for_each_entry(pdata, &data_list->list, list) {
		if (pdata->index == index) {
			*itag = pdata->input_tag;
			*itag2 = pdata->input_tag2;
			/* clear after fetch */
			pdata->input_tag = pdata->input_tag2 = 0;
			break;
		}
	}
	mutex_unlock(&data_list->lock);

	return rc;
}

int msm_comm_release_input_tag(struct msm_vidc_inst *inst)
{
	struct msm_vidc_buf_data *pdata, *next;

	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid params %pK\n",
			__func__, inst);
		return -EINVAL;
	}

	mutex_lock(&inst->etb_data.lock);
	list_for_each_entry_safe(pdata, next, &inst->etb_data.list, list) {
		list_del(&pdata->list);
		kfree(pdata);
	}
	mutex_unlock(&inst->etb_data.lock);

	mutex_lock(&inst->fbd_data.lock);
	list_for_each_entry_safe(pdata, next, &inst->fbd_data.list, list) {
		list_del(&pdata->list);
		kfree(pdata);
	}
	mutex_unlock(&inst->fbd_data.lock);

	return 0;
}

int msm_comm_set_color_format_constraints(struct msm_vidc_inst *inst,
		enum hal_buffer buffer_type,
		struct msm_vidc_format_constraint *pix_constraint)
{
	struct hfi_uncompressed_plane_actual_constraints_info
		*pconstraint = NULL;
	u32 num_planes = 2;
	u32 size = 0;
	int rc = 0;
	struct hfi_device *hdev;
	u32 hfi_fmt;

	if (!inst || !inst->core || !inst->core->device) {
		dprintk(VIDC_ERR, "%s - invalid param\n", __func__);
		return -EINVAL;
	}

	hdev = inst->core->device;

	size = 2 * sizeof(u32)
			+ num_planes
			* sizeof(struct hfi_uncompressed_plane_constraints);

	pconstraint = kzalloc(size, GFP_KERNEL);
	if (!pconstraint) {
		dprintk(VIDC_ERR, "No memory cannot alloc constrain\n");
		rc = -ENOMEM;
		goto exit;
	}

	hfi_fmt = msm_comm_convert_color_fmt(pix_constraint->fourcc);
	pconstraint->buffer_type = get_hfi_buffer(buffer_type);
	pconstraint->num_planes = pix_constraint->num_planes;
	//set Y plan constraints
	dprintk(VIDC_HIGH, "Set Y plan constraints.\n");
	pconstraint->rg_plane_format[0].stride_multiples =
			VENUS_Y_STRIDE(hfi_fmt, 1);
	pconstraint->rg_plane_format[0].max_stride =
			pix_constraint->y_max_stride;
	pconstraint->rg_plane_format[0].min_plane_buffer_height_multiple =
			VENUS_Y_SCANLINES(hfi_fmt, 1);
	pconstraint->rg_plane_format[0].buffer_alignment =
			pix_constraint->y_buffer_alignment;

	//set UV plan constraints
	dprintk(VIDC_HIGH, "Set UV plan constraints.\n");
	pconstraint->rg_plane_format[1].stride_multiples =
			VENUS_UV_STRIDE(hfi_fmt, 1);
	pconstraint->rg_plane_format[1].max_stride =
			pix_constraint->uv_max_stride;
	pconstraint->rg_plane_format[1].min_plane_buffer_height_multiple =
			VENUS_UV_SCANLINES(hfi_fmt, 1);
	pconstraint->rg_plane_format[1].buffer_alignment =
			pix_constraint->uv_buffer_alignment;

	rc = call_hfi_op(hdev,
		session_set_property,
		inst->session,
		HFI_PROPERTY_PARAM_UNCOMPRESSED_PLANE_ACTUAL_CONSTRAINTS_INFO,
		pconstraint,
		size);
	if (rc)
		dprintk(VIDC_ERR,
			"Failed to set input color format constraint\n");
	else
		dprintk(VIDC_HIGH, "Set color format constraint success\n");

exit:
	if (!pconstraint)
		kfree(pconstraint);
	return rc;
}

int msm_comm_set_index_extradata(struct msm_vidc_inst *inst,
	uint32_t extradata_id, uint32_t value)
{
	int rc = 0;
	struct hfi_index_extradata_config extradata;
	struct hfi_device *hdev;

	hdev = inst->core->device;

	extradata.index_extra_data_id = extradata_id;
	extradata.enable = value;

	rc = call_hfi_op(hdev, session_set_property, (void *)
		inst->session, HFI_PROPERTY_PARAM_INDEX_EXTRADATA, &extradata,
		sizeof(extradata));

	return rc;
}

int msm_comm_set_extradata(struct msm_vidc_inst *inst,
	uint32_t extradata_id, uint32_t value)
{
	int rc = 0;
	struct hfi_index_extradata_config extradata;
	struct hfi_device *hdev;

	hdev = inst->core->device;

	extradata.index_extra_data_id = extradata_id;
	extradata.enable = value;

	rc = call_hfi_op(hdev, session_set_property, (void *)
			inst->session, extradata_id, &extradata,
			sizeof(extradata));

	return rc;
}

bool msm_comm_check_for_inst_overload(struct msm_vidc_core *core)
{
	u32 instance_count = 0;
	u32 secure_instance_count = 0;
	struct msm_vidc_inst *inst = NULL;
	bool overload = false;

	mutex_lock(&core->lock);
	list_for_each_entry(inst, &core->instances, list) {
		instance_count++;
		if (inst->flags & VIDC_SECURE)
			secure_instance_count++;
	}
	mutex_unlock(&core->lock);

	if (instance_count > core->resources.max_inst_count ||
		secure_instance_count > core->resources.max_secure_inst_count) {
		overload = true;
		dprintk(VIDC_ERR,
			"%s: inst_count:%u max_inst:%u sec_inst_count:%u max_sec_inst:%u\n",
			__func__, instance_count,
			core->resources.max_inst_count, secure_instance_count,
			core->resources.max_secure_inst_count);
	}
	return overload;
}

int msm_comm_check_window_bitrate(struct msm_vidc_inst *inst,
	struct vidc_frame_data *frame_data)
{
	struct msm_vidc_window_data *pdata, *temp = NULL;
	u32 bitrate, max_br, window_size;
	int buf_cnt = 1, fps, window_start;

	if (!inst || !inst->core || !frame_data) {
		dprintk(VIDC_ERR, "%s: Invalid arguments\n", __func__);
		return -EINVAL;
	}

	if (!inst->core->resources.avsync_window_size ||
		!frame_data->filled_len)
		return 0;

	fps = inst->clk_data.frame_rate >> 16;
	max_br = MAX_BITRATE_DECODER_CAVLC;
	if (inst->entropy_mode == HFI_H264_ENTROPY_CABAC)
		max_br = inst->clk_data.work_mode == HFI_WORKMODE_2 ?
			MAX_BITRATE_DECODER_2STAGE_CABAC :
			MAX_BITRATE_DECODER_1STAGE_CABAC;
	window_size = inst->core->resources.avsync_window_size * fps;
	window_size = DIV_ROUND_UP(window_size, 1000);

	bitrate = frame_data->filled_len;
	window_start = inst->count.etb;

	mutex_lock(&inst->window_data.lock);
	list_for_each_entry(pdata, &inst->window_data.list, list) {
		if (buf_cnt < window_size && pdata->frame_size) {
			bitrate += pdata->frame_size;
			window_start = pdata->etb_count;
			buf_cnt++;
		} else {
			pdata->frame_size = 0;
			temp = pdata;
		}
	}

	pdata = NULL;
	if(!temp) {
		pdata = kzalloc(sizeof(*pdata), GFP_KERNEL);
		if (!pdata)  {
			dprintk(VIDC_ERR, "%s: malloc failure.\n", __func__);
			mutex_unlock(&inst->window_data.lock);
			return -ENOMEM;
		}
	} else {
		pdata = temp;
		list_del(&pdata->list);
	}
	pdata->frame_size = frame_data->filled_len;
	pdata->etb_count = inst->count.etb;
	list_add(&pdata->list, &inst->window_data.list);
	mutex_unlock(&inst->window_data.lock);

	bitrate = DIV_ROUND_UP(((u64)bitrate * 8 * fps), window_size);
	if (bitrate > max_br) {
		dprintk(VIDC_PERF,
			"Unsupported bitrate %u max %u, window size %u [%u,%u]",
			bitrate, max_br, window_size,
			window_start, inst->count.etb);
	}

	return 0;
}

void msm_comm_clear_window_data(struct msm_vidc_inst *inst)
{
	struct msm_vidc_window_data *pdata;

	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid params %pK\n",
			__func__, inst);
		return;
	}

	mutex_lock(&inst->window_data.lock);
	list_for_each_entry(pdata, &inst->window_data.list, list) {
		pdata->frame_size = 0;
	}
	mutex_unlock(&inst->window_data.lock);
}

void msm_comm_release_window_data(struct msm_vidc_inst *inst)
{
	struct msm_vidc_window_data *pdata, *next;

	if (!inst) {
		dprintk(VIDC_ERR, "%s: invalid params %pK\n",
			__func__, inst);
		return;
	}

	mutex_lock(&inst->window_data.lock);
	list_for_each_entry_safe(pdata, next, &inst->window_data.list, list) {
		list_del(&pdata->list);
		kfree(pdata);
	}
	mutex_unlock(&inst->window_data.lock);
}
