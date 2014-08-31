/*

*/
#include <hexrays.hpp>
#include <ida.hpp>
#include <kernwin.hpp>
#include <nalt.hpp>
#include <name.hpp>
#include <typeinf.hpp>

#include "new_struc_place.h"
#include "helpers.h"

// shortcut to make the text more readable
typedef new_struc_place_t cp_t;

//--------------------------------------------------------------------------
// Short information about the current location.
// It will be displayed in the status line
void ida_export new_struc_place_t__print(const cp_t *ths, void *,char *buf, size_t bufsize)
{
	qsnprintf(buf, bufsize, "%d %d", ths->idx, ths->subtype);
}

//--------------------------------------------------------------------------
// Convert current location to 'uval_t'
uval_t ida_export new_struc_place_t__touval(const cp_t *ths, void *)
{
	//QASSERT(ths->section < 4);
	//QASSERT(ths->idx <= get_last_class_idx());
//	QASSERT( ths->subsection < 65536 );
	return (ths->idx << 5) + ths->subtype;
}

//--------------------------------------------------------------------------
// Make a copy
place_t *ida_export new_struc_place_t__clone(const cp_t *ths)
{
	new_struc_place_t *p = qnew(new_struc_place_t);
	if ( p == NULL )
		nomem("new_struc_place");
	memcpy(p, ths, sizeof(*ths));
	return p;
}

//--------------------------------------------------------------------------
// Copy from another new_struc_place_t object
void ida_export new_struc_place_t__copyfrom(cp_t *ths, const place_t *from)
{
	new_struc_place_t *s = (new_struc_place_t *)from;
	ths->idx     = s->idx;
	ths->lnnum = s->lnnum;
	ths->subtype = s->subtype;
}

