#!/usr/bin/env bash
CURRENT_PATH=$(pwd)
cd ../../../../ #top dir
./build.sh ./boards/best1700_ep/aos_evb/configs/ap SMF=1 MM_PROJECT=watch_aos_1700_open_vela MM_PLATFORM=m55 MM_CORE=m55c0 HIFI4_PSRAM_BASE=0x19C00000 HIFI4_PSRAM_SIZE=0x3F4000 DSP_HIFI4_TRC_TO_MCU=1 CHIP_DMA_CFG_IDX=3 -j32 A2DP_SOURCE_AAC_ON=1 AAC_RAW_ENABLE=1 CONFIG_BT_ADDR=0x11,0x22,0x12,0x22,0x66,0x12
sed -i 's/"cc"/"arm-none-eabi-gcc"/g' compile_commands.json
sed -i 's/"c++"/"arm-none-eabi-g++"/g' compile_commands.json
cd ${CURRENT_PATH}
find . -type f -name "*.o" -exec rm -f {} +