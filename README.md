## About
This driver is specifically designed for Lenovo's Legion Go S platform, primarily enabling the switching of performance modes and the configuration of CPU power settings.

## Compilation Commands

1. Compile Source Code
  - make

2. Install Driver
  - sudo make install

3. Uninstall Driver
  - sudo make clean

## Settings and Read Information

1. Set Mode
    - echo "SetSmartFanMode,1/2/3/255" | sudo tee /proc/acpi/legion_go_call
  - Read Mode
    - echo "GetSmartFanMode" | sudo tee /proc/acpi/legion_go_call

2. Set SPL
    - echo "SetSPL,X" | sudo tee /proc/acpi/legion_go_call
  - Read SPL
    - echo "GetSPL" | sudo tee /proc/acpi/legion_go_call

3. Set sPPT
    - echo "SetSPPT,X" | sudo tee /proc/acpi/legion_go_call
  - Read sPPT
    - echo "GetSPPT" | sudo tee /proc/acpi/legion_go_call

4. Set fPPT
    - echo "SetFPPT,X" | sudo tee /proc/acpi/legion_go_call
  - Read fPPT
    - echo "GetFPPT" | sudo tee /proc/acpi/legion_go_call

5. Read the Latest Settings (only the latest settings can be read)
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

