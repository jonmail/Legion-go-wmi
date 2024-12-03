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
	{ LENOVO_OTHER_METHOD_GUID, NULL },
	{}
};

/* Tunable Attributes */
struct ll_tunables {
	u32 ppt_pl1_spl;
	u32 ppt_pl2_sppt;
	u32 ppt_fppt;
	u32 cpu_temp;
	u32 ppt_apu_spl;
};

static const struct class *fw_attr_class;

static struct other_method_wmi om_wmi = { .mutex = __MUTEX_INITIALIZER(
						  om_wmi.mutex) };

struct other_method_attr_group {
	const struct attribute_group *attr_group;
	char *data_guid;
};

static int other_method_fan_profile_get(int *sel_prof)
{
	struct platform_profile_handler *pprof;
	int err;

	pprof = &drvdata.gz_wmi->pprof;

	err = gamezone_wmi_fan_profile_get(pprof, sel_prof);
	if (err)
		return err;

	return 0;
}

/* Simple attribute creation */

/*
 * att_current_value_store() - Send an int to wmi_dev, check in within min/max
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
ssize_t attr_current_value_store(struct kobject *kobj,
				 struct kobj_attribute *attr, const char *buf,
				 size_t count, u32 *store_value, u8 device_id,
				 u8 feature_id)
{
	pr_info("lenovo_legion_wmi_other: attr_current_value_store start\n");
	u32 value;
	int min = 1;
	int max = 50;

	int sel_prof; /* Current fan profile mode */
	int err;
	int retval;
	struct wmi_device *wdev = drvdata.om_wmi->wdev;

	err = other_method_fan_profile_get(&sel_prof);

	if (err) {
		pr_err("Error getting gamezone fan profile.\n");
		return err;
	}

	err = kstrtouint(buf, 10, &value);
	if (err) {
		pr_err("Error converting value to int.\n");
		return err;
	}

	// TODO: Get min/max from LENOVO_CAPABILITY_DATA_01
	if (value < min || value > max) {
		pr_warn("Value %d is not between %d and %d.\n", value, min,
			max);
		return -EINVAL;
	}

	// Construct the WMI attribute id from the given args.
	struct om_attribute_id attribute_id = { sel_prof << 8, feature_id,
						device_id };

	err = lenovo_legion_evaluate_method_2(wdev, 0x0,
					      WMI_METHOD_ID_VALUE_SET,
					      *(int *)&attribute_id, value,
					      &retval);

	pr_info("lenovo_legion_wmi_other: retval: %d\n", retval);

	if (err) {
		pr_err("Error setting attribute");
		return err;
	}

	if (store_value != NULL)
		*store_value = value;

	sysfs_notify(kobj, NULL, attr->attr.name);
	pr_info("lenovo_legion_wmi_other: attr_current_value_store end\n");
	return count;
}

/*
 * attr_current_value_show() - Get the current value of the given attribute
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
ssize_t attr_current_value_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf,
				u8 device_id, u8 feature_id)
{
	int sel_prof; /* Current fan profile mode */
	int err;
	int retval;
	struct wmi_device *wdev = drvdata.om_wmi->wdev;

	err = other_method_fan_profile_get(&sel_prof);

	if (err) {
		pr_err("Error getting gamezone fan profile.\n");
		return err;
	}

	// Construct the WMI attribute id from the given args.
	struct om_attribute_id attribute_id = { sel_prof << 8, feature_id,
						device_id };

	err = lenovo_legion_evaluate_method_1(wdev, 0x0,
					      WMI_METHOD_ID_VALUE_GET,
					      *(int *)&attribute_id, &retval);

	if (err) {
		pr_err("Error getting attribute");
		return err;
	}

	return sysfs_emit(buf, "%u\n", retval);
}

/**
 * attr_cap_data_show() - Get the value of the attributes spicified
 * property from LENOVO_CAPABILITY_DATA_01
 * @kobj: Pointer to the driver object.
 * @kobj_attribute: Pointer to the attribute calling this function.
 * @buf: The buffer to write to.
 * @retval: Pointer to returned data.
 * @device_id: The WMI functions Device ID to use.
 * @feature_id: The WMI functions Feature ID to use.
 * @prop: The property of this attribute to be read.
 *
 * This function is intended to be generic so it can be called from any "_show"
 * attribute which works only with integers.
 *
 * If the WMI is success, then the sysfs attribute is notified.
 *
 * Returns: Either count, or an error.
 */
ssize_t attr_cap_data_show(struct kobject *kobj, struct kobj_attribute *attr,
			   char *buf, u8 device_id, u8 feature_id,
			   enum attribute_property prop)
{
	pr_info("attr_cap_data_show for property %d\n", prop);
	int sel_prof; /* Current fan profile mode */
	int err;
	int retval;
	struct capability_data_01 cap_data;

	err = other_method_fan_profile_get(&sel_prof);
	if (err) {
		pr_err("Error getting gamezone fan profile.\n");
		return err;
	}

	pr_info("Got fan mode: %d\n", sel_prof);

	// Construct the WMI attribute id from the given args.
	struct om_attribute_id attribute_id = { sel_prof << 8, feature_id,
						device_id };

	err = capdata_01_wmi_get(attribute_id, &cap_data);
	if (err) {
		pr_err("Got no data YO!");
	}
	pr_info("Got Capability Data: ");
	pr_info("Step: %d, ", cap_data.step);
	pr_info("Default Value: %d, ", cap_data.default_value);
	pr_info("Max Value: %d, ", cap_data.max_value);
	pr_info("Min Value: %d\n", cap_data.min_value);

