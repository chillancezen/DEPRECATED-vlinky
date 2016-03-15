# vlinky
special customization for NFV-load  requirments
#####1st vlinky kernel-driver part
```sh
root@chillancezen-virtual-machine:/mnt/projects/vlinky/vlinky-pci# make ;make install 
root@chillancezen-virtual-machine:/mnt/projects/vlinky/vlinky-pci# dmesg |tail -28
[ 6757.652298] [x]bar 0:reg addr:febd3000
[ 6757.652301] [x]bar 0:reg len :256
[ 6757.652302] [x]bar 0:map addr:ffffc900001d2000
[ 6757.652312] [x]bar 0:nvectors:6
[ 6757.652313] [x]bar 0:nr-links:2
[ 6757.652322] [x]link 0 has 2 channels
[ 6757.652330] [x]link 1 has 4 channels
[ 6757.652694] [x]bar 3:reg addr:e1000000
[ 6757.652695] [x]bar 3:reg len :8388608
[ 6757.652696] [x]bar 3:map addr:ffffc90000900000
[ 6757.652718] [x] :1024.1024.1024.1024
[ 6757.652736] [x] :1024.1024.1024.1024
[ 6757.652757] [x] :0.10240.10240.0
[ 6757.652774] [x] :10240.10240.10240.10240
[ 6757.652792] [x] :10240.10240.10240.10240
[ 6757.652810] [x] :10240.10240.10240.10240
[ 6757.676048] [x]bar 2:reg addr:c0000000
[ 6757.676050] [x]bar 2:reg len :536870912
[ 6757.676051] [x]bar 2:map addr:ffffc900e1500000
[ 6757.676115] vlinky-net 0000:00:05.0: irq 46 for MSI/MSI-X
[ 6757.676130] vlinky-net 0000:00:05.0: irq 47 for MSI/MSI-X
[ 6757.676144] vlinky-net 0000:00:05.0: irq 48 for MSI/MSI-X
[ 6757.676158] vlinky-net 0000:00:05.0: irq 49 for MSI/MSI-X
[ 6757.676172] vlinky-net 0000:00:05.0: irq 50 for MSI/MSI-X
[ 6757.676187] vlinky-net 0000:00:05.0: irq 51 for MSI/MSI-X
[ 6757.677171] [x] netdev success:0
[ 6757.677327] [x] netdev success:1
[ 6757.677350] vlinkty_pci init
#again we could find ethernet interface by following commands
root@chillancezen-virtual-machine:/mnt/projects/vlinky/vlinky-pci# ip -d li
1: lo: <LOOPBACK,UP,LOWER_UP> mtu 65536 qdisc noqueue state UNKNOWN mode DEFAULT group default 
    link/loopback 00:00:00:00:00:00 brd 00:00:00:00:00:00 promiscuity 0 
2: eth0: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc pfifo_fast state UP mode DEFAULT group default qlen 1000
    link/ether 52:54:00:12:34:56 brd ff:ff:ff:ff:ff:ff promiscuity 0 
3: eth1: <BROADCAST,MULTICAST,UP,LOWER_UP> mtu 1500 qdisc pfifo_fast state UP mode DEFAULT group default qlen 1000
    link/ether 52:54:00:12:34:57 brd ff:ff:ff:ff:ff:ff promiscuity 0 
18: vlinky0: <BROADCAST,MULTICAST> mtu 1500 qdisc noop state DOWN mode DEFAULT group default qlen 1000
    link/ether 00:00:00:00:00:00 brd ff:ff:ff:ff:ff:ff promiscuity 0 
19: vlinky1: <BROADCAST,MULTICAST> mtu 1500 qdisc noop state DOWN mode DEFAULT group default qlen 1000
    link/ether 00:00:00:00:00:00 brd ff:ff:ff:ff:ff:ff promiscuity 0 
#the vlinky0 and vlinky1 are the device I have created,-----_______------
root@chillancezen-virtual-machine:/mnt/projects/vlinky/vlinky-pci# ip link set vlinky1 up
root@chillancezen-virtual-machine:/mnt/projects/vlinky/vlinky-pci# dmesg |tail -5
[ 7047.197087] vlinky irq:48 1.0
[ 7048.200430] vlinky irq:48 1.0
[ 7049.204806] vlinky irq:48 1.0
[ 7050.217174] vlinky irq:48 1.0
[ 7051.246207] vlinky irq:48 1.0
#once the device is open, the interrupts are enabled.we could check the interrupt by 
root@chillancezen-virtual-machine:/mnt/projects/vlinky/vlinky-pci# cat /proc/interrupts 
           CPU0       CPU1       
 ............
 46:          0          0   PCI-MSI-edge      vlinky-channel0.0
 47:          0          0   PCI-MSI-edge      vlinky-channel0.1
 48:        114          0   PCI-MSI-edge      vlinky-channel1.0
 49:          0          0   PCI-MSI-edge      vlinky-channel1.1
 50:          0          0   PCI-MSI-edge      vlinky-channel1.2
 51:          0          0   PCI-MSI-edge      vlinky-channel1.3
.............
```
