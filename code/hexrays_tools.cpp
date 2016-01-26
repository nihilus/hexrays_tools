#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <Windows.h>
#include <hexrays.hpp>
#include <struct.hpp>
#include <bytes.hpp>
#include <kernwin.hpp>
#include <algorithm>
#include <pro.h>
#include <srarea.hpp>
#include <auto.hpp>
#include <funcs.hpp>
#include <expr.hpp>//for VT_LONG
#include <frame.hpp>//for recalc_spd

#include "choosers.h"
#include "structures.h"
#include "negative_cast.h"
#include "helpers.h"
#include "struct_graph.h"
#include "classes_view.h"
#include "classes.h"
#include "new_struct.h"
#include "new_struc_view.h"
#include "ripped.h"
#include "zeroizer.h"


// Hex-Rays API pointer
hexdsp_t *hexdsp = NULL;

extern plugin_t PLUGIN;

static bool inited = false;

bool name_for_struct_crawler(vdui_t &vu, qstring & name, lvar_t ** out_var, ea_t * ea);
bool set_lvar_type(vdui_t * vu, lvar_t *  lv, tinfo_t * ts);
bool structs_with_this_size(asize_t size);

/*
idea:
 cexpr_t * call;
*/

struct hx_tree_match_node_t
{
	hx_tree_match_node_t * x;
	hx_tree_match_node_t * y;
	hx_tree_match_node_t * z;
	cexpr_t ** out;
	virtual bool accept(cexpr_t * expr) 
	{
		if (expr)
		{
			if(x && expr->x)
			{
				if(!x->accept(expr->x))
					return false;
			}

			if(y && expr->y)
			{
				if(!y->accept(expr->y))
					return false;
			}
			return true;
		}
		return false;		
	};
	hx_tree_match_node_t():x(0),y(0),z(0),out(0){};
	hx_tree_match_node_t(cexpr_t ** out_){out = out_;}
	hx_tree_match_node_t(hx_tree_match_node_t * x_, hx_tree_match_node_t * y_):x(x_),y(y_),z(0),out(0){};
};

template <int goal> struct hx_tree_match_op_t: public hx_tree_match_node_t 
{	
	//hx_tree_match_node_t * z;
	//int goal; // cot_xxx
	hx_tree_match_op_t(cexpr_t ** out_){out = out_;}
	virtual bool accept(cexpr_t * expr)
	{		
		if (expr && expr->op == goal)
		{
			if(x && expr->x)
			{
				if(!x->accept(expr->x))
					return false;
			}

			if(y && expr->y)
			{
				if(!y->accept(expr->y))
					return false;
			}

			if(out)
				*out = expr;
			return true;
		}
		return false;
	}
};


//--------------------------------------------------------------------------
// Check if the item under the cursor is 'if' or 'else' keyword
// If yes, return pointer to the corresponding ctree item
static cinsn_t *find_if_statement(vdui_t &vu)
{
	// 'if' keyword: straightforward check
	if ( vu.item.is_citem() )
	{
		cinsn_t *i = vu.item.i;
		if ( i->op == cit_block  )
			return i;
	}
	return NULL;
}

static cexpr_t *find_memptr_var(vdui_t &vu, bool * is_call = 0, ea_t * ea = 0 )
{	
	if ( vu.item.is_citem() )
	{
		cexpr_t *f;
		cexpr_t *e;
		e = f = vu.item.e;		
		if (is_call)
		{
			const citem_t *p = vu.cfunc->body.find_parent_of(e);

			// call => cast => memptr
			if (p && p->op == cot_cast)
			{
				p = vu.cfunc->body.find_parent_of(p);
			}

			if (p)
			{									
				*is_call = p->op == cot_call;
				//msg("tools, is call: %d\n", *is_call);
				if (*is_call && ea)
				{
					*ea = p->ea;
					//msg("tools, is call ea: %p\n", *ea);				
				}				
			}
		}

		if ( e->op == cot_memptr  || e->op == cot_memref )
		{
			if ( e->x->op == cot_idx )
				e = e->x;

			typestring t = e->x->type;
			while (t.is_ptr_or_array())
				t.remove_ptr_or_array();

			if (t.is_struct())
			{
				return f;
			}

			if ( (e->x->op == cot_obj) || (e->x->op == cot_var) || (e->x->op == cot_call) || (e->x->op == cot_memptr) || (e->x->op == cot_memref) )
			{
				return f;
			}
		}
	}
	return NULL;
}

static cexpr_t *find_cast(vdui_t &vu)
{	
	if ( vu.item.is_citem() )
	{
		cexpr_t *e = vu.item.e;
		if ( e->op == cot_cast  )
		{
			return e;			
		}
	}
	return NULL;
}

static cexpr_t *find_var(vdui_t &vu)
{	
	if ( vu.item.is_citem() )
	{
		cexpr_t *e = vu.item.e;
		if ( e->op == cot_var  )
		{
			return e;	
		}
	}
	return NULL;
}


bool jumptype(tinfo_t t, int offset, bool virtual_calls = false)
{
	while(t.is_ptr_or_array())
		t.remove_ptr_or_array();

	if(t.is_struct())
	{
		tid_t id = get_struc_from_typestring( t );

		if (id == BADADDR)
		{
			msg("!get_struc_id\n");
			return false;
		}

		if (virtual_calls)
		{
			char comment[MAXSTR];
			get_struc_cmt(id, true, comment, sizeof(comment));
			char * strpos = strstr(comment, "@0x");
			if (strpos)
			{
				ea_t ea;				

				if ( my_atoea( strpos + 1, &ea) )
				{
					ea_t fnc = get_long(ea + offset);
					flags_t flags = getFlags(fnc);

					if (isFunc(flags))
					{
						open_pseudocode(fnc, -1);
					}
					else
					{
						if (isCode(flags))
						{
							jumpto(fnc);
						}
						else
						{
							jumpto(ea + offset);
						}
					}
					return true;
				}
			}
		}

		open_structs_window(id, offset);
		return true;
	}
	else
		return false;
}

static bool has_VT(void * ud)
{
	vdui_t &vu = *(vdui_t *)ud;
	bool is_call = false;	
	cexpr_t *e = find_memptr_var(vu, &is_call);
	if (!e)
		return false;
	int offset = e->m;
	cexpr_t * var = e->x;		
	if(var->type.empty())
		return false;
	tid_t id = get_struc_from_typestring(var->type);
	if (id == BADNODE)
		return false;
	char comment[MAXSTR];
	get_struc_cmt(id, true, comment, sizeof(comment));

	char * strpos = strstr(comment, "@0x");
	if (!strpos)
		return false;	

	return true;
}

static bool is_virtual_call(void * ud)
{
	vdui_t &vu = *(vdui_t *)ud;
	bool is_call = false;	
	cexpr_t *e = find_memptr_var(vu, &is_call);
	if (!e)
		return false;
	return is_call;
}

static bool idaapi jump_to_VT(void * ud)
{
	vdui_t &vu = *(vdui_t *)ud;
	bool is_call = false;	
	cexpr_t *e = find_memptr_var(vu, &is_call);
	if (e)
	{
		int offset = e->m;
		if (e->op == cot_idx)
			e = e->x;
		cexpr_t * var = e->x;
		if(var->type.empty())
		{			
			return false;
		}
		return jumptype(var->type, offset, true);
	}
	e = find_cast(vu);
	if (e)
	{				
		if(e->type.empty())
		{			
			return false;
		}
		return jumptype(e->type, 0);
	}
	return false;
}


//resets all calls to ea
static void reset_alt_calls(ea_t ea)
{
	static const char * nname;
	if ( ph.id == PLFM_MIPS )
		nname = "$ mips";
	else if ( ph.id == PLFM_ARM )
		nname = " $arm";
	else
		nname = "$ vmm functions";
	netnode n(nname);
	for(nodeidx_t idx= n.alt1st(); idx != BADNODE; idx = n.altnxt(idx))
	{
		if(n.altval(idx)-1 == ea)
		{
			n.altdel(idx);
			noUsed(idx);                 // reanalyze the current instruction
			recalc_spd(idx);
		}	
		//n.altdel_all(atag);
	}
}

static const char reset_alt_idc_args[] = { VT_LONG, 0 };
static error_t idaapi reset_alt_idc(idc_value_t *argv, idc_value_t *res)
{
	msg("[Hexrays-Tools] reset_alt_idc is called with arg0=%x\n", argv[0].num);
	reset_alt_calls(argv[0].num);
	return eOk;
}


static const char structs_of_size_args[] = { VT_LONG, 0 };
static error_t idaapi structs_of_size(idc_value_t *argv, idc_value_t *res)
{
	msg("[Hexrays-Tools] structs_of_size is called with arg0=%x\n", argv[0].num);
	asize_t sz = argv[0].num;
	structs_with_this_size(sz);
	return eOk;
}

void register_idc_functions()
{
	set_idc_func_ex("reset_alt_calls", reset_alt_idc, reset_alt_idc_args, 0);
	set_idc_func_ex("structs_of_size", structs_of_size, structs_of_size_args, 0);
	//set_idc_func_ex("get_member_offset", idc_get_member_by_fullname, idc_get_member_by_fullname_args, 0);
}

