/*
 * dsv4l2_core.c
 *
 * Core device management for DSV4L2
 * Phase 2 implementation with v4l2 I/O
 */

#include "dsv4l2_core.h"
#include "dsv4l2_tempest.h"
#include "dsv4l2rt.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

/* Buffer info for MMAP */
typedef struct {
    void *start;
    size_t length;
} buffer_info_t;

int dsv4l2_open_device(
    const char *device_path,
    const dsv4l2_profile_t *profile,
    dsv4l2_device_t **out_dev
) {
    if (!device_path || !out_dev) {
        return -EINVAL;
    }

    /* Allocate extended device handle */
    dsv4l2_device_ex_t *dev_ex = calloc(1, sizeof(*dev_ex));
    if (!dev_ex) {
        return -ENOMEM;
    }

    /* Open device */
    dev_ex->fd = open(device_path, O_RDWR | O_NONBLOCK);
    if (dev_ex->fd < 0) {
        free(dev_ex);
        return -errno;
    }

    /* Set up device path */
    strncpy(dev_ex->dev_path_buf, device_path, sizeof(dev_ex->dev_path_buf) - 1);
    dev_ex->dev_path = dev_ex->dev_path_buf;
    dev_ex->tempest_state = DSV4L2_TEMPEST_DISABLED;

    /* Apply profile if provided */
    if (profile) {
        dev_ex->profile = malloc(sizeof(*profile));
        if (dev_ex->profile) {
            memcpy(dev_ex->profile, profile, sizeof(*profile));
            strncpy(dev_ex->role_buf, profile->role, sizeof(dev_ex->role_buf) - 1);
            dev_ex->role = dev_ex->role_buf;
            dev_ex->layer = 0; /* Set appropriate layer */
            /* TODO: Apply profile settings in Phase 2 */
        }
    }

    /* Return as base type */
    *out_dev = (dsv4l2_device_t *)dev_ex;
    return 0;
}

void dsv4l2_close_device(dsv4l2_device_t *dev) {
    if (!dev) {
        return;
    }

    dsv4l2_device_ex_t *dev_ex = (dsv4l2_device_ex_t *)dev;

    if (dev_ex->streaming) {
        dsv4l2_stop_stream(dev);
    }

    /* Unmap buffers */
    if (dev_ex->buffers) {
        buffer_info_t *bufs = (buffer_info_t *)dev_ex->buffers;
        for (int i = 0; i < dev_ex->num_buffers; i++) {
            if (bufs[i].start && bufs[i].start != MAP_FAILED) {
                munmap(bufs[i].start, bufs[i].length);
            }
        }
        free(dev_ex->buffers);
    }

    if (dev_ex->current_format) {
        free(dev_ex->current_format);
    }

    if (dev_ex->current_parm) {
        free(dev_ex->current_parm);
    }

    if (dev_ex->fd >= 0) {
        close(dev_ex->fd);
    }

    if (dev_ex->profile) {
        free(dev_ex->profile);
    }

    free(dev_ex);
}

int dsv4l2_capture_frame(
    dsv4l2_device_t *dev,
    dsv4l2_frame_t *out_frame
) {
    if (!dev || !out_frame) {
        return -EINVAL;
    }

    dsv4l2_device_ex_t *dev_ex = (dsv4l2_device_ex_t *)dev;

    /* TEMPEST policy check (required by DSLLVM) */
    dsv4l2_tempest_state_t state = dsv4l2_get_tempest_state(dev);
    int policy_rc = dsv4l2_policy_check_capture(dev, state, "dsv4l2_capture_frame");
    if (policy_rc != 0) {
        return -EACCES;
    }

    if (!dev_ex->streaming) {
        return -EINVAL;
    }

    /* Dequeue buffer */
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(dev_ex->fd, VIDIOC_DQBUF, &buf) < 0) {
        return -errno;
    }

    /* Get buffer data */
    buffer_info_t *bufs = (buffer_info_t *)dev_ex->buffers;
    out_frame->data = (uint8_t *)bufs[buf.index].start;
    out_frame->len = buf.bytesused;
    out_frame->timestamp_ns = buf.timestamp.tv_sec * 1000000000ULL + buf.timestamp.tv_usec * 1000ULL;
    out_frame->sequence = buf.sequence;

    /* Re-queue buffer */
    if (ioctl(dev_ex->fd, VIDIOC_QBUF, &buf) < 0) {
        return -errno;
    }

    dsv4l2rt_log_capture_start((uint32_t)(uintptr_t)dev);
    dsv4l2rt_log_capture_end((uint32_t)(uintptr_t)dev, 0);

    return 0;
}

