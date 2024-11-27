// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * legion-go-wmi.c - Lenovo Legion Go WMI Control
 *
 * Copyright © 2024 zhixin zhang <zhangzx36@lenovo.com>
 */

#include <linux/kernel.h>
#include <linux/acpi.h>
#include <linux/printk.h>
#include <linux/module.h>
#include <linux/wmi.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/version.h>

//extern struct proc_dir_entry *acpi_root_dir;
struct proc_dir_entry *acpi_root_dir;

#define BUFFER_SIZE 256

#define LEGION_GO_WMI_GAMEZONE_GUID			"887B54E3-DDDC-4B2C-8B88-68A26A8835D0"
#define LEGION_GO_WMI_OTHER_GUID			"dc2a8805-3a8c-41ba-a6f7-092e0089cd3b"

//wmi_device_id context string
#define LEGION_GO_WMI_GAMEZONE_CONTEXT	"GameZone"
#define LEGION_GO_WMI_OTHER_CONTEXT		"Other"

//funciton name
#define CMD_SET_SPL				"SetSPL"
#define CMD_GET_SPL				"GetSPL"
#define CMD_SET_SPPT			"SetSPPT"
#define CMD_GET_SPPT			"GetSPPT"
#define CMD_SET_FPPT			"SetFPPT"
#define CMD_GET_FPPT			"GetFPPT"
#define CMD_SET_SMART_FAN_MODE	"SetSmartFanMode"
#define CMD_GET_SMART_FAN_MODE	"GetSmartFanMode"

//function arg for ids
enum legion_go_wmi_ids{
	ARG_SPL_CUSTOM_MODE = 0x0102FF00,
	ARG_SPL_GET_VALUE = 0x0102FF00,

	ARG_SPPT_CUSTOM_MODE = 0x0101FF00,
	ARG_SPPT_GET_VALUE = 0x0101FF00,

	ARG_FPPT_CUSTOM_MODE = 0x0103FF00,
	ARG_FPPT_GET_VALUE = 0x0103FF00,

	ARG_SMART_FAN_QUIENT_MODE = 0x1,
	ARG_SMART_FAN_BALANCE_MODE = 0x2,
	ARG_SMART_FAN_PERFORMANCE_MODE = 0x3,
	ARG_SMART_FAN_CUSTOM_MODE = 0xFF,
};

static const struct wmi_device_id legion_go_wmi_id_table[] = {
	{ LEGION_GO_WMI_GAMEZONE_GUID, LEGION_GO_WMI_GAMEZONE_CONTEXT },
	{ LEGION_GO_WMI_OTHER_GUID, LEGION_GO_WMI_OTHER_CONTEXT },
	{ }
};


enum legion_go_wmi_gamezone_method {
	legion_go_wmi_gamezone_method	= 0xAA,	// WMAA, DSDT
	LEGION_GO_WMI_OTHER_METHOD		= 0xAE,	// WMAA, DSDT
};

//wmi command
enum legion_go_wmi_command {
	// smart fan mode
	LEGION_GO_WMI_GAMEZONE_SET_SMARTFANMODE	= 0x2C,
	LEGION_GO_WMI_GAMEZONE_GET_SMARTFANMODE	= 0x2D,
	// set bois feature
	LEGION_GO_WMI_OTHER_SET_FEATURE_VALUE	= 0x12,
	LEGION_GO_WMI_OTHER_GET_FEATURE_VALUE	= 0x11,
};

//wmi call function
enum legion_go_call_function {
	LEGION_GO_FUNC_NONE,
	LEGION_GO_FUNC_SET_SPL,
	LEGION_GO_FUNC_GET_SPL,
	LEGION_GO_FUNC_SET_SPPT,
	LEGION_GO_FUNC_GET_SPPT,
	LEGION_GO_FUNC_SET_FPPT,
	LEGION_GO_FUNC_GET_FPPT,
	LEGION_GO_FUNC_SET_SMART_FAN_MODE,
	LEGION_GO_FUNC_GET_SMART_FAN_MODE
};

struct legion_go_wmi_args_3i {
	u32 arg1;
	u32 arg2;
	u32 arg3;
};

struct legion_go_wmi_args_2i {
	u32 arg1;
	u32 arg2;
};

