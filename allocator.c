#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define ALLOC_SHIFT			(0)
#define LR_SHIFT			(1)
#define VALID_SHIFT			(2)
#define SIZE_SHIFT			(3)

#define ALLOC_BITS			(1)
#define LR_BITS				(1)
#define VALID_BITS			(1)
#define SIZE_BITS			(5)

#define ALLOC_MASK			(((1 << ALLOC_BITS) - 1) << ALLOC_SHIFT)
#define LR_MASK				(((1 << LR_BITS) - 1) << LR_SHIFT)
#define VALID_MASK			(((1 << VALID_BITS) - 1) << VALID_SHIFT)
#define SIZE_MASK			(((1 << SIZE_BITS) - 1) << SIZE_SHIFT)

#define ALLOC_BUSY			(1 << ALLOC_SHIFT)
#define ALLOC_FREE			(0 << ALLOC_SHIFT)

#define LR_LEFT				(0 << LR_SHIFT)
#define LR_RIGHT			(1 << LR_SHIFT)

#define VALID_YES			(1 << VALID_SHIFT)
#define VALID_NO			(0 << VALID_SHIFT)

#define MAX_MAX_ORDER			(1 << SIZE_BITS) - 1
#define order(x)			(((x) & SIZE_MASK) >> SIZE_SHIFT)

#define TRUE				(1)
#define FALSE				(0)

#define BITS_PER_UINT32			(32)
#define UINT32_MASK			(0xffffffff)

#define pr_err printf

#if defined (__KERNEL__)
#define plat_bug BUG
#else
static inline void plat_bug()
{
	while(1);
}
#endif
static inline uint32_t is_pot(uint32_t num);

struct lock_ops {
	uint32_t (*lock)(void);
	void (*unlock)(void);
};

struct sheap {
	struct lock_ops *ops;

	uint8_t *mem;
	uint32_t min;
	uint32_t size;

	uint32_t min_shift;
	uint32_t max_order;

	uint32_t num_min_blocks;

	uint8_t *meta;
	uint32_t meta_size;
	uint32_t meta_order;
};


static inline void init_meta(struct sheap *sheap)
{
	uint32_t size = sheap->meta_order;
	uint32_t current_pos = 0;

	sheap->meta[current_pos] = ALLOC_BUSY | LR_LEFT | VALID_YES | (size << SIZE_SHIFT);
	current_pos = current_pos + (1 << size);

	while(current_pos < sheap->meta_size) {
		sheap->meta[current_pos] = ALLOC_FREE | LR_RIGHT | VALID_YES | (size << SIZE_SHIFT);
		current_pos += (1 << size);
		size++;
	}
}

static inline uint32_t plat_log2(uint32_t num)
{
#if defined (__KERNEL__)
	return ilog2(num);
#else
	int bit;
	for(bit = 0; bit < BITS_PER_UINT32; bit++)
		if(((UINT32_MASK << bit) & num) == 0)
			return bit - 1;
	return BITS_PER_UINT32 - 1;
#endif
}

static inline uint32_t ceil_order_of_min(struct sheap *sheap, uint32_t num)
{
	uint32_t ceil_one = FALSE;
	uint32_t ret;

	if(is_pot(num) == FALSE)
		ceil_one = TRUE;

	num >>= sheap->min_shift;
	ret = plat_log2(num);

	return (ceil_one == TRUE) ? (ret + 1) : ret; 
}

static inline void *plat_alloc(uint32_t size)
{
#if defined(__KERNEL__)
	return kmalloc(size, GFP_KERNEL);
#else
	return malloc(size);
#endif
}

static inline void *plat_zalloc(uint32_t size)
{
#if defined(__KERNEL__)
	return kzalloc(size, GFP_KERNEL);
#else
	return calloc(size, 1);
#endif
}

static inline void *plat_alloc_atomic(uint32_t size)
{
#if defined(__KERNEL__)
	return kmalloc(size, GFP_ATOMIC);
#else
	return plat_alloc(size);
#endif
}

static inline void *plat_zalloc_atomic(uint32_t size)
{
#if defined(__KERNEL__)
	return kzalloc(size, GFP_ATOMIC);
#else
	return plat_zalloc(size);
#endif
}

static inline void *plat_free(void *m, uint32_t size)
{
#if defined(__KERNEL__)
	kfree(m);
#else
	free(m);
#endif
}