void unregister_idc_functions()
{
	set_idc_func_ex("reset_alt_calls", NULL, NULL, 0);
	set_idc_func_ex("structs_of_size", NULL, NULL, 0);
	//set_idc_func_ex("get_member_offset", NULL, NULL, 0);
}

static void set_call(ea_t from, ea_t to)
{	
	static const char * nname;
	if ( ph.id == PLFM_MIPS )
		nname = "$ mips";
	else if ( ph.id == PLFM_ARM )
		nname = " $arm";
	else
		nname = "$ vmm functions";
	netnode n(nname);
	ea_t ea = from;    // get current address
	if ( !isCode(get_flags_novalue(ea)) ) return; // not an instruction
	ea_t callee = n.altval(ea)-1;         // get the callee address from the database
	// remove thumb bit for arm
	if ( ph.id == PLFM_ARM )
		callee &= ~1;
	//char buf[MAXSTR];
	//qsnprintf(buf, sizeof(buf), form, help);
	//if ( AskUsingForm_c(buf, &callee) )
	callee = to;
	{
		if ( callee == BADADDR )
		{
			n.altdel(ea);
		}
		else 
		{
			#define T 20
			if ( ph.id == PLFM_ARM && (callee & 1) == 0 )
			{
				// if we're calling a thumb function, set bit 0
				sel_t tbit = getSR(callee, T);
				if ( tbit != 0 && tbit != BADSEL )
					callee |= 1;
			}
			#undef T
			n.altset(ea, callee+1);     // save the new address
		}
		noUsed(ea);                 // reanalyze the current instruction
	}

}

static bool idaapi add_VT_cref(void * ud)
{
	vdui_t &vu = *(vdui_t *)ud;
	bool is_call = false;
	vu.get_current_item(USE_KEYBOARD);
	ea_t caller_ea;
	cexpr_t *e = find_memptr_var(vu, &is_call, &caller_ea);
	if (e)
	{
		int offset = e->m;
		cexpr_t * var = e->x;
		if(var->type.empty())
		{			
			return false;
		}
		tid_t id = get_struc_from_typestring( var->type );
		if (id==BADNODE)
			return false;
		char comment[MAXSTR];
		get_struc_cmt(id, true, comment, sizeof(comment));
		char * strpos = strstr(comment, "@0x");
		if ( strpos )
		{
			ea_t ea;
			if ( my_atoea( strpos + 1, &ea) )
			{					
				ea_t fnc = get_long(ea + offset);
				flags_t flags = getFlags(fnc);
				if (isFunc(flags))
				{
					msg("[Hexrays-Tools] adding cref from %p to %p\n", caller_ea, fnc);
					//add_cref(caller_ea, fnc, fl_CF);
					set_call(caller_ea, fnc);
					//n.altset(ea, callee+1);     // save the new address
					//set_cmt();
					//char comment[MAXSTR];
					comment[0]=0;
					char new_comment[MAXSTR];
					new_comment[0]=0;
					get_cmt(caller_ea, false, comment, sizeof(comment));
					qsnprintf( new_comment, MAXSTR,  "%s\ncalls %p", comment, fnc );					
					set_cmt(caller_ea, new_comment, false);
					return true;
				}						
				else
				{
					msg("!isFunc\n");
				}
			}
			else
			{
				msg("!atoea %s \n", strpos + 1);
			}
		}	
		else
		{
			msg("!strpos\n");			
		}
	}	
	return false;
}

static bool idaapi jump_to_struct(void * ud)
{	
	vdui_t &vu = *(vdui_t *)ud;
	bool is_call = false;
	vu.get_current_item(USE_KEYBOARD);
	cexpr_t *e = find_memptr_var(vu, &is_call);
	if (e)
	{
		int offset = e->m;
		if (e->op == cot_idx)
			e = e->x;
		cexpr_t * var = e->x;
		if( var->type.empty() )
		{			
			return false;
		}
		return jumptype(var->type, offset, is_call);
	}
	e = find_cast(vu);
	if (e)
	{				
		if(e->type.empty())
		{			
			return false;
		}
		return jumptype(e->type, 0);
	}
	e = find_var(vu);
	if (e)
	{				
		if(e->type.empty())
		{			
			return false;
		}
		return jumptype(e->type, 0);
	}

	return false;
}

bool show_VT_from_tid(tid_t id)
{
	if (id == BADADDR)
	{
		//msg("!get_struc_id\n");
		return false;
	}
	{
		char comment[MAXSTR];
		get_struc_cmt(id, true, comment, sizeof(comment));
		char * strpos = strstr(comment, "@0x");

		if (strpos)
		{
			ea_t ea;
			if ( my_atoea( strpos + 1, &ea) )
			{
				function_list fl;
				{
					while (1)
					{
						ea_t fncea = get_long(ea);

						flags_t fnc_flags = getFlags(fncea);

						if ( isFunc(fnc_flags)  )
						{
							fl.functions.push_back(fncea);
						}		
						ea = next_head(ea, BADADDR);
						flags_t ea_flags = getFlags(ea);
						if (has_any_name(ea_flags))
							break;
					}
				}
				fl.choose("[Hexrays-Tools] virtual table");
				fl.open_pseudocode();

				return true;
			}
		}
		struc_t * struc = get_struc(id);
		member_t * member = get_member(struc, 0);
		if(member)
		{
			tinfo_t t;
			if( get_member_type(member, &t) )
			{
				tid_t child = get_struc_from_typestring(t);
				if(child!=BADNODE)
				{
					return show_VT_from_tid(child);
				}
			}
		}
	}
	return false;
}

static bool idaapi show_VT(void * ud)
{
	vdui_t &vu = *(vdui_t *)ud;

	cexpr_t *e = find_memptr_var(vu, 0);
	if (e)
	{
		int offset = e->m;
		if (e->op == cot_idx)
			e = e->x;
		cexpr_t * var = e->x;
		tinfo_t t = var->type;
		if(t.empty())
		{			
			return false;
		}

		while(t.is_ptr_or_array())
			t.remove_ptr_or_array();

		if(t.is_struct())
		{
			tid_t id = get_struc_from_typestring( t );
			show_VT_from_tid(id);
			//open_structs_window(id, offset);		
		}

		//return jumptype(var->type, offset, true);
	}

	return false;
}


struct offset_locator_t : public ctree_parentee_t
{
	lvars_t * lvars;
	lvar_t * var_hled;
	std::set<int> idxs;
	std::set<int> offsets;
	std::map<int, typestring> types;

	bool is_our(int idx)
	{
		return idxs.find(idx)!=idxs.end();
	}

	offset_locator_t(lvars_t * lvars_, lvar_t * lvar_) : lvars(lvars_)
	{		
		int idx = get_idx_of(lvars_, lvar_);
		//deprecated index of
		/*int c=0;
		for(lvars_t::iterator i = lvars_->begin(); i!=lvars_->end(); i++)
		{
			if (&*i == lvar_)
			{
				idx = c;
				break;
			}
			c++;			
		}*/
		//QASSERT(0, idx!=-1);
		idxs.insert(idx);		
	}

	int idaapi visit_expr(cexpr_t * e)
	{
		if ( e->op == cot_asg )
		{
			if (e->x->op == cot_var &&  e->y->op == cot_var)
			{
				if(idxs.find(e->y->v.idx)!=idxs.end() && idxs.find(e->x->v.idx)==idxs.end())
				{
					idxs.insert(e->x->v.idx);
				}
			}
			return 0;
		}
		if ( e->op == cot_memptr )
		{
			if ( e->x->op == cot_var )
			{
				cexpr_t * var = e->x;
				if(idxs.find(var->v.idx)!=idxs.end())
				{
					msg( "var: %s.%d\n", (*lvars)[var->v.idx].name.c_str(), e->m );
					offsets.insert(e->m);
				}			
			}		
		}
		
		{			
			bool is_lvar = (e->op == cot_var && is_our(e->v.idx));
			bool is_glob_obj_ptr = (e->op == cot_obj && is_our(e->obj_ea));
			int index = 0;
			if (is_lvar)
			{
				index = e->v.idx;
			}
			/*if (is_glob_obj_ptr)
			{
				index = e->obj_ea;
			}*/		
			if ( is_lvar /*|| is_glob_obj_ptr*/ )
			{ // found our var. are we inside a pointer expression?
				uint64 delta1 = 0;
				uint64 delta2 = 0;
				bool delta_defined1 = true;
				bool delta_defined2 = false;
				bool is_array = false;
				int i = parents.size() - 1;
				if ( parents[i]->op == cot_add ) // possible delta
				{
					cexpr_t *d = ((cexpr_t*)parents[i])->theother(e);
					delta_defined1 = d->get_const_value(&delta1);
					i--;
					//possible array indexing
					if ( parents[i]->op == cot_add /*|| parents[i]->op == cot_sub*/ )
					{
						cexpr_t *d = ((cexpr_t*)parents[i])->theother((cexpr_t*)parents[i+1]);						
						delta_defined2 = d->get_const_value(&delta2);
						is_array = true;
						i--;
					}
				}
				
				uint64 delta;
				if (delta_defined1)
					delta = delta1;
				if (delta_defined2)
					delta = delta2;
				typestring t;				
				if ( delta_defined1 || delta_defined2 )
				{
					// skip casts					
					while ( parents[i]->op == cot_cast )
					{
						t = ((cexpr_t*)parents[i])->type;
						i--;
					}
					//is_ptr = type.type.is_ptr();
					t = remove_pointer(t);
					offsets.insert((int)delta);
					if (t.size()>0)
					{
						types[(int)delta] = t;
						char buff[MAXSTR];
						t.print(buff, sizeof(buff));
						msg("[Hexrays-Tools] new var: %s.%d [%s][array: %d]\n", (*lvars)[index].name.c_str(), (int)delta, buff, is_array);
					}
					else
						msg("[Hexrays-Tools] new var: %s.%d[array:%d]\n", (*lvars)[index].name.c_str(), (int)delta, is_array);
				}
			}
		}

		if ( e->op == cot_ptr )
		{			
			if(e->x->op == cot_cast)
			{
				cexpr_t * cast = e->x;
				if( cast->x->op == cot_add )
				{
					cexpr_t * add = cast->x;
					if ( add->x->op == cot_var )
					{
						cexpr_t * var = add->x;
						if (add->y->op == cot_num)
						{
							cexpr_t * num = add->y;
							if(idxs.find(var->v.idx)!=idxs.end() )
							{
								msg("[Hexrays-Tools] var: %s.%d\n", (*lvars)[var->v.idx].name.c_str(), (int)num->numval());
								offsets.insert((int)num->numval());
								types[(int)num->numval()] = cast->type;
							}
						}
						else
						{
							if(&(*lvars)[var->v.idx] == var_hled)
							{
								msg("[Hexrays-Tools] xvar: %s\n", (*lvars)[var->v.idx].name.c_str());
							}
						}
					}
				}
				else
					if( cast->x->op == cot_var )
					{
						cexpr_t * var = cast->x;
						if(idxs.find(var->v.idx)!=idxs.end() )
						{
							msg("cast var: %s\n", (*lvars)[var->v.idx].name.c_str());
							offsets.insert(0);
							types[0] = cast->type;
						}
					}

			}
		}
		return 0;
	}
	int idaapi visit_insn(cinsn_t *i)
	{

		return 0; // continue enumeration
	}
};

