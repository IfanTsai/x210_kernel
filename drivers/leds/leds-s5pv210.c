#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <mach/leds-gpio.h>
#include <mach/regs-gpio.h>
#include <mach/gpio-bank.h>

struct s5pv210_gpio_led {
    struct led_classdev          cdev;
    struct s5pv210_led_platdata *pdata;
};

static void s5pv210_led_set(struct led_classdev *led_cdev, enum led_brightness brightness)
{
    struct s5pv210_gpio_led *led = container_of(led_cdev, struct s5pv210_gpio_led, cdev);
    struct s5pv210_led_platdata *pd = led->pdata;

    printk(KERN_INFO "s5pv210_led_set\n");
    switch (brightness)
    {
        case LED_OFF: // 用户写的是0
            gpio_set_value(pd->gpio, 1);
            break;
        case LED_FULL: // 用户写的是255
            gpio_set_value(pd->gpio, 0);
            break;
        case LED_HALF:
            break;
    }
}

static int s5pv210_led_probe(struct platform_device *pdev)
{
    // 获取device中的platform_data
    struct s5pv210_led_platdata *pdata = pdev->dev.platform_data;
    struct s5pv210_gpio_led *led;
    if ( !(led = kzalloc(sizeof(struct s5pv210_gpio_led), GFP_KERNEL)) ) {
        dev_err(&pdev->dev, "No memory for device\n");
        return -ENOMEM;
    }

    if (gpio_request(pdata->gpio, pdata->name) < 0) {
        printk(KERN_ERR "gpio_request failed\r\n");
        goto err_gpio_request;
    } else {
        // 设置为输出模式，初始化值为1，LED灭
        gpio_direction_output(pdata->gpio, 1);
    }

    led->cdev.name = pdata->name;
    led->cdev.brightness = 0;
    led->cdev.brightness_set = s5pv210_led_set;
    led->pdata = pdata;
    platform_set_drvdata(pdev, led);

    if (led_classdev_register(&pdev->dev, &led->cdev) < 0) {
        printk(KERN_ERR "led_classdev_register failed\r\n");
        goto err_led_classdev_register;
    }

    return 0;

err_led_classdev_register:
    gpio_free(pdata->gpio);
err_gpio_request:
    kfree(led);
    return -1;
}

static int s5pv210_led_remove(struct platform_device *pdev)
{
    struct s5pv210_gpio_led *led = platform_get_drvdata(pdev);
    led_classdev_unregister(&led->cdev);
    gpio_free(led->pdata->gpio);
    kfree(led);

    return 0;

}

static struct platform_driver s5pv210_led_driver = {
    .probe      = s5pv210_led_probe,
    .remove     = s5pv210_led_remove,
    .driver     = {
        .name       = "s5pv210-led",
        .owner      = THIS_MODULE,
    },
};

static int __init s5pv210_led_init(void)
{
    return platform_driver_register(&s5pv210_led_driver);
}

static void __exit s5pv210_led_exit(void)
{
    platform_driver_unregister(&s5pv210_led_driver);
}

module_init(s5pv210_led_init);
module_exit(s5pv210_led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ifan Tsai <i@caiyifan.cn>");
MODULE_DESCRIPTION("S5PV210 LED driver");
MODULE_ALIAS("platform:s5pv210_led");
