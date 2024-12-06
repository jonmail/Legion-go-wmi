// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * LENOVO_CAPABILITY_DATA_01 WMI data block driver. This interface provides
 * information on tunable attributes, including if it is supported by the 
 * hardware, the default_value, max_value, min_value, and step increment.
 *
 * Copyright(C) 2024 Derek J. Clark <derekjohn.clark@gmail.com>
 *
 */

#include "lenovo-legion-wmi.h"

#define LENOVO_CAPABILITY_DATA_01_GUID "7A8F5407-CB67-4D6E-B547-39B3BE018154"

static const struct wmi_device_id capdata_01_wmi_id_table[] = {
	{ LENOVO_CAPABILITY_DATA_01_GUID, NULL },
	{}
};

MODULE_DEVICE_TABLE(wmi, capdata_01_wmi_id_table);

int capdata_01_wmi_get(struct om_attribute_id attr_id,
		       struct capability_data_01 *cap_data)
{
	union acpi_object *ret_obj;
	int count;
	int instance_id;
	u32 attribute_id = *(int *)&attr_id;

	count = wmidev_instance_count(drvdata.cd01_wmi->wdev);
	for (instance_id = 0; instance_id < count; instance_id++) {
		ret_obj =
			wmidev_block_query(drvdata.cd01_wmi->wdev, instance_id);
		if (!ret_obj) {
			pr_err("wmidev_block_query failed\n");
			continue;
		}

		if (ret_obj->type != ACPI_TYPE_BUFFER) {
			pr_err("wmidev_block_query returned type: %u\n",
			       ret_obj->type);
			kfree(ret_obj);
			continue;
		}

		if (ret_obj->buffer.length != sizeof(*cap_data)) {
			pr_err("buffer length is not correct, got %d\n",
			       ret_obj->buffer.length);
			kfree(ret_obj);
			continue;
		}

		memcpy(cap_data, ret_obj->buffer.pointer,
		       ret_obj->buffer.length);
		kfree(ret_obj);

		if (cap_data->id != attribute_id) {
			continue;
		}
		break;
	}
	if (cap_data->id == 0) {
		pr_err("Failed to get capability data\n");
		return -EINVAL;
	}
	return 0;
}
EXPORT_SYMBOL_NS_GPL(capdata_01_wmi_get, CAPDATA_WMI);

/* Driver Setup */
static int capdata_01_wmi_probe(struct wmi_device *wdev, const void *context)
{
	struct capdata_wmi *cd01_wmi;

	cd01_wmi = kzalloc(sizeof(struct capdata_wmi), GFP_KERNEL);
	if (!cd01_wmi)
		return -ENOMEM;

	cd01_wmi->wdev = wdev;
	drvdata.cd01_wmi = cd01_wmi;
	pr_info("lenovo_legion_wmi_capdata_01: Added.\n");

	return 0;
}

static void capdata_01_wmi_remove(struct wmi_device *wdev)
{
	pr_info("lenovo_legion_wmi_capdata_01: Removed.\n");
	return;
}

static struct wmi_driver capdata_01_wmi_driver = {
    .driver =
        {
            .name = "capdata_01_wmi",
        },
    .id_table = capdata_01_wmi_id_table,
    .probe = capdata_01_wmi_probe,
    .remove = capdata_01_wmi_remove,
};

module_wmi_driver(capdata_01_wmi_driver);

MODULE_DEVICE_TABLE(wmi, capdata_01_wmi_id_table);
MODULE_AUTHOR("Derek J. Clark <derekjohn.clark@gmail.com>");
MODULE_DESCRIPTION("Lenovo Capability Data 01 WMI Driver");
MODULE_LICENSE("GPL");
