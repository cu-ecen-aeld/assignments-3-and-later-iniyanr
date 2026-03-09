#!/bin/bash
# Finalized Script for Kernel/Busybox Build

set -e
set -u

OUTDIR=/tmp/aeld
KERNEL_REPO=git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
KERNEL_VERSION=v5.15.163
BUSYBOX_VERSION=1_33_1
FINDER_APP_DIR=$(realpath $(dirname $0))
ARCH=arm64
CROSS_COMPILE=aarch64-linux-gnu-

# Check for the standard Ubuntu/Debian name first
if command -v aarch64-linux-gnu-gcc > /dev/null; then
    CROSS_COMPILE=aarch64-linux-gnu-
# Then check for the "none" version often found in ARM's official toolchain
elif command -v aarch64-none-linux-gnu-gcc > /dev/null; then
    CROSS_COMPILE=aarch64-none-linux-gnu-
else
    echo "ERROR: No aarch64 cross-compiler found in PATH"
    exit 1
fi

if [ $# -lt 1 ]; then
    echo "Using default directory ${OUTDIR} for output"
else
    OUTDIR=$1
    echo "Using passed directory ${OUTDIR} for output"
fi

mkdir -p "${OUTDIR}"
OUTDIR=$(realpath "${OUTDIR}")

########################################
# Kernel Build
########################################
cd "$OUTDIR"
if [ ! -d "${OUTDIR}/linux-stable" ]; then
    git clone ${KERNEL_REPO} --depth 1 --single-branch --branch ${KERNEL_VERSION}
fi

if [ ! -e ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ]; then
    cd linux-stable
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} mrproper
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} defconfig
    make -j$(nproc) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} Image
    make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} dtbs
fi
cp ${OUTDIR}/linux-stable/arch/${ARCH}/boot/Image ${OUTDIR}/Image

########################################
# Rootfs Prep & BusyBox
########################################
if [ -d "${OUTDIR}/rootfs" ]; then
    sudo rm -rf "${OUTDIR}/rootfs"
fi

# Create essential directory structure
mkdir -p ${OUTDIR}/rootfs/{bin,dev,etc,home,lib,lib64,proc,sbin,sys,tmp,var}
mkdir -p ${OUTDIR}/rootfs/usr/{bin,lib,sbin}
mkdir -p ${OUTDIR}/rootfs/home/conf

cd "$OUTDIR"
if [ ! -d "${OUTDIR}/busybox" ]; then
    git clone git://busybox.net/busybox.git
    cd busybox
    git checkout ${BUSYBOX_VERSION}
    make distclean
    make defconfig
else
    cd busybox
fi
make -j$(nproc) ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE}
make ARCH=${ARCH} CROSS_COMPILE=${CROSS_COMPILE} CONFIG_PREFIX=${OUTDIR}/rootfs install

########################################
# Library Dependencies - BRUTE FORCE FIX
########################################
echo "Resolving Library dependencies..."

# We know from your 'find' command that they live here:
SRC_LIB="/usr/aarch64-linux-gnu/lib"

# If the path doesn't exist, try to find it via compiler
if [ ! -d "$SRC_LIB" ]; then
    SYSROOT=$(${CROSS_COMPILE}gcc -print-sysroot)
    SRC_LIB="${SYSROOT}/lib"
fi

# Copy libraries to BOTH /lib and /lib64 to ensure loader finds them
# -L is CRITICAL: it copies the actual binary, not a broken shortcut
for libpath in "${OUTDIR}/rootfs/lib" "${OUTDIR}/rootfs/lib64"; do
    cp -L "${SRC_LIB}/ld-linux-aarch64.so.1" "$libpath/"
    cp -L "${SRC_LIB}/libc.so.6" "$libpath/"
    cp -L "${SRC_LIB}/libm.so.6" "$libpath/"
    cp -L "${SRC_LIB}/libresolv.so.2" "$libpath/"
done

########################################
# Device Nodes & Writer Utility
########################################
sudo mknod -m 666 ${OUTDIR}/rootfs/dev/null c 1 3 || true
sudo mknod -m 600 ${OUTDIR}/rootfs/dev/console c 5 1 || true

cd "${FINDER_APP_DIR}"
${CROSS_COMPILE}gcc -o writer writer.c
cp writer "${OUTDIR}/rootfs/home/"
cp finder.sh finder-test.sh autorun-qemu.sh "${OUTDIR}/rootfs/home/"
cp ../conf/username.txt ../conf/assignment.txt "${OUTDIR}/rootfs/home/conf/"

# Fix paths and compatibility
sed -i 's|\.\./conf|conf|g' "${OUTDIR}/rootfs/home/finder-test.sh"
sed -i '1s|^#! */bin/bash|#!/bin/sh|' "${OUTDIR}/rootfs/home/finder.sh"
chmod +x "${OUTDIR}/rootfs/home/"*.sh

########################################
# Create init script
########################################
cat << 'EOF' > "${OUTDIR}/rootfs/init"
#!/bin/sh
# Mount essential filesystems
mount -t proc none /proc
mount -t sysfs none /sys
mount -t devtmpfs devtmpfs /dev

echo "--- BOOT SUCCESSFUL ---"

# Move to home and run the test
cd /home
if [ -f "./autorun-qemu.sh" ]; then
    # Run through setsid to fix the "can't access tty" warning 
    # and redirect all streams to the console
    setsid cttyhack /bin/sh ./autorun-qemu.sh < /dev/console > /dev/console 2>&1
fi

# Fallback to a shell if the test finishes or fails
exec /bin/sh < /dev/console > /dev/console 2>&1
EOF
chmod +x "${OUTDIR}/rootfs/init"

########################################
# Set Ownership and Create initramfs
########################################
sudo chown -R root:root "${OUTDIR}/rootfs"

cd "${OUTDIR}/rootfs"
find . | cpio -H newc -ov --owner root:root > "${OUTDIR}/initramfs.cpio"
gzip -f "${OUTDIR}/initramfs.cpio"

echo "Build Completed."