uint32 idaapi sizer(void *obj)
{
	intvec_t * vec = (intvec_t*)obj;
	return vec->size() + 1;
}

void idaapi get_type_line(void *obj,uint32 n,char * const *arrptr)
{
	intvec_t * vec = (intvec_t*)obj;
	if (n==0)
	{
		qstpncpy( arrptr[0], "type", MAXSTR );
		qstpncpy( arrptr[1], "sz", MAXSTR );
		qstpncpy( arrptr[2], "sz hx", MAXSTR );
	}
	else if (n==1)
	{
		qstpncpy( arrptr[0], "<create new>", MAXSTR );
		arrptr[1][0] = 0;
		arrptr[2][0] = 0;
	}
	else
	{
		tid_t id = (*vec)[n-2];
		asize_t size = get_struc_size(id);
		qstring tmpname;
		get_struc_name(&tmpname, id);		
		qsnprintf(arrptr[0], MAXSTR, "%s", tmpname.c_str());
		qsnprintf(arrptr[1], MAXSTR, "%d", size);
		qsnprintf(arrptr[2], MAXSTR, "0x%08x", size);
	}	
}
/*
void idaapi enter(void *obj, uint32 n)
{
	intvec_t * idcka = (intvec_t *)obj;
	uint32 choosed = n;
	if(n>1)
	{
		open_structs_window((*idcka)[n], 0);
	}
}
*/

bool idaapi extract_substruct_callback(void *ud)
{
	//Controls::TCustomControl * cc  = (Controls::TCustomControl *)ud;
	Controls::TCustomControl * cc = get_current_viewer();

	if(!cc)
		return false;

	twinpos_t b,e;
	if (!readsel2(cc, &b, &e))
		return false;

	structplace_t * sb = (structplace_t *)b.at;
	structplace_t * se = (structplace_t *)e.at;
	msg("%d, %d\n", sb->offset, se->offset);
	if (sb->idx != se->idx)
		return false;

	if ( extract_substruct(sb->idx, sb->offset, se->offset) )
	{
		//this should be called automatically
		//refresh_custom_viewer(cc);
		return true;
	}				
	return false;
}


bool idaapi which_struct_matches_here_callback(void *ud)
{
	//Controls::TCustomControl * cc  = (Controls::TCustomControl *)ud;
	Controls::TCustomControl * cc = get_current_viewer();
	if(!cc)
		return false;
	twinpos_t b,e;
	if (!readsel2(cc, &b, &e))
		return false;
	structplace_t * sb = (structplace_t *)b.at;
	structplace_t * se = (structplace_t *)e.at;
	if (sb->idx != se->idx)
		return false;
	if ( which_struct_matches_here(sb->idx, sb->offset, se->offset) )
	{
		//we dont need refresh after this (I guess)
		return false;//true;
	}				
	return false;
}

tid_t create_VT_struc(ea_t VT_ea, char * name, uval_t idx = BADADDR, unsigned int * vt_len  = NULL )
{
	char name_[MAXNAMELEN];

	get_name(BADADDR, VT_ea, name_, sizeof(name_));
	if(qstrstr(name_,"_VT_"))
	{
		char * ch = strchr(name_, 0);
		*(--ch) = 0;
		name = name_;		
	}
	else
	if( !name )
	{		
		qsnprintf(name_, MAXNAMELEN, "VT_%p", VT_ea);
		name = name_;
	}
	
	{//TODO: do this better
		ea_t fncea = get_long(VT_ea);
		flags_t fnc_flags = getFlags(fncea);
		if (!isFunc(fnc_flags))
			return BADNODE;
	}

	tid_t newid = get_struc_id(name);
	if (newid == BADADDR)
		newid = add_struc(idx, name);

	if (newid == BADADDR)
		return BADNODE;

	struc_t * newstruc = get_struc(newid);
	if (!newstruc)
		return BADNODE;

	char comment[MAXSTR];
	qsnprintf(comment, sizeof(comment), "@0x%p", VT_ea);
	set_struc_cmt(newid, comment, true);

	ea_t ea = VT_ea;
	int offset = 0;
	int len = 0;
	while (1)
	{
		offset = ea - VT_ea;
		char funcname[MAXSTR];
		memset(funcname, 0, sizeof(funcname));
		ea_t fncea = get_long(ea);

		//there are no holes in vftables (I hope)
		if(fncea  == 0)
			break;
		//there are also no false pointers in vftables
		if (!isEnabled(fncea))
			break;

		flags_t fnc_flags = getFlags(fncea);
		char * fncname = NULL;
		if (isFunc(fnc_flags) /*&& has_user_name(fnc_flags)*/ )
		{
			//if ( get_short_name(BADADDR, fncea,  funcname, sizeof(funcname)) )
			if(get_func_name(fncea, funcname, MAXSTR))
				fncname = funcname;
		}		
		add_struc_member(newstruc, NULL, offset, dwrdflag(), NULL, 4);
		len++;
		if (fncname)
		{
			set_member_cmt(get_member(newstruc, offset), fncname, true);
			if(!set_member_name( newstruc, offset, fncname ))
			{
				get_name(NULL, fncea, funcname, sizeof(funcname));
				set_member_name( newstruc, offset, fncname );
			}
		}
		
		//ea = next_head(ea, BADADDR);
		//TODO: this is only for 32bit processors but so is hxrs
		ea += 4;
		flags_t ea_flags = getFlags(ea);
		if (has_any_name(ea_flags))
			break;
	}
	if(vt_len)
		*vt_len = len;
	return newid;
}


static void set_vt_type(struc_t *struc, char name_of_vt_struct[MAXSTR])
{
	typestring type = create_numbered_type_from_name(name_of_vt_struct);//create_typedef(name);
	type = make_pointer(type);
	member_t * member = get_member(struc, 0);
	if(!member)
	{		
		if(add_struc_member(struc, NULL, 0, dwrdflag(), NULL, 4) != 0)
			return;
		member = get_member(struc, 0);
	}
	set_member_tinfo(idati, struc, member, 0, type.c_str(), /*fields.c_str()*/NULL, SET_MEMTI_MAY_DESTROY);
	set_member_name(struc, 0, "VT");
}
static bool create_VT(uval_t idx)
{
	tid_t id = get_struc_by_idx( idx );
	if (is_union(id))
	{
		msg("[Hexrays-Tools] union!\n");
		return false;
	}

	struc_t * struc = get_struc(id);
	if(!struc)
	{
		msg("[Hexrays-Tools] !struc\n");
		return false;
	}

	qstring tmpname;
	get_struc_name(&tmpname, id);
	char *name = qstrdup(tmpname.c_str());
	qstrncat(name, "_VT", MAXSTR);

	///---------------------
	char name_VT[MAXSTR];
	qstrncpy(name_VT, name, MAXSTR);
	qstrncat(name_VT, "_", MAXSTR);
	ea_t VT_ea;

	VT_ea = get_name_ea(BADADDR, name_VT);
	if (VT_ea == BADADDR)
	{
		if (askaddr(&VT_ea,  "[Hexrays-Tools] Gimme address of virtual function table")==0)
			return false;	
	}

	create_VT_struc(VT_ea, name, idx + 1 );
	
	///--------------------- set pointer to VT in struct

	//TODO: change this code using function create_typedef
	//create_typedef(name);
	//
	/*char ptrname[MAXSTR];
	qsnprintf(ptrname, MAXSTR, "%s * dummy;", name);
	qstring qname;
	qtype fields;
	qtype type;
	parse_decl(idati, ptrname, &qname, &type, &fields, PT_VAR);*/
	
	set_vt_type(struc, name);
	return true;
}

