/*
 * dsv4l2_profiles.c
 *
 * Profile loading and management for DSV4L2
 * YAML parsing using libyaml
 */

#include "dsv4l2_profiles.h"
#include "dsv4l2_core.h"
#include "dsv4l2_tempest.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <yaml.h>

static char profile_dir[256] = "dsv4l2/profiles";

/* Helper: fourcc string to uint32 */
static uint32_t fourcc_to_u32(const char *fourcc) {
    if (!fourcc || strlen(fourcc) != 4) {
        return 0;
    }
    return ((uint32_t)fourcc[0]) |
           ((uint32_t)fourcc[1] << 8) |
           ((uint32_t)fourcc[2] << 16) |
           ((uint32_t)fourcc[3] << 24);
}

/* Parse YAML profile file */
int dsv4l2_profile_load_from_file(
    const char *filepath,
    dsv4l2_profile_t **out_profile
) {
    if (!filepath || !out_profile) {
        return -EINVAL;
    }

    FILE *fp = fopen(filepath, "r");
    if (!fp) {
        return -errno;
    }

    dsv4l2_profile_t *profile = calloc(1, sizeof(*profile));
    if (!profile) {
        fclose(fp);
        return -ENOMEM;
    }

    /* Initialize defaults */
    profile->buffer_count = 4;
    profile->constant_time_required = 0;
    profile->quantum_candidate = 0;

    yaml_parser_t parser;
    yaml_event_t event;

    if (!yaml_parser_initialize(&parser)) {
        free(profile);
        fclose(fp);
        return -ENOMEM;
    }

    yaml_parser_set_input_file(&parser, fp);

    char current_key[64] = "";
    char context[64] = "";  /* Track nesting: controls, tempest_control, etc */
    int in_controls = 0;
    int in_tempest = 0;
    int in_mode_map = 0;

    int done = 0;
    while (!done) {
        if (!yaml_parser_parse(&parser, &event)) {
            yaml_parser_delete(&parser);
            free(profile);
            fclose(fp);
            return -EINVAL;
        }

        switch (event.type) {
            case YAML_SCALAR_EVENT: {
                const char *value = (const char *)event.data.scalar.value;

                if (current_key[0] == '\0') {
                    /* This is a key */
                    strncpy(current_key, value, sizeof(current_key) - 1);
                } else {
                    /* This is a value for current_key */
                    if (strcmp(current_key, "id") == 0) {
                        strncpy(profile->id, value, sizeof(profile->id) - 1);
                    } else if (strcmp(current_key, "role") == 0) {
                        strncpy(profile->role, value, sizeof(profile->role) - 1);
                    } else if (strcmp(current_key, "device_hint") == 0) {
                        strncpy(profile->device_hint, value, sizeof(profile->device_hint) - 1);
                    } else if (strcmp(current_key, "classification") == 0) {
                        strncpy(profile->classification, value, sizeof(profile->classification) - 1);
                    } else if (strcmp(current_key, "pixel_format") == 0) {
                        profile->pixel_format = fourcc_to_u32(value);
                    } else if (strcmp(current_key, "fps") == 0) {
                        profile->fps_num = atoi(value);
                        profile->fps_den = 1;
                    } else if (strcmp(current_key, "meta_device") == 0) {
                        strncpy(profile->meta_device_path, value, sizeof(profile->meta_device_path) - 1);
                    } else if (strcmp(current_key, "buffer_count") == 0) {
                        profile->buffer_count = atoi(value);
                    } else if (strcmp(current_key, "constant_time_required") == 0) {
                        profile->constant_time_required = (strcmp(value, "true") == 0);
                    } else if (strcmp(current_key, "quantum_candidate") == 0) {
                        profile->quantum_candidate = (strcmp(value, "true") == 0);
                    }

                    /* Control values */
                    if (in_controls && profile->num_controls < DSV4L2_MAX_CONTROLS) {
                        /* current_key is control name, value is the setting */
                        /* For now, store as string - will resolve to ID later */
                        /* This is simplified - full implementation would use a lookup table */
                    }

                    /* TEMPEST control mapping */
                    if (in_tempest) {
                        if (strcmp(current_key, "id") == 0) {
                            profile->tempest_control.control_id = strtoul(value, NULL, 0);
                        } else if (strcmp(current_key, "auto_detect") == 0) {
                            profile->tempest_control.auto_detect = (strcmp(value, "true") == 0);
                        } else if (in_mode_map) {
                            if (strcmp(current_key, "DISABLED") == 0) {
                                profile->tempest_control.disabled_value = atoi(value);
                            } else if (strcmp(current_key, "LOW") == 0) {
                                profile->tempest_control.low_value = atoi(value);
                            } else if (strcmp(current_key, "HIGH") == 0) {
                                profile->tempest_control.high_value = atoi(value);
                            } else if (strcmp(current_key, "LOCKDOWN") == 0) {
                                profile->tempest_control.lockdown_value = atoi(value);
                            }
                        }
                    }

                    current_key[0] = '\0';
                }
                break;
            }

            case YAML_SEQUENCE_START_EVENT:
                /* Handle arrays like resolution: [1280, 720] */
                if (strcmp(current_key, "resolution") == 0) {
                    /* Next two scalars will be width, height */
                    strcpy(context, "resolution");
                }
                break;

            case YAML_MAPPING_START_EVENT:
                if (strcmp(current_key, "controls") == 0) {
                    in_controls = 1;
                } else if (strcmp(current_key, "tempest_control") == 0) {
                    in_tempest = 1;
                } else if (strcmp(current_key, "mode_map") == 0) {
                    in_mode_map = 1;
                }
                current_key[0] = '\0';
                break;

            case YAML_MAPPING_END_EVENT:
                if (in_mode_map) {
                    in_mode_map = 0;
                } else if (in_tempest) {
                    in_tempest = 0;
                } else if (in_controls) {
                    in_controls = 0;
                }
                break;

            case YAML_STREAM_END_EVENT:
                done = 1;
                break;

            default:
                break;
        }

        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);
    fclose(fp);

    *out_profile = profile;
    return 0;
}