int dsv4l2_capture_iris(
    dsv4l2_device_t *dev,
    dsv4l2_biometric_frame_t *out_frame
) {
    if (!dev || !out_frame) {
        return -EINVAL;
    }

    dsv4l2_device_ex_t *dev_ex = (dsv4l2_device_ex_t *)dev;

    /* TEMPEST policy check */
    dsv4l2_tempest_state_t state = dsv4l2_get_tempest_state(dev);
    int policy_rc = dsv4l2_policy_check_capture(dev, state, "dsv4l2_capture_iris");
    if (policy_rc != 0) {
        return -EACCES;
    }

    if (!dev_ex->streaming) {
        return -EINVAL;
    }

    /* Same as regular capture, but with biometric classification */
    struct v4l2_buffer buf;
    memset(&buf, 0, sizeof(buf));
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(dev_ex->fd, VIDIOC_DQBUF, &buf) < 0) {
        return -errno;
    }

    buffer_info_t *bufs = (buffer_info_t *)dev_ex->buffers;
    out_frame->data = (uint8_t *)bufs[buf.index].start;
    out_frame->len = buf.bytesused;
    out_frame->timestamp_ns = buf.timestamp.tv_sec * 1000000000ULL + buf.timestamp.tv_usec * 1000ULL;
    out_frame->sequence = buf.sequence;

    /* Re-queue buffer */
    if (ioctl(dev_ex->fd, VIDIOC_QBUF, &buf) < 0) {
        return -errno;
    }

    dsv4l2rt_log_capture_start((uint32_t)(uintptr_t)dev);
    dsv4l2rt_log_capture_end((uint32_t)(uintptr_t)dev, 0);

    return 0;
}

int dsv4l2_start_stream(dsv4l2_device_t *dev) {
    if (!dev) {
        return -EINVAL;
    }

    dsv4l2_device_ex_t *dev_ex = (dsv4l2_device_ex_t *)dev;

    if (dev_ex->streaming) {
        return 0; /* Already streaming */
    }

    /* Request buffers if not already done */
    if (dev_ex->num_buffers == 0) {
        int buffer_count = (dev_ex->profile && dev_ex->profile->buffer_count > 0)
                          ? dev_ex->profile->buffer_count : 4;

        struct v4l2_requestbuffers req;
        memset(&req, 0, sizeof(req));
        req.count = buffer_count;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;

        if (ioctl(dev_ex->fd, VIDIOC_REQBUFS, &req) < 0) {
            return -errno;
        }

        /* Allocate buffer info array */
        dev_ex->buffers = calloc(req.count, sizeof(buffer_info_t));
        if (!dev_ex->buffers) {
            return -ENOMEM;
        }
        dev_ex->num_buffers = req.count;

        /* Map buffers */
        buffer_info_t *bufs = (buffer_info_t *)dev_ex->buffers;
        for (unsigned int i = 0; i < req.count; i++) {
            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;

            if (ioctl(dev_ex->fd, VIDIOC_QUERYBUF, &buf) < 0) {
                return -errno;
            }

            bufs[i].length = buf.length;
            bufs[i].start = mmap(NULL, buf.length,
                                 PROT_READ | PROT_WRITE,
                                 MAP_SHARED,
                                 dev_ex->fd, buf.m.offset);

            if (bufs[i].start == MAP_FAILED) {
                return -errno;
            }
        }

        /* Queue all buffers */
        for (int i = 0; i < dev_ex->num_buffers; i++) {
            struct v4l2_buffer buf;
            memset(&buf, 0, sizeof(buf));
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;

            if (ioctl(dev_ex->fd, VIDIOC_QBUF, &buf) < 0) {
                return -errno;
            }
        }
    }

    /* Start streaming */
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(dev_ex->fd, VIDIOC_STREAMON, &type) < 0) {
        return -errno;
    }

    dev_ex->streaming = 1;
    return 0;
}

