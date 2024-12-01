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
├── cpu_temp
│   ├── current_value
│   ├── default_value
│   ├── display_name
│   ├── max_value
│   ├── min_value
│   ├── scalar_increment
│   └── type
├── ppt_apu_spl
│   ├── current_value
│   ├── default_value
│   ├── display_name
│   ├── max_value
│   ├── min_value
│   ├── scalar_increment
│   └── type
├── ppt_fppt
│   ├── current_value
│   ├── default_value
│   ├── display_name
│   ├── max_value
│   ├── min_value
│   ├── scalar_increment
│   └── type
├── ppt_pl1_spl
│   ├── current_value
│   ├── default_value
│   ├── display_name
│   ├── max_value
│   ├── min_value
│   ├── scalar_increment
│   └── type
└── ppt_pl2_sppt
    ├── current_value
    ├── default_value
    ├── display_name
    ├── max_value
    ├── min_value
    ├── scalar_increment
    └── type

6 directories, 35 files
```

**TODO** Finish these. Currently reading or writing to any attribute related to value crashes the driver.
