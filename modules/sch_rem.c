/*
 * net/sched/sch_rem.c
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 *
 * Authors:	Junlong Qiao, <zheolong@126.com>
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/skbuff.h>
#include <net/pkt_sched.h>
#include <net/inet_ecn.h>
//#include "/root/AQM/rem/include/rem.h"
#include "rem.h"
#include <rem_queue.h>
#include <math.h>
#include <asm/i387.h>   //to support the Floating Point Operation
#include <linux/rwsem.h>
#include <linux/kthread.h>
#include <linux/interrupt.h>

//Queue Length Statistics
#define CYC_MAX 20
int cyc_count;

struct task_struct *tsk_prob;
struct task_struct *tsk_stop_prob;


#ifndef SLEEP_MILLI_SEC  
#define SLEEP_MILLI_SEC(nMilliSec)\
do {\
long timeout = (nMilliSec) * HZ / 1000;\
while(timeout > 0)\
{\
timeout = schedule_timeout(timeout);\
}\
}while(0);
#endif


/*	Parameters, settable by user:
	-----------------------------

	limit		- bytes (must be > qth_max + burst)

	Hard limit on queue length, should be chosen >qth_max
	to allow packet bursts. This parameter does not
	affect the algorithms behaviour and can be chosen
	arbitrarily high (well, less than ram size)
	Really, this limit will never be reached
	if RED works correctly.
 */

struct rem_sched_data {
	struct timer_list 	ptimer;	

	u32			limit;		/* HARD maximal queue length */
	unsigned char		flags;

	struct rem_parms	parms;
	struct rem_stats	stats;
	struct Qdisc		*qdisc;
};

struct queue_show queue_show_base_rem[QUEUE_SHOW_MAX];EXPORT_SYMBOL(queue_show_base_rem);
int array_element_rem = 0;EXPORT_SYMBOL(array_element_rem);

static void __inline__ rem_mark_probability(struct Qdisc *sch);

static inline int rem_use_ecn(struct rem_sched_data *q)
{
	return q->flags & TC_REM_ECN;
}

static inline int rem_use_harddrop(struct rem_sched_data *q)
{
	return q->flags & TC_REM_HARDDROP;
}

static int rem_enqueue(struct sk_buff *skb, struct Qdisc *sch)
{
	struct rem_sched_data *q = qdisc_priv(sch);
    struct rem_parms *parms = &q->parms;
	struct Qdisc *child = q->qdisc;
	int ret;

	switch (rem_action(&q->parms)) {
	case REM_DONT_MARK:
		
		//每CYC_MAX统计一次队列长度值
		//record queue length once every CYC_MAX
		cyc_count++;
		parms->in++;
		parms->pktsize = (parms->pktsize * parms->pktcount + skb->len) / (++(parms->pktcount));
		parms->ptc = parms->capacity / (8 * parms->pktsize);
		parms->q_len = sch->q.qlen;
		if(cyc_count==CYC_MAX){
			queue_show_base_rem[array_element_rem].length=sch->q.qlen;
			queue_show_base_rem[array_element_rem].numbers=array_element_rem;
			queue_show_base_rem[array_element_rem].mark_type=REM_DONT_MARK;
			queue_show_base_rem[array_element_rem].p=*((long long *)(&parms->proba));
			queue_show_base_rem[array_element_rem].in=parms->in;
			queue_show_base_rem[array_element_rem].in_avg=*((long long *)(&parms->in_avg));
			queue_show_base_rem[array_element_rem].pktsize=*((long long *)(&parms->pktsize));
			queue_show_base_rem[array_element_rem].price_link=*((long long *)(&parms->price_link));

//--------------------------------------------------------------------------------------------
			//历史数据更新(很难保持数据同步，所以还是在入队列以后进行这个更新操作比较好)
			//每次包来的时候调用
			
			if(array_element_rem < QUEUE_SHOW_MAX-1)  array_element_rem++;
			cyc_count = 0;
		}

		break;

	case REM_PROB_MARK:
		
		//每CYC_MAX统计一次队列长度值
		//record queue length once every CYC_MAX
		cyc_count++;
		parms->in++;
		parms->pktsize = (parms->pktsize * parms->pktcount + skb->len) / (++(parms->pktcount));
		parms->ptc = parms->capacity / (8 * parms->pktsize);
		parms->q_len = sch->q.qlen;
		if(cyc_count==CYC_MAX){
			queue_show_base_rem[array_element_rem].length=sch->q.qlen;
			queue_show_base_rem[array_element_rem].numbers=array_element_rem;
			queue_show_base_rem[array_element_rem].mark_type=REM_PROB_MARK;
			queue_show_base_rem[array_element_rem].p=*((long long *)(&parms->proba));
			queue_show_base_rem[array_element_rem].in=parms->in;
			queue_show_base_rem[array_element_rem].in_avg=*((long long *)(&parms->in_avg));
			queue_show_base_rem[array_element_rem].pktsize=*((long long *)(&parms->pktsize));
			queue_show_base_rem[array_element_rem].price_link=*((long long *)(&parms->price_link));

//--------------------------------------------------------------------------------------------
			//历史数据更新(很难保持数据同步，所以还是在入队列以后进行这个更新操作比较好)
			//每次包来的时候调用

			if(array_element_rem < QUEUE_SHOW_MAX-1)  array_element_rem++;
			cyc_count = 0;
		}

		sch->qstats.overlimits++;
		if (!rem_use_ecn(q) || !INET_ECN_set_ce(skb)) {
			q->stats.prob_drop++;
			goto congestion_drop;
		}

		q->stats.prob_mark++;
		break;
	}

	ret = qdisc_enqueue(skb, child);

//--------------------------------------------------------------------------------------------
	if (likely(ret == NET_XMIT_SUCCESS)) {
		sch->q.qlen++;

		sch->qstats.backlog += skb->len;/*2012-1-21*/
		//sch->qstats.bytes += skb->len;/*2012-1-21*/
		//sch->qstats.packets++;/*2012-1-21*/

	} else if (net_xmit_drop_count(ret)) {
		q->stats.pdrop++;
		sch->qstats.drops++;
	}

	return ret;

congestion_drop:
	qdisc_drop(skb, sch);
	return NET_XMIT_CN;
}

