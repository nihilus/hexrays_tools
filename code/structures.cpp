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

bool extract_substruct(uval_t idx, uval_t begin, uval_t end)
{
	tid_t id = get_struc_by_idx( idx );
	if (is_union(id))
	{
		msg("union!\n");
		return false;
	}

	struc_t * struc = get_struc(id);
	if(!struc)
	{
		msg("!struc\n");
		return false;
	}

	int i = 1;
	qstring tmpname;
	get_struc_name(&tmpname, id);
	char *struc_name = qstrdup(tmpname.c_str());
	char * number = strstr(struc_name, "_obj_");
	char * number2;
	//find last _obj_
	while (number && (number2 = strstr(number+1, "_obj_")))
	{
		number = number2;	
	}

	if (number)
		*number = 0;
	

	char new_struc_name[MAXSTR];
	char *nsn = NULL;
	do
	{
		qsnprintf(new_struc_name, MAXSTR, "%s_obj_%d", struc_name, i);
		i++;
	} while ((get_name_ea(BADADDR, new_struc_name) != BADADDR) && i<100);

	if (i<100)
		nsn = new_struc_name;

	tid_t newid = add_struc(idx+1, nsn);
	struc_t * newstruc = get_struc(newid);

	asize_t delta = begin;

	asize_t off = begin;

	while( off <= end)
	{		
		member_t * member = get_member(struc, off);
		if(member)
		{
			char name[MAXSTR];
			get_member_name(member->id, name, sizeof(name));
			opinfo_t mt;
			retrieve_member_info(member, &mt);

			asize_t size = get_member_size(member);
			//TODO: if name is already used, this doesn't work!!!!
			add_struc_member(newstruc, name, off - delta, member->flag, &mt, size);	

			//get_member_type(member, );
			//set_member_type(newstruc, off-delta, member->flag, &mt, size);

			qtype type;
			qtype fields;
			if(get_or_guess_member_tinfo(member, &type, &fields))
			{
				member_t * newmember = get_member(newstruc, off-delta);
				//set_member_tinfo(idati, sptr, mptr, 0, type.c_str(), fields.c_str(), 0);
				set_member_tinfo(idati, newstruc, newmember, 0, type.c_str(), fields.c_str(), SET_MEMTI_COMPATIBLE /*| SET_MEMTI_MAY_DESTROY*/);
			}
		}
		off = get_struc_next_offset(struc, off);
	}

	
	//newstruc->age
	//parse_decl(idati, "");
	qtype type;
	qtype fields;
	qstring qname;
	//guess_tinfo(newid, &type, &fields );

	//
	//create_typedef();

	qstring tmpname2;
	get_struc_name(&tmpname2, newid);
	char *name = qstrdup(tmpname2.c_str());
	qstrncat(name, " dummy;", MAXSTR);
	parse_decl(idati, name, &qname, &type, &fields, PT_VAR);

	member_t * member = get_member(struc, begin);
	set_member_tinfo(idati, struc, member, 0, type.c_str(), fields.c_str(), SET_MEMTI_MAY_DESTROY);

	//set_struc_cmt(newid, "generated struct", false);	
	//get_next_member_idx();
	return true;
}


/*
Recursively travels struc_t to find which member exactly is at specified offset
returns whether succeeded.
*/
bool idaapi struct_get_member(struc_t * str, asize_t offset, member_t ** out_member )
{
	if (get_struc_size(str) < offset )
		return false;
	member_t * member = get_member(str, offset);	
	if ( member )
	{
		if ( member->get_soff() == offset )
		{
			if(out_member)
				*out_member = member;
			return true;
		}
		
		struc_t * membstr = get_sptr(member);
		if ( membstr )
		{
			return struct_get_member(membstr, offset-member->get_soff());
		}	
	}
	return false;
}


bool idaapi struct_has_member(struc_t * str, asize_t offset)
{	
	return struct_get_member(str, offset, NULL);
}