int dsv4l2_stop_stream(dsv4l2_device_t *dev) {
    if (!dev) {
        return -EINVAL;
    }

    dsv4l2_device_ex_t *dev_ex = (dsv4l2_device_ex_t *)dev;

    if (!dev_ex->streaming) {
        return 0; /* Not streaming */
    }

    /* Stop streaming */
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl(dev_ex->fd, VIDIOC_STREAMOFF, &type) < 0) {
        return -errno;
    }

    dev_ex->streaming = 0;
    return 0;
}

int dsv4l2_set_format(
    dsv4l2_device_t *dev,
    uint32_t pixel_format,
    uint32_t width,
    uint32_t height
) {
    if (!dev) {
        return -EINVAL;
    }

    dsv4l2_device_ex_t *dev_ex = (dsv4l2_device_ex_t *)dev;

    struct v4l2_format fmt;
    memset(&fmt, 0, sizeof(fmt));
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = width;
    fmt.fmt.pix.height = height;
    fmt.fmt.pix.pixelformat = pixel_format;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;

    if (ioctl(dev_ex->fd, VIDIOC_S_FMT, &fmt) < 0) {
        return -errno;
    }

    /* Store current format */
    if (!dev_ex->current_format) {
        dev_ex->current_format = malloc(sizeof(struct v4l2_format));
    }
    if (dev_ex->current_format) {
        memcpy(dev_ex->current_format, &fmt, sizeof(fmt));
    }

    dsv4l2rt_log_format_change((uint32_t)(uintptr_t)dev, pixel_format, width, height);
    return 0;
}

int dsv4l2_set_framerate(
    dsv4l2_device_t *dev,
    uint32_t fps_num,
    uint32_t fps_den
) {
    if (!dev) {
        return -EINVAL;
    }

    dsv4l2_device_ex_t *dev_ex = (dsv4l2_device_ex_t *)dev;

    struct v4l2_streamparm parm;
    memset(&parm, 0, sizeof(parm));
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (ioctl(dev_ex->fd, VIDIOC_G_PARM, &parm) < 0) {
        return -errno;
    }

    parm.parm.capture.timeperframe.numerator = fps_den;
    parm.parm.capture.timeperframe.denominator = fps_num;

    if (ioctl(dev_ex->fd, VIDIOC_S_PARM, &parm) < 0) {
        return -errno;
    }

    /* Store current parm */
    if (!dev_ex->current_parm) {
        dev_ex->current_parm = malloc(sizeof(struct v4l2_streamparm));
    }
    if (dev_ex->current_parm) {
        memcpy(dev_ex->current_parm, &parm, sizeof(parm));
    }

    return 0;
}

int dsv4l2_get_info(
    dsv4l2_device_t *dev,
    char *driver,
    char *card,
    char *bus_info
) {
    if (!dev) {
        return -EINVAL;
    }

    dsv4l2_device_ex_t *dev_ex = (dsv4l2_device_ex_t *)dev;

    struct v4l2_capability cap;
    if (ioctl(dev_ex->fd, VIDIOC_QUERYCAP, &cap) < 0) {
        return -errno;
    }

    if (driver) {
        strncpy(driver, (char*)cap.driver, 16);
    }
    if (card) {
        strncpy(card, (char*)cap.card, 32);
    }
    if (bus_info) {
        strncpy(bus_info, (char*)cap.bus_info, 32);
    }

    return 0;
}

