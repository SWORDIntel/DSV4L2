#ifndef DSV4L2_CORE_H
#define DSV4L2_CORE_H

#include "dsv4l2_annotations.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of controls in a profile */
#define DSV4L2_MAX_CONTROLS 32

/* Forward declarations */
/* Note: dsv4l2_device_t is defined in dsv4l2_annotations.h */
typedef struct dsv4l2_profile dsv4l2_profile_t;
typedef struct dsv4l2_meta_handle dsv4l2_meta_handle_t;
struct v4l2_queryctrl;

/**
 * Device profile structure
 * Loaded from YAML configuration files
 */
struct dsv4l2_profile {
    char id[64];                    /* USB VID:PID or PCI ID */
    char role[32];                  /* camera, iris_scanner, ir_sensor, etc. */
    char device_hint[256];          /* Preferred device path */
    char classification[64];        /* UNCLASSIFIED, SECRET_BIOMETRIC, etc. */

    /* Format preferences */
    uint32_t pixel_format;          /* fourcc */
    uint32_t width;
    uint32_t height;
    uint32_t fps_num;
    uint32_t fps_den;

    /* Control presets */
    struct {
        uint32_t id;
        int32_t  value;
    } controls[DSV4L2_MAX_CONTROLS];
    int num_controls;

    /* TEMPEST control mapping */
    struct {
        uint32_t control_id;
        int32_t  disabled_value;
        int32_t  low_value;
        int32_t  high_value;
        int32_t  lockdown_value;
        int      auto_detect;       /* Scan for TEMPEST/PRIVACY controls */
    } tempest_control;

    /* Companion metadata device */
    char meta_device_path[256];
    uint32_t meta_format;

    /* Advanced options */
    int buffer_count;
    int constant_time_required;
    int quantum_candidate;
};

/**
 * Extended device information (internal use)
 * Base dsv4l2_device_t is defined in dsv4l2_annotations.h
 * This extends it with internal state
 */
typedef struct dsv4l2_device_ex {
    /* Public fields (match dsv4l2_device_t) */
    int fd;
    const char *dev_path;
    const char *role;
    uint32_t layer;

    /* Extended fields (internal) */
    char dev_path_buf[256];                 /* Buffer for dev_path */
    char role_buf[32];                      /* Buffer for role */
    dsv4l2_profile_t *profile;              /* Loaded profile */

    /* Current state */
    dsv4l2_tempest_state_t tempest_state;
    void *current_format;                   /* v4l2_format* */
    void *current_parm;                     /* v4l2_streamparm* */

    /* Buffer management */
    int num_buffers;
    void *buffers;                          /* buffer_info* array */

    /* Metadata companion (optional) */
    dsv4l2_meta_handle_t *meta_handle;

    /* Runtime */
    int streaming;
} dsv4l2_device_ex_t;

/**
 * Frame buffer (generic)
 */
typedef struct {
    uint8_t *data;
    size_t   len;
    uint64_t timestamp_ns;
    uint32_t sequence;
} DSMIL_SECRET("generic_frame") dsv4l2_frame_t;

/**
 * Biometric frame buffer (high security)
 */
typedef struct {
    uint8_t *data;
    size_t   len;
    uint64_t timestamp_ns;
    uint32_t sequence;
} DSMIL_SECRET("biometric_frame") dsv4l2_biometric_frame_t;

/**
 * Open a v4l2 device with optional profile
 *
 * @param device_path Path to device (e.g., "/dev/video0")
 * @param profile Optional profile to apply (can be NULL)
 * @param out_dev Output device handle
 * @return 0 on success, negative error code on failure
 */
int dsv4l2_open_device(
    const char *device_path,
    const dsv4l2_profile_t *profile,
    dsv4l2_device_t **out_dev
);

/**
 * Close a device
 *
 * @param dev Device handle
 */
void dsv4l2_close_device(dsv4l2_device_t *dev);

/**
 * Capture a generic frame
 *
 * @param dev Device handle
 * @param out_frame Output frame buffer
 * @return 0 on success, negative error code on failure
 */
int dsv4l2_capture_frame(
    dsv4l2_device_t *dev,
    dsv4l2_frame_t *out_frame
) DSMIL_REQUIRES_TEMPEST_CHECK;

/**
 * Capture an iris frame (biometric, secret region)
 *
 * @param dev Device handle
 * @param out_frame Output biometric frame buffer
 * @return 0 on success, negative error code on failure
 */
int DSMIL_SECRET_REGION
dsv4l2_capture_iris(
    dsv4l2_device_t *dev,
    dsv4l2_biometric_frame_t *out_frame
) DSMIL_REQUIRES_TEMPEST_CHECK;

/**
 * Start streaming
 *
 * @param dev Device handle
 * @return 0 on success, negative error code on failure
 */
int dsv4l2_start_stream(dsv4l2_device_t *dev);

/**
 * Stop streaming
 *
 * @param dev Device handle
 * @return 0 on success, negative error code on failure
 */
int dsv4l2_stop_stream(dsv4l2_device_t *dev);

/**
 * Set format
 *
 * @param dev Device handle
 * @param pixel_format fourcc pixel format
 * @param width Width in pixels
 * @param height Height in pixels
 * @return 0 on success, negative error code on failure
 */
int dsv4l2_set_format(
    dsv4l2_device_t *dev,
    uint32_t pixel_format,
    uint32_t width,
    uint32_t height
);

/**
 * Set frame rate
 *
 * @param dev Device handle
 * @param fps_num Numerator
 * @param fps_den Denominator
 * @return 0 on success, negative error code on failure
 */
int dsv4l2_set_framerate(
    dsv4l2_device_t *dev,
    uint32_t fps_num,
    uint32_t fps_den
);

/**
 * Get device info
 *
 * @param dev Device handle
 * @param driver Output driver name (min 16 bytes)
 * @param card Output card name (min 32 bytes)
 * @param bus_info Output bus info (min 32 bytes)
 * @return 0 on success, negative error code on failure
 */
int dsv4l2_get_info(
    dsv4l2_device_t *dev,
    char *driver,
    char *card,
    char *bus_info
);

/**
 * Get a control value
 *
 * @param dev Device handle
 * @param control_id V4L2 control ID
 * @param value Output value
 * @return 0 on success, negative error code on failure
 */
int dsv4l2_get_control(dsv4l2_device_t *dev, uint32_t control_id, int32_t *value);

/**
 * Set a control value
 *
 * @param dev Device handle
 * @param control_id V4L2 control ID
 * @param value Value to set
 * @return 0 on success, negative error code on failure
 */
int dsv4l2_set_control(dsv4l2_device_t *dev, uint32_t control_id, int32_t value);

/**
 * Enumerate all controls
 *
 * @param dev Device handle
 * @param callback Callback function for each control
 * @param user_data User data passed to callback
 * @return 0 on success, negative error code on failure
 */
int dsv4l2_enum_controls(
    dsv4l2_device_t *dev,
    int (*callback)(const struct v4l2_queryctrl *qctrl, void *user_data),
    void *user_data
);

/**
 * Convert control name to ID
 *
 * @param name Control name
 * @param out_id Output control ID
 * @return 0 on success, -ENOENT if not found
 */
int dsv4l2_control_name_to_id(const char *name, uint32_t *out_id);

#ifdef __cplusplus
}
#endif

#endif /* DSV4L2_CORE_H */
