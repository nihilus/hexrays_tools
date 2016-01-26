/*

*/
#include <ida.hpp>
#include <kernwin.hpp>
#include <nalt.hpp>
#include <name.hpp>
#include <typeinf.hpp>
#include "classplace.h"
#include "classes.h"

// shortcut to make the text more readable
typedef class_place_t cp_t;

//--------------------------------------------------------------------------
// Short information about the current location.
// It will be displayed in the status line
void ida_export class_place_t__print(const cp_t *ths, void *,char *buf, size_t bufsize)
{
	qsnprintf(buf, bufsize, "%d::%d+%d:%d", ths->lnnum, ths->idx, ths->section, ths->subsection);
}

//--------------------------------------------------------------------------
// Convert current location to 'uval_t'
uval_t ida_export class_place_t__touval(const cp_t *ths, void *)
{
	//QASSERT(ths->section < 4);
	//QASSERT(ths->idx <= get_last_class_idx());
//	QASSERT( ths->subsection < 65536 );
	return (ths->idx << 16) + (ths->section << 14) + ths->subsection;
}

//--------------------------------------------------------------------------
// Make a copy
place_t *ida_export class_place_t__clone(const cp_t *ths)
{
	class_place_t *p = qnew(class_place_t);
	if ( p == NULL )
		nomem("class_place");
	memcpy(p, ths, sizeof(*ths));
	return p;
}

//--------------------------------------------------------------------------
// Copy from another class_place_t object
void ida_export class_place_t__copyfrom(cp_t *ths, const place_t *from)
{
	class_place_t *s = (class_place_t *)from;
	ths->idx     = s->idx;
	ths->section = s->section;
	ths->subsection = s->subsection;
	ths->lnnum = s->lnnum;
}

//--------------------------------------------------------------------------
// Create a class_place_t object at the specified address
// with the specified data
place_t *ida_export class_place_t__makeplace(const cp_t *, void *, uval_t x, short lnnum)
{
	static class_place_t p;
	p.idx     = x>>16;
	p.section = (x>>14)&3;
	p.subsection = x & 0x3FFF;
	p.lnnum = lnnum;
	return &p;
}

//--------------------------------------------------------------------------
// Compare two locations except line numbers (lnnum)
  // This function is used to organize loops.
  // For example, if the user has selected an area, its boundaries are remembered
  // as location objects. Any operation within the selection will have the following
  // look: for ( loc=starting_location; loc < ending_location; loc.next() )
  // In this loop, the comparison function is used.
  // Returns: -1: if the current location is less than 't2'
  //           0: if the current location is equal to than 't2'
  //           1: if the current location is greater than 't2'
int ida_export class_place_t__compare(const cp_t *ths, const place_t *t2)
{
	class_place_t *s = (class_place_t *)t2;
	uval_t i1 = class_place_t__touval( ths, NULL);
	uval_t i2 = class_place_t__touval( s, NULL);

	if (i1 == i2)
		return 0;
	if (i1>i2)
		return 1;
	else
		return -1;
}

//--------------------------------------------------------------------------
// Check if the location data is correct and if not, adjust it
void ida_export class_place_t__adjust(cp_t *ths, void *ud)
{
	strvec_t &sv = *(strvec_t *)ud;
	if ( ths->idx >= get_last_class_idx() )
	{
		ths->idx = get_last_class_idx();
	}
	if ( ths->section > 2 )
	{
		ths->section = 2;	
	}

	if (ths->section != 2)
	{
		ths->subsection = 0;		
	}
	else
	{
		tid_t tid = get_class_by_idx(ths->idx);
		class_t * clas = get_class(tid);
		if(!clas)
		{
			ths->section = 0;
			ths->subsection = 0;
			ths->idx = get_first_class_idx();
			return;
		}

		if(!clas->functions_ea.size())
		{
			ths->subsection = 0;
			ths->section = 1;
		}
		else
			if(ths->subsection == -1)
			{
				ths->subsection = clas->functions_ea.size()-1;
			}
			else
			{
				if(ths->subsection+1 > clas->functions_ea.size())
				{
					ths->subsection = clas->functions_ea.size()-1;
				}	
			}
	}
}

//--------------------------------------------------------------------------
// Move to the previous location
bool ida_export class_place_t__prev(cp_t *ths, void *)
{
	if ( (ths->idx == get_first_class_idx()) && (ths->subsection == 0) && (ths->section == 0))
		return false;
	
	switch(ths->section)
	{
	case 0:
		{
			uval_t idx = get_prev_class_idx(ths->idx);
			//QASSERT(idx!=BADNODE);
			class_t * clas = get_class(get_class_by_idx(idx));
			if(!clas)
				return false;
			if (clas->functions_ea.size()>0)
			{
				ths->idx = idx;
				ths->section = 2;
				ths->subsection =clas->functions_ea.size()-1;
			}
			else
			{
				ths->idx = idx;
				ths->section = 1;
				ths->subsection =0;
			}		
		}
		break;
	case 1:
		{
			ths->section=0;
			ths->subsection=0;
		}
		break;
	case 2:
		{
			if(ths->subsection>0)
				ths->subsection--;
			else
				ths->section--;
		}
		break;	
	}	

	return true;
}