/* Control management functions */

int dsv4l2_get_control(dsv4l2_device_t *dev, uint32_t control_id, int32_t *value) {
    if (!dev || !value) {
        return -EINVAL;
    }

    dsv4l2_device_ex_t *dev_ex = (dsv4l2_device_ex_t *)dev;

    struct v4l2_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = control_id;

    if (ioctl(dev_ex->fd, VIDIOC_G_CTRL, &ctrl) < 0) {
        return -errno;
    }

    *value = ctrl.value;
    return 0;
}

int dsv4l2_set_control(dsv4l2_device_t *dev, uint32_t control_id, int32_t value) {
    if (!dev) {
        return -EINVAL;
    }

    dsv4l2_device_ex_t *dev_ex = (dsv4l2_device_ex_t *)dev;

    struct v4l2_control ctrl;
    memset(&ctrl, 0, sizeof(ctrl));
    ctrl.id = control_id;
    ctrl.value = value;

    if (ioctl(dev_ex->fd, VIDIOC_S_CTRL, &ctrl) < 0) {
        return -errno;
    }

    return 0;
}

int dsv4l2_enum_controls(
    dsv4l2_device_t *dev,
    int (*callback)(const struct v4l2_queryctrl *qctrl, void *user_data),
    void *user_data
) {
    if (!dev || !callback) {
        return -EINVAL;
    }

    dsv4l2_device_ex_t *dev_ex = (dsv4l2_device_ex_t *)dev;

    struct v4l2_queryctrl qctrl;
    memset(&qctrl, 0, sizeof(qctrl));

    /* Enumerate user controls */
    qctrl.id = V4L2_CTRL_FLAG_NEXT_CTRL;
    while (ioctl(dev_ex->fd, VIDIOC_QUERYCTRL, &qctrl) == 0) {
        if (!(qctrl.flags & V4L2_CTRL_FLAG_DISABLED)) {
            if (callback(&qctrl, user_data) != 0) {
                break;
            }
        }
        qctrl.id |= V4L2_CTRL_FLAG_NEXT_CTRL;
    }

    return 0;
}

/* Control name to ID lookup helper */
typedef struct {
    const char *name;
    uint32_t id;
} control_name_map_t;

static const control_name_map_t control_name_table[] = {
    {"brightness", V4L2_CID_BRIGHTNESS},
    {"contrast", V4L2_CID_CONTRAST},
    {"saturation", V4L2_CID_SATURATION},
    {"hue", V4L2_CID_HUE},
    {"gain", V4L2_CID_GAIN},
    {"exposure_auto", V4L2_CID_EXPOSURE_AUTO},
    {"exposure_absolute", V4L2_CID_EXPOSURE_ABSOLUTE},
    {"focus_auto", V4L2_CID_FOCUS_AUTO},
    {"focus_absolute", V4L2_CID_FOCUS_ABSOLUTE},
    {"sharpness", V4L2_CID_SHARPNESS},
    {"backlight_compensation", V4L2_CID_BACKLIGHT_COMPENSATION},
    {"power_line_frequency", V4L2_CID_POWER_LINE_FREQUENCY},
    {"white_balance_temperature_auto", V4L2_CID_AUTO_WHITE_BALANCE},
    {"white_balance_temperature", V4L2_CID_WHITE_BALANCE_TEMPERATURE},
    {NULL, 0}
};

int dsv4l2_control_name_to_id(const char *name, uint32_t *out_id) {
    if (!name || !out_id) {
        return -EINVAL;
    }

    for (int i = 0; control_name_table[i].name != NULL; i++) {
        if (strcmp(name, control_name_table[i].name) == 0) {
            *out_id = control_name_table[i].id;
            return 0;
        }
    }

    return -ENOENT;
}