bool idaapi create_VT_callback(void *ud)
{
	Controls::TCustomControl * cc = get_current_viewer();

	if(!cc)
		return false;

	structplace_t * place;
	int x, y;
	place = (structplace_t *)get_custom_viewer_place(cc, false, &x, &y);
	if(!place)
		return false;

	msg("%d, %d\n", place->idx, place->offset);

	if ( create_VT(place->idx) )
	{
		return true;
	}			
	return false;
}




bool idaapi add_to_classes_callback(void *ud)
{
	Controls::TCustomControl * cc = get_current_viewer();
	if(!cc)
		return false;

	structplace_t * place;
	int x, y;
	place = (structplace_t *)get_custom_viewer_place(cc, false, &x, &y);
	if(!place)
		return false;

	//msg("%d, %d\n", place->idx, place->offset);
	tid_t tid = get_struc_by_idx(place->idx);
	if (add_class(tid))
		return true;
	return false;
}

bool idaapi unpack_this_member_callback(void *ud)
{
	Controls::TCustomControl * cc = get_current_viewer();

	if(!cc)
		return false;

	structplace_t * place;
	int x, y;
	place = (structplace_t *)get_custom_viewer_place(cc, false, &x, &y);
	if(!place)
		return false;

	//msg("%d, %d\n", place->idx, place->offset);

	if ( unpack_this_member(place->idx, place->offset) )
	{
		return true;
	}			
	return false;
}

static void add_structures_popup_items(Controls::TCustomControl * cc)
{
	//todo: replace this by calling ui_add_menu_item with SETMENU_CTXSTR flag
	if (cc)
	{
		add_custom_viewer_popup_item(cc, "[Hexrays-Tools] Extract object", "", extract_substruct_callback, NULL);
		add_custom_viewer_popup_item(cc, "[Hexrays-Tools] Unpack object", "", unpack_this_member_callback, NULL);
		add_custom_viewer_popup_item(cc, "[Hexrays-Tools] Which struct matches here?", "", which_struct_matches_here_callback, NULL);
		add_custom_viewer_popup_item(cc, "[Hexrays-Tools] Add to classes", "", add_to_classes_callback, NULL);
		add_custom_viewer_popup_item(cc, "[Hexrays-Tools] Add VT", "", create_VT_callback, NULL);
	}
}

static bool convert_cc_to_special(func_type_info_t & fti)
{
	switch(fti.cc & CM_CC_MASK)
	{
	case CM_CC_CDECL:
		fti.cc = CM_CC_SPECIAL;
		break;
	case CM_CC_STDCALL:
	case CM_CC_PASCAL:
	case CM_CC_FASTCALL:
	case CM_CC_THISCALL:
		fti.cc = CM_CC_SPECIALP;
		break;
	case CM_CC_ELLIPSIS:
		fti.cc = CM_CC_SPECIALE;
		break;
	case CM_CC_SPECIAL:
	case CM_CC_SPECIALE:
	case CM_CC_SPECIALP:
		//do nothing but return true
		break;
	default:
		msg("convert to __usercall: Unhandled function cc, %x", fti.cc & CM_CC_MASK);
		return false;
	}
	return true;
}


#pragma pack(push, 1)
struct objvec
{
  void *ptr;
  int delka;
  int alloc;
  char field_C;
  char field_D[3];
  int field_10;
};
#pragma pack(pop)

/*
//typedef char  ( __fastcall *create_new_type_t)(vdui_t *a1, int var_index);
//create_new_type_t create_new_type = (create_new_type_t)0x1709A170;
C<char   __fastcall (vdui_t *a1, int var_index), 0x1709A170> create_new_type_t;

//C<char __thiscall (cfunc_t *cfunc, int lvar_index, objvec *a3), 0x01702E210> can_crate_new_type;
typedef char (__thiscall *can_crate_new_type_t)(cfunc_t *cfunc, int lvar_index, objvec *a3);
can_crate_new_type_t can_crate_new_type = (can_crate_new_type_t)0x01702E210;

typedef char (__fastcall *objvec_to_typestring_t)(objvec *a1, qstring * types_, qstring * names);
objvec_to_typestring_t objvec_to_typestring = (objvec_to_typestring_t)0x0170CABE0;

typedef int (__thiscall *smitec_t)(objvec *thi);
smitec_t smitec = (smitec_t)0x17097150;
*/

objvec obj = {0};

#pragma pack(push, 1)

struct pair
{
  int cislo;
  qstring str;
};

struct tree_node
{
  tree_node *left;
  tree_node *parent;
  tree_node *right;
  pair data;
  BYTE color;
  BYTE isnil;
};



struct mapa
{
  tree_node *iterator_begin;
  char field_4[20];
  tree_node *struct_result_1_ptr;
  DWORD len;
};



struct struct_a1
{
  int *VT;
  DWORD cv_flags;
  int **parents;
  DWORD index_in_parents;
  DWORD dword10;
  BYTE do_work;
  char f15[3];
  mapa  labels;
  DWORD lvar_index;

};
#pragma pack(pop)

/*
typedef struct_a1 * (__stdcall * create_var_locator_t)(struct_a1 *a1, int index, char a3);
create_var_locator_t create_var_locator = (create_var_locator_t)0x01702D770;
typedef int (__thiscall *visitor_delete_t)(struct_a1 *thi);
visitor_delete_t visitor_delete = (visitor_delete_t)0x01702D7F0;
*/

//state for type deducer
field_info_t fi;
//struct_a1 aaa = {0};
//char padding[100] = {0};
//bool initedx = false;


//this gathers types
static bool idaapi var_testing(void *ud)
{
	vdui_t &vu = *(vdui_t *)ud;		
	can_be_converted_to_ptr2(vu, vu.item, &fi);
	return true;
}

static bool idaapi var_testing1_5(void *ud)
{
	vdui_t &vu = *(vdui_t *)ud;
	show_new_struc_view(&fi);

	
	/*
	for(field_info_t::const_iterator i = fi.begin(); i!=fi.end(); i++)
	{
		uval_t off = i->first;
		const scan_info_v2_t & si =  (i->second);
		qstring result;		
		for(typevec_t::const_iterator i = si.types.begin(); i != si.types.end(); i++ )
		{
			print_type_to_qstring(&result, 0, 5, 40, 12|2, idati, (type_t *)i->c_str(), "test");
		msg("%i -- %s\n", off, result.c_str());	
		}
	}*/	
	return true;
}

//this prints them
static bool idaapi var_testing2(void *ud)
{
	vdui_t &vu = *(vdui_t *)ud;

	qstring name;
	lvar_t * lvar;
	ea_t ea;
	if (!name_for_struct_crawler(vu, name, &lvar, &ea))
		return false;

	tinfo_t type;
	strtype_info_v2_t sti;
	if (!fi.to_type(name, type, &sti))
		return false;
	


	tinfo_t restype = make_pointer(type);
	
#if 1
	std::for_each(fi.scanned_variables.begin(), fi.scanned_variables.end(), [&](std::pair<scanned_variables_t::key_type, scanned_variables_t::mapped_type> p)
	{
		tinfo_t typ = restype;
		func_t * func = get_func(p.first);
		if (!func)
			return;
		hexrays_failure_t failure;
		//TODO: this doesn't work as expected when new_window == false :-(
		vdui_t * ui = open_pseudocode(p.first, true);
		if(!ui)
			return; 
		cfunc_t* decompilation = ui->cfunc;
		//cfunc_t* decompilation = decompile(func, &failure);		
		if(!decompilation)
			return;
		decompilation->verify(ALLOW_UNUSED_LABELS, true);
		lvars_t * lvars = decompilation->get_lvars();
		if(!lvars)
			return;
		typedef decltype(p.second) TMP;
		std::for_each(p.second.begin(), p.second.end(), [&](TMP::value_type & x)
		{
			//msg("ea: 0x%08X  loc: %08X defea: %08X offset: %d\n", p.first, x.second.location, x.second.defea, x.first);
			lvar_t * var = lvars->find(x.second);
			if (var)
			{
				//var->set_lvar_type(restype);

				if (x.first == 0)
				{
					if (var->accepts_type(typ))
					{
						ui->set_lvar_type(var, typ);
					}					
				}
				else
				{
					tinfo_t tt;
					tt.clear();
					
					if (fi.types_cache.find(x.first) != fi.types_cache.end())
					{
						tt = fi.types_cache[x.first];
					}
					else
					{
						if (sti.getat(x.first, tt))
						{
							fi.types_cache[x.first] = tt;
						}
					}
					if(!tt.empty())
					{
						tt = make_pointer(tt);
						if (var->accepts_type(tt))
						{
							ui->set_lvar_type(var, tt);
						}
					}
				}
			}
		});		
		//delete decompilation;
	});
#endif

	if (lvar)
	{
		set_lvar_type(&vu, lvar, &restype);
	}
	else
	{
		if(ea && ea!=BADADDR)
		{	
			apply_tinfo(idati, ea, make_pointer(type).c_str(), NULL, 1);
		}		
		//C++0x test
		std::for_each(fi.global_pointers.begin(), fi.global_pointers.end(), [&](ea_t& i)
		{
			if(i != ea) apply_tinfo(idati, i, make_pointer(type).c_str(), NULL, 1);
		});	
		/*for(global_pointers_t::iterator iter = fi.global_pointers.begin(); iter!=fi.global_pointers.end(); iter++)
		{
			if(*iter == ea)
				continue;
			apply_tinfo(idati, *iter, make_pointer(type).c_str(), NULL, 1);
		}*/
	}
	fi.clear();
	return true;
}