static inline uint32_t is_pot(uint32_t num)
{
	/* Is there only one '1'? */
	if(num & (num - 1))
		return FALSE;

	return TRUE;
}

static inline uint32_t is_power_of(uint32_t res, uint32_t base)
{
	uint32_t sum = res | base;

	/* eliminate the '1' on LSB */
	sum = (sum & (sum - 1));

	if(sum & (sum - 1))
		return FALSE;

	return TRUE;
}

struct sheap *sheap_init(uint8_t *mem, uint32_t size, uint32_t min, struct lock_ops *ops)
{
	struct sheap *sheap;

	if(ops == NULL) {
		pr_err("ops = %p must be non-null\n", ops);
		return NULL;
	}

	if(ops->lock == NULL) {
		pr_err("ops->lock = %p must be non-null\n", ops->lock);
		return NULL;
	}

	if(min == 0) {
		pr_err("min = %u, min must be non-zero\n", min);
		return NULL;
	}

	if(size == 0) {
		pr_err("size = %u, size must be non-zero\n", size);
		return NULL;
	}

	if(size < min) {
		pr_err("size = %u, min = %u, size must be greater-equal min\n", size, min);
		return NULL;
	}

	if(is_pot(min) == FALSE) {
		pr_err("min = %u not a power of 2\n", min);
		return NULL;
	}

	if(is_pot(size) == FALSE) {
		pr_err("size = %u not a power of 2\n", size);
		return NULL;
	}

	if(is_power_of(size, min) == FALSE) {
		pr_err("min = %u cannot be left-shifted to size = %u\n", min, size);
		return NULL;
	}

	sheap = plat_zalloc(sizeof(struct sheap));
	if(sheap == NULL) {
		pr_err("heap allocation failed\n");
		return NULL;
	}

	sheap->min = min;
	sheap->size = size;
	sheap->ops = ops;
	sheap->mem = mem;

	sheap->min_shift = plat_log2(min);
	sheap->max_order = ceil_order_of_min(sheap, size);
	if(sheap->max_order > MAX_MAX_ORDER) {
		pr_err("max_order = %u for size = %u, min = %u does not fit in size_cell = %u\n", sheap->max_order, size, min, MAX_MAX_ORDER);
		plat_free(sheap, sizeof(struct sheap));
		return NULL;
	}

	sheap->num_min_blocks = size / min;
	sheap->meta_size = sheap->num_min_blocks;
	sheap->meta_order = ceil_order_of_min(sheap, sheap->meta_size);

	sheap->meta = mem;

	init_meta(sheap);

	return sheap;
}

static inline uint32_t next_block(struct sheap *sheap, uint32_t current_pos)
{
	uint8_t meta = sheap->meta[current_pos];	
	uint32_t order_pos = order(meta);
	uint32_t size = sheap->min << order_pos;

	return current_pos + size;
}


void *sheap_alloc(struct sheap *sheap, uint32_t req_size)
{
	uint32_t alloc_order;
	uint32_t search_pos = 0;
	uint32_t partner_pos;
	uint32_t order_pos;
	uint32_t order_size;
	uint8_t meta;

	if(!req_size) {
		//pr_err("requested size = 0\n");
		return NULL;
	}
	if(req_size > (sheap->size / 2)) {
		//pr_err("requested size %u greater than max alloc limit = %u\n", req_size, sheap->size / 2);
		return NULL;
	}

	if(req_size < sheap->min)
		alloc_order = 0;
	else
		alloc_order = ceil_order_of_min(sheap, req_size);

	while(sheap->ops->lock() == FALSE);

	while(search_pos < sheap->num_min_blocks) {
		meta = sheap->meta[search_pos];
		order_pos = order(meta);
		order_size = (1 << order_pos);

		if((meta & VALID_MASK) != VALID_YES) {
			pr_err("debug landing at unaligned block\n");
			plat_bug();
		}

		if(alloc_order <= order_pos) {
			if((meta & ALLOC_MASK) == ALLOC_BUSY) {
				/* Go to the next block of same order */
				search_pos += order_size;
				continue;
			} else if(order_pos == alloc_order) {
				/* Mark allocated and return */
				sheap->meta[search_pos] = (sheap->meta[search_pos] & (~ALLOC_MASK)) | ALLOC_BUSY;
				sheap->ops->unlock();
				return &sheap->mem[sheap->min * search_pos];
			} else {
				/* Partition the block */
				partner_pos = (search_pos + (order_size / 2));
				sheap->meta[search_pos] = ALLOC_FREE | LR_LEFT | VALID_YES | ((order_pos - 1) << SIZE_SHIFT); 
				sheap->meta[partner_pos] = ALLOC_FREE | LR_RIGHT | VALID_YES | ((order_pos - 1) << SIZE_SHIFT); 
				continue;
			}
		} else {
			/* Go to the next block of same order */
			search_pos += order_size;
		}

	}

	sheap->ops->unlock();
	return NULL;
}