//--------------------------------------------------------------------------
// Create a new_struc_place_t object at the specified address
// with the specified data
place_t *ida_export new_struc_place_t__makeplace(const cp_t *, void *, uval_t x, short lnnum)
{
	static new_struc_place_t p;
	p.idx     = (x>>5);
	p.subtype = x & 0x1F;
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
int ida_export new_struc_place_t__compare(const cp_t *ths, const place_t *t2)
{
	new_struc_place_t *s = (new_struc_place_t *)t2;
	uval_t i1 = new_struc_place_t__touval( ths, NULL);
	uval_t i2 = new_struc_place_t__touval( s, NULL);

	if (i1 == i2)
		return 0;
	if (i1>i2)
		return 1;
	else
		return -1;
}

//--------------------------------------------------------------------------
// Check if the location data is correct and if not, adjust it
void ida_export new_struc_place_t__adjust(cp_t *ths, void *ud)
{
	field_info_t &sv = *(field_info_t *)ud;
	if (ud == NULL)
		ths->idx = BADADDR;
	
	if (ths->idx >= sv.size())
		ths->idx = sv.size()-1;
	if( ths->subtype >= sv.types_at_idx_qty(ths->idx) )
	{
		ths->subtype = sv.types_at_idx_qty(ths->idx)-1;
	}
	
}

//--------------------------------------------------------------------------
// Move to the previous location
bool ida_export new_struc_place_t__prev(cp_t *ths, void * ud)
{
	field_info_t &sv = *(field_info_t *)ud;
	if (ths->idx == BADADDR)
		return false;

	if ( ths->idx == 0 && ths->subtype == 0 )
		return false;
	if(ths->subtype>0)
	{
		--ths->subtype;
		return true;
	}
	--ths->idx;
	ths->subtype = sv.types_at_idx_qty(ths->idx)-1;
	return true;
}

//--------------------------------------------------------------------------
// Move to the next location
bool ida_export new_struc_place_t__next(cp_t *ths, void *ud)
{
	field_info_t &sv = *(field_info_t *)ud;

	if (ths->idx + 1 >  sv.size())
		return false;

	
	ths->subtype++;
	if( ths->subtype == sv.types_at_idx_qty(ths->idx) )
	{
		++ths->idx;
		ths->subtype = 0;
		if (ths->idx ==  sv.size())
			return false;
	}
	else
	{
		//++ths->subtype;	
	}
	return true;
}

//--------------------------------------------------------------------------
// Are we at the beginning of the data?
bool ida_export new_struc_place_t__beginning(const cp_t *ths, void *)
{
	if (ths->idx == BADADDR)
		return true;
	return ths->idx == 0 && ths->subtype == 0;
}

//--------------------------------------------------------------------------
// Are we at the end of the data?
bool ida_export new_struc_place_t__ending(const cp_t *ths, void *ud)
{
	field_info_t &sv = *(field_info_t *)ud;
	if (ths->idx == BADADDR)
		return true;
	if ( ths->idx+1 >= sv.size() )
	{
		return true;	
	}
	return ((ths->idx+1  == sv.size()) && (ths->subtype + 1 == sv.types_at_idx_qty(ths->idx)));
}

//--------------------------------------------------------------------------
// Generate text for the current location
int ida_export new_struc_place_t__generate(
									   const cp_t *ths,
									   void *ud,
									   char *lines[],
									   int maxsize,
									   int *default_lnnum,
									   color_t *prefix_color,
									   bgcolor_t *bg_color)
{
	field_info_t &sv = *(field_info_t *)ud;
	uval_t idx = ths->idx;

	if (sv.size()==0)
	{
		return 0;	
	}

	if ( idx >= sv.size() || maxsize <= 0 )
		return 0;

	char line[MAXSTR];
	
	field_info_t::iterator iter =  sv.begin();

	if (iter == sv.end())
		return 0;
	if (!safe_advance(iter, sv.end(), idx))
		return 0;

	if ( iter == sv.end() )
		return 0;
	const scan_info_v2_t & si = iter->second;
	int offset = iter->first;
	qstring result;
	int c = 0;

	char name[MAXNAMELEN];
	qsnprintf(name, sizeof(name), "field_%02d", offset );
	char prefix[MAXSTR];
	unsigned int len = si.nesting_counter*2;
	if (len >= MAXSTR)
		len = MAXSTR - 1;
	memset(prefix, ' ', len);
	prefix[len] = 0;

	typevec_t::const_iterator i = si.types.begin();

	if(i == si.types.end())
		return 0;

	if (!safe_advance(i, si.types.end(), ths->subtype))
	{
		return 0;
	}

	if (i != si.types.end())
	{
		print_type_to_qstring( &result, 0, 5, 40, PRTYPE_1LINE, idati, (type_t *)i->type.c_str(), name );

		if ( sv.current_offset == offset  && c==0)
		{
			qsnprintf(line, MAXSTR, "%s" COLSTR( "%08d", SCOLOR_ERROR) ": %s", prefix, offset, result.c_str());
		}
		else
		{
			qsnprintf(line, MAXSTR, "%s" "%08d" ": %s", prefix, offset, result.c_str());				
		}
		if (si.is_array)
		{
			qstrncat(line, COLSTR( "[]", SCOLOR_ERROR), sizeof(line));
		}

		lines[c++] = qstrdup(line);	

		if (i->enabled)
		{
			if (si.types.in_conflict())
			{
				*bg_color = 0xA0FFFF;
			}
			else
			{
				*bg_color = 0xFFFFFF;
			}
		}
		else
		{
			*bg_color = 0xDCDCDC;
		}		
	}
	/*
	for(typevec_t::const_iterator i = si.types.begin(); i != si.types.end(); i++ )
	{
		print_type_to_qstring( &result, 0, 5, 40, PRTYPE_1LINE, idati, (type_t *)i->c_str(), name );

		if ( sv.current_offset == offset  && c==0)
		{
			qsnprintf(line, MAXSTR, "%s" COLSTR( "%08d", SCOLOR_ERROR) ": %s", prefix, offset, result.c_str());
		}
		else
		{
			qsnprintf(line, MAXSTR, "%s" "%08d" ": %s", prefix, offset, result.c_str());		
		}

		lines[c++] = qstrdup(line);
	}*/	
	//lines[0] = qstrdup(line);

	if( si.types.size() == ths->subtype+1 )
	{
		for(max_adjustments_t::iterator j = sv.max_adjustments.begin(); j!= sv.max_adjustments.end(); j++)
		{
			if (j->second == offset)
			{
				qstring s;
				s.sprnt( "%s%08d: -------------- last accessed offset from scan %08d  ----------", prefix, offset, j->first );
				lines[c++] = qstrdup( s.c_str() );
			}
		}
	}
	/*if ( sv.current_offset == offset )
	{
		*bg_color = ((*bg_color)& (0xC0FFFF)) | 0xCCFFFF;
	}*/

	//*prefix_color = sv[idx%sv.size()].color;
	//*bg_color = sv[idx%sv.size()].bgcolor;
	*default_lnnum = 0;
	return c; // generated one line
	//setup_makeline(-1, ..., save_line_in_array, 0)
	//finish_makeline();
}