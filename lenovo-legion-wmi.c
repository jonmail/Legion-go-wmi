// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Lenovo Legion WMI interface driver. The Lenovo Legion WMI interface is
 * broken up into multiple GUID interfaces that require cross-references 
 * between GUID's for some functionality. The "Custom Mode" interface is a 
 * legacy interface for managing and displaying CPU & GPU power and hwmon
 * settings and readings. The "Other Mode" interface is a modern interface 
 * that replaces and extends the "Custom Mode" interface. The "GameZone"
 * interface adds advanced power and tuning features such as fan profiles
 * and overclocking. The "Lighting" interface adds control of various status
 * lights related to different hardware components. There are also three WMI 
 * data interfaces that allow for probing the hardware to determine if features
 * are supported, and if so what operational limits exist for those attributes.
 * This file acts as a repository of common functionalities.
 *
 * Copyright(C) 2024 Derek J. Clark <derekjohn.clark@gmail.com>
 *
 */

#include "lenovo-legion-wmi.h"

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
		printk("Attempt to get method_id %u value failed with error: %u\n",
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

EXPORT_SYMBOL_NS_GPL(lenovo_legion_evaluate_method_2, LL_WMI);

int lenovo_legion_evaluate_method_1(struct wmi_device *wdev, u8 instance,
				    u32 method_id, u32 arg0, u32 *retval)
{
	return lenovo_legion_evaluate_method_2(wdev, instance, method_id, arg0,
					       0, retval);
}

EXPORT_SYMBOL_NS_GPL(lenovo_legion_evaluate_method_1, LL_WMI);

MODULE_AUTHOR("Derek J. Clark <derekjohn.clark@gmail.com>");
MODULE_DESCRIPTION("Lenovo WMI Common Functions");
MODULE_LICENSE("GPL");
