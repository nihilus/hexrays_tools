#pragma once
#include "typestring.h"

extern tid_t get_struc_from_typestring(tinfo_t t);
extern bool my_atoea(const char * str_, ea_t * pea );

template <typename T, int addr> class C
{
public:
	T * call;
	C():call((T*)addr){};
	T* operator()() { return call; }
};


template <typename T> bool safe_advance(T & iter, const T & end, int count)
{
	while(count-- > 0)
	{
		if (iter == end)
			return false;
		iter++;		
	}
	if (iter == end)
			return false;
	return true;
}


//BEWARE: these are dangerous (but qasserted)!
template <typename T> size_t get_idx_of(std::vector<T> * vec, T *item)
{	
	QASSERT(3, vec);
	QASSERT(4, item);
	size_t idx = ((size_t)(item) - (size_t)&vec->front())/sizeof(T);
	QASSERT(2, (idx<vec->size()) && (idx>=0));
	return idx;
}

template <typename T> size_t get_idx_of(qvector<T> * vec, T *item)
{
	QASSERT(3, vec);
	QASSERT(4, item);
	size_t idx = ((size_t)(item) - (size_t)&vec->front())/sizeof(T);
	QASSERT(2, (idx<vec->size()) && (idx>=0));
	return idx;
}

extern int get_idx_of_lvar(vdui_t &vu, lvar_t *lvar);

extern tinfo_t create_numbered_type_from_name(const char * name);