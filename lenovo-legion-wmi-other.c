// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Lenovo Legion other method driver. This driver uses the fw_attributes
 * class to expose the various WMI functions provided by the "Other Method" WMI
 * interface. This enables CPU and GPU power limit as well as various other
 * attributes for devices that fall under the "Gaming Series" of Lenovo Legion
 * devices. Each attribute exposed by the Other Method interface has a
 * corresponding data struct that allows the driver to probe details about the
 * attribute such as set/get support, step, min, max, and default value. These
 * attributes typically don't fit anywhere else in the sysfs and
 * are set in Windows using one of Lenovo's multiple user applications.
 *
 * Copyright(C) 2024 Derek J. Clark <derekjohn.clark@gmail.com>
 *
 */

#include "lenovo-legion-wmi.h"

#include "firmware_attributes_class.h"

static const struct wmi_device_id other_method_wmi_id_table[] = {
    {LENOVO_OTHER_METHOD_GUID, NULL}, {}};

/* Tunable Attributes */
struct ll_tunables {
  u32 ppt_pl1_spl;
  u32 ppt_pl2_sppt;
  u32 ppt_fppt;
  u32 cpu_temp;
  u32 ppt_apu_spl;
};

static const struct class *fw_attr_class;

static struct other_method_wmi om_wmi = {.mutex =
                                             __MUTEX_INITIALIZER(om_wmi.mutex)};

struct om_attribute_id {
  u32 device_id : 8;
  u32 feature_id : 8;
  u32 type_id : 16;
} __packed;

struct other_method_attr_group {
  const struct attribute_group *attr_group;
  char *data_guid;
};

// static struct capability_data_00 {
//	u32 ids;
//	u32 capability;
//	u32 default_value;
// };
//
// static struct capability_data_01 {
//	u32 ids;
//	u32 capability;
//	u32 default_value;
//	u32 step;
//	u32 min_value;
//	u32 max_value;
// };

static int other_method_fan_profile_get(int *mode) {
  struct platform_profile_handler *pprof;

  // Get the current platform_profile
  pprof = &drvdata.gz_wmi->pprof;
  return gamezone_wmi_fan_profile_get(pprof, mode);
}

/**
 * attr_int_store() - Send an int to wmi_dev, check in within min/max exclusive
 * @kobj: Pointer to the driver object.
 * @kobj_attribute: Pointer to the attribute calling this function.
 * @buf: The buffer to read from, this is parsed to `int` type.
 * @count: Required by sysfs attribute macros, pass in from the callee attr.
 * @store_value: Pointer to where the parsed value should be stored.
 * @device_id: The WMI functions Device ID to use.
 * @feature_id: The WMI functions Feature ID to use.
 *
 * This function is intended to be generic so it can be called from any "_store"
 * attribute which works only with integers. The integer to be send to the WMI
 * method is range checked and an error returned if out of range.
 *
 * If the value is valid and WMI is success, then the sysfs attribute is
 * notified.
 *
 * Returns: Either count, or an error.
 */
static ssize_t attr_compat_data_store(struct kobject *kobj,
                                      struct kobj_attribute *attr,
                                      const char *buf, size_t count,
                                      u32 *store_value, u8 device_id,
                                      u8 feature_id) {
  u32 value;
  int min = 1;
  int max = 300;
  int mode; /* Current fan profile mode */
  int err = other_method_fan_profile_get(&mode);
  struct wmi_device *wdev = drvdata.om_wmi->wdev;

  if (err) {
    pr_err("Error getting gamezone fan profile.\n");
    return err;
  }

  err = kstrtouint(buf, 10, &value);
  if (err)
    return err;

  // TODO: Get min/max from LENOVO_CAPABILITY_DATA_01
  if (value < min || value > max)
    return -EINVAL;

  // Construct the WMI attribute id from the given args.
  struct om_attribute_id attribute_id = {device_id, feature_id, mode << 8};

  err = lenovo_legion_evaluate_method_2(wdev, 0x0, WMI_METHOD_ID_VALUE_SET,
                                        *(int *)&attribute_id, value, NULL);

  if (err) {
    pr_err("Error setting attribute");
    return err;
  }

  if (store_value != NULL)
    *store_value = value;

  sysfs_notify(kobj, NULL, attr->attr.name);

  return count;
}