static bool idaapi remove_argument(void *ud)
{
	vdui_t &vu = *(vdui_t *)ud;
	if (vu.item.citype != VDI_LVAR)
		return false;

	lvar_t * lvar =  vu.item.get_lvar();	

	if (!lvar->is_arg_var())
		return false;

	//this doesn't work
	/*
	lvar->clr_arg_var();		
	vu.cfunc->build_c_tree();
	vu.refresh_ctext();				
	*/

	if (vu.cfunc->entry_ea==BADADDR)
		return false;

	tinfo_t type;
	qtype fields;	
	if (!vu.cfunc->get_func_type(type, fields))
		return false;
	
	func_type_info_t fti;
	if ( -1 == build_funcarg_info(idati, type.c_str(), fields.c_str(), &fti, 0) )
		return false;
	
	if (!convert_cc_to_special(fti))
		return false;	

//TODO: match by register number
	/*
	from forum:
al    8
ah    9
eax   8
dl    12
dh    13
edx   12
cl    16
ch    17
ecx   16
bl    20
bh    21
ebx   20
esp   24
ebp   28
edi   32
esi   36	
	*/
	//modify fti	
	auto i = std::find_if(fti.begin(), fti.end(), [&](funcarg_info_t&i) {return i.name.size() && i.name == lvar->name;});
	if (i!=fti.end()) fti.erase(i);
	
	/*for (func_type_info_t::iterator i =  fti.begin(); i!=fti.end(); i++)
	{
		if (i->name.size() && i->name == lvar->name)
		{
			fti.erase(i);			
			break;
		}	
	}*/
	fields.clear();
	type.clear();
	build_func_type(&type, &fields, fti);
	if ( !apply_tinfo(idati, vu.cfunc->entry_ea, type.c_str(), fields.c_str(), 1) )
		return false;
	
	vu.refresh_view(true);
	return true;		
}

static bool idaapi remove_rettype(void *ud)
{
	vdui_t &vu = *(vdui_t *)ud;
	if (vu.item.citype != VDI_FUNC)
		return false;

	//lvar_t * lvar =  vu.item.get_lvar();
	/*if (!lvar->is_result_var())
		return false;
		*/

	//this doesn't work
	/*lvar->clr_arg_var();		
	vu.cfunc->build_c_tree();
	vu.refresh_ctext();				
	*/

	if (vu.cfunc->entry_ea==BADADDR)
		return false;

	typestring type;
	qtype fields;

	if (!vu.cfunc->get_func_type(type, fields))
		return false;
	
	func_type_info_t fti;
	if ( -1 == build_funcarg_info(idati, type.c_str(), fields.c_str(), &fti, 0) )
		return false;
	
	if (!convert_cc_to_special(fti))
		return false;	

	//modify fti
	fti.rettype.clear();
	fields.clear();
	type.clear();
	build_func_type(&type, &fields, fti);
	if ( !apply_tinfo(idati, vu.cfunc->entry_ea, type.c_str(), fields.c_str(), 1) )
		return false;
	
	vu.refresh_view(true);
	return true;		
}

//
static bool idaapi convert_to_usercall(void *ud)
{
	vdui_t &vu = *(vdui_t *)ud;
	if (!vu.cfunc)
		return false;
	if ( vu.cfunc->entry_ea == BADADDR )
		return false;
	tinfo_t type;
	qtype fields;		
	if (!vu.cfunc->get_func_type(type, fields))
		return false;
	func_type_info_t fti;
	int a = build_funcarg_info(idati, type.c_str(), fields.c_str(), &fti, 0);
	if (!convert_cc_to_special(fti))
		return false;
	fields.clear();
	type.clear();
	build_func_type(&type, &fields, fti);
	if ( !apply_tinfo(idati, vu.cfunc->entry_ea, type.c_str(), fields.c_str(), 1) )
		return false;
	vu.refresh_view(true);
	return true;
}

static bool struct_matches(offset_locator_t &ifi, struc_t *struc)
{
	auto i = std::find_if_not(ifi.offsets.begin(),ifi.offsets.end(), [&](const asize_t & offset){return struct_has_member(struc, offset);});
	return i == ifi.offsets.end();
	/*for(std::set<int>::iterator it = ifi.offsets.begin(); it!=ifi.offsets.end(); ++it )
	{
		asize_t offset = *it;
		if(!struct_has_member(struc, offset))
		{
			return false;			
		}
	}
	return true;*/
}
static bool new_structure_from_offset_locator(offset_locator_t &ifi, char name[MAXSTR], bool &pShouldReturn)
{
	pShouldReturn = false;
	tid_t id = add_struc(BADADDR, NULL);
	struc_t * struc = get_struc(id);
	if(!struc)
	{
		msg("[Hexrays-Tools] failed to create new structure\n");
		pShouldReturn = true;
		return false;
	}
	ifi.offsets.insert(0);
	for(std::set<int>::iterator it = ifi.offsets.begin(); it!=ifi.offsets.end(); ++it )
	{
		asize_t offset = *it;
		int err;
		std::map<int, typestring>::iterator i = ifi.types.find(offset);
		if (i != ifi.types.end())
		{
			typestring ts = i->second;
			ts.remove_ptr_or_array();
			const type_t * t = ts.resolve();


			if (is_type_short(*t) || get_full_type(*t) == BT_UNK_WORD)
			{
				err = add_struc_member(struc, NULL, offset, wordflag(), NULL, 2);
			}
			else
				if (is_type_long(*t) || get_full_type(*t) == BT_UNK_DWORD)
				{
					err = add_struc_member(struc, NULL, offset, dwrdflag(), NULL, 4);
				}
				else
					if(is_type_char(*t) || get_full_type(*t) == BT_UNK_BYTE)
					{
						err = add_struc_member(struc, NULL, offset, byteflag(), NULL, 1);
					}
					else
					{
						err = add_struc_member(struc, NULL, offset, 0, NULL, 1);
					}
		}
		else
		{
			err = add_struc_member(struc, NULL, offset, 0, NULL, 1);
		}
		if( err != 0)
			msg("[Hexrays-Tools] failed to add member at offset %d err %d\n", offset, err);
	}
	qstring tmpname;
	get_struc_name(&tmpname, id);
	qstrncpy(name, tmpname.c_str(), MAXSTR);
	msg("[Hexrays-Tools] created struct %s\n", name);
	return false;
}
static bool idaapi traverse_(void *ud)
{
	vdui_t &vu = *(vdui_t *)ud;
	vu.get_current_item(USE_KEYBOARD);
	lvar_t * var = vu.item.get_lvar();
	if ( var )
	{
		msg("lvar->name: %s\n", var->name.c_str());
	}
	else
	{
		return 1;
	}
	lvars_t * lvars = vu.cfunc->get_lvars();

	offset_locator_t ifi(lvars, var);
	ifi.apply_to(&vu.cfunc->body, NULL); // go!

	intvec_t idcka;
	idcka.clear();

	if( !ifi.offsets.empty() )
	{
		//std::sort(ifi.offsets.begin(), ifi.offsets.end());
		for(uval_t idx = get_first_struc_idx(); idx!=BADNODE; idx=get_next_struc_idx(idx))
		{
			tid_t id = get_struc_by_idx(idx);
			struc_t * struc = get_struc(id);
			if(!struc)
				continue;
			if (is_union(id))
				continue;
			if(struct_matches(ifi, struc))
			{
				idcka.push_back(id);
			}
		}
	}

	/* //chcem nabidnout moznost strukturu vyrobit
	if (idcka.empty())
	return true;
	*/

	static const int widths[] = { 20, 8|CHCOL_DEC, CHCOL_HEX|8 };

	chooser_info_t chooser = {0};
	chooser.cb = sizeof(chooser_info_t);
	chooser.columns = 3;
	chooser.widths = widths;
	chooser.getl = get_type_line;
	chooser.sizer = sizer;
	chooser.title = "possible types";
	chooser.obj = (void *)&idcka;
	chooser.flags = CH_MODAL;
	chooser.icon = -1;	
	//chooser.enter = enter;
	int choosed = choose3(&chooser);

	//int choosed = choose2((void *)&idcka, 3, widths, sizer, get_type_line, "possible types");

	qstring name;
	if ( choosed > 1 )
	{
		get_struc_name(&name, (idcka)[choosed-2]);

		typestring ts = create_numbered_type_from_name(name.c_str());//create_typedef(name);		
		//var->set_lvar_type(make_pointer(ts));
		//var->set_user_type();		
		vu.set_lvar_type(var, make_pointer(ts));
		vu.refresh_view(false);
		msg("struct: %s\n", name);
	}
	else
		if (choosed == 1)
		{
			int idx = get_idx_of(lvars, var);
			strtype_info_t typeinfo;
			vu.cfunc->gather_derefs(idx, &typeinfo);
			typestring restype;
			typestring resfields;
			typeinfo.build_udt_type(&restype, &resfields);

			typestring ts;			
			if (!structure_from_restype_resfields(var->name, ts, restype, resfields))
				return false;
			//var->set_final_lvar_type(make_pointer(ts));
			vu.set_lvar_type(var, make_pointer(ts));
			vu.refresh_view(false);
			return true;
			/*
			bool lResult = new_structure_from_offset_locator(ifi, name, lShouldReturn);
			if (lShouldReturn)
				return lResult;
			typestring ts = create_typedef(name);
			var->set_final_lvar_type(make_pointer(ts));
			//var->set_lvar_type(make_pointer(ts));
			//var->force_lvar_type(make_pointer(ts));
			//var->set_user_type();
			vu.refresh_view(false);
			msg("struct: %s\n", name);*/
		}
	


		return true; // done
}

