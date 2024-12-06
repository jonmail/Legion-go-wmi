// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Lenovo GameZone WMI interface driver. The GameZone WMI interface provides
 * platform profile and fan curve settings for devices that fall under the
 * "Gaming Series" of Lenovo Legion devices.
 *
 * Copyright(C) 2024 Derek J. Clark <derekjohn.clark@gmail.com>
 *
 */

#include "lenovo-legion-wmi.h"

#define LENOVO_GAMEZONE_GUID "887B54E3-DDDC-4B2C-8B88-68A26A8835D0"

static const struct wmi_device_id gamezone_wmi_id_table[] = {
	{ LENOVO_GAMEZONE_GUID, NULL }, /* LENOVO_GAMEZONE_DATA */
	{}
};

MODULE_DEVICE_TABLE(wmi, gamezone_wmi_id_table);

/* Platform Profile Methods */
static int
gamezone_wmi_platform_profile_supported(struct platform_profile_handler *pprof,
					int *supported)
{
	return lenovo_legion_evaluate_method_1(drvdata.gz_wmi->wdev, 0x0,
					       WMI_METHOD_ID_SMARTFAN_SUPP, 0,
					       supported);
}

int gamezone_wmi_fan_profile_get(struct platform_profile_handler *pprof,
				 int *sel_prof)
{
	return lenovo_legion_evaluate_method_1(drvdata.gz_wmi->wdev, 0x0,
					       WMI_METHOD_ID_SMARTFAN_GET, 0,
					       sel_prof);
}
EXPORT_SYMBOL_NS_GPL(gamezone_wmi_fan_profile_get, GZ_WMI);

static int
gamezone_wmi_platform_profile_get(struct platform_profile_handler *pprof,
				  enum platform_profile_option *profile)
{
	int sel_prof;
	int err;

	err = gamezone_wmi_fan_profile_get(pprof, &sel_prof);
	if (err)
		return err;

	switch (sel_prof) {
	case SMARTFAN_MODE_QUIET:
		*profile = PLATFORM_PROFILE_QUIET;
		break;
	case SMARTFAN_MODE_BALANCED:
		*profile = PLATFORM_PROFILE_BALANCED;
		break;
	case SMARTFAN_MODE_PERFORMANCE:
		*profile = PLATFORM_PROFILE_PERFORMANCE;
		break;
	case SMARTFAN_MODE_CUSTOM:
		/* *profile = PLATFORM_PROFILE_CUSTOM; */
		*profile = PLATFORM_PROFILE_BALANCED_PERFORMANCE;
		break;

	default:
		return -EINVAL;
	}
	drvdata.gz_wmi->current_profile = *profile;

	return 0;
}

static int
gamezone_wmi_platform_profile_set(struct platform_profile_handler *pprof,
				  enum platform_profile_option profile)
{
	int sel_prof;

	switch (profile) {
	case PLATFORM_PROFILE_QUIET:
		sel_prof = SMARTFAN_MODE_QUIET;
		break;
	case PLATFORM_PROFILE_BALANCED:
		sel_prof = SMARTFAN_MODE_BALANCED;
		break;
	case PLATFORM_PROFILE_PERFORMANCE:
		sel_prof = SMARTFAN_MODE_PERFORMANCE;
		break;
	case PLATFORM_PROFILE_BALANCED_PERFORMANCE:
		/* case PLATFORM_PROFILE_CUSTOM: */
		sel_prof = SMARTFAN_MODE_CUSTOM;
		break;
	default:
		return -EOPNOTSUPP;
	}
	drvdata.gz_wmi->current_profile = profile;

	return lenovo_legion_evaluate_method_1(drvdata.gz_wmi->wdev, 0x0,
					       WMI_METHOD_ID_SMARTFAN_SET,
					       sel_prof, NULL);
}

static int platform_profile_setup(struct gamezone_wmi *gz_wmi)
{
	int err;
	int supported;

	gz_wmi->pprof.profile_get = gamezone_wmi_platform_profile_get;
	gz_wmi->pprof.profile_set = gamezone_wmi_platform_profile_set;

	gamezone_wmi_platform_profile_supported(&gz_wmi->pprof, &supported);
	if (!supported) {
		pr_warn("lenovo_legion_wmi_gamezone: Platform profiles are not supported "
			"by this device.\n");
		return -ENOTSUPP;
	}
	gz_wmi->platform_profile_support = supported;

	err = gamezone_wmi_platform_profile_get(&gz_wmi->pprof,
						&gz_wmi->current_profile);
	if (err) {
		pr_err("lenovo_legion_wmi_gamezone: Failed to get current platform "
		       "profile: %d\n",
		       err);
		return err;
	}

	/* Setup supported modes */
	set_bit(PLATFORM_PROFILE_QUIET, gz_wmi->pprof.choices);
	set_bit(PLATFORM_PROFILE_BALANCED, gz_wmi->pprof.choices);
	set_bit(PLATFORM_PROFILE_PERFORMANCE, gz_wmi->pprof.choices);
	set_bit(PLATFORM_PROFILE_BALANCED_PERFORMANCE, gz_wmi->pprof.choices);

	/* Create platform_profile structure and register */
	err = platform_profile_register(&gz_wmi->pprof);
	if (err) {
		pr_err("lenovo_legion_wmi_gamezone: Failed to register platform profile "
		       "support: %d\n",
		       err);
		return err;
	}

	return 0;
}

/* Driver Setup */
static int gamezone_wmi_probe(struct wmi_device *wdev, const void *context)
{
	struct gamezone_wmi *gz_wmi;
	int err;

	gz_wmi = kzalloc(sizeof(struct gamezone_wmi), GFP_KERNEL);
	if (!gz_wmi)
		return -ENOMEM;

	gz_wmi->wdev = wdev;
	drvdata.gz_wmi = gz_wmi;

	err = platform_profile_setup(gz_wmi);
	if (err) {
		kfree(gz_wmi);
	}

	return err;
}

static void gamezone_wmi_remove(struct wmi_device *wdev)
{
	int err;
	err = platform_profile_remove();
	if (err) {
		pr_err("lenovo_legion_wmi_gamezone: Failed to remove platform profile: %d\n",
		       err);
	} else {
		pr_info("lenovo_legion_wmi_gamezone: Removed platform profile support\n");
	}

	return;
}

static struct wmi_driver gamezone_wmi_driver = {
    .driver =
        {
            .name = "gamezone_wmi",
        },
    .id_table = gamezone_wmi_id_table,
    .probe = gamezone_wmi_probe,
    .remove = gamezone_wmi_remove,
};

module_wmi_driver(gamezone_wmi_driver);

MODULE_DEVICE_TABLE(wmi, gamezone_wmi_id_table);
MODULE_AUTHOR("Derek J. Clark <derekjohn.clark@gmail.com>");
MODULE_DESCRIPTION("Lenovo GameZone WMI Driver");
MODULE_LICENSE("GPL");
