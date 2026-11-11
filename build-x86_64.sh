export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
cd /home/thiago/t-os || exit 1
LOG=/home/thiago/t-os/output/x86_64/build.log
mkdir -p output/x86_64
{
  date
  make -C /home/thiago/t-os/buildroot BR2_EXTERNAL=/home/thiago/t-os/external O=/home/thiago/t-os/output/x86_64 tos_x86_64_defconfig
  make -C /home/thiago/t-os/buildroot BR2_EXTERNAL=/home/thiago/t-os/external O=/home/thiago/t-os/output/x86_64
  echo "EXIT=$?"
  date
} > "$LOG" 2>&1
