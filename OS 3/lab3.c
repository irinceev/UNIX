#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/time.h>

#define PROC_NAME "tsulab"



static int tsulab_proc_show(struct seq_file *m, void *v)
{
    /*
     * Параметры полёта Восток-6:
     * старт: 16 июня 1963 года 09:29:52 UTC
     * реальная длительность: ~48 витков
     * орбитальный период: ~88.3 минуты
     */

    /* время старта Восток-6 (1963-06-16 09:29:52 UTC) */
    const time64_t start_ts = -185450408; /* заранее посчитанный timestamp */

    /* длительность одного витка в секундах (~88.3 мин) */
    const time64_t orbit_period_sec = 88 * 60 + 18; /* 88 мин 18 сек ≈ 88.3 мин */

    const long long real_orbits = 48; /* фактическое число витков */

    time64_t now = ktime_get_real_seconds();
    long long total_orbits = 0;
    long long extra_orbits = 0;

    if (now > start_ts) {
        time64_t dt = now - start_ts;
        total_orbits = dt / orbit_period_sec;
        if (total_orbits > real_orbits)
            extra_orbits = total_orbits - real_orbits;
        else
            extra_orbits = 0;
    } else {
        total_orbits = 0;
        extra_orbits = 0;
    }

    seq_printf(m,
        "Если бы Восток-6 не сводили с орбиты,\n"
        "Терешкова совершила бы ещё примерно %lld витков.\n",
        extra_orbits);

    return 0;
}

static int tsulab_proc_open(struct inode *inode, struct file *file)
{
    return single_open(file, tsulab_proc_show, NULL);
}

static const struct proc_ops tsulab_proc_ops = {
    .proc_open    = tsulab_proc_open,
    .proc_read    = seq_read,
    .proc_lseek   = seq_lseek,
    .proc_release = single_release,
};

static int __init tsulab_init(void)
{
    if (!proc_create(PROC_NAME, 0444, NULL, &tsulab_proc_ops))
        return -ENOMEM;

    pr_info("Welcome to the Tomsk State University\n");
    return 0;
}

static void __exit tsulab_exit(void)
{
    remove_proc_entry(PROC_NAME, NULL);
    pr_info("Tomsk State University forever!\n");
}

module_init(tsulab_init);
module_exit(tsulab_exit);

MODULE_LICENSE("GPL");