## 说明
此驱动是lenovo公司的Legion Go S平台的驱动程序，主要实现了性能模式切换和CPU功耗的设置。

## 一、编译命令

1. 编译源码
  - make
2. 安装驱动
  - sudo make install
3. 卸载驱动
  - sudo make clean

## 二、设置和获取消息

1. 模式
    - echo "SetSmartFanMode,1/2/3/255" | sudo tee /proc/acpi/legion_go_call
  - 读取设置模式
    - echo "GetSmartFanMode" | sudo tee /proc/acpi/legion_go_call
2. 设置SPL
    - echo "SetSPL,X" | sudo tee /proc/acpi/legion_go_call
  - 读取SPL
    - echo "GetSPL" | sudo tee /proc/acpi/legion_go_call
3. 设置sPPT
    - echo "SetSPPT,X" | sudo tee /proc/acpi/legion_go_call
  - 读取sPPT
    - echo "GetSPPT" | sudo tee /proc/acpi/legion_go_call
4. 设置fPPT
    - echo "SetFPPT,X" | sudo tee /proc/acpi/legion_go_call
  - 读取fPPT
    - echo "GetFPPT" | sudo tee /proc/acpi/legion_go_call
5. 获最后设置的参数，且只能读取最后设置的参数
    - sudo cat /proc/acpi/legion_go_call
  - 返回值
    - GetSmartFanMode,X
    - GetSPL,X
    - GetSPPT,X
    - GetFPPT,X

## 特殊说明
1. SetSmartFanMode只有设置成255,SPL、fPPT、sPPT设置才有效
1. 如果读取不是最后设置的值，需要先调用对应的读取命令，然后再调用sudo cat /proc/acpi/legion_go_call
1. 命令中“X"代表设置的值
1. SetSmartFanMode参数只能是1、2、3、255中的一个