static inline uint32_t decide_lr(struct sheap *sheap, uint32_t order, uint32_t pos)
{
	if ((pos >> order) % 2)
		return LR_RIGHT;
	else
		return LR_LEFT;

}

static inline uint32_t merge(struct sheap *sheap, uint32_t *pos)
{
	uint32_t meta;
	uint8_t *pmeta;
	uint32_t partner_pos, partner_meta;
	uint8_t *partner_pmeta;
	uint32_t order_size;
	uint32_t order_pos;

	meta = sheap->meta[*pos];
	pmeta = &sheap->meta[*pos];
	
	order_pos = order(meta);
	order_size = (1 << order_pos);

	if(order_pos >= sheap->max_order)
		return FALSE;

	if((meta & LR_MASK) == LR_LEFT) {
		partner_pos = (*pos + (order_size));
		partner_meta = sheap->meta[partner_pos];
		partner_pmeta = &sheap->meta[partner_pos];

		if(order(partner_meta) != order_pos)
			return FALSE;
		if((partner_meta & ALLOC_MASK) != ALLOC_FREE)
			return FALSE;

		*pmeta = ALLOC_FREE | decide_lr(sheap, order_pos + 1, *pos) | VALID_YES | ((order_pos + 1) << SIZE_SHIFT);
		*partner_pmeta = 0;

		return TRUE;
	} else {
		partner_pos = (*pos - (order_size));
		partner_meta = sheap->meta[partner_pos];
		partner_pmeta = &sheap->meta[partner_pos];

		if(order(partner_meta) != order_pos)
			return FALSE;
		if((partner_meta & ALLOC_MASK) != ALLOC_FREE)
			return FALSE;

		*partner_pmeta = ALLOC_FREE | decide_lr(sheap, order_pos + 1, partner_pos)| VALID_YES | ((order_pos + 1) << SIZE_SHIFT);
		*pmeta = 0;

		*pos = partner_pos;

		return TRUE;
	}
}

void sheap_free(struct sheap *sheap, void *ptr)
{
	uint32_t pos;
	uint32_t meta;

	if(ptr == NULL)
		return;

	if((uint8_t *)ptr < sheap->mem || (uint8_t *)ptr > (sheap->mem + sheap->size))
		return;

	pos = ((uint8_t *)ptr - sheap->mem) / sheap->min;

	while(sheap->ops->lock() == FALSE);

	meta = sheap->meta[pos];

	if(meta == 0)
		goto out;

	if((meta & ALLOC_MASK) == ALLOC_FREE)
		goto out;

	sheap->meta[pos] = ((sheap->meta[pos] & ~ALLOC_MASK) | ALLOC_FREE);

	while(merge(sheap, &pos) == TRUE);

out:
	sheap->ops->unlock();

}


static uint32_t dummy_lock(void)
{
	return TRUE;
}

static void dummy_unlock(void)
{
}

#if 1
#include <wayland-util.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>

enum action {
	ACTION_ALLOC,
	ACTION_FREE,
	ACTION_USE,
	ACTION_CHECK,
	ACTION_MAX,
};

enum action coin_flip()
{
	return rand() % ACTION_MAX;
}

struct data {
	void *mem;
	uint32_t sz;
	uint32_t data_present;
	uint32_t checksum_offset;
	struct wl_list link;
};
struct sheap *__sheap = NULL;
struct wl_list glist;
int glist_count = 0;
int urandom_file = 0;

struct data *traverse_list(struct wl_list *list, int idx)
{
	struct data *d;
	int c = 0;
	wl_list_for_each(d, list, link) {
		if(c == idx)
			break;
		c++;
	}
	return d;
		
}

void checksum_calc(void *p, uint32_t off, uint32_t sz)
{
	uint8_t *mem = p;
	uint8_t sum = 0;
	int i;

	mem[off] = 0;
	for(i = 0; i < sz; i++)
		sum += mem[i];
	mem[off] = sum;

}

