#ifndef __UTILS_H__
#define __UTILS_H__

#define util_printf printf
#define util_fatal exit

struct zigzag_iterator {
	int x, y;
	uint32_t hmax, vmax;

	bool reset_needed, diag_up, diag_down;
};

#define ZZITER_INITIALIZER(w, h) {.x = 0, .y = 0, .reset_needed = false, .diag_up = true, .diag_down = false}
#define DEFINE_ZZITER(name, w, h) struct zigzag_iterator name = ZZITER_INITIALIZER(w, h)

static inline void init_zziter(struct zigzag_iterator *z, uint32_t w, uint32_t h)
{
	z->x = 0;
	z->y = 0;

	z->hmax = w;
	z->vmax = h;

	z->reset_needed = false;
	z->diag_up = true;
	z->diag_down = false;
}

static inline void reset_zziter(struct zigzag_iterator *z)
{
	z->x = 0;
	z->y = 0;

	z->reset_needed = false;
	z->diag_up = true;
	z->diag_down = false;
}

int zziter_next(struct zigzag_iterator *z, uint32_t *x, uint32_t *y);

uint32_t gcd(uint32_t a, uint32_t b);

struct perm_iterator;

static inline bool all_uintptr_t_equal_to(uintptr_t *arr, uint32_t count, uintptr_t val)
{
	int i;
	for(i = 0; i < count; i++)
		if(val != arr[i])
			return false;
	return true;
}

static inline bool any_uintptr_t_equal_to(uintptr_t *arr, uint32_t count, uintptr_t val)
{
	int i;
	for(i = 0; i < count; i++)
		if(val == arr[i])
			return true;
	return false;
}

static inline char *arsprintf(const char *fmt, ...)
{
	char *str, *ret = NULL;
	va_list ap;
	va_start(ap, fmt);
	if(vasprintf(&str, fmt, ap) != -1) {
		ret = strdup(str);
		free(str);
		goto out;
	}
out:
	va_end(ap);
	return ret;
}

#define all_equal_to(type, arr, count, val) all_ ##type ##_equal_to(arr, count, val)
#define any_equal_to(type, arr, count, val) any_ ##type ##_equal_to(arr, count, val)


#define PP_NARG(...) \
         PP_NARG_(__VA_ARGS__,PP_RSEQ_N())
#define PP_NARG_(...) \
         PP_ARG_N(__VA_ARGS__)
#define PP_ARG_N( \
          _1, _2, _3, _4, _5, _6, _7, _8, _9,_10, \
         _11,_12,_13,_14,_15,_16,_17,_18,_19,_20, \
         _21,_22,_23,_24,_25,_26,_27,_28,_29,_30, \
         _31,_32,_33,_34,_35,_36,_37,_38,_39,_40, \
         _41,_42,_43,_44,_45,_46,_47,_48,_49,_50, \
         _51,_52,_53,_54,_55,_56,_57,_58,_59,_60, \
         _61,_62,_63,_64,_65,_66,_67,_68,_69,_70, \
         _71,_72,_73,_74,_75,_76,_77,_78,_79,_80, \
         _81,_82,_83,_84,_85,_86,_87,_88,_89,_90, \
         _91,_92,_93,_94,_95,_96,_97,_98,_99,_100, \
         _101,_102,_103,_104,_105,_106,_107,_108,_109,_110, \
         _111,_112,_113,_114,_115,_116,_117,_118,_119,_120, \
         _121,_122,_123,_124,_125,_126,_127,N,...) N
#define PP_RSEQ_N() \
         127,126,125,124,123,122,121,120, \
         119,118,117,116,115,114,113,112,111,110, \
         109,108,107,106,105,104,103,102,101,100, \
         99,98,97,96,95,94,93,92,91,90, \
         89,88,87,86,85,84,83,82,81,80, \
         79,78,77,76,75,74,73,72,71,70, \
         69,68,67,66,65,64,63,62,61,60, \
         59,58,57,56,55,54,53,52,51,50, \
         49,48,47,46,45,44,43,42,41,40, \
         39,38,37,36,35,34,33,32,31,30, \
         29,28,27,26,25,24,23,22,21,20, \
         19,18,17,16,15,14,13,12,11,10, \
         9,8,7,6,5,4,3,2,1,0


void perm_iter_init(struct perm_iterator *p, uint32_t num_lists);
void perm_iter_reset(struct perm_iterator *p);

void perm_iter_add_range_uint32_t(struct perm_iterator *p, uint32_t idx, uint32_t min, uint32_t max, uint32_t incr);
void _perm_iter_add_uint32_t(struct perm_iterator *perm, uint32_t idx, size_t argc, ...);
#define perm_iter_add_uint32_t(p, idx, ...) _perm_iter_add_uint32_t(p, idx, PP_NARG(__VA_ARGS__), __VA_ARGS__)
void _perm_iter_add_str(struct perm_iterator *perm, uint32_t idx, size_t argc, ...);
#define perm_iter_add_str(p, idx, ...) _perm_iter_add_str(p, idx, PP_NARG(__VA_ARGS__), __VA_ARGS__)

void perm_iter_get_values(struct perm_iterator *perm, ...);
int perm_iter_next(struct perm_iterator *perm);

#endif /*__UTILS_H__*/
