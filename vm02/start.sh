export PATH=$PATH:/usr/local/bin:/usr/local/sbin

ethtool -G ens3f1 rx 4096
ethtool -G ens3f1 tx 4096

echo "1024000000" > /proc/sys/net/core/rmem_max
echo "1024000000" > /proc/sys/net/core/wmem_max

source /home/root/linkerovs/dpdkrc

mount -t hugetlbfs -o pagesize=1G none /dev/hugepages

modprobe uio
insmod $DPDK_BUILD/kmod/igb_uio.ko

$DPDK_DIR/tools/dpdk_nic_bind.py --bind=igb_uio ens3f1

modprobe openvswitch

ovsdb-server --remote=punix:/usr/local/var/run/openvswitch/db.sock --remote=db:Open_vSwitch,Open_vSwitch,manager_options --pidfile --detach --log-file

ovs-vswitchd --dpdk -c 0x2 -n 4 --socket-mem 2048,2048 -- --pidfile --detach --log-file

iptables -t nat -A POSTROUTING -o eno1 -j MASQUERADE
iptables -t nat -A POSTROUTING -o br0 -j MASQUERADE

ifconfig br0 192.168.200.2/24
ip link set br0 up

ifconfig br0 mtu 9000

ip link set br0 qlen 1000


