cmd_/home/openstack/linkerovs/ovs/datapath/linux/flex_array.o := gcc -Wp,-MD,/home/openstack/linkerovs/ovs/datapath/linux/.flex_array.o.d  -nostdinc -isystem /usr/lib/gcc/x86_64-linux-gnu/4.8/include -I/home/openstack/linkerovs/ovs/include -I/home/openstack/linkerovs/ovs/datapath/linux/compat -I/home/openstack/linkerovs/ovs/datapath/linux/compat/include  -I./arch/x86/include -Iarch/x86/include/generated/uapi -Iarch/x86/include/generated  -Iinclude -I./arch/x86/include/uapi -Iarch/x86/include/generated/uapi -I./include/uapi -Iinclude/generated/uapi -include ./include/linux/kconfig.h -Iubuntu/include  -D__KERNEL__ -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Werror-implicit-function-declaration -Wno-format-security -std=gnu89 -m64 -falign-jumps=1 -falign-loops=1 -mno-80387 -mno-fp-ret-in-387 -mtune=generic -mno-red-zone -mcmodel=kernel -funit-at-a-time -maccumulate-outgoing-args -DCONFIG_X86_X32_ABI -DCONFIG_AS_FXSAVEQ=1 -DCONFIG_AS_SSSE3=1 -DCONFIG_AS_CRC32=1 -DCONFIG_AS_AVX=1 -DCONFIG_AS_AVX2=1 -pipe -Wno-sign-compare -fno-asynchronous-unwind-tables -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -mno-avx -fno-delete-null-pointer-checks -O2 --param=allow-store-data-races=0 -Wframe-larger-than=1024 -fstack-protector -Wno-unused-but-set-variable -fno-omit-frame-pointer -fno-optimize-sibling-calls -fno-var-tracking-assignments -pg -mfentry -DCC_USING_FENTRY -Wdeclaration-after-statement -Wno-pointer-sign -fno-strict-overflow -fconserve-stack -Werror=implicit-int -Werror=strict-prototypes -DCC_HAVE_ASM_GOTO -DVERSION=\"2.5.0\" -I/home/openstack/linkerovs/ovs/datapath/linux/.. -I/home/openstack/linkerovs/ovs/datapath/linux/.. -g -include /home/openstack/linkerovs/ovs/datapath/linux/kcompat.h  -DMODULE  -D"KBUILD_STR(s)=\#s" -D"KBUILD_BASENAME=KBUILD_STR(flex_array)"  -D"KBUILD_MODNAME=KBUILD_STR(openvswitch)" -c -o /home/openstack/linkerovs/ovs/datapath/linux/.tmp_flex_array.o /home/openstack/linkerovs/ovs/datapath/linux/flex_array.c

source_/home/openstack/linkerovs/ovs/datapath/linux/flex_array.o := /home/openstack/linkerovs/ovs/datapath/linux/flex_array.c

deps_/home/openstack/linkerovs/ovs/datapath/linux/flex_array.o := \
  /home/openstack/linkerovs/ovs/datapath/linux/kcompat.h \
  include/generated/uapi/linux/version.h \

/home/openstack/linkerovs/ovs/datapath/linux/flex_array.o: $(deps_/home/openstack/linkerovs/ovs/datapath/linux/flex_array.o)

$(deps_/home/openstack/linkerovs/ovs/datapath/linux/flex_array.o):
