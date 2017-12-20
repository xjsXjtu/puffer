#include <linux/module.h>
#include <net/tcp.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/stat.h>
#include <linux/string.h>
#include <linux/socket.h>

#define TABLE_SIZE 65536
#define PROC_NAME "tcp_dumb"

/* hold cwnd info indexed by port number
*/
static u16 cwnd_table[TABLE_SIZE];
static const u32 default_cwnd = 42;

struct tcp_tuple {
        u32 saddr;
        u32 iaddr;
        u16 sport;
        u16 iport;
};

struct ctl_msg {
        unsigned short cwnd;
        struct tcp_tuple tcp;
};

/* proc */
static ssize_t dumb_write(struct file *filp, const char *buf, size_t count,
                loff_t *offp)
{
        struct ctl_msg msg;
        u16 port, cwnd;

        memset((char *)&msg, 0, sizeof(msg));
        if (count > sizeof(msg)) {
                printk(KERN_ERR "write size %ld exceeds u16 size %ld", count,
                                sizeof(u16));
        } else {
                copy_from_user(&msg, buf, count);
                cwnd = msg.cwnd;
                port = msg.tcp.iport;
                cwnd_table[port] = cwnd;
        }
        return count;
}


/* TCP */
static void dumb_init(struct sock *sk)
{
}

static u32 dumb_ssthresh(struct sock *sk)
{
        struct tcp_sock *tp = tcp_sk(sk);
        return tp->snd_ssthresh;
}

static void dumb_cong_avoid(struct sock *sk, u32 ack, u32 acked)
{
}

static void dumb_state(struct sock *sk, u8 new_state)
{
}

static u32 dumb_undo_cwnd(struct sock *sk)
{
        const struct tcp_sock *tp = tcp_sk(sk);
        return tp->snd_cwnd;
}

static void get_tcp_tuple(struct sock *sk, struct tcp_tuple *tuple) {
        struct sockaddr_in addr_in;
        const struct tcp_sock *tp = tcp_sk(sk);
        tp->inet_conn.icsk_af_ops->addr2sockaddr(sk,
                        (struct sockaddr *)&addr_in);
        tuple->saddr  = tp->inet_conn.icsk_inet.inet_saddr;
        tuple->sport  = tp->inet_conn.icsk_inet.inet_sport;
        tuple->iaddr  = addr_in.sin_addr.s_addr;
        tuple->iport  = addr_in.sin_port;
}

static void dumb_acked(struct sock *sk, const struct ack_sample *sample)
{
        struct tcp_sock *tp = tcp_sk(sk);
        u16 port, cwnd;
        struct tcp_tuple tuple;

        get_tcp_tuple(sk, &tuple);
        port = tuple.iport;
        cwnd = cwnd_table[port];
        tp->snd_cwnd = cwnd;
        printk(KERN_INFO "set sock %d to cwnd %d\n", port, cwnd);
}

static struct tcp_congestion_ops tcp_dumb = {
        .name           = "tcp_dumb",
        .owner          = THIS_MODULE,
        .init           = dumb_init,
        .ssthresh       = dumb_ssthresh,
        .cong_avoid     = dumb_cong_avoid,
        .set_state      = dumb_state,
        .undo_cwnd      = dumb_undo_cwnd,
        .pkts_acked     = dumb_acked,
};

static const struct file_operations dumb_proc_fops = {
        .owner = THIS_MODULE,
        .write = dumb_write,
};

static int __init tcp_dumb_register(void)
{
        int i;
        /* initialize cwnd table */
        for (i = 0; i < TABLE_SIZE; i++) {
                cwnd_table[i] = default_cwnd;
        }
        /* register TCP */
        tcp_register_congestion_control(&tcp_dumb);
        /* create proc file */
        proc_create(PROC_NAME, S_IRWXUGO, NULL, &dumb_proc_fops);
        return 0;
}

static void __exit tcp_dumb_unregister(void)
{
        tcp_unregister_congestion_control(&tcp_dumb);
        remove_proc_entry(PROC_NAME, NULL);
}

module_init(tcp_dumb_register);
module_exit(tcp_dumb_unregister);
MODULE_LICENSE("GPL");