/* Simple attribute creation */

/**
 * attr_int_show() - Get the current value of the given attribute
 * @kobj: Pointer to the driver object.
 * @kobj_attribute: Pointer to the attribute calling this function.
 * @buf: The buffer to write to.
 * @retval: Pointer to returned data.
 * @device_id: The WMI functions Device ID to use.
 * @feature_id: The WMI functions Feature ID to use.
 *
 * This function is intended to be generic so it can be called from any "_show"
 * attribute which works only with integers.
 *
 * If the WMI is success, then the sysfs attribute is notified.
 *
 * Returns: Either count, or an error.
 */
static ssize_t attr_compat_data_show(struct kobject *kobj,
                                     struct kobj_attribute *attr, char *buf,
                                     u8 device_id, u8 feature_id) {
  int mode; /* Current fan profile mode */
  int err = other_method_fan_profile_get(&mode);
  int retval;
  struct wmi_device *wdev = drvdata.om_wmi->wdev;

  if (err) {
    pr_err("Error getting gamezone fan profile.\n");
    return err;
  }

  // Construct the WMI attribute id from the given args.
  struct om_attribute_id attribute_id = {device_id, feature_id, mode << 8};

  err = lenovo_legion_evaluate_method_1(wdev, 0x0, WMI_METHOD_ID_VALUE_GET,
                                        *(int *)&attribute_id, &retval);

  if (err) {
    pr_err("Error getting attribute");
    return err;
  }

  // TODO: parse retval into buf

  sysfs_notify(kobj, NULL, attr->attr.name);

  return 0;
}

/* Simple attribute creation */
ATTR_GROUP_LL_TUNABLE(ppt_pl1_spl, "ppt_pl1_spl", WMI_DEVICE_ID_CPU,
                      WMI_FEATURE_ID_CPU_SPL, 1,
                      "Set the CPU sustained power limit");
ATTR_GROUP_LL_TUNABLE(ppt_pl2_sppt, "ppt_pl2_sppt", WMI_DEVICE_ID_CPU,
                      WMI_FEATURE_ID_CPU_SPPT, 1,
                      "Set the CPU slow package power tracking limit");
ATTR_GROUP_LL_TUNABLE(ppt_fppt, "ppt_fppt", WMI_DEVICE_ID_CPU,
                      WMI_FEATURE_ID_CPU_FPPT, 1,
                      "Set the CPU fast package power tracking limit");
ATTR_GROUP_LL_TUNABLE(cpu_temp, "cpu_temp", WMI_DEVICE_ID_CPU,
                      WMI_FEATURE_ID_CPU_TEMP, 1,
                      "Set the CPU thermal control limit");
ATTR_GROUP_LL_TUNABLE(ppt_apu_spl, "ppt_apu_spl", WMI_DEVICE_ID_CPU,
                      WMI_FEATURE_ID_APU_SPL, 1,
                      "Set the APU sustained power limit");

static const struct other_method_attr_group other_method_attr_groups[] = {
    {&ppt_pl1_spl_attr_group, LENOVO_CAPABILITY_DATA_01_GUID},
    {&ppt_pl2_sppt_attr_group, LENOVO_CAPABILITY_DATA_01_GUID},
    {&ppt_fppt_attr_group, LENOVO_CAPABILITY_DATA_01_GUID},
    {&cpu_temp_attr_group, LENOVO_CAPABILITY_DATA_01_GUID},
    {&ppt_apu_spl_attr_group, LENOVO_CAPABILITY_DATA_01_GUID},
    {},
};