//-----------------------------------------------------
static bool idaapi is_number(void *ud)
{
	vdui_t &vu = *(vdui_t *)ud;
	if (!vu.item.is_citem())
		return false;
	cexpr_t *e = vu.item.e;
	if (e->op != cot_num)
		return false;
	asize_t offset = e->numval();
	if (offset > 16777216)
		return false;
	return true;
}



/*
 var_of_type_type = (cast to type)something;

 //cursor is on var
 asg -> x -> var with type A
     -> y -> cast to type A -> x -> something with type B
	 =>
 asg -> x -> var with type B
     -> y -> something with type B

*/
static bool idaapi is_cast_assign(void *ud, tinfo_t * ts)
{
	vdui_t &vu = *(vdui_t *)ud;
	if (!vu.item.is_citem())
		return false;

	cexpr_t * var = vu.item.e;
	if (var->op != cot_var)
		return false;
		
	citem_t * asg_ci = vu.cfunc->body.find_parent_of(var);
	if(!asg_ci->is_expr())
		return false;

	cexpr_t * asg = (cexpr_t *)asg_ci;
	if(asg->op != cot_asg)
		return false;

	if(asg->x != var)
		return false;

	cexpr_t * cast = asg->y;
	if(cast->op!=cot_cast)
		return false;
	cexpr_t * data = cast->x;

	//is this necessary?
	if (!data->is_expr())
		return false;

	if(ts)
		*ts = data->type;
	return true;
}


static bool idaapi is_union(void *ud)
{
	vdui_t &vu = *(vdui_t *)ud;
	if (!vu.item.is_citem())
		return false;

	cexpr_t * var = vu.item.e;
	if (var->op != cot_memptr && var->op != cot_memref)
		return false;

	if (!var->type.is_enum())
		return false;	

	return true;
}


/*
   v12 = ((int (__thiscall *)(struct_v7_0 *))v8->pvt_004047b00->anonymous_2)(v8);
   want to change type of anonymous_2

 //cursor is on memptr
 cast to type A -> x -> memptr  -> something with type ptr to struct   
	 => 
 struct+offset has now type A

*/
static bool idaapi is_vt_call_cast(void *ud, bool doit = false)
{
	vdui_t &vu = *(vdui_t *)ud;
	if (!vu.item.is_citem())
		return false;

	cexpr_t * memptr = vu.item.e;

	if (memptr->op != cot_memptr)
		return false;

/*
	hx_tree_match_node_t * m = new hx_tree_match_op_t<cot_cast>(&memptr);
	m->x =  new hx_tree_match_op_t<cot_memptr>(0);
	m->x->x = new hx_tree_match_node_t();

	if(m->accept(vu.item.e))
	{
	
	}*/

		
	citem_t * cast_ci = vu.cfunc->body.find_parent_of(memptr);
	if(!cast_ci->is_expr())
		return false; 

	cexpr_t * cast = (cexpr_t *)cast_ci;
	if(cast->op != cot_cast)
		return false;

	tinfo_t t = memptr->x->type;
	if(!t.is_ptr())
		return false;

	tid_t ti = get_struc_from_typestring(t);
	if(ti==BADNODE)
		return false;
	
	if(!doit)
		return true;

	struc_t * struc = 0;
	member_t * member = vu.item.get_memptr(&struc);
	if(!member)
		return false;
	
	if(!set_member_tinfo(idati, struc, member, 0, cast->type.c_str(), NULL, SET_MEMTI_COMPATIBLE /*| SET_MEMTI_MAY_DESTROY*/))
		return false;
	
	vu.refresh_view(false);
	return true;
}

//http://www.softwareverify.com/software-verify-blog/?p=319 slightly modified
int isOKToExecuteMemory(void    *ptr,
                        DWORD   size)
{
	SIZE_T                          dw;
	MEMORY_BASIC_INFORMATION	mbi;
	int                             ok;
	dw = VirtualQuery(ptr, &mbi, sizeof(mbi));
	ok = ((mbi.Protect & PAGE_EXECUTE_READ) ||  (mbi.Protect & PAGE_EXECUTE_READWRITE) ||  (mbi.Protect & PAGE_EXECUTE_WRITECOPY));
	// check the page is not a guard page
	if (mbi.Protect & PAGE_GUARD)
		ok = FALSE;
	if (mbi.Protect & PAGE_NOACCESS)
		ok = FALSE;
	return ok;
}


C<char   __fastcall  (vdui_t *, lvar_t *, typestring *), 0x17097C10> do_set_lvar_type;
//typedef char  ( __fastcall  * do_set_lvar_type_t)(vdui_t *, lvar_t *, typestring *);
//do_set_lvar_type_t do_set_lvar_type  = (do_set_lvar_type_t)0x17097C10;

bool set_lvar_type(vdui_t * vu, lvar_t *  lv, tinfo_t * ts)
{
	
	if(!(vu && lv && ts ))
		return false;

	if(lv->accepts_type(*ts))
	{
		return vu->set_lvar_type(lv, *ts);
	}
	else
	{
		msg("type not accepted\n");
		return false;
	}
}

static bool idaapi cast_assign(void *ud)
{
	vdui_t &vu = *(vdui_t *)ud;
	tinfo_t ts;
	if(!is_cast_assign(ud, &ts))
		return false;

	lvar_t * lv = vu.item.get_lvar();
	return set_lvar_type(&vu, lv, &ts);
}

static bool idaapi vt_call_cast(void *ud)
{
	vdui_t &vu = *(vdui_t *)ud;
	typestring ts;
	return is_vt_call_cast(ud, true);	
}

//-----------------------------------------------------
static bool idaapi possible_structs_for_one_offset(void *ud)
{
	vdui_t &vu = *(vdui_t *)ud;
	vu.get_current_item(USE_KEYBOARD);
	if (!vu.item.is_citem())
		return false;
	cexpr_t *e = vu.item.e;	
	if (e->op != cot_num)
		return false;	
	asize_t offset = e->numval();

	matched_structs_with_offsets m;
	m.offset = offset;
	m.idcka.clear();

	for( uval_t idx = get_first_struc_idx(); idx!=BADNODE; idx=get_next_struc_idx(idx) )
	{
		tid_t id = get_struc_by_idx(idx);
		struc_t * struc = get_struc(id);
		if(!struc)
			continue;
		if (is_union(id))
			continue;

		if( struct_has_member(struc, offset) )
		{		
			m.idcka.push_back(id);
		}
	}
	int choosed = m.choose("[Hexrays-Tools] structs with offset");//choose((void *)&m, 40, matched_structs_sizer, matched_structs_get_type_line, "possible types");

	if ( choosed > 0 )
	{
		qstring tmpname;
		get_struc_name(&tmpname, m.idcka[choosed-1]);
		//typestring ts = create_typedef(name);
		//var->set_final_lvar_type(make_pointer(ts));
		//var->set_lvar_type(make_pointer(ts));
		//var->force_lvar_type(make_pointer(ts));
		//var->set_user_type();
		//vu.refresh_view(false);
		//msg("struct: %s\n", name);
		open_structs_window(m.idcka[choosed-1], offset);
	}
	return true; // done
}

bool structs_with_this_size(asize_t size)
{
	matched_structs m;
	m.idcka.clear();

	for( uval_t idx = get_first_struc_idx(); idx!=BADNODE; idx=get_next_struc_idx(idx) )
	{
		tid_t id = get_struc_by_idx(idx);
		struc_t * struc = get_struc(id);
		if(!struc)
			continue;

		if (is_union(id))
			continue;

		if (get_struc_size(struc) == size)
		{		
			m.idcka.push_back(id);
		}
	}
	//TODO: use another functions
	int choosed = m.choose("[Hexrays-Tools] structures of this size");//choose((void *)&m, 40, matched_structs_sizer, matched_structs_get_type_line, "possible types");
	qstring name;
	if ( choosed > 0 )
	{
		get_struc_name(&name, m.idcka[choosed-1]);
		open_structs_window(m.idcka[choosed-1], 0);
	}
	return true; // done
}

//-----------------------------------------------------
static bool idaapi structs_with_this_size(void *ud)
{
	vdui_t &vu = *(vdui_t *)ud;
	vu.get_current_item(USE_KEYBOARD);
	if (!vu.item.is_citem())
		return false;
	cexpr_t *e = vu.item.e;	
	if (e->op != cot_num)
		return false;
	asize_t size = e->numval();

	return structs_with_this_size(size);
}