struct legion_go_wmi_args_1i {
	u32 arg1;
};

struct legion_go_global {
	struct wmi_device *legion_device[2]; //0:"GameZone"  1:"Other"
	enum legion_go_call_function last_call_function;
	bool first_read;
	struct proc_dir_entry *acpi_entry;
	char result_buffer[BUFFER_SIZE];
};

static struct legion_go_global g_Legion_Go_Global = {
	.legion_device = {NULL, NULL},
	.last_call_function = LEGION_GO_FUNC_NONE,
	.first_read = true,
	.acpi_entry = NULL,
};

static acpi_status legion_go_wmi_perform_query(struct wmi_device *wdev,
		enum legion_go_wmi_gamezone_method method_id,
		const struct acpi_buffer *in,
		struct acpi_buffer *out)
{
	acpi_status ret = wmidev_evaluate_method(wdev, 0x0, method_id, in, out);

	if (ACPI_FAILURE(ret)) {
		dev_warn(&wdev->dev, "LEGION GO WMI: WMI query failed with error: %d\n", ret);
		return -EIO;
	}

	return 0;
}

static acpi_status legion_go_wmi_query_integer(struct wmi_device *wdev,
		enum legion_go_wmi_gamezone_method method_id,
		const struct acpi_buffer *in,
		u32 *res)
{
	union acpi_object *obj;
	struct acpi_buffer result = { ACPI_ALLOCATE_BUFFER, NULL };
	acpi_status ret;

	ret = legion_go_wmi_perform_query(wdev, method_id, in, &result);
	if (ret) {
		return ret;
	}

	obj = result.pointer;
	if (obj && obj->type == ACPI_TYPE_INTEGER) {
		*res = obj->integer.value;
	}
	else {
		ret = -EIO;
	}

	kfree(result.pointer);
	return ret;
}


