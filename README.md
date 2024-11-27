## About
This driver is specifically designed for Lenovo's Legion Go S platform, primarily enabling the switching of performance modes and the configuration of CPU power settings.

## Compilation Commands

1. Compile Source Code
  - make
1. Install Driver
  - sudo make install
1. Uninstall Driver
  - sudo make clean

## Settings and Read Information

1. Mode
  - echo "SetSmartFanMode,1/2/3/255" | sudo tee /proc/acpi/legion_go_call
  - echo "GetSmartFanMode" | sudo tee /proc/acpi/legion_go_call
1. SPL
  - echo "SetSPL,X" | sudo tee /proc/acpi/legion_go_call
  - echo "GetSPL" | sudo tee /proc/acpi/legion_go_call
1. sPPT
  - echo "SetSPPT,X" | sudo tee /proc/acpi/legion_go_call
  - echo "GetSPPT" | sudo tee /proc/acpi/legion_go_call
1. fPPT
  - echo "SetFPPT,X" | sudo tee /proc/acpi/legion_go_call
  - echo "GetFPPT" | sudo tee /proc/acpi/legion_go_call
1. Read the Latest Settings (only the latest settings can be read)
  - sudo cat /proc/acpi/legion_go_call
  - Return Values
    - GetSmartFanMode,X
    - GetSPL,X
    - GetSPPT,X
    - GetFPPT,X

## Special Notes

1. The settings for SPL, fPPT, and sPPT are only effective when SetSmartFanMode is set to 255.
1. If the value read is not the last one set, you need to first invoke the corresponding read command, and then use sudo cat /proc/acpi/legion_go_call.
1. In the commands, "X" represents the value to be set.
1. The parameter for SetSmartFanMode can only be one of 1, 2, 3, or 255.

