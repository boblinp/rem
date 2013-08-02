/*qjl*/
#ifndef __NET_SCHED_REM_H
#define __NET_SCHED_REM_H

#include <linux/types.h>
#include <net/pkt_sched.h>
#include <net/inet_ecn.h>
#include <net/dsfield.h>
#include <asm/i387.h>   //为了支持浮点运算
/* The largest number rand will return (same as INT_MAX).  */
#define	RAND_MAX	2147483647

/*	REM algorithm.
	=======================================

*/
#define REM_STAB_SIZE	256
#define REM_STAB_MASK	(REM_STAB_SIZE - 1)

struct rem_stats {
	u32		prob_drop;	/* Early probability drops */
	u32		prob_mark;	/* Early probability marks */
	u32		forced_drop;	/* Forced drops, qavg > max_thresh */
	u32		forced_mark;	/* Forced marks, qavg > max_thresh */
	u32		pdrop;          /* Drops due to queue limits */
	u32		other;          /* Drops due to drop() calls */
};

struct rem_parms {
	/* Parameters */
	//队列控制
	int 	sampl_period;   //采样时间
	int q_ref;//参考队列长度
	double p_init;//初始丢弃概率
	double p_min;//最小丢弃概率
	double p_max;//最大丢弃概率
	//REM
	double inw;
	double gamma;
	double phi;

	double capacity;
		
	/* Variables */
	//队列控制
	//REM
	double pktsize;
	int pktcount;
	int ptc;
	double proba;	/* Packet marking probability */
	double price_link;
	double in;
	double in_avg;
	int q_len;
	double c;

	u32		Scell_max;
    u8		Scell_log;
	u8		Stab[REM_STAB_SIZE];
};

static inline void rem_set_parms(struct rem_parms *p, int sampl_period, 
                             int q_ref, double p_init, double p_min, double p_max, 
                             double inw, double gamma, double phi, double capacity,
				 u8 Scell_log, u8 *stab)
{
	/* Reset average queue length, the value is strictly bound
	 * to the parameters below, reseting hurts a bit but leaving
	 * it might result in an unreasonable qavg for a while. --TGR
	 */
//------------------------------------------------------
	/* Parameters */
	p->sampl_period	= sampl_period;
	p->q_ref		= q_ref;	// 参考队列长度 设置为300
	p->p_init		= p_init;	// 初始丢弃/标记概率  设置为0
	p->p_min		= p_min;	// 最小丢弃/标记概率  设置为0
	p->p_max		= p_max;	// 最大丢弃/标记概率  设置为1

	p->inw			= inw;
	p->gamma		= gamma;
	p->phi			= phi;

	p->capacity		= capacity;
//------------------------------------------------------
	/* Variables */
	p->pktsize		= 0; // 每次接收到包时更新（本来应该指定的，但是我们在真实环境下做不到，所以只能求个pktsize的平均值）
	p->pktcount		= 0; // 自算法启动以来接收到的数据包总数（用于计算pktsize的平均值-上一个变量，每收到一个包加1，并更新pktsize）
	p->ptc			= 0; // 链路上每秒所能容纳的最大包数（capacity/8/pktsize，其中capacity为链路容量，以bit/s为单位，例如100Mb/s，除以8是因为pktsize是以Byte为单位的）
	p->proba		= p_init;	/* Packet marking probability (每次采样周期更新)*/
	p->price_link	= 0; // 链路代价(每次采样周期更新)
	p->in			= 0; // 本采样周期中收到的数据包总数（每次接收到包时加1；每次采样周期更新完有用数据后，重新置为0）
	p->in_avg		= 0; // 所有（直到当前）采样周期的数据包总数加权平均值(每次采样周期更新)
	
	p->q_len		= 0;
	p->c			= 0;
//------------------------------------------------------
	p->Scell_log	= Scell_log;
	p->Scell_max	= (255 << Scell_log);

	memcpy(p->Stab, stab, sizeof(p->Stab));
}

static inline void rem_restart(struct rem_parms *p)
{
	//待定？？？？
}


/*-------------------------------------------------*/

enum {
	REM_BELOW_PROB,
	REM_ABOVE_PROB,
};

static inline int rem_cmp_prob(struct rem_parms *p)
{
	int p_random,current_p;
	p_random = abs(net_random());

	//p->p_k will be written by another thread, so when reading it's value in a diffent thread, "current_p_sem" should be set
	//kernel_fpu_begin();
	current_p = (int)(p->proba*RAND_MAX);
	//kernel_fpu_end();
		
	if ( p_random < current_p){
		return REM_BELOW_PROB;
	}
	else{
		return REM_ABOVE_PROB;
	}
}

enum {
	REM_DONT_MARK,
	REM_PROB_MARK,
};

static inline int rem_action(struct rem_parms *p)
{
	switch (rem_cmp_prob(p)) {
		case REM_ABOVE_PROB:
			return REM_DONT_MARK;

		case REM_BELOW_PROB:
			return REM_PROB_MARK;
	}

	BUG();
	return REM_DONT_MARK;
}

#endif
