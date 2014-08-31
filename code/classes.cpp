#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <hexrays.hpp>
#include <struct.hpp>
#include <bytes.hpp>
#include <kernwin.hpp>
#include <algorithm>
#include <pro.h>
#include <srarea.hpp>
#include <auto.hpp>
#include <funcs.hpp>

#pragma pack(push, 1)           // IDA uses 1 byte alignments!

#include <bytes.hpp>
#include <netnode.hpp>
#include "classes.h"

/*
for simplicity saved as blob with my tag inside corresponding structure netnode
*/

netnode classes_indexes;

void classes_init()
{
	netnode_check(&classes_indexes, "$ classes", 0, true);
}

tid_t add_class(tid_t tid_of_struc)
{
	//classes are built on top of structures, make sure there is one before continuing
	if(!get_struc(tid_of_struc))
		return BADNODE;
	if(get_class(tid_of_struc))
	{
		return tid_of_struc;	
	}

	class_t clas;
	clas.flags = 0;
	clas.tid = tid_of_struc;
	clas.functions_ea.clear();
	clas.parents_tid.clear();
	clas.virt_table_ea = BADADDR;
	save_class(&clas);
	//DONE: add to classes index
	add_class_at_idx(tid_of_struc, BADNODE);
	return tid_of_struc;
}

//remark: does not check whether we have tid already in index
bool add_class_at_idx(tid_t clas, size_t idx)
{
	size_t qty = get_class_qty();
	if ( idx > qty )
		idx = qty;
	nodeidx_t sa;
	for ( size_t j = qty; j > idx; netnode_supset(classes_indexes, j--, &sa, 4, 'A') )
		sa = netnode_altval(classes_indexes, j - 1, 'A');
	//save our new class
	nodeidx_t v14 = clas + 1;
	netnode_supset(classes_indexes, idx, &v14, 4, 'A');
	//save new class_qty
	nodeidx_t v13 = qty + 1;
	netnode_supset(classes_indexes, -1, &v13, 4, 'A');
	return true;
}

void remove_class_at_idx(size_t idx)
{
	size_t pocet_struktur = get_struc_qty() - 1;
	for ( size_t i = idx; i < pocet_struktur; ++i )
	{
		nodeidx_t s = netnode_altval(classes_indexes, i + 1, 'A');
		netnode_supset(classes_indexes, i, &s, 4, 'A');
	}
	netnode_supdel(classes_indexes, pocet_struktur, 'A');      
	netnode_supset(classes_indexes, -1, &pocet_struktur, 4, 'A');
}

bool save_class(class_t * clas)
{	
	bytevec_t buffer;
	append_ea(buffer, clas->virt_table_ea);
	append_dd(buffer, clas->flags);
	append_eavec(buffer, 0, clas->functions_ea);
	append_eavec(buffer, 0, clas->parents_tid);
	netnode n = netnode(clas->tid);
	n.setblob(&buffer.front(), buffer.size(), 0, 'm');
	return true;
}

class_t classes_buffer[1280];

uint classes_count=0;

class_t * get_class(tid_t tid)
{
	for(unsigned int i=0; i<classes_count; i++)
	{
		if (classes_buffer[i].tid == tid)
		return &classes_buffer[i];	
	}
	netnode n(tid);
	if (n==BADNODE)
		return NULL;
	size_t s = n.blobsize(0, 'm');
	if (s==0)
		return NULL;

	if (classes_count == qnumber(classes_buffer))
		return NULL;

	const uchar * buffer;

	buffer = (uchar*)n.getblob(NULL, &s, 0, 'm');
	const uchar * buf = buffer;
	const uchar * end = buffer+s;

	class_t * clas = &classes_buffer[classes_count++];
	clas->tid = tid;
	clas->position = classes_count-1;

	//TODO: aging
	clas->age = 0;	
	clas->virt_table_ea = unpack_ea(&buf, end);		
	clas->flags = unpack_dd(&buf, end);		
	unpack_eavec(0, clas->functions_ea, &buf, end);	
	unpack_eavec(0, clas->parents_tid, &buf, end);
	qfree((void *)buffer);
	return clas;
}


/*
idaman tid_t ida_export add_class(const char *name)
{

}
*/

tid_t get_class_by_idx(int index)
{
  int result; // eax@2

  if ( index == -1 )
    result = -1;
  else
	  result = classes_indexes.altval(index) -1;
  return result;
}


bool classes_term()
{
	classes_count = 0;
	classes_indexes=BADNODE;
	return true;
}

size_t get_class_qty(void)
{
	return classes_indexes.altval(-1);
}

uval_t get_first_class_idx(void)
{
	if (classes_indexes.altval(0))	  
		return 0;
	else
		return -1;
}

uval_t get_last_class_idx(void)
{
	return get_class_qty() - 1;  
}

inline uval_t get_prev_class_idx(uval_t idx) { return (idx==BADNODE) ? idx : idx - 1; }

uval_t get_next_class_idx(uval_t idx)
{
	uval_t result;
	if ( idx + 1 < (unsigned int)get_class_qty() )
		result = idx + 1;
	else
		result = -1;
	return result;
}

uval_t get_class_idx(tid_t id)        // get internal number of the class
{
	int i;
	for ( i = get_last_class_idx(); i >= 0; --i )
	{
	    if ( id == get_class_by_idx(i) )
			break;
	}
	return i;
}

tid_t  get_class_by_idx(uval_t idx)   // get class id by class number
{
	tid_t result;
	if ( idx == -1 )
		result = -1;
	else
		result =  classes_indexes.altval(idx)-1;
	return result;
}

class_t *get_class(tid_t id);

inline tid_t get_class_id(const char *name)              // get struct id by name
{
  tid_t id = netnode(name);
  return get_class(id) == NULL ? BADADDR : id;
}

//same as get_struc_name for now
inline ssize_t get_class_name(tid_t id, char *buf, size_t bufsize) { return netnode(id).name(buf, bufsize); }

//same as get_struc_cmt
inline ssize_t get_class_cmt(tid_t id, bool repeatable, char *buf, size_t bufsize) { return netnode(id).supstr(repeatable != 0, buf, bufsize); }
//idaman asize_t ida_export get_class_size(const struc_t *sptr);
//inline asize_t get_class_size(tid_t id) { return get_class_size(get_class(id)); }

#pragma pack(pop)