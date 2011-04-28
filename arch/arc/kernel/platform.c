#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/init.h>

#ifdef CONFIG_XILINX_TEMAC

static __init add_xtemac(void)
{
    struct platform_device *pd;
    pd = platform_device_register_simple("xilinx_temac",0,NULL,0);

    if (IS_ERR(pd))
    {
        printk("Fail\n");
    }

    return IS_ERR(pd) ? PTR_ERR(pd): 0;
}

device_initcall(add_xtemac);

#endif

#ifdef CONFIG_ISS_MAC
static __init add_iss(void)
{
    struct platform_device *pd;
    pd = platform_device_register_simple("arc_iss",0,NULL,0);

    if (IS_ERR(pd))
    {
        printk("Fail\n");
    }

    return IS_ERR(pd) ? PTR_ERR(pd): 0;
}

device_initcall(add_iss);
#endif
