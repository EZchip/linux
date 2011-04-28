#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/init.h>

static __init add_xtemac(void)
{
    struct platform_device *pd;
printk("ADDING XTEMAC\n");
    pd = platform_device_register_simple("xilinx_temac",0,NULL,0);
printk("PD is %x\n", pd);

    if (IS_ERR(pd))
    {
        printk("Fail\n");
    }

    return IS_ERR(pd) ? PTR_ERR(pd): 0;
}

device_initcall(add_xtemac);
