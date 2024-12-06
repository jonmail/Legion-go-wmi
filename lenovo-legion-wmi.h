// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Lenovo Legion WMI interface driver. The Lenovo Legion WMI interface is
 * broken up into multiple GUID interfaces that require cross-references
 * between GUID's for some functionality. The "Custom Mode" interface is a
 * legacy interface for managing and displaying CPU & GPU power and hwmon
 * settings and readings. The "Other Mode" interface is a modern interface
 * that replaces or extends the "Custom Mode" interface methods. The "GameZone"
 * interface adds advanced features such as fan profiles and overclocking.
 * The "Lighting" interface adds control of various status lights related to
 * different hardware components. 
 *
 * Copyright(C) 2024 Derek J. Clark <derekjohn.clark@gmail.com>
 *
 */

#ifndef _LENOVO_LEGION_WMI_H_
#define _LENOVO_LEGION_WMI_H_

#include <linux/mutex.h>
#include <linux/platform_profile.h>
#include <linux/types.h>
#include <linux/wmi.h>

#define DRIVER_NAME "lenovo-legion-wmi"

/* Device IDs */
#define WMI_DEVICE_ID_CPU 0x01

/* CPU feature IDs */
#define WMI_FEATURE_ID_CPU_SPPT 0x01 /* Short Term Power Limit */
#define WMI_FEATURE_ID_CPU_SPL 0x02 /* Peak Power Limit */
#define WMI_FEATURE_ID_CPU_FPPT 0x03 /* Long Term Power Limit */

/* Method IDs */
#define WMI_METHOD_ID_VALUE_GET 17 /* Other Method Getter */
#define WMI_METHOD_ID_VALUE_SET 18 /* Other Method Setter */
#define WMI_METHOD_ID_SMARTFAN_SUPP 43 /* IsSupportSmartFan */
#define WMI_METHOD_ID_SMARTFAN_SET 44 /* SetSmartFanMode */
#define WMI_METHOD_ID_SMARTFAN_GET 45 /* GetSmartFanMode */

/* Platform Profile Modes */
#define SMARTFAN_MODE_QUIET 0x01
#define SMARTFAN_MODE_BALANCED 0x02
#define SMARTFAN_MODE_PERFORMANCE 0x03
#define SMARTFAN_MODE_CUSTOM 0xFF

struct gamezone_wmi {
	struct wmi_device *wdev;
	enum platform_profile_option current_profile;
	struct platform_profile_handler pprof;
	bool platform_profile_support;
};

struct other_method_wmi {
	struct wmi_device *wdev;
	struct device *fw_attr_dev;
	struct kset *fw_attr_kset;
	struct ll_tunables *ll_tunables;
	struct mutex mutex;
};

struct capdata_wmi {
	struct wmi_device *wdev;
};

struct ll_drvdata {
	struct other_method_wmi *om_wmi; /* Other method GUID device */
	struct gamezone_wmi *gz_wmi; /* Gamezone GUID device */
	struct capdata_wmi *cd01_wmi; /* Capability Data 01 GUID device */
} drvdata;

struct wmi_method_args {
	u32 arg0;
	u32 arg1;
} __packed;

struct om_attribute_id {
	u32 mode_id : 16;
	u32 feature_id : 8;
	u32 device_id : 8;
} __packed;

enum attribute_property {
	DEFAULT_VAL = 0,
	MAX_VAL,
	MIN_VAL,
	STEP_VAL,
	SUPPORTED,
};

struct capability_data_01 {
	u32 id;
	u32 capability;
	u32 default_value;
	u32 step;
	u32 min_value;
	u32 max_value;
} __packed;

static int lenovo_legion_evaluate_method(struct wmi_device *wdev, u8 instance,
					 u32 method_id, struct acpi_buffer *in,
					 struct acpi_buffer *out)
{
	acpi_status status;
	status = wmidev_evaluate_method(wdev, instance, method_id, in, out);

	if (ACPI_FAILURE(status)) {
		printk("Failed to read current platform profile\n");
		return -EIO;
	}

	return 0;
}

int lenovo_legion_evaluate_method_2(struct wmi_device *wdev, u8 instance,
				    u32 method_id, u32 arg0, u32 arg1,
				    u32 *retval);

int lenovo_legion_evaluate_method_2(struct wmi_device *wdev, u8 instance,
				    u32 method_id, u32 arg0, u32 arg1,
				    u32 *retval)
{
	int ret;
	uint32_t temp_val;
	struct wmi_method_args args = { arg0, arg1 };
	struct acpi_buffer input = { (acpi_size)sizeof(args), &args };
	struct acpi_buffer output = { ACPI_ALLOCATE_BUFFER, NULL };
	union acpi_object *ret_obj = NULL;

	ret = lenovo_legion_evaluate_method(wdev, instance, method_id, &input,
					    &output);

	if (ret) {
		pr_err("Attempt to get method_id %u value failed with error: %u\n",
		       method_id, ret);
		return ret;
	}

	if (retval) {
		ret_obj = (union acpi_object *)output.pointer;
		if (ret_obj && ret_obj->type == ACPI_TYPE_INTEGER) {
			temp_val = (u32)ret_obj->integer.value;
		}

		*retval = temp_val;
	}

	kfree(ret_obj);

	return 0;
}

int lenovo_legion_evaluate_method_1(struct wmi_device *wdev, u8 instance,
				    u32 method_id, u32 arg0, u32 *retval);