/**
 * procfs write callback. Called when writing into /proc/acpi/call.
*/
static ssize_t acpi_proc_write(struct file *filp,
		const char __user *buff,
		size_t len,
		loff_t *data)
{
    char input[2 * BUFFER_SIZE] = { '\0' };
    union acpi_object *args;
    int nargs, i;
    char *method;

	u32 prod_id;
	acpi_status ret;

    if (len > sizeof(input) - 1) {
        printk(KERN_ERR "LEGION GO WMI: Input too long! (%lu)\n", len);
        return -ENOSPC;
    }

    if (copy_from_user( input, buff, len )) {
        return -EFAULT;
    }

    input[len] = '\0';
    if (input[len-1] == '\n')
        input[len-1] = '\0';

	printk("LEGION GO WMI: procfs write is %s\n", input);

	char cmd[2 * BUFFER_SIZE] = { '\0' };
	char arg1[2 * BUFFER_SIZE] = { '\0' };
	int arg1Num = 0;
	int retNum = 0;

	int pos = -1;
	for(int i=0;i<2 * BUFFER_SIZE;i++) {
		if(input[i]== ',') {
			memcpy(cmd,input,i*sizeof(char));
			pos = i+1;
		}
		else if(input[i]=='\0' && pos != -1) {
			memcpy(arg1,input+pos,(i-pos)*sizeof(char));
			pos = i+1;
			break;
		}
	}
	if(pos == -1) {
		memcpy(cmd,input,len*sizeof(char));
	}
	else {
		printk(KERN_ERR "LEGION GO WMI: cmd = %s, arg1 : %s\n", cmd,arg1);
		retNum = kstrtoint(arg1,10,&arg1Num);
		if(retNum != 0)
		{
			printk(KERN_ERR "LEGION GO WMI: arg1 = %s param error!\n",arg1);
			return -ENOSPC;
		}
	}

	if(ret == 0) {
		if(strcmp(cmd,CMD_SET_SPL)==0) {
			struct legion_go_wmi_args_2i args = {
				.arg1 = ARG_SPL_CUSTOM_MODE,
				.arg2 = arg1Num,
			};
			const struct acpi_buffer in = {
				.length = sizeof(args),
				.pointer = &args,
			};

			g_Legion_Go_Global.last_call_function = LEGION_GO_FUNC_SET_SPL;

			ret = legion_go_wmi_query_integer(g_Legion_Go_Global.legion_device[1], 
					LEGION_GO_WMI_OTHER_SET_FEATURE_VALUE, &in, &prod_id);
			if (ret == 0) {
				dev_info(&g_Legion_Go_Global.legion_device[1]->dev, 
						"LEGION GO WMI: SetSPL result is %d\n", prod_id);
			}
			else {
				dev_warn(&g_Legion_Go_Global.legion_device[1]->dev,
						"LEGION GO WMI: SetSPL query failed with err: %d\n", ret);
			}
		}
		else if(strcmp(cmd,CMD_GET_SPL)==0) {
			g_Legion_Go_Global.last_call_function = LEGION_GO_FUNC_GET_SPL;
		}
		else if(strcmp(cmd,CMD_SET_SPPT)==0) {
			struct legion_go_wmi_args_2i args = {
				.arg1 = ARG_SPPT_CUSTOM_MODE,
				.arg2 = arg1Num,
			};
			const struct acpi_buffer in = {
				.length = sizeof(args),
				.pointer = &args,
			};

			g_Legion_Go_Global.last_call_function = LEGION_GO_FUNC_SET_SPPT;

			ret = legion_go_wmi_query_integer(g_Legion_Go_Global.legion_device[1],
					LEGION_GO_WMI_OTHER_SET_FEATURE_VALUE,
					&in,
					&prod_id);
			if (ret == 0) {
				dev_info(&g_Legion_Go_Global.legion_device[1]->dev,
						"LEGION GO WMI: SetSPPT result is %d\n", prod_id);
			}
			else {
				dev_warn(&g_Legion_Go_Global.legion_device[1]->dev,
						"LEGION GO WMI: SetSPPT query failed with err: %d\n", ret);
			}
		}
		else if(strcmp(cmd,CMD_GET_SPPT)==0) {
			g_Legion_Go_Global.last_call_function = LEGION_GO_FUNC_GET_SPPT;
		}
		else if(strcmp(cmd,CMD_SET_FPPT)==0) {
			struct legion_go_wmi_args_2i args = {
				.arg1 = ARG_FPPT_CUSTOM_MODE,
				.arg2 = arg1Num,
			};
			const struct acpi_buffer in = {
				.length = sizeof(args),
				.pointer = &args,
			};

			g_Legion_Go_Global.last_call_function = LEGION_GO_FUNC_SET_FPPT;

			ret = legion_go_wmi_query_integer(g_Legion_Go_Global.legion_device[1],
					LEGION_GO_WMI_OTHER_SET_FEATURE_VALUE,
					&in,
					&prod_id);
			if (ret == 0) {
				dev_info(&g_Legion_Go_Global.legion_device[1]->dev,
						"LEGION GO WMI: SetFPPT result is %d\n", prod_id);
			}
			else {
				dev_warn(&g_Legion_Go_Global.legion_device[1]->dev,
						"LEGION GO WMI: SetFPPT query failed with err: %d\n", ret);
			}
		}
		else if(strcmp(cmd,CMD_GET_FPPT)==0) {
			g_Legion_Go_Global.last_call_function = LEGION_GO_FUNC_GET_FPPT;
		}
		else if(strcmp(cmd,CMD_SET_SMART_FAN_MODE)==0) {
			if(arg1Num != 1 && arg1Num != 2 && arg1Num != 3 && arg1Num != 0xFF) {
				printk(KERN_ERR "LEGION GO WMI: %s arg1 = %s param error!\n",
						CMD_SET_SMART_FAN_MODE,arg1);
				return -ENOSPC;
			}

			struct legion_go_wmi_args_1i args = {
				.arg1 = arg1Num,
			};
			const struct acpi_buffer in = {
				.length = sizeof(args),
				.pointer = &args,
			};
			g_Legion_Go_Global.last_call_function = LEGION_GO_FUNC_SET_SMART_FAN_MODE;
			ret = legion_go_wmi_query_integer(g_Legion_Go_Global.legion_device[0],
					LEGION_GO_WMI_GAMEZONE_SET_SMARTFANMODE,
					&in,
					&prod_id);

			if (ret == 0) {
				dev_info(&g_Legion_Go_Global.legion_device[0]->dev,
					"LEGION GO WMI: SetSmartFanMode query result is %d\n", prod_id);
			} 
			else {
				dev_warn(&g_Legion_Go_Global.legion_device[0]->dev,
				"LEGION GO WMI: SetSmartFanMode query failed with err: %d\n", ret);
			}
		}
		else if(strcmp(cmd,CMD_GET_SMART_FAN_MODE)==0) {
			g_Legion_Go_Global.last_call_function = LEGION_GO_FUNC_GET_SMART_FAN_MODE;
		}
	}

    return len;
}

