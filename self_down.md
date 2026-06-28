⏺ # 1. 安装工具链
  brew install arm-none-eabi-gcc openocd cmake

  # 2. 克隆项目
  git clone https://github.com/loliweive/stm32-oop.git
  cd stm32-oop
  git submodule update --init

  # 3. 构建 (默认 app 模式, 配合 bootloader)
  cmake -B build/freertos -DBUILD_MODE=freertos
  cmake --build build/freertos

  # 4. 烧录 bootloader + app
  openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
    -c "program build/bootloader/stm32-oop.bin 0x08000000 verify" \
    -c "program build/freertos/stm32-oop.bin 0x08002000 verify" \
    -c "reset run" -c "shutdown"

  # 5. 串口
  screen /dev/cu.usbserial-3130 115200