int checksum_validate(void *p, uint32_t off, uint32_t sz)
{
	uint8_t *mem = p;
	uint8_t sum = mem[off];
	uint8_t newsum = 0;
	int i;

	mem[off] = 0;
	for(i = 0; i < sz; i++)
		newsum += mem[i];

	mem[off] = sum;

	if(sum == newsum)
		return 0;
	else
		return 1;
}

float gauss_rand()
{
	static float max = 0;
	float ret;
	float a1 = rand()/ (float)RAND_MAX;
	float a2 = rand()/ (float)RAND_MAX;

	ret = sqrt(-2 * log(a1)) * cos(2 * M_PI * a2);

	if(fabs(ret) > max) {
		max = fabs(ret);
		//printf("till now : %f\n", max);
	}
	return ret / 3;
}

static inline void nop()
{
}

static void print_list()
{
	struct data *data, *d;
	uint32_t sz = 0;
	wl_list_for_each(d, &glist, link) {
		printf("%u, ", d->sz);
		sz += d->sz;
	}
	printf("%u = %u\n", (2 * 1024 * 1024) / 128, sz + (2 * 1024 * 1024) / 128);
}

void allocate_one()
{
	struct data *data, *d;
	void *ret;
	uint32_t sz = fabs(gauss_rand()) * (256 * 1024);
	sz = __sheap->min << ceil_order_of_min(__sheap, sz);
	ret = sheap_alloc(__sheap, sz);
	if(ret != NULL) {
		wl_list_for_each(d, &glist, link) {
			if(ret >= d->mem && ret < (d->mem + d->sz)) {
				printf("looping\n");
				while(1);
			}
		}
		//printf("allocation success for sz = %u %p\n", sz, ret);
		data = calloc(sizeof(*data), 1);
		if(!data) {
			printf("data alloc error\n");
			exit(-1);
		}
		data->mem = ret;
		data->sz = sz;
		data->data_present = 0;
		wl_list_insert(&glist, &data->link);
		glist_count++;
	} else {
		if(sz == 0 || sz >= 8192)
			nop();
		else {
			printf("NULL allocation for sz = %u\n", sz);
			print_list();
		}
	}
}

void deallocate_one()
{
	struct data *data;
	if(glist_count == 0)
		return;
	uint32_t idx = rand() % glist_count;
	data = traverse_list(&glist, idx);
	sheap_free(__sheap, data->mem);
	//printf("deallocation success %p\n", data->mem);
	wl_list_remove(&data->link);
	free(data);
	glist_count--;
}

void use_one()
{
	static int s = 0;
	struct data *data;
	if(glist_count == 0)
		return;
	uint32_t idx = rand() % glist_count;
	data = traverse_list(&glist, idx);

	uint8_t *mem = calloc(data->sz, 1);
	int ret = read(urandom_file, mem, data->sz);
	if(ret != data->sz) {
		printf("read error\n");
		exit(-1);
	}
	int i;
	for(i = 0; i < data->sz; i++) {
		((uint8_t *)data->mem)[i] = mem[i];
		//if(&(((uint8_t *)(data->mem))[i]) == 0x7ffff7f97010) {
		//	s++;
		//	printf("break %d\n", s);
		//}
	}
	free(mem);
	data->data_present = 1;
	data->checksum_offset = rand() % data->sz;
	checksum_calc(data->mem, data->checksum_offset, data->sz);
}

void check_one()
{
	struct data *data;
	if(glist_count == 0)
		return;
	uint32_t idx = rand() % glist_count;
	data = traverse_list(&glist, idx);

	if(data->data_present == 0)
		return;

	if(checksum_validate(data->mem, data->checksum_offset, data->sz)) {
		printf("checksum issue\n");
		exit(-1);
	}
}