//read other mothod
acpi_status acpi_proc_read_other(struct wmi_device *wdev,
		enum legion_go_wmi_command cmd,
		struct legion_go_wmi_args_1i* args,
		char* funciton_name)
{
	u32 prod_id = 0;
	const struct acpi_buffer in = {
		.length = sizeof(*args),
		.pointer = args,
	};
	acpi_status ret = legion_go_wmi_query_integer(wdev, cmd,  &in, &prod_id);
	if (ret == 0) {
		dev_info(&wdev->dev, "LEGION GO WMI: Integer query result is %d\n", prod_id);
		snprintf(g_Legion_Go_Global.result_buffer,BUFFER_SIZE,"%s,%u",funciton_name,prod_id);
	} 
	else {
		dev_warn(&wdev->dev, "LEGION GO WMI: Integer query failed with err: %d\n", ret);
		snprintf(g_Legion_Go_Global.result_buffer,BUFFER_SIZE,"%s,error",funciton_name);
	}
	return ret;
}

static ssize_t acpi_proc_read(struct file *filp, char __user *buff, size_t count, loff_t *off)
{
	u32 prod_id;
	acpi_status ret;
	int len = strlen(g_Legion_Go_Global.result_buffer);

	memset(g_Legion_Go_Global.result_buffer,'\0',len);

	if(g_Legion_Go_Global.last_call_function == LEGION_GO_FUNC_NONE) {
		ssize_t result = simple_read_from_buffer(buff,
				count,
				off,
				g_Legion_Go_Global.result_buffer,
				len + 1);
		return result;
		//return -EIO;
	}


	switch(g_Legion_Go_Global.last_call_function) {
		case LEGION_GO_FUNC_SET_SPL:
		case LEGION_GO_FUNC_GET_SPL:
		{
			struct legion_go_wmi_args_1i args = {
				.arg1 = ARG_SPL_GET_VALUE,
			};
			ret = acpi_proc_read_other(g_Legion_Go_Global.legion_device[1],
				LEGION_GO_WMI_OTHER_GET_FEATURE_VALUE,
				&args,
				CMD_GET_SPL);

			break;
		}
		case LEGION_GO_FUNC_SET_SPPT:
		case LEGION_GO_FUNC_GET_SPPT:
		{
			struct legion_go_wmi_args_1i args = {
				.arg1 = ARG_SPPT_GET_VALUE,
			};
			ret = acpi_proc_read_other(g_Legion_Go_Global.legion_device[1],
					LEGION_GO_WMI_OTHER_GET_FEATURE_VALUE,
					&args,
					CMD_GET_SPPT);

			break;
		}
		case LEGION_GO_FUNC_SET_FPPT:
		case LEGION_GO_FUNC_GET_FPPT:
		{
			struct legion_go_wmi_args_1i args = {
				.arg1 = ARG_FPPT_GET_VALUE,
			};
			ret = acpi_proc_read_other(g_Legion_Go_Global.legion_device[1],
					LEGION_GO_WMI_OTHER_GET_FEATURE_VALUE,
					&args,
					CMD_GET_FPPT);

			break;
		}
		case LEGION_GO_FUNC_SET_SMART_FAN_MODE:
		case LEGION_GO_FUNC_GET_SMART_FAN_MODE:
		{
			struct legion_go_wmi_args_1i args = {
				.arg1 = 255,
			};
			const struct acpi_buffer in = {
				.length = sizeof(args),
				.pointer = &args,
			};

			ret = legion_go_wmi_query_integer(g_Legion_Go_Global.legion_device[0],
					LEGION_GO_WMI_GAMEZONE_GET_SMARTFANMODE,
					&in,
					&prod_id);
			if (ret == 0) {
				dev_info(&g_Legion_Go_Global.legion_device[0]->dev,
						"LEGION GO WMI: Integer query result is %d\n", prod_id);
				snprintf(g_Legion_Go_Global.result_buffer,BUFFER_SIZE,"%s,%u",
						CMD_GET_SMART_FAN_MODE,prod_id);
			}
			else {
				dev_warn(&g_Legion_Go_Global.legion_device[0]->dev,
						"LEGION GO WMI: Integer query failed with err: %d\n", ret);
				snprintf(g_Legion_Go_Global.result_buffer,BUFFER_SIZE,"%s,error",
						CMD_GET_SMART_FAN_MODE);
			}
			break;
		}
		default:
		{
			strcpy(g_Legion_Go_Global.result_buffer,"LEGION GO WMI: nothing to write");
		}
	}

	if(g_Legion_Go_Global.first_read == true) {
		char temp[BUFFER_SIZE] = {'\0'};
		strcpy(temp, g_Legion_Go_Global.result_buffer);
		strcpy(g_Legion_Go_Global.result_buffer+1, temp);
		g_Legion_Go_Global.first_read = false;
	}
	// output the current result buffer
	ssize_t result = simple_read_from_buffer(buff,
			count,
			off,
			g_Legion_Go_Global.result_buffer,
			len + 1);

    return result;
}