static int om_fw_attr_add(void) {
  int err, i;

  pr_info("Started om_fw_attr_add.\n");
  err = fw_attributes_class_get(&fw_attr_class);
  if (err) {
    pr_err("Failed to get firmware_attributes_class.\n");
    return err;
  }
  pr_info("Got firmware_attributes_class.\n");

  om_wmi.fw_attr_dev =
      device_create(fw_attr_class, NULL, MKDEV(0, 0), NULL, "%s", DRIVER_NAME);
  if (IS_ERR(om_wmi.fw_attr_dev)) {
    pr_err("Failed to create firmware_attributes_class device.\n");
    err = PTR_ERR(om_wmi.fw_attr_dev);
    goto fail_class_get;
  }
  pr_info("Created firmware_attributes_class device.\n");

  om_wmi.fw_attr_kset =
      kset_create_and_add("attributes", NULL, &om_wmi.fw_attr_dev->kobj);
  if (!om_wmi.fw_attr_kset) {
    pr_err("Failed to create firmware_attributes_class kset.\n");
    err = -ENOMEM;
    goto err_destroy_classdev;
  }
  pr_info("Created firmware_attributes_class kset.\n");

  for (i = 0; i < ARRAY_SIZE(other_method_attr_groups) - 1; i++) {
    err = sysfs_create_group(&om_wmi.fw_attr_kset->kobj,
                             other_method_attr_groups[i].attr_group);
    if (err) {
      pr_err("Failed to create sysfs-group for %s\n",
             other_method_attr_groups[i].attr_group->name);
      goto err_remove_groups;
    }
    pr_info("Created sysfs-group for %s\n",
            other_method_attr_groups[i].attr_group->name);
  }

  pr_info("Finished om_fw_attr_add.\n");

  return 0;

err_remove_groups:
  while (--i >= 0) {
    sysfs_remove_group(&om_wmi.fw_attr_kset->kobj,
                       other_method_attr_groups[i].attr_group);
  }
err_destroy_classdev:
  device_destroy(fw_attr_class, MKDEV(0, 0));
fail_class_get:
  fw_attributes_class_put();
  return err;
}

/* Driver Setup */
static int other_method_wmi_probe(struct wmi_device *wdev,
                                  const void *context) {
  int err;
  pr_info("Lenovo Other Method WMI probe start\n");

  om_wmi.wdev = wdev;
  pr_info("Set wdev\n");
  om_wmi.ll_tunables = kzalloc(sizeof(struct ll_tunables), GFP_KERNEL);
  if (!om_wmi.ll_tunables)
    return -ENOMEM;
  pr_info("Set ll_tunables\n");

  err = om_fw_attr_add();
  if (err)
    return err;
  pr_info("Firmware attributes added\n");

  drvdata.om_wmi = &om_wmi;
  pr_info("drvdata set. Probe end.\n");

  return 0;
}

static void other_method_wmi_remove(struct wmi_device *wdev) {
  pr_info("Lenovo Other Method WMI remove\n");
  return;
}

static void other_method_wmi_notify(struct wmi_device *device,
                                    union acpi_object *data) {
  pr_info("Lenovo Other Method WMI notify\n");
  mutex_lock(&om_wmi.mutex);

  kset_unregister(om_wmi.fw_attr_kset);
  device_destroy(fw_attr_class, MKDEV(0, 0));
  fw_attributes_class_put();

  mutex_unlock(&om_wmi.mutex);

  return;
}

static struct wmi_driver other_method_wmi_driver = {
    .driver =
        {
            .name = "other_method_wmi",
        },
    .id_table = other_method_wmi_id_table,
    .probe = other_method_wmi_probe,
    .remove = other_method_wmi_remove,
    .notify = other_method_wmi_notify,
    /* .no_notify_data = true,
    .no_singleton = true, */
};

module_wmi_driver(other_method_wmi_driver);

MODULE_IMPORT_NS(LL_WMI);
MODULE_IMPORT_NS(GZ_WMI);
MODULE_DEVICE_TABLE(wmi, other_method_wmi_id_table);
MODULE_AUTHOR("Derek J. Clark <derekjohn.clark@gmail.com>");
MODULE_DESCRIPTION("Lenovo Legion Other Method Driver");
MODULE_LICENSE("GPL");