bool idaapi show_struct_graph(void *ud)
{
	fill_graphreferences();
	run_graph(0);
	return true;
}

void show_function_with_this_struct_tid_as_parameter(tid_t goal)
{
	function_list fl;
	if (goal == BADNODE)
	{
		msg("[Hexrays-Tools] show_function_with_this_struct_tid_as_parameter, bad goal tid\n");
		return;
	}
	show_wait_box("Scanning functions…");
	for(size_t i=0; i<get_func_qty(); i++)
	{
		if(wasBreak())
			break;

		func_t * func = getn_func(i);		
		showAddr(func->startEA);
		qtype type;
		qtype fields;
		if(!get_tinfo(func->startEA, &type, &fields))
		{
			if(guess_func_tinfo(func,&type, &fields)!=GUESS_FUNC_OK)
				continue;
		}	

		func_type_info_t fti;
		if(!build_funcarg_info(idati, type.c_str(), fields.c_str(), &fti, 0))
			continue;

		for(unsigned int j=0; j<fti.size(); j++)
		{
			if (get_struc_from_typestring(fti[j].type) == goal)
			{
				fl.functions.push_back(func->startEA);
				continue;
			}		
		}		
	}
	hide_wait_box();
	char name[MAXNAMESIZE];
	qstring tmpname;
	get_struc_name(&tmpname, goal);
	qstrncpy(name, tmpname.c_str(), MAXNAMESIZE);
	fl.choose(name);
	fl.open_pseudocode();
}

static bool create_delphi_class(ea_t ea, bool force = true)
{
	if (ea==BADADDR)
		return false;

	if (!isEnabled(ea))
		return false;

	ea_t vt_offset = get_full_long(ea);
	ea_t string_ea = get_full_long(ea+8*4);
	ea_t parent_ea = get_full_long(ea+10*4);
	ea_t types_ea = get_full_long(ea+5*4);
	int struct_size = get_full_long(ea+9*4);

	do_data_ex(ea, offflag() | dwrdflag(), 4, BADADDR);
	do_data_ex(ea+8*4, offflag() | dwrdflag(), 4, BADADDR);
	do_data_ex(ea+10*4, offflag() | dwrdflag(), 4, BADADDR);
	do_data_ex(ea+9*4, dwrdflag(), 4, BADADDR);
	do_data_ex(ea+5*4, offflag() | dwrdflag(), 4, BADADDR);

	//msg("parent ea: %08X", parent_ea);
	if(!force)
	{
		if (has_user_name(getFlags(ea)))
		{
			//return false;		
		}
	}
	if (parent_ea)
	{
		create_delphi_class(parent_ea, false);	
	}
	

	char name[MAXSTR];
	char name_vt[MAXSTR];
	memset(name, 0, sizeof(name));
	memset(name_vt, 0, sizeof(name_vt));
	size_t used_sz;
	//size_t len = get_max_ascii_length(string_ea, ASCSTR_PASCAL, ALOPT_IGNHEADS);
	size_t len = get_byte(string_ea)+1;	
	get_ascii_contents2(string_ea, len, ASCSTR_PASCAL, name, sizeof(name), &used_sz);
	if (used_sz>50 || used_sz == 0)
		return false;
	do_unknown_range(string_ea, len, DOUNK_SIMPLE );
	if(!make_ascii_string(string_ea, used_sz+1, ASCSTR_PASCAL))
	{
		msg("[Hexrays-Tools] could not make ascii string\n");	
	}
	qstrncat(name, "_", sizeof(name));
	set_name(ea, name);
	name[qstrlen(name)-1]=0;
	qstrncpy(name_vt, name, sizeof(name_vt));
	qstrncat(name_vt, "_VT", sizeof(name_vt));

	tid_t id = get_struc_id(name);
	if (id == BADADDR)
		id = add_struc(BADADDR, name);
	
	assert(id != BADADDR);
	/*if (id == BADADDR)
		return BADNODE;*/		
	
	msg("%08X: struct %s created\n", ea, name);
	struc_t * struc = get_struc(id);
	if ( (vt_offset != string_ea) && (isEnabled(get_full_long(vt_offset))))
	{
		create_VT_struc( vt_offset, name_vt );	
		set_vt_type(struc, name_vt);
		qstrncat(name_vt, "_", sizeof(name_vt));
		set_name(vt_offset, name_vt);
		//add_struc_member(struc, "VT", 0, dwrdflag(), NULL, 4);
	}

	add_struc_member(struc, "end", struct_size-1, byteflag(), NULL, 1);

	if (types_ea)
	{
		int members_cnt = get_word(types_ea);
		if (members_cnt>0)
		{
			ea_t types_array = get_long(types_ea+2);
			ea_t current = types_ea + 6;
			int types_cnt = 0;
			if (types_array)
			{
				types_cnt =  get_word(types_array);
				{
					char class_types[MAXSTR]= {0};
					qstrncpy(class_types, name, sizeof(class_types));
					qstrncat(class_types, "_types",sizeof(class_types));
					set_name( types_array, class_types );
					//do_data_ex(types_array, offflag() | dwrdflag(), 4, BADADDR);
				}

				ea_t current_type = types_array+2;
				for(int i=0; i<types_cnt; i++)
				{
					ea_t type_ea = get_long(current_type);
					do_data_ex(current_type, offflag() | dwrdflag(), 4, BADADDR);
					create_delphi_class(type_ea, false);
					current_type+=4;
				}

				{
					char class_members[MAXSTR]= {0};
					qstrncpy(class_members, name, sizeof(class_members));
					qstrncat(class_members, "_members",sizeof(class_members));
					set_name( types_ea, class_members );
					//doDwrd(types_ea, 4);
					
					do_data_ex(types_ea+2, offflag() | dwrdflag(), 4, BADADDR);
				}

				for(int i=0; i<members_cnt; i++)
				{
					char m_name[MAXSTR];
					memset(m_name, 0, sizeof(m_name));
					int m_off = get_word(current);
					current+=2;
					int m_something = get_word(current);
					current+=2;
					int m_type =  get_word(current);
					current+=2;
					int str_len = get_max_ascii_length(current, ASCSTR_PASCAL, ALOPT_IGNHEADS);
					get_ascii_contents2(current, str_len, ASCSTR_PASCAL, m_name, sizeof(m_name));
					current += str_len;
					add_struc_member(struc, m_name, m_off, dwrdflag(), NULL, 4);				
					//todo: apply data types
				}
			}
		}	
	}
	return true;
}
//-----------------------------------------------

static bool idaapi create_delphi_class_callback(void *ud)
{
	ea_t ea = get_screen_ea();
	return create_delphi_class(ea);
}


bool jump_custom_viewer(TCustomControl *custom_viewer, int line, int x, int y)
{
	place_t * place;
	place =  get_custom_viewer_place(custom_viewer, false, NULL, NULL);
	simpleline_place_t * newplace = (simpleline_place_t*)place->clone();
	newplace->n = line;
	return jumpto(custom_viewer, newplace, x, y);
}


static bool idaapi testjump(void *ud)
{
	vdui_t &vu = *(vdui_t *)ud;
	jump_custom_viewer(vu.ct, 3, 10, 0);	
	return true;	
}



