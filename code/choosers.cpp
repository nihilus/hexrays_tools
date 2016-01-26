#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <hexrays.hpp>
#include <struct.hpp>
#include <bytes.hpp>
#include <kernwin.hpp>
#include <algorithm>
#include <pro.h>

#include "structures.h"
#include "choosers.h"

uint32 idaapi matched_structs_with_offsets::sizer(void *obj)
{
	matched_structs_with_offsets & ms = *(matched_structs_with_offsets*)obj;
	return ms.idcka.size();
}

void idaapi matched_structs_with_offsets::get_type_line(void *obj,uint32 n,char * const *arrptr)
{
	matched_structs_with_offsets & ms = *(matched_structs_with_offsets*)obj;
	if (n==0)
	{
		qstrncpy(arrptr[0], "member", MAXSTR);
		qstrncpy(arrptr[1], "type", MAXSTR);		
	}
	else
	{
		char name[MAXSTR];
		name[0]=0;

		char type_str[MAXSTR];
		type_str[0]=0;

		tid_t id = ms.idcka[n-1];
		struc_t * struc = get_struc(id);
		print_struct_member_name(struc, ms.offset, name, sizeof(name));
		member_t * member = 0;
		if(struct_get_member(get_struc(id), ms.offset, &member) && member)
		{
			//typestring type;
			tinfo_t type;
			if(get_member_type(member, &type))
			{
				type.print(new qstring(type_str));
			}
		}
		qstpncpy(arrptr[0],  name, MAXSTR);
		qstpncpy(arrptr[1],  type_str, MAXSTR);		
	}	
}


uint32 idaapi matched_structs::sizer(void *obj)
{
	matched_structs & ms = *(matched_structs*)obj;
	return ms.idcka.size();
}


char * idaapi matched_structs::get_type_line(void *obj, uint32 n, char *buf)
{
	matched_structs & ms = *(matched_structs*)obj;
	if (n==0)
		qstpncpy( buf, "type", MAXSTR );
	else
	{
		qstring name;
;
		tid_t id =  ms.idcka[n-1];
		struc_t * struc = get_struc(id);
		if (struc)
			get_struc_name(&name, id);
		//asize_t size = get_struc_size(id);
		qsnprintf(buf, MAXSTR, "%s", name.c_str());
	}
	return buf;
}

uint32 idaapi function_list::sizer(void *obj)
{
	function_list & ms = *(function_list*)obj;
	return ms.functions.size();
}



static const char *header[] =
{
  "Address",
  "Function name",
  "Comment",
};

void idaapi function_list::get_line(void *obj,uint32 n,char * const *arrptr)
{
  if ( n == 0 ) // generate the column headers
  {
    for ( int i=0; i < qnumber(header); i++ )
      qstrncpy(arrptr[i], header[i], MAXSTR);
    return;
  }
  function_list & ms = *(function_list*)obj;

  ea_t ea = ms.functions[n-1];
  qsnprintf(arrptr[0], MAXSTR, "%08a", ea);
  get_func_name(ea, arrptr[1], MAXSTR);
  //get_short_name(BADADDR, ea, arrptr[1], MAXSTR);
  //get_demangled_name(BADADDR, ea,  arrptr[1], MAXSTR, inf.long_demnames, DEMNAM_NAME, 0);
  func_t * f = get_func(ea);
  if(f)
  {
	  const char * cmt = get_func_cmt(f, true);
	  if(cmt)
	  {
		  qstrncpy(arrptr[2], cmt, MAXSTR);		
	  }
	
  }

}