static struct sk_buff *rem_dequeue(struct Qdisc *sch)
{
	struct sk_buff *skb;
	struct rem_sched_data *q = qdisc_priv(sch);
	struct Qdisc *child = q->qdisc;

	skb = child->dequeue(child);
	if (skb) {
		qdisc_bstats_update(sch, skb);

		sch->qstats.backlog -= skb->len;/*2012-1-21*/	

		sch->q.qlen--;
	} else {
	}
	return skb;
}

static struct sk_buff *rem_peek(struct Qdisc *sch)
{
	struct rem_sched_data *q = qdisc_priv(sch);
	struct Qdisc *child = q->qdisc;

	return child->ops->peek(child);
}

static unsigned int rem_drop(struct Qdisc *sch)
{
	struct rem_sched_data *q = qdisc_priv(sch);
	struct Qdisc *child = q->qdisc;
	unsigned int len;

	if (child->ops->drop && (len = child->ops->drop(child)) > 0) {

		sch->qstats.backlog -= len;/*2012-1-21*/					

		q->stats.other++;
		sch->qstats.drops++;
		sch->q.qlen--;
		return len;
	}

	return 0;
}

static void rem_reset(struct Qdisc *sch)
{
	struct rem_sched_data *q = qdisc_priv(sch);

	qdisc_reset(q->qdisc);

	sch->qstats.backlog = 0;/*2012-1-21*/	
	
	sch->q.qlen = 0;

	array_element_rem = 0;/*2012-1-21*/
	
	rem_restart(&q->parms);
}

static void rem_destroy(struct Qdisc *sch)
{
	struct rem_sched_data *q = qdisc_priv(sch);

    array_element_rem = 0;

	kernel_fpu_end();

	qdisc_destroy(q->qdisc);
}

static const struct nla_policy rem_policy[TCA_REM_MAX + 1] = {
	[TCA_REM_PARMS]	= { .len = sizeof(struct tc_rem_qopt) },
	[TCA_REM_STAB]	= { .len = REM_STAB_SIZE },
};