int dsv4l2_profile_load(
    const char *device_path,
    const char *role,
    dsv4l2_profile_t **out_profile
) {
    if (!device_path || !role || !out_profile) {
        return -EINVAL;
    }

    /* Build profile filepath */
    char filepath[512];
    snprintf(filepath, sizeof(filepath), "%s/%s.yaml", profile_dir, role);

    return dsv4l2_profile_load_from_file(filepath, out_profile);
}

int dsv4l2_profile_load_by_vidpid(
    uint16_t vendor_id,
    uint16_t product_id,
    const char *role,
    dsv4l2_profile_t **out_profile
) {
    if (!role || !out_profile) {
        return -EINVAL;
    }

    /* For now, just use role-based loading */
    /* Full implementation would search for matching VID:PID */
    (void)vendor_id;
    (void)product_id;

    return dsv4l2_profile_load(NULL, role, out_profile);
}

int dsv4l2_profile_apply(
    dsv4l2_device_t *dev,
    const dsv4l2_profile_t *profile
) {
    if (!dev || !profile) {
        return -EINVAL;
    }

    int rc;

    /* Apply format if specified */
    if (profile->pixel_format && profile->width && profile->height) {
        rc = dsv4l2_set_format(dev, profile->pixel_format, profile->width, profile->height);
        if (rc != 0 && rc != -ENOSYS) {
            return rc;
        }
    }

    /* Apply framerate if specified */
    if (profile->fps_num > 0) {
        uint32_t fps_den = (profile->fps_den > 0) ? profile->fps_den : 1;
        rc = dsv4l2_set_framerate(dev, profile->fps_num, fps_den);
        /* Ignore errors - not all devices support framerate setting */
    }

    /* Apply control presets */
    for (int i = 0; i < profile->num_controls; i++) {
        rc = dsv4l2_set_control(dev, profile->controls[i].id, profile->controls[i].value);
        /* Continue on errors - some controls may not be available */
    }

    /* Apply TEMPEST control mapping if specified */
    if (profile->tempest_control.control_id != 0) {
        /* Control ID explicitly specified in profile */
        rc = dsv4l2_apply_tempest_mapping(dev);
    } else if (profile->tempest_control.auto_detect) {
        /* Auto-detect TEMPEST control */
        uint32_t tempest_id;
        rc = dsv4l2_discover_tempest_control(dev, &tempest_id);
        if (rc == 0) {
            /* Found a TEMPEST control, set initial state */
            dsv4l2_set_tempest_state(dev, DSV4L2_TEMPEST_DISABLED);
        }
    }

    return 0;
}

void dsv4l2_profile_free(dsv4l2_profile_t *profile) {
    if (profile) {
        free(profile);
    }
}

const char* dsv4l2_get_profile_dir(void) {
    return profile_dir;
}

void dsv4l2_set_profile_dir(const char *dir_path) {
    if (dir_path) {
        strncpy(profile_dir, dir_path, sizeof(profile_dir) - 1);
        profile_dir[sizeof(profile_dir) - 1] = '\0';
    }
}
