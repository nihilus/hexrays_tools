#include <hexrays.hpp>
#include <struct.hpp>
#include <bytes.hpp>
#include <kernwin.hpp>
#include <algorithm>
#include <pro.h>

#include "choosers.h"
#include "structures.h"
#include "negative_cast.h"

#include "helpers.h"

tid_t get_struc_from_typestring_my(tinfo_t t)
{
	tinfo_t tmp = t;
	//until I get better solution, compile this with /EHsa
	try
	{
		while(t.is_ptr_or_array())
		{				
			t.remove_ptr_or_array();
		}
	}
	catch( char * ex)
	{
		t = tmp;
		while(t.get_size() > 0);
			//t = t.substr(1);
	}

	if ( t.is_struct() )
	{	
		const type_t * ptr = (type_t*)t.get_realtype();
		/*
		if ( get_dt(ptr) != 0 )
		return BADNODE; // this is an inplace definition, fail
		*/
		char name[MAXNAMELEN];
		memset(name, 0, sizeof(name));
		if ( !extract_name(ptr, name) )
			return BADNODE; // bad type string
		return import_type(idati, -1, name);
	}
	{
		qstring temp;
		t.print(&temp);
		//msg("get_struc_from_typestring: %s\n", tmp);
		//QASSERT(112, false);
	}
	return BADNODE;
}

#if IDA_SDK_VERSION >= 630
tid_t get_struc_from_typestring(typestring t)
{
	typestring tmp = t;
	char buf[MAXSTR];
	if(!get_name_of_named_type(buf, sizeof(buf), t.c_str()))
		return get_struc_from_typestring_my(t);
	return import_type(idati, -1, buf);
}
#else
tid_t get_struc_from_typestring(typestring t)
{
	return get_struc_from_typestring_my(t);	
}


#endif


bool my_atoea(const char * str_, ea_t * pea )
{
	char str[MAXSTR];
	qstrncpy(str, str_, 12);
	//qstrncpy(str, str_, 12);
	char * ptr = str;
	while(*ptr && (!isspace(*ptr)))
	{
		ptr++;
	}
	*ptr=0;			
	return atoea(str, pea);
}

int get_idx_of_lvar(vdui_t &vu, lvar_t *lvar)
{
	return get_idx_of(vu.cfunc->get_lvars(), lvar);
}

tinfo_t create_numbered_type_from_name(const char * name)
{
	tinfo_t out_type;
	int32 ord = get_type_ordinal(idati, name);
	char ordname[32];
	memset(ordname, 0, sizeof(ordname));

	assert( create_numbered_type_name(ord, ordname, sizeof(ordname)) != 0);
	out_type = create_typedef(ordname);
	return out_type;
}