/*qjl
缩写的一些说明：
Qdisc    		Queue discipline
rem      		rem method
nlattr   		net link attributes
rem_sched_data   rem scheduler data
qdisc_priv           qdisc private（Qdisc中针对特定算法如REM的数据）
tca			traffic controll attributes
nla 			net link attributes
*/
static int rem_change(struct Qdisc *sch, struct nlattr *opt)
{
	struct rem_sched_data *q = qdisc_priv(sch);
	struct nlattr *tb[TCA_REM_MAX + 1];
	struct tc_rem_qopt *ctl;
	struct Qdisc *child = NULL;
	int err;

	if (opt == NULL)
		return -EINVAL;

	err = nla_parse_nested(tb, TCA_REM_MAX, opt, rem_policy);
	if (err < 0)
		return err;

	if (tb[TCA_REM_PARMS] == NULL ||
	    tb[TCA_REM_STAB] == NULL)
		return -EINVAL;
	
	/*求有效载荷的起始地址*/
	ctl = nla_data(tb[TCA_REM_PARMS]);

	if (ctl->limit > 0) {
		child = fifo_create_dflt(sch, &bfifo_qdisc_ops, ctl->limit);
		if (IS_ERR(child))
			return PTR_ERR(child);
	}

	sch_tree_lock(sch);
	q->flags = ctl->flags;
	q->limit = ctl->limit;
	if (child) {
		qdisc_tree_decrease_qlen(q->qdisc, q->qdisc->q.qlen);
		qdisc_destroy(q->qdisc);
		q->qdisc = child;
	}

	//设置算法参数，此函数是在rem.h中定义的
	rem_set_parms(&q->parms, ctl->sampl_period, 
								 ctl->q_ref, ctl->p_init, ctl->p_min, ctl->p_max, 
							 	 ctl->inw, ctl->gamma, ctl->phi, ctl->capacity,
								 ctl->Scell_log, nla_data(tb[TCA_REM_STAB]));

	array_element_rem = 0;/*2012-1-21*/

	sch_tree_unlock(sch);

	return 0;
}

int rem_thread_func(void* data)
{
	struct Qdisc *sch=(struct Qdisc *)data;
	struct rem_sched_data *q = qdisc_priv(sch);
    struct rem_parms *parms = &q->parms;
	do{
		SLEEP_MILLI_SEC(10);
	}
	while(parms->in < 200);
			
	do{
		sch=(struct Qdisc *)data;
		rem_mark_probability(sch);
		SLEEP_MILLI_SEC(100);
		//schedule_timeout(1000000*HZ);
		printk("<1>haha");
	}while(!kthread_should_stop());
	return 0;
}
int rem_stop_prob_thread_func(void* data)
{
	struct Qdisc *sch=(struct Qdisc *)data;
	struct rem_sched_data *q = qdisc_priv(sch);
	SLEEP_MILLI_SEC(50000);
	//删除计时器，并将输出数据需要的array_element_rem置为0
	kthread_stop(tsk_prob);
	printk("<1>tsk prob stop");
	return 0;
}

static void __inline__ rem_mark_probability(struct Qdisc *sch)
{
    struct rem_sched_data *data = qdisc_priv(sch);
    struct rem_parms *parms = &data->parms;

	double in, in_avg, price_link, proba, c;


//--------------------------REM算法丢弃/标记概率更新过程------------------------
	//link price
	//the congestion measure
	price_link = parms->price_link;

	//in is the number of packets arriving at the link(input rate) during one update time interval
	in = parms->in;

	//in_avg is the low pss filtered input rate which is in packets
	in_avg = parms->in_avg;

	//----------------------------------
	in_avg = (1.0 - parms->inw) * in_avg + parms->inw * in;
	
	//c measures the maximum number of packets that could be sent during one update interval
	//c = parms->sampl_period * parms->ptc;
	parms->c = 0.5 * parms->ptc;

	price_link = price_link + parms->gamma * (in_avg + 0.1 * (parms->q_len - parms->q_ref) - parms->c);

	//price_link = max(price_link,0)
	if(price_link < 0.0)
		price_link = 0.0;
	
	proba = 1.0 - pow(parms->phi, -price_link);
	//proba = 0.01;
	
    if (parms->proba < parms->p_min)
         parms->proba = parms->p_min;
    if (parms->proba > parms->p_max)
         parms->proba = parms->p_max;

	//----------------------------------
	parms->in = 0;
	parms->in_avg = in_avg;
	parms->price_link = price_link;
	parms->proba = proba;
//--------------------------------------------------------------------------------

}


static int rem_init(struct Qdisc *sch, struct nlattr *opt)
{
	struct rem_sched_data *q = qdisc_priv(sch);
	int ret;

	kernel_fpu_begin();

	array_element_rem = 0;/*2012-1-21*/

	cyc_count = 0;

	q->qdisc = &noop_qdisc;
	ret=rem_change(sch, opt);

	tsk_prob=kthread_run(rem_thread_func,(void*)sch,"rem");
	tsk_stop_prob=kthread_run(rem_stop_prob_thread_func,(void*)sch,"rem_stop_prob");

	return ret;
}

