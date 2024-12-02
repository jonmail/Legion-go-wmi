sudo rmmod amd_pmf
sudo rmmod lenovo_legion_wmi_other
sudo rmmod lenovo_legion_wmi_gamezone
sudo rmmod lenovo_legion_wmi
sudo rmmod firmware_attributes_class
make clean
make
sudo insmod firmware_attributes_class.ko
sudo insmod lenovo-legion-wmi.ko
sudo insmod lenovo-legion-wmi-gamezone.ko
sudo insmod lenovo-legion-wmi-other.ko
