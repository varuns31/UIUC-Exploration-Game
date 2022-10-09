echo running shell script
cd module
sudo /sbin/rmmod tuxctl
make
sudo /sbin/insmod ./tuxctl.ko
cd ..
make input
echo end of Script