bool name_for_struct_crawler(vdui_t &vu, qstring & name, lvar_t ** out_var, ea_t * ea)
{
	lvar_t * lvar =  vu.item.get_lvar();
	if(lvar)
	{
		name = lvar->name;
		if(out_var)
			*out_var = lvar;
		if (ea)
			*ea = BADADDR;
		return true;
	}
	if (vu.item.citype == VDI_EXPR && vu.item.e && vu.item.e->op == cot_obj)
	{	
		char buff[MAXSTR];
		if (!get_name(BADADDR, vu.item.e->obj_ea, buff, sizeof(buff)))
			return false;
		name = buff;
		if (out_var)
			*out_var = 0;
		if (ea)
			*ea = vu.item.e->obj_ea;
		return true;
	}
	return false;
}
//--------------------------------------------------------------------------
// This callback handles various hexrays events.
static int idaapi callback(void *, hexrays_event_t event, va_list va)
{
	switch ( event )
	{

	case hxe_text_ready:
		//TODO: color items from structure builder
		{
			/*vdui_t &vu = *va_arg(va, vdui_t *);
			vu.sv.size();
			history_item_t hist;
			hist.ea = vu.cfunc->entry_ea;
			hist.lnnum = 2;
			hist.x = 2;
			hist.y = 4;
			vu.history.push(hist);*/
			//todo
			//vu.pop_current_location();
		}
		break;
	case hxe_right_click:
		{
			vdui_t &vu = *va_arg(va, vdui_t *);
							
			if (can_be_converted_to_ptr2(vu, vu.item, nullptr) )
			{
				add_custom_viewer_popup_item(vu.ct, "[Hexrays-Tools] scan variable", "S", var_testing, &vu);
				add_custom_viewer_popup_item(vu.ct, "[Hexrays-Tools] open structure builder", "", var_testing1_5, &vu);
				add_custom_viewer_popup_item(vu.ct, "[Hexrays-Tools] finalize structure", "", var_testing2, &vu);
			}

			if (vu.item.citype == VDI_FUNC)
			{
				add_custom_viewer_popup_item(vu.ct, "[Hexrays-Tools] Convert to __usercall", "", convert_to_usercall, &vu);

				//if (lvar->is_result_var())
				{					
					add_custom_viewer_popup_item(vu.ct, "[Hexrays-Tools] Remove return type", "R", remove_rettype, &vu);
				}				
			}

			if (vu.item.citype == VDI_LVAR)
			{
				lvar_t * lvar =  vu.item.get_lvar();
				if (lvar->is_arg_var())
				{
					add_custom_viewer_popup_item(vu.ct, "[Hexrays-Tools] Remove this argument", "A", remove_argument, &vu);
				}
			}

			add_custom_viewer_popup_item(vu.ct, "[Hexrays-Tools] recognize shape", "T", traverse_, &vu);
			
			add_custom_viewer_popup_item(vu.ct, "[Hexrays-Tools] Jump to struct or virtual call", "J", jump_to_struct, &vu);

			
			if (is_virtual_call(&vu))
			{
				add_custom_viewer_popup_item(vu.ct, "[Hexrays-Tools] Add virtual call cref", "", add_VT_cref, &vu);
				add_custom_viewer_popup_item(vu.ct, "[Hexrays-Tools] Show VT", "", show_VT, &vu);
			}

			if (can_be_recast(&vu))
			{
				add_custom_viewer_popup_item(vu.ct, "[Hexrays-Tools] Use NEGATIVE_CAST here", "D", change_negative_cast_callback, &vu);
			}

			if (is_cast_assign(&vu, NULL))
			{
				add_custom_viewer_popup_item(vu.ct, "[Hexrays-Tools] recast item", "R", cast_assign, &vu);
			}

			if (is_vt_call_cast(&vu, NULL))
			{
				add_custom_viewer_popup_item(vu.ct, "[Hexrays-Tools] recast item", "R", vt_call_cast, &vu);
			}

			if (is_number(&vu))
			{
				add_custom_viewer_popup_item(vu.ct, "[Hexrays-Tools] Which structs have this offset?", "", possible_structs_for_one_offset, &vu);
				add_custom_viewer_popup_item(vu.ct, "[Hexrays-Tools] Which structs have this size?", "", structs_with_this_size, &vu);
			}

		}
		break;
	case hxe_keyboard:
		{
			vdui_t &vu = *va_arg(va, vdui_t *);
			int key_code  = va_arg(va, int);
			int shift_state = va_arg(va, int);
			if (shift_state == 0)
			{
				switch (key_code)
				{
				case 'A':
					{
						vu.get_current_item(USE_KEYBOARD);
						if (vu.item.citype == VDI_LVAR)
						{
							lvar_t * lvar =  vu.item.get_lvar();
							if (lvar && lvar->is_arg_var())
							{					
								return remove_argument(&vu);
							}
						}
					}
					break;
				case 'S':
					{
						vu.get_current_item(USE_KEYBOARD);
						//int idx;
						//if( idx_for_struct_crawler(vu, idx) )
						{
							if ( can_be_converted_to_ptr2(vu, vu.item, nullptr) )
							{
								return var_testing(&vu);
							}
						}
					}
					break;
				case 'T':
					{
						vu.get_current_item(USE_KEYBOARD);
						traverse_(&vu);
						return 1;// Should return: 1 if the event has been handled
					}
					break;
				case 'J':
					{
						vu.get_current_item(USE_KEYBOARD);
						if (jump_to_VT(&vu))
							return 1;// Should return: 1 if the event has been handled
					}

				case 'R':
					{
						vu.get_current_item(USE_KEYBOARD);
						switch(vu.item.citype)
						{
						case VDI_FUNC:
							remove_rettype(&vu);
							break;
						case VDI_EXPR:
							if (is_cast_assign(&vu, NULL))
							{
								cast_assign(&vu);
								return 1;
							}
							if (is_vt_call_cast(&vu, false))
							{
								vt_call_cast(&vu);
								return 1;
							}
						break;
						}
					}
					break;
				}
			}
		}
		break;
	case hxe_double_click:
		{
			vdui_t &vu = *va_arg(va, vdui_t *);
			vu.get_current_item(USE_MOUSE);
			if (jump_to_VT(&vu))
				return 1;// Should return: 1 if the event has been handled
		}
		break;

	case hxe_maturity:
		{
			cfunc_t *cfunc = va_arg(va, cfunc_t *);
			ctree_maturity_t new_maturity = va_argi(va, ctree_maturity_t);
			if ( new_maturity == CMAT_BUILT ) // ctree is ready
			{
				convert_negative_offset_casts(cfunc);
			}
			if ( new_maturity == CMAT_FINAL )
			{
				convert_zeroes(cfunc);
			}
		}
		break;

	}
	return 0;
}


//-----------
static int idaapi ui_callback(void *user_data, int notification_code, va_list va)
{
	if ( notification_code == ui_tform_visible )
	{
		TForm * form  = va_arg(va, TForm *);
		HWND hwnd  = va_arg(va, HWND);
		if( form == find_tform("Structures") )
		{
			Controls::TCustomControl * cc  = get_current_viewer();
			static bool already_installed = false;
			if(!already_installed)
			{
				add_structures_popup_items(cc);
				if (!is_idaq())
					already_installed = true;
			}
		}

	}
	return 0;
}


static int idaapi idb_callback(void *user_data, int notification_code, va_list va)
{

#define CASE(x) case x: do{/*msg("%s\n", #x); */goto dirty;}while(0);
	switch (notification_code)
	{
		//CASE(idb_event::struc_created)
		//CASE(idb_event::struc_deleted)
		//CASE(idb_event::struc_expanded)
		CASE(idb_event::struc_member_created)
			CASE(idb_event::struc_member_deleted)
			CASE(idb_event::struc_member_changed)
dirty:
		struc_t * sptr = va_arg(va, struc_t *);
		if( sptr && ((sptr->props & SF_FRAME)  == 0) )
		{
			//msg("dirty set\n", sptr);
			//if (sptr)
			set_cache_dirty();
		}
		else
		{
			/*if(sptr)
			msg("changed frame\n");		
			*/
		}
		break;
	}
#undef CASE


	return 0;
}


//--------------------------------------------------------------------------
// Initialize the plugin.
int idaapi init(void)
{
	if(inited)
		return PLUGIN_KEEP;

	if ( !init_hexrays_plugin() )
	{		
		return PLUGIN_SKIP; // no decompiler
	}

	install_hexrays_callback(callback, NULL);
	const char *hxver = get_hexrays_version();
	msg("[Hexrays-Tools] Hex-rays version %s has been detected, %s ready to use\n", hxver, PLUGIN.wanted_name);
	hook_to_notification_point(HT_UI, ui_callback, NULL);	
	hook_to_notification_point(HT_IDB, idb_callback, NULL);
	inited = true;
	add_menu_item("Edit/Structs/Create struct from data...", "[Hexrays-Tools] Create Delphi class", "", SETMENU_INS, create_delphi_class_callback, NULL);
	add_menu_item("View/Graphs/Flow Chart", "[Hexrays-Tools] Show structures graph", "", SETMENU_INS, show_struct_graph, NULL);
	add_menu_item("View/Open subviews/Problems", "[Hexrays-Tools] Classes", "", SETMENU_APP, show_classes_view, NULL);	
	

	register_idc_functions();

	classes_init();
#if IDA_SDK_VERSION >= 620
	addon_info_t addon;
	addon.id = "mila.bohacek.tools";
	addon.name = "Collection of useful tools";
	addon.producer = "Milan Bohacek";
	addon.url = "nope";
	addon.version = "0.1";
	register_addon(&addon);	
#endif

	return PLUGIN_KEEP;
}

//--------------------------------------------------------------------------
void idaapi term(void)
{
	if ( inited )
	{
		// clean up
		del_menu_item("View/Graphs/[Hexrays-Tools] Show structures graph");
		del_menu_item("View/Open subviews/[Hexrays-Tools] Classes");
		del_menu_item("Edit/Structs/[Hexrays-Tools] Create Delphi class");
		remove_hexrays_callback(callback, NULL);
		
		unhook_from_notification_point(HT_UI, ui_callback, NULL);
		unhook_from_notification_point(HT_IDB, idb_callback, NULL);
		unregister_idc_functions();		
		//ui_new_custom_viewer
		classes_term();
		term_hexrays_plugin();
		inited = false;
	}
}

//--------------------------------------------------------------------------
void idaapi run(int)
{
	// should not be called because of PLUGIN_HIDE
}

//--------------------------------------------------------------------------
static char comment[] = "\n[Hexrays-Tools] Milan's useful functions plugin for Hex-Rays decompiler";

//--------------------------------------------------------------------------
//
//      PLUGIN DESCRIPTION BLOCK
//
//--------------------------------------------------------------------------
plugin_t PLUGIN =
{
	IDP_INTERFACE_VERSION,
	PLUGIN_HIDE,          // plugin flags
	init,                 // initialize
	term,                 // terminate. this pointer may be NULL.
	run,                  // invoke plugin
	comment,              // long comment about the plugin
	// it could appear in the status line
	// or as a hint
	"",                   // multiline help about the plugin
	"\n[Hexrays-Tools] Milan's hexrays tools collection", // the preferred short name of the plugin
	""                    // the preferred hotkey to run the plugin
};