	switch (prop) {
	case DEFAULT_VAL:
		retval = cap_data.default_value;
		break;
	case MAX_VAL:
		retval = cap_data.max_value;
		break;
	case MIN_VAL:
		retval = cap_data.min_value;
		break;
	case STEP_VAL:
		retval = cap_data.step;
		break;
	default:
		return -EINVAL;
	}
	return sysfs_emit(buf, "%u\n", retval);
}

/* Simple attribute creation */
ATTR_GROUP_LL_TUNABLE(ppt_pl1_spl, "ppt_pl1_spl", WMI_DEVICE_ID_CPU,
		      WMI_FEATURE_ID_CPU_SPL,
		      "Set the CPU sustained power limit");
ATTR_GROUP_LL_TUNABLE(ppt_pl2_sppt, "ppt_pl2_sppt", WMI_DEVICE_ID_CPU,
		      WMI_FEATURE_ID_CPU_SPPT,
		      "Set the CPU slow package power tracking limit");
ATTR_GROUP_LL_TUNABLE(ppt_fppt, "ppt_fppt", WMI_DEVICE_ID_CPU,
		      WMI_FEATURE_ID_CPU_FPPT,
		      "Set the CPU fast package power tracking limit");
ATTR_GROUP_LL_TUNABLE(cpu_temp, "cpu_temp", WMI_DEVICE_ID_CPU,
		      WMI_FEATURE_ID_CPU_TEMP,
		      "Set the CPU thermal control limit");
ATTR_GROUP_LL_TUNABLE(ppt_apu_spl, "ppt_apu_spl", WMI_DEVICE_ID_CPU,
		      WMI_FEATURE_ID_APU_SPL,
		      "Set the APU sustained power limit");

static const struct other_method_attr_group other_method_attr_groups[] = {
	{ &ppt_pl1_spl_attr_group, LENOVO_CAPABILITY_DATA_01_GUID },
	{ &ppt_pl2_sppt_attr_group, LENOVO_CAPABILITY_DATA_01_GUID },
	{ &ppt_fppt_attr_group, LENOVO_CAPABILITY_DATA_01_GUID },
	{ &cpu_temp_attr_group, LENOVO_CAPABILITY_DATA_01_GUID },
	{ &ppt_apu_spl_attr_group, LENOVO_CAPABILITY_DATA_01_GUID },
	{},
};

static int om_fw_attr_add(void)
{
	int err, i;

	err = fw_attributes_class_get(&fw_attr_class);
	if (err) {
		pr_err("Failed to get firmware_attributes_class.\n");
		return err;
	}

	om_wmi.fw_attr_dev = device_create(fw_attr_class, NULL, MKDEV(0, 0),
					   NULL, "%s", DRIVER_NAME);
	if (IS_ERR(om_wmi.fw_attr_dev)) {
		pr_err("Failed to create firmware_attributes_class device.\n");
		err = PTR_ERR(om_wmi.fw_attr_dev);
		goto fail_class_get;
	}

	om_wmi.fw_attr_kset = kset_create_and_add("attributes", NULL,
						  &om_wmi.fw_attr_dev->kobj);
	if (!om_wmi.fw_attr_kset) {
		pr_err("Failed to create firmware_attributes_class kset.\n");
		err = -ENOMEM;
		goto err_destroy_classdev;
	}

	for (i = 0; i < ARRAY_SIZE(other_method_attr_groups) - 1; i++) {
		err = sysfs_create_group(
			&om_wmi.fw_attr_kset->kobj,
			other_method_attr_groups[i].attr_group);
		if (err) {
			pr_err("Failed to create sysfs-group for %s\n",
			       other_method_attr_groups[i].attr_group->name);
			goto err_remove_groups;
		}
	}

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
static int other_method_wmi_probe(struct wmi_device *wdev, const void *context)
{
	int err;

	om_wmi.wdev = wdev;
	drvdata.om_wmi = &om_wmi;
	om_wmi.ll_tunables = kzalloc(sizeof(struct ll_tunables), GFP_KERNEL);
	if (!om_wmi.ll_tunables)
		return -ENOMEM;

	err = om_fw_attr_add();
	if (err)
		return err;
	pr_info("lenovo_legion_wmi_other: Firmware attributes added\n");

	return 0;
}

static void other_method_wmi_remove(struct wmi_device *wdev)
{
	pr_info("lenovo_legion_wmi_other: Lenovo Other Method WMI remove\n");

	mutex_lock(&om_wmi.mutex);

	kset_unregister(om_wmi.fw_attr_kset);
	device_destroy(fw_attr_class, MKDEV(0, 0));
	fw_attributes_class_put();

	mutex_unlock(&om_wmi.mutex);

	return;
}

// static void other_method_wmi_notify(struct wmi_device *device,
//                                     union acpi_object *data) {
//   pr_info("lenovo_legion_wmi_other: Lenovo Other Method WMI notify\n");
//   return;
// }

static struct wmi_driver other_method_wmi_driver = {
    .driver =
        {
            .name = "other_method_wmi",
        },
    .id_table = other_method_wmi_id_table,
    .probe = other_method_wmi_probe,
    .remove = other_method_wmi_remove,
    //.notify = other_method_wmi_notify,
};

module_wmi_driver(other_method_wmi_driver);

MODULE_IMPORT_NS(LL_WMI);
MODULE_IMPORT_NS(GZ_WMI);
MODULE_IMPORT_NS(CAPDATA_WMI);
MODULE_DEVICE_TABLE(wmi, other_method_wmi_id_table);
MODULE_AUTHOR("Derek J. Clark <derekjohn.clark@gmail.com>");
MODULE_DESCRIPTION("Lenovo Legion Other Method Driver");
MODULE_LICENSE("GPL");
