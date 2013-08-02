/*qjl*/

#define QUEUE_SHOW_MAX 30000

struct queue_show{
           int numbers;
           __u32 length;
		   int mark_type; //REM_DONT_MARK or REM_PROB_MARK
		   long long p;
		   int in;
		   long long pktsize;
		   long long in_avg;
		   long long price_link;
};
extern struct queue_show queue_show_base_rem[QUEUE_SHOW_MAX];
extern int array_element_rem;
extern struct queue_show queue_show_base1[30000];
extern int array_element1;