int lenovo_legion_evaluate_method_1(struct wmi_device *wdev, u8 instance,
				    u32 method_id, u32 arg0, u32 *retval)
{
	return lenovo_legion_evaluate_method_2(wdev, instance, method_id, arg0,
					       0, retval);
}

int capdata_01_wmi_get(struct om_attribute_id attr_id,
		       struct capability_data_01 *cap_data);

int gamezone_wmi_fan_profile_get(struct platform_profile_handler *pprof,
				 int *sel_prof);

/* current_value */
ssize_t attr_current_value_store(struct kobject *kobj,
				 struct kobj_attribute *attr, const char *buf,
				 size_t count, u32 *store_value, u8 device_id,
				 u8 feature_id);

ssize_t attr_current_value_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf,
				u8 device_id, u8 feature_id);

/* LENOVO_CAPABILITY_DATA_01 */
ssize_t attr_cap_data_show(struct kobject *kobj, struct kobj_attribute *attr,
			   char *buf, u8 device_id, u8 feature_id,
			   enum attribute_property prop);

static ssize_t int_type_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
	return sysfs_emit(buf, "integer\n");
}

#define __LL_ATTR_RO(_func, _name)                                    \
	{                                                             \
		.attr = { .name = __stringify(_name), .mode = 0444 }, \
		.show = _func##_##_name##_show,                       \
	}

#define __LL_ATTR_RO_AS(_name, _show)                                 \
	{                                                             \
		.attr = { .name = __stringify(_name), .mode = 0444 }, \
		.show = _show,                                        \
	}

#define __LL_ATTR_RW(_func, _name) \
	__ATTR(_name, 0644, _func##_##_name##_show, _func##_##_name##_store)

/* Shows a formatted static variable */
#define __ATTR_SHOW_FMT(_prop, _attrname, _fmt, _val)                         \
	static ssize_t _attrname##_##_prop##_show(                            \
		struct kobject *kobj, struct kobj_attribute *attr, char *buf) \
	{                                                                     \
		return sysfs_emit(buf, _fmt, _val);                           \
	}                                                                     \
	static struct kobj_attribute attr_##_attrname##_##_prop =             \
		__LL_ATTR_RO(_attrname, _prop)

/* Attribute current_value show/store */
#define __LL_TUNABLE_RW(_attrname, _dev_id, _feat_id)                         \
	static ssize_t _attrname##_current_value_store(                       \
		struct kobject *kobj, struct kobj_attribute *attr,            \
		const char *buf, size_t count)                                \
	{                                                                     \
		return attr_current_value_store(                              \
			kobj, attr, buf, count,                               \
			&om_wmi.ll_tunables->_attrname, _dev_id, _feat_id);   \
	}                                                                     \
	static ssize_t _attrname##_current_value_show(                        \
		struct kobject *kobj, struct kobj_attribute *attr, char *buf) \
	{                                                                     \
		return attr_current_value_show(kobj, attr, buf, _dev_id,      \
					       _feat_id);                     \
	}                                                                     \
	static struct kobj_attribute attr_##_attrname##_current_value =       \
		__LL_ATTR_RW(_attrname, current_value);

/* Attribute property show only */
#define __LL_TUNABLE_RO(_prop, _attrname, _dev_id, _feat_id, _prop_type)      \
	static ssize_t _attrname##_##_prop##_show(                            \
		struct kobject *kobj, struct kobj_attribute *attr, char *buf) \
	{                                                                     \
		return attr_cap_data_show(kobj, attr, buf, _dev_id, _feat_id, \
					  _prop_type);                        \
	}                                                                     \
	static struct kobj_attribute attr_##_attrname##_##_prop =             \
		__LL_ATTR_RO(_attrname, _prop);

#define ATTR_GROUP_LL_TUNABLE(_attrname, _fsname, _dev_id, _feat_id,       \
			      _dispname)                                   \
	__LL_TUNABLE_RW(_attrname, _dev_id, _feat_id);                     \
	__LL_TUNABLE_RO(default_value, _attrname, _dev_id, _feat_id,       \
			DEFAULT_VAL);                                      \
	__ATTR_SHOW_FMT(display_name, _attrname, "%s\n", _dispname);       \
	__LL_TUNABLE_RO(max_value, _attrname, _dev_id, _feat_id, MAX_VAL); \
	__LL_TUNABLE_RO(min_value, _attrname, _dev_id, _feat_id, MIN_VAL); \
	__LL_TUNABLE_RO(scalar_increment, _attrname, _dev_id, _feat_id,    \
			STEP_VAL);                                         \
	static struct kobj_attribute attr_##_attrname##_type =             \
		__LL_ATTR_RO_AS(type, int_type_show);                      \
	static struct attribute *_attrname##_attrs[] = {                   \
		&attr_##_attrname##_current_value.attr,                    \
		&attr_##_attrname##_default_value.attr,                    \
		&attr_##_attrname##_display_name.attr,                     \
		&attr_##_attrname##_max_value.attr,                        \
		&attr_##_attrname##_min_value.attr,                        \
		&attr_##_attrname##_scalar_increment.attr,                 \
		&attr_##_attrname##_type.attr,                             \
		NULL,                                                      \
	};                                                                 \
	static const struct attribute_group _attrname##_attr_group = {     \
		.name = _fsname, .attrs = _attrname##_attrs                \
	}

#endif /* !_LENOVO_LEGION_WMI_H_ */
