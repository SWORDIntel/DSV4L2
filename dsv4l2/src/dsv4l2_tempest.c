/*
 * dsv4l2_tempest.c
 *
 * TEMPEST state management for DSV4L2
 * Phase 2 implementation with auto-discovery
 */

#include "dsv4l2_tempest.h"
#include "dsv4l2_core.h"
#include "dsv4l2rt.h"
#include <errno.h>
#include <string.h>
#include <ctype.h>
#include <linux/videodev2.h>

dsv4l2_tempest_state_t
dsv4l2_get_tempest_state(dsv4l2_device_t *dev) {
    if (!dev) {
        return DSV4L2_TEMPEST_DISABLED;
    }
    dsv4l2_device_ex_t *dev_ex = (dsv4l2_device_ex_t *)dev;
    return dev_ex->tempest_state;
}

int
dsv4l2_set_tempest_state(
    dsv4l2_device_t *dev,
    dsv4l2_tempest_state_t target_state
) {
    if (!dev) {
        return -EINVAL;
    }

    dsv4l2_device_ex_t *dev_ex = (dsv4l2_device_ex_t *)dev;
    dsv4l2_tempest_state_t old_state = dev_ex->tempest_state;

    /* TODO: Implement actual v4l2 control writes in Phase 2 */

    /* Update cached state */
    dev_ex->tempest_state = target_state;

    /* Log transition */
    dsv4l2rt_log_tempest_transition(
        (uint32_t)(uintptr_t)dev,
        old_state,
        target_state
    );

    return 0;
}

int
dsv4l2_policy_check_capture(
    dsv4l2_device_t *dev,
    dsv4l2_tempest_state_t current_state,
    const char *context
) {
    if (!dev) {
        return -EINVAL;
    }

    /* Simple policy: deny capture in LOCKDOWN mode */
    int result = (current_state == DSV4L2_TEMPEST_LOCKDOWN) ? -EACCES : 0;

    /* Log policy check */
    dsv4l2rt_log_policy_check(
        (uint32_t)(uintptr_t)dev,
        context,
        result
    );

    return result;
}

/* Helper: check if control name matches TEMPEST patterns */
static int is_tempest_control(const char *name) {
    const char *patterns[] = {
        "tempest", "privacy", "secure", "shutter",
        "led", "indicator", "emission", "lockdown",
        NULL
    };

    char lower_name[256];
    strncpy(lower_name, name, sizeof(lower_name) - 1);
    lower_name[sizeof(lower_name) - 1] = '\0';

    /* Convert to lowercase */
    for (char *p = lower_name; *p; p++) {
        *p = tolower((unsigned char)*p);
    }

    /* Check each pattern */
    for (int i = 0; patterns[i] != NULL; i++) {
        if (strstr(lower_name, patterns[i]) != NULL) {
            return 1;
        }
    }

    return 0;
}

/* Callback for control enumeration */
typedef struct {
    uint32_t found_id;
    int found;
} tempest_search_ctx_t;

static int tempest_search_callback(const struct v4l2_queryctrl *qctrl, void *user_data) {
    tempest_search_ctx_t *ctx = (tempest_search_ctx_t *)user_data;

    if (is_tempest_control((const char *)qctrl->name)) {
        ctx->found_id = qctrl->id;
        ctx->found = 1;
        return 1; /* Stop enumeration */
    }

    return 0; /* Continue */
}

int
dsv4l2_discover_tempest_control(
    dsv4l2_device_t *dev,
    uint32_t *out_control_id
) {
    if (!dev || !out_control_id) {
        return -EINVAL;
    }

    tempest_search_ctx_t ctx = {0, 0};

    /* Enumerate controls looking for TEMPEST patterns */
    dsv4l2_enum_controls(dev, tempest_search_callback, &ctx);

    if (ctx.found) {
        *out_control_id = ctx.found_id;
        return 0;
    }

    return -ENOENT;  /* Not found */
}

int
dsv4l2_apply_tempest_mapping(dsv4l2_device_t *dev) {
    if (!dev) {
        return -EINVAL;
    }

    dsv4l2_device_ex_t *dev_ex = (dsv4l2_device_ex_t *)dev;
    if (!dev_ex->profile) {
        return -EINVAL;
    }

    /* TODO: Implement profile-based TEMPEST control mapping in Phase 2 */

    return 0;
}