static int rem_dump(struct Qdisc *sch, struct sk_buff *skb)
{
	struct rem_sched_data *q = qdisc_priv(sch);
	struct nlattr *opts = NULL;
	struct tc_rem_qopt opt = {
		.limit		= q->limit,
		.sampl_period	= q->parms.sampl_period,
		.q_ref		= q->parms.q_ref,
		.p_max		= q->parms.p_max,
		.p_min		= q->parms.p_min,
		.p_init		= q->parms.p_init,
		.inw		= q->parms.inw,
		.gamma		= q->parms.gamma,
		.phi		= q->parms.phi,
		.capacity	= q->parms.capacity,
	};

	sch->qstats.backlog = q->qdisc->qstats.backlog;
	opts = nla_nest_start(skb, TCA_OPTIONS);
	if (opts == NULL)
		goto nla_put_failure;
	NLA_PUT(skb, TCA_REM_PARMS, sizeof(opt), &opt);
	return nla_nest_end(skb, opts);

nla_put_failure:
	nla_nest_cancel(skb, opts);
	return -EMSGSIZE;
}

static int rem_dump_stats(struct Qdisc *sch, struct gnet_dump *d)
{
	struct rem_sched_data *q = qdisc_priv(sch);
	struct tc_rem_xstats st = {
		.early	= q->stats.prob_drop + q->stats.forced_drop,
		.pdrop	= q->stats.pdrop,
		.other	= q->stats.other,
		.marked	= q->stats.prob_mark + q->stats.forced_mark,
	};

	return gnet_stats_copy_app(d, &st, sizeof(st));
}

static int rem_dump_class(struct Qdisc *sch, unsigned long cl,
			  struct sk_buff *skb, struct tcmsg *tcm)
{
	struct rem_sched_data *q = qdisc_priv(sch);

	tcm->tcm_handle |= TC_H_MIN(1);
	tcm->tcm_info = q->qdisc->handle;
	return 0;
}

static int rem_graft(struct Qdisc *sch, unsigned long arg, struct Qdisc *new,
		     struct Qdisc **old)
{
	struct rem_sched_data *q = qdisc_priv(sch);

	if (new == NULL)
		new = &noop_qdisc;

	sch_tree_lock(sch);
	*old = q->qdisc;
	q->qdisc = new;
	qdisc_tree_decrease_qlen(*old, (*old)->q.qlen);
	qdisc_reset(*old);
	sch_tree_unlock(sch);
	return 0;
}

static struct Qdisc *rem_leaf(struct Qdisc *sch, unsigned long arg)
{
	struct rem_sched_data *q = qdisc_priv(sch);
	return q->qdisc;
}

static unsigned long rem_get(struct Qdisc *sch, u32 classid)
{
	return 1;
}

static void rem_put(struct Qdisc *sch, unsigned long arg)
{
}

static void rem_walk(struct Qdisc *sch, struct qdisc_walker *walker)
{
	if (!walker->stop) {
		if (walker->count >= walker->skip)
			if (walker->fn(sch, 1, walker) < 0) {
				walker->stop = 1;
				return;
			}
		walker->count++;
	}
}

static const struct Qdisc_class_ops rem_class_ops = {
	.graft		=	rem_graft,
	.leaf		=	rem_leaf,
	.get		=	rem_get,
	.put		=	rem_put,
	.walk		=	rem_walk,
	.dump		=	rem_dump_class,
};

static struct Qdisc_ops rem_qdisc_ops __read_mostly = {
	.id		=	"rem",
	.priv_size	=	sizeof(struct rem_sched_data),
	.cl_ops	=	&rem_class_ops,
	.enqueue	=	rem_enqueue,
	.dequeue	=	rem_dequeue,
	.peek		=	rem_peek,
	.drop		=	rem_drop,
	.init		=	rem_init,
	.reset		=	rem_reset,
	.destroy	=	rem_destroy,
	.change	=	rem_change,
	.dump		=	rem_dump,
	.dump_stats	=	rem_dump_stats,
	.owner		=	THIS_MODULE,
};

static int __init rem_module_init(void)
{
	return register_qdisc(&rem_qdisc_ops);
}

static void __exit rem_module_exit(void)
{
	unregister_qdisc(&rem_qdisc_ops);
}

module_init(rem_module_init)
module_exit(rem_module_exit)

MODULE_LICENSE("GPL");