static const struct proc_ops proc_acpi_operations = {
        .proc_read     = acpi_proc_read,
        .proc_write    = acpi_proc_write,
};

static int legion_go_wmi_probe(struct wmi_device *wdev, const void *context)
{
	dev_info(&wdev->dev, "LEGION GO WMI: Probe is starting.\n");

	if (!wmi_has_guid(LEGION_GO_WMI_OTHER_GUID)) {
		dev_warn(&wdev->dev, "LEGION GO WMI: No known OTHER WMI GUID found\n");
		return -ENODEV;
	}

	if (!wmi_has_guid(LEGION_GO_WMI_GAMEZONE_GUID)) {
		dev_warn(&wdev->dev, "LEGION GO WMI: No known GAMEZONE WMI GUID found\n");
		return -ENODEV;
	}

	if (g_Legion_Go_Global.acpi_entry == NULL) {
		g_Legion_Go_Global.acpi_entry = proc_create("legion_go_call", 
				0660,
				acpi_root_dir,
				&proc_acpi_operations);
	}

    if (g_Legion_Go_Global.acpi_entry == NULL)
	{
      dev_warn(&wdev->dev, "LEGION GO WMI: Couldn't create procfs entry\n");
      return -ENOMEM;
    }

    dev_info(&wdev->dev, "LEGION GO WMI: procfs entry at /proc/acpi/legion_go_call created.\n");

	dev_info(&wdev->dev, "LEGION GO WMI: Probe is exiting.\n");

	if(strcmp(context, LEGION_GO_WMI_GAMEZONE_CONTEXT)== 0) {
		g_Legion_Go_Global.legion_device[0] = wdev;
	}
	else {
		g_Legion_Go_Global.legion_device[1] = wdev;
	}

	return 0;
}

static void legion_go_wmi_remove(struct wmi_device *wdev)
{
	g_Legion_Go_Global.legion_device[0] = NULL;
	g_Legion_Go_Global.legion_device[1] = NULL;

    remove_proc_entry("legion_go_call", acpi_root_dir);

    dev_info(&wdev->dev, "LEGION GO WMI: procfs entry removed\n");
}

static struct wmi_driver legion_go_wmi_driver = {
	.driver = {
		.name = "legion-go-wmi",
	},
	.id_table = legion_go_wmi_id_table,
	.probe = legion_go_wmi_probe,
	.remove = legion_go_wmi_remove
};

module_wmi_driver(legion_go_wmi_driver);

MODULE_DEVICE_TABLE(wmi, legion_go_wmi_id_table);

MODULE_DESCRIPTION("Lenovo Legion Go WMI Driver");
MODULE_AUTHOR("zhixin zhang<zhangzx36@lenovo.com>");
MODULE_AUTHOR("Derek J. Clark <derekjohn.clark@gmail.com>");
MODULE_LICENSE("GPL");
