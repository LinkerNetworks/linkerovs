source /root/linkerovs/dpdkrc

cd ./dpdk-2.2.0

rm -rf x86_64-native-linuxapp-gcc

make install T=x86_64-native-linuxapp-gcc


cd ..

cd ./ovs-branch-2.5-dpdk

./boot.sh

./configure --with-dpdk=$DPDK_BUILD

make clean

make

make install

cd /usr/local/bin/dpdk

unalias cp

cp -rf * /