//--------------------------------------------------------------------------
// Move to the next location
bool ida_export class_place_t__next(cp_t *ths, void *ud)
{
	//strvec_t &sv = *(strvec_t *)ud;
	class_t * clas = get_class(get_class_by_idx(ths->idx));
	if(!clas)
		return false;
	if ( (ths->idx >= get_last_class_idx()) && (ths->section == 2) && ( ths->subsection >= clas->functions_ea.size() - 1) )
		return false;

	switch(ths->section)
	{
	case 0:
		ths->section = 1;
		ths->subsection = 0;
		return true;

	case 1:
		ths->section = 2;
		ths->subsection = 0;
		if (clas->functions_ea.size() == 0)
			goto next_class;
		break;

	case 2:
		if ((ths->subsection + 1) < clas->functions_ea.size())
		{
			ths->subsection++;
		}
		else
		{
next_class:
			uval_t idx = get_next_class_idx(ths->idx);
			if(idx==-1)
				return false;
			ths->idx = idx;
			ths->section = 0;
			ths->subsection = 0;
		}
		break;
	}
	return true;
}

//--------------------------------------------------------------------------
// Are we at the beginning of the data?
bool ida_export class_place_t__beginning(const cp_t *ths, void *)
{
	return ths->idx == (get_first_class_idx()) && (ths->section) == 0 && (ths->subsection == 0);
}

//--------------------------------------------------------------------------
// Are we at the end of the data?
bool ida_export class_place_t__ending(const cp_t *ths, void *ud)
{
	strvec_t &sv = *(strvec_t *)ud;
	if (ths->idx == BADADDR)
		return true;
	class_t * clas = get_class(get_class_by_idx(ths->idx));
	//QASSERT(clas!=0);
	if(!clas)
		return true;
	if ((ths->idx >= get_last_class_idx()) && (ths->section==1) && (clas->functions_ea.size()==0))
		return true;

	return ((ths->idx >= get_last_class_idx()) && (ths->section==2) && ((ths->subsection+1) == clas->functions_ea.size()));	
}

//--------------------------------------------------------------------------
// Generate text for the current location
int ida_export class_place_t__generate(
									   const cp_t *ths,
									   void *ud,
									   char *lines[],
									   int maxsize,
									   int *default_lnnum,
									   color_t *prefix_color,
									   bgcolor_t *bg_color)
{
	strvec_t &sv = *(strvec_t *)ud;
	uval_t idx = ths->idx;
	if ( idx > get_last_class_idx() || maxsize <= 0 )
		return 0;
	char name[MAXNAMESIZE];

	tid_t tid = get_class_by_idx(idx);
	if (tid==BADNODE)
		return 0;
	switch (ths->section)
	{
	case 0:	
		if (get_class_name(tid, name, MAXNAMESIZE))
		{
			char line[MAXSTR];
			class_t * clas = get_class(tid);
			if(!clas)
				return 0;
			if (clas->parents_tid.size())
				qsnprintf(line, MAXSTR, "class %s: derived from: 0x%p", name, clas->parents_tid.front());
			else
				qsnprintf(line, MAXSTR, "class %s", name );
				
			lines[0] = qstrdup(line);
			*bg_color = 0xC0C0FF;
		}
		break;

	case 1:
		{
			char line[MAXSTR];
			class_t * clas = get_class(tid);
			if(!clas)
				return 0;
			if (clas->virt_table_ea != BADADDR)
				qsnprintf(line, MAXSTR, "vftable: %p", clas->virt_table_ea);
			else
				qsnprintf(line, MAXSTR, "no virtual table");				
			lines[0] = qstrdup(line);
			*bg_color = 0xC0FFC0;
		}
		break;

	case 2:
		{
			char *line;
			class_t * clas = get_class(tid);
			if(!clas)
				return 0;

			ea_t ea = BADADDR;
			if (clas->functions_ea.size() && ths->subsection<=clas->functions_ea.size())
				ea = clas->functions_ea[ths->subsection];
			if (ea!=BADADDR)
			{
				qstring tmpline;
				get_colored_long_name(&tmpline, ea);

				qtype type;
				qtype fields;  
				if (!get_tinfo(ea, &type, &fields))
				{
					if (!guess_func_tinfo(get_func(ea), &type, &fields))
						goto pokracuj; 
				}
				line = qstrdup(tmpline.c_str()); 
				print_type_to_one_line(line, MAXSTR, idati, type.c_str(), line, 0, fields.c_str(), 0);
pokracuj:
				;				

			}
			else qsnprintf(line, MAXSTR, "bad func");
			lines[0] = qstrdup(line);
			*bg_color = 0xC0FFFF;
		}
		break;

	}
	

	//*prefix_color = sv[idx%sv.size()].color;
	//*bg_color = sv[idx%sv.size()].bgcolor;
	*default_lnnum = 0;
	return 1; // generated one line

	//setup_makeline(-1, ..., save_line_in_array, 0)
	//finish_makeline();
}