int main(int argc, char **argv)
{
	uint32_t size = 2 * 1024 * 1024;
	uint32_t min = 128;
	uint8_t *mem = calloc(size, 1);
	struct lock_ops ops = {
		.lock = dummy_lock,
		.unlock = dummy_unlock,
	};

	__sheap = sheap_init(mem, size, min, &ops);

	uint32_t uval;
	if(argc > 1)
		sscanf(argv[1], "%u", &uval);
	else {
		time_t val = time(0);
		uval = val;
	}

	printf("experiment with time = %u\n", uval);
	srand(uval);
	wl_list_init(&glist);
	urandom_file = open("/dev/urandom", O_RDONLY);

	while(1) {
		print_list();
		enum action action = coin_flip();
		if(action == ACTION_ALLOC)
			allocate_one();
		else if(action == ACTION_FREE)
			deallocate_one();
		else if(action == ACTION_USE)
			use_one();
		else if(action == ACTION_CHECK)
			check_one();
		else {
			printf("bad coin flip %u\n", action);
			exit(-1);
		}
	}
}
#else

#include <math.h>

struct sheap *__sheap = NULL;
int data[4096] = {0};
enum action {
	ACTION_ALLOC,
	ACTION_FREE,
	//ACTION_USE,
	//ACTION_CHECK,
	ACTION_MAX,
};

enum action coin_flip()
{
	return rand() % ACTION_MAX;
}

float gauss_rand()
{
	float a1 = rand()/ (float)RAND_MAX;
	float a2 = rand()/ (float)RAND_MAX;

	return (sqrt(-2 * log(a1)) * cos(2 * M_PI * a2)) / 3;
}

void allocate_one()
{
	int start;
	int len;
	int i;
	uint8_t *ret;
	uint32_t sz = fabs(gauss_rand()) * (256 * 1024);
	sz = __sheap->min << ceil_order_of_min(__sheap, sz);
	ret = sheap_alloc(__sheap, sz);
	if(ret != NULL) {
		//printf("allocation success for sz = %u %p\n", sz, ret);
		start = (ret - (uint8_t *)__sheap->mem) / 128;
		len = sz / 128;
			
		for(i = start; i < start + len; i++) {
			if(data[i] != 0)
				printf("problem\n");
			if(len == 1) {
				data[i] = 5;
				continue;
			}
			if(i == start)
				data[i] = 2;
			else if(i == start + len -1)
				data[i] = 3;
			else
				data[i] = 1;
		}
	} else {
		//printf("NULL allocation for sz = %u\n", sz);
	}
}

int arr[4096] = {0};

void deallocate_one()
{
	int i;
	int count = 0;
	memset(arr, 0, sizeof(arr));
	for(i = 0; i < 4096; i++) {
		if(data[i] == 2 || data[i] == 5) {
			arr[count] = i;
			count++;
		}
	}

	if(!count)
		return;

	int idx1 = rand();
	int idx = arr[idx1 % count];
	void *ret = __sheap->mem + (idx * 128);
	
	sheap_free(__sheap, ret);
	//printf("deallocation success %p\n", ret);
	if(data[idx] == 5) {
		data[idx] = 0;
		return;
	}
	while(1) {
		int p = data[idx];
		data[idx] = 0;
		if(p == 3)
			break;
		idx++;
		
	}
}

int main(int argc, char **argv)
{
	uint32_t size = 512 * 1024;
	uint32_t min = 128;
	uint8_t *mem = calloc(size, 1);
	struct lock_ops ops = {
		.lock = dummy_lock,
		.unlock = dummy_unlock,
	};

	__sheap = sheap_init(mem, size, min, &ops);

	uint32_t uval;
	if(argc > 1)
		sscanf(argv[1], "%u", &uval);
	else {
		time_t val = time(0);
		uval = val;
	}

	printf("experiment with time = %u\n", uval);
	srand(uval);
	//wl_list_init(&glist);
	//urandom_file = open("/dev/urandom", O_RDONLY);

	while(1) {
		enum action action = coin_flip();
		if(action == ACTION_ALLOC)
			allocate_one();
		else if(action == ACTION_FREE)
			deallocate_one();
		//else if(action == ACTION_USE)
		//	use_one();
		//else if(action == ACTION_CHECK)
		//	check_one();
		else {
			printf("bad coin flip %u\n", action);
			exit(-1);
		}
	}
}
#endif
#ifdef l;adjsa;kl
static void rotate(struct config_local *c, struct rotation_stats *s, int index)
{
	if(c->e[index].delta)
		c->e[index].curr += c->e[index].delta;
	else
		c->e[index].curr += 1;

	s->last_peg = index;

	if(c->e[index].curr > c->e[index].max) {
		c->e[index].curr = c->e[index].min;
		if(index < c->num_elems - 1)
			rotate(c, s, index + 1);
		else
			s->overflow = true;
	}
}
#endif
