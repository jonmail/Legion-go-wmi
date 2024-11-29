## About

This DKMS driver is a WMI interface driver for Lenovo's Legion line of laptops and handhelds.

## Compilation Commands

1. Compile Source Code

- make

2. Install Driver (DKMS)

- sudo make dkms

3. Uninstall Driver (DKMS)

- sudo make dkms_clean

## Settings and Read Information

### ACPI Platform Profile

The SetFanMode WMI method can be set by changing the platform profile attribute.

**TODO** Custom mode will require this patch series to be applied to the kernel

<https://lore.kernel.org/platform-driver-x86/20241119171739.77028-1-mario.limonciello@amd.com/>

View available platform profiles:

```
$ cat /sys/firmware/acpi/platform_profile_choices 
quiet balanced performance
```

Read current platform profile:

```
$ cat /sys/firmware/acpi/platform_profile
quiet
```

Set platform_profile:

```
$ echo performance | sudo tee /sys/firmware/acpi/platform_profile
performance

```

### Tunables

Tunables are available at the following paths:

```
$ pwd
/sys/class/firmware-attributes/lenovo-legion-wmi
$ tree
.
├── attributes
│   ├── cpu_temp
│   │   └── current_value
│   ├── ppt_apu_spl
│   │   └── current_value
│   ├── ppt_fppt
│   │   └── current_value
│   ├── ppt_pl1_spl
│   │   └── current_value
│   └── ppt_pl2_sppt
│       └── current_value
├── power
│   ├── autosuspend_delay_ms
│   ├── control
│   ├── runtime_active_time
│   ├── runtime_status
│   └── runtime_suspended_time
├── subsystem -> ../../../../class/firmware-attributes
└── uevent

9 directories, 11 files
```

**TODO** Finish these. Planned paths include (for each attribute) min/max/defualt_value, scalar_increment, type, display_name.
