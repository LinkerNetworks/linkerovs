#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

__visible struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

static const struct modversion_info ____versions[]
__used
__attribute__((section("__versions"))) = {
	{ 0x1fc32c62, __VMLINUX_SYMBOL_STR(module_layout) },
	{ 0x18d40d4a, __VMLINUX_SYMBOL_STR(rpl_geneve_xmit) },
	{ 0xf4721ed7, __VMLINUX_SYMBOL_STR(ovs_netdev_tunnel_destroy) },
	{ 0xbccea4c4, __VMLINUX_SYMBOL_STR(ovs_vport_ops_unregister) },
	{ 0x69f8e35e, __VMLINUX_SYMBOL_STR(__ovs_vport_ops_register) },
	{ 0xe6ed05df, __VMLINUX_SYMBOL_STR(__skb_get_hash) },
	{ 0xc31c3682, __VMLINUX_SYMBOL_STR(ovs_tunnel_get_egress_info) },
	{ 0x8bc8d441, __VMLINUX_SYMBOL_STR(ovs_vport_free) },
	{ 0xa8ce66de, __VMLINUX_SYMBOL_STR(ovs_netdev_link) },
	{ 0x6e720ff2, __VMLINUX_SYMBOL_STR(rtnl_unlock) },
	{ 0x227074ad, __VMLINUX_SYMBOL_STR(dev_change_flags) },
	{ 0xbe1315ac, __VMLINUX_SYMBOL_STR(rpl_geneve_dev_create_fb) },
	{ 0xc7a4fbed, __VMLINUX_SYMBOL_STR(rtnl_lock) },
	{ 0x99adf22d, __VMLINUX_SYMBOL_STR(ovs_vport_alloc) },
	{ 0xcd279169, __VMLINUX_SYMBOL_STR(nla_find) },
	{ 0xe7fd6047, __VMLINUX_SYMBOL_STR(nla_put) },
	{ 0xbdfb6dbb, __VMLINUX_SYMBOL_STR(__fentry__) },
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=openvswitch";


MODULE_INFO(srcversion, "9CD0AD87EA1D5685E1DBAD8");
