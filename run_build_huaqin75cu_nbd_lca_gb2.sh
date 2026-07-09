#!/bin/bash
# MT6575 Lenovo A60+ kernel build runner
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
TARGET_PRODUCT=huaqin75cu_nbd_lca_gb2
TOOLCHAIN_DIR="$SCRIPT_DIR/prebuilt/linux-x86/toolchain/arm-linux-androideabi-4.4.3"

export PATH="$TOOLCHAIN_DIR/bin:$PATH"
export TARGET_PRODUCT
export MTK_ROOT_CUSTOM="$SCRIPT_DIR/mediatek/custom/"
export MAKEJOBS=-j8

echo "==== ENV CHECK ===="
echo "PATH=$PATH"
which arm-eabi-gcc || { echo "NO CROSS COMPILER"; exit 98; }
echo "Cross compiler: $(arm-eabi-gcc --version | head -1)"
echo "Source dir: $SCRIPT_DIR"
echo "================"

cd "$SCRIPT_DIR" || exit 99

echo "==== CLEAN GENERATED CUSTOM OUT ===="
rm -rf "mediatek/config/out/$TARGET_PRODUCT"
rm -rf "mediatek/custom/out/$TARGET_PRODUCT"

echo "==== GENERATE CUSTOM OUT ===="
BUILD_KERNEL=yes make -f mediatek/build/custgen.mk all
custgen_rc=$?
echo "==== custgen exit code: $custgen_rc ===="
if [ "$custgen_rc" -ne 0 ]; then
    exit "$custgen_rc"
fi

cd "$SCRIPT_DIR/kernel" || exit 99
echo "Kernel dir: $(pwd)"

echo "==== CLEAN ===="
./build.sh clean "$TARGET_PRODUCT"
clean_rc=$?
echo "==== build.sh clean exit code: $clean_rc ===="
if [ "$clean_rc" -ne 0 ]; then
    exit "$clean_rc"
fi

rm -f "kernel_${TARGET_PRODUCT}.bin" "rootfs_${TARGET_PRODUCT}.bin"

echo "==== REBUILD ===="
./build.sh rebuild "$TARGET_PRODUCT"
rc=$?
echo "==== build.sh exit code: $rc ===="
echo "==== output artifacts ===="
ls -la arch/arm/boot/zImage arch/arm/boot/Image "kernel_${TARGET_PRODUCT}.bin" "rootfs_${TARGET_PRODUCT}.bin" 2>/dev/null
echo "==== DONE rc=$rc ===="
exit "$rc"