void print_struct_member_name(struc_t * str, asize_t offset, char * name,  size_t len)
{	
	member_t * member = get_member(str, offset);
	if ( member )
	{
		if ( member->get_soff() == offset )
		{
			qstring tmpname;
			get_member_fullname(&tmpname, member->id);
			qstrncpy(name, tmpname.c_str(), len);
			return;
		}		
		struc_t * membstr = get_sptr(member);
		if ( membstr )
		{
			qstring tmpname;
			ssize_t s = get_struc_name(&tmpname, str->id);
			qstrncpy(name, tmpname.c_str(), len);
			name[s]='.';
			print_struct_member_name(membstr, offset - member->get_soff(), name+s+1, len-s-1);
			return;
		}	
	}	
}

bool which_struct_matches_here(uval_t idx1, uval_t begin, uval_t end)
{
	tid_t id = get_struc_by_idx( idx1 );
	if (is_union(id))
	{
		msg("union!\n");
		return false;
	}

	struc_t * struc = get_struc(id);
	if(!struc)
	{
		msg("!struc\n");
		return false;
	}

	asize_t size = end + get_member_size(get_member(struc, end)) - begin;
	

	matched_structs m;	
	m.idcka.clear();

	for( uval_t idx = get_first_struc_idx(); idx!=BADNODE; idx=get_next_struc_idx(idx) )
	{
		tid_t id = get_struc_by_idx(idx);
		struc_t * struc_candidate = get_struc(id);
		if(!struc_candidate)
			continue;

		if (is_union(id))
			continue;

		if (get_struc_size(struc_candidate) != size)
			continue;

		if ( compare_structs(struc, begin, struc_candidate ) )
			m.idcka.push_back(id);
	}
	//TODO: use another functions

	msg("found %d candidate structs\n", m.idcka.size());
	int choosed = m.choose("possible matches");	
	qstring name;
	if ( choosed > 0 )
	{
		get_struc_name(&name, m.idcka[choosed-1]);
		open_structs_window(m.idcka[choosed-1], 0);
	}
	return true; // done
}

//compares two structs, first very simple version, recursive version will be later
//for every member of str2 we look for member with same width at str1 + begin
//
bool compare_structs(struc_t * str1, asize_t begin, struc_t * str2 )
{
	asize_t off = 0;
	while( off != BADADDR )
	{
		member_t * member2 = get_member(str2, off);
		member_t * member1 = get_member(str1, off + begin);
		if (!member2)
			break;

		if( !member1  )
		{
			return false;		
		}

		if (member1->get_soff() != off + begin)
			return false;
		
		if (get_member_size( member1 ) != get_member_size( member2 ))
			return false;
				
		off = get_struc_next_offset(str2, off);
	}
	return true;
}



bool unpack_this_member(uval_t idx, uval_t offset)
{
	tid_t id = get_struc_by_idx( idx );
	if (is_union(id))
	{
		msg("union!\n");
		return false;
	}

	struc_t * struc = get_struc(id);
	if(!struc)
	{
		msg("!struc\n");
		return false;
	}

	member_t * member = get_member( struc, offset);

	if (member->get_soff() != offset)
		return false;


	struc_t * membstr = get_sptr(member);
	if(!membstr)
		return false;

	if (is_union(membstr->id))
		return false;

	asize_t delta = offset;
	asize_t off = 0;
	asize_t end = get_struc_size(membstr);
	del_struc_member(struc, offset);
	while( off <= end )
	{		
		member_t * member = get_member(membstr, off);
		if(member)
		{
			char name[MAXSTR];
			get_member_name(member->id, name, sizeof(name));
			opinfo_t mt;
			retrieve_member_info(member, &mt);

			asize_t size = get_member_size(member);			
			add_struc_member(struc, name, off + delta, member->flag, &mt, size);
			
			qtype type;
			qtype fields;
			if( get_or_guess_member_tinfo(member, &type, &fields) )
			{
				member_t * newmember = get_member(struc, off + delta);
				//set_member_tinfo(idati, sptr, mptr, 0, type.c_str(), fields.c_str(), 0);
				set_member_tinfo(idati, struc, newmember, 0, type.c_str(), fields.c_str(), /*SET_MEMTI_COMPATIBLE | */SET_MEMTI_MAY_DESTROY);
			}
		}
		off = get_struc_next_offset(membstr, off);
	}
	return true;
}