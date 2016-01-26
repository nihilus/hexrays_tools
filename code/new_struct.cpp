#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <windows.h>
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

#define ida_local

//typedef strtype_info_v2_t

#include "ripped.h"


extern tid_t create_VT_struc(ea_t VT_ea, char * name, uval_t idx, unsigned int * vt_len  = NULL );

tid_t is_vt(ea_t ea, unsigned int * vt_len = NULL)
{
	//1. at ea is struct which we already created

	//2. ea looks like vftable
	return create_VT_struc(ea, NULL, -1, vt_len);
}




//---------------------------------------------------------------------------------

struct ida_local ptr_checker_t : public ctree_parentee_t
{
	bool need_field_info;
	field_info_t * fields;
	std::set<int> idxs;

	std::map<int, int> offsets;//idx -> offset

	bool is_our(int idx)
	{		
		return idxs.find(idx)!=idxs.end();	
	}

	bool collect_scanned_vars(cfunc_t *cfunc)
	{
		if(!cfunc)
			return false;
		lvars_t * lvars = cfunc->get_lvars();
		

		std::for_each(idxs.begin(), idxs.end(), [&](int p)
		{
			//TODO: deal with global pointers
			if (p<lvars->size() && p >= 0 )
			{
				scanned_variable_t sv;
				sv.first = offset_for_var_idx(p);
				
				sv.second = (*lvars)[p];
				fields->scanned_variables[cfunc->entry_ea].add_unique(sv);
			}
		});
		return true;
	}

	//var with index idx is ptr to struct+returned_value
	int offset_for_var_idx(int idx)
	{
		if(!fields)
			return 0;
		std::map<int, int>::iterator i = offsets.find(idx);
		if (i == offsets.end())
		{
			return fields->current_offset;
		}
		else
		{
			return i->second;
		}
	}

	typerecord_t& add_type(bool is_ptr, uint64 &delta, int cur_var_offset, int i, typerecord_t& type, cexpr_t * e, bool is_array)
	{
		if ( is_ptr )
		{
			if ( type.type.is_void() ) // all we have is void*
				type.type = t_char;
			scan_info_v2_t & sif  = (*fields)[uval_t(delta) + cur_var_offset ];
			sif.types.add_unique(type);
			sif.is_array |= is_array;
			fields->update_max_offset(cur_var_offset, uval_t(delta) + cur_var_offset);
		}
		else
		{
			scan_info_v2_t &sif  = (*fields)[uval_t(delta) + cur_var_offset];
			fields->update_max_offset(cur_var_offset, uval_t(delta) + cur_var_offset);
			typerecord_t tr;
			tr.type = t_byte;
			sif.types.add_unique(tr);
			sif.is_array |= is_array;
			if (need_field_info && parents[i]->op == cot_asg )
			{
				cexpr_t * ex = (cexpr_t *)parents[i];
				int new_var_idx = ex->x->v.idx;
				if ( ex->x->op == cot_var  && !is_our(new_var_idx))
				{
					//if(ex->y->type == ex->x->type)
					{
						idxs.insert(new_var_idx);
						offsets[new_var_idx] = cur_var_offset + uval_t(delta);
						msg("[Hexrays-Tools] scanning also var %d + %d\n", ex->x->v.idx, uval_t(delta) );
					}
				}
			}

			if (parents[i]->op == cot_call && need_field_info )
			{
				cexpr_t * call = (cexpr_t *)parents[i];
				ea_t fncea = BADADDR;
				if( call->x->op == cot_obj )
				{
					ea_t fncea_ = call->x->obj_ea;
					if (isFunc(getFlags(fncea_)))
					{
						fncea = fncea_;
					}
				}
				if( fncea != BADADDR )
				{
					fields->function_adjustments[fncea] = uval_t(delta) + cur_var_offset;
					int idx;
					if (parents.size() == i+1)
					{
						idx = get_idx_of(call->a, (carg_t*)e);
					}
					else
					{
						idx = get_idx_of(call->a, (carg_t*)parents[i+1]);						
					}
					argument_t a;
					a.arg_num  = idx;
					a.arg_cnt = call->a->size();
					fields->argument_numbers[fncea] = a;
				}
			}
		}
		return type;
	}

	void handle_vtables(bool &is_ptr, uint64 &delta, int i, typestring & t)
	{		
		{
			int j = i;
			while ( parents[j]->op == cot_cast ){j--;}
			if (parents[j]->op == cot_ptr && parents[j-1]->op == cot_asg )
			{
				cexpr_t * obj = NULL;
				cexpr_t * asg = ((cexpr_t*)parents[j-1]);
				if (asg->y->op == cot_ref)
				{
					if (asg->y->x->op == cot_obj)
					{
						obj = (cexpr_t*)asg->y->x;
					}
				}
				if (asg->y->op == cot_obj)
				{
					obj = (cexpr_t*)asg->y;
				}

				if ( obj && isEnabled(obj->obj_ea) )
				{
					unsigned int vt_len = 0;

					tid_t tid = is_vt(obj->obj_ea, &vt_len);
					if(tid!=BADNODE)
					{
						qstring name;
						get_struc_name(&name, tid);
						t = make_pointer(create_typedef(name.c_str()));
						for(unsigned int k = 0; k<vt_len; k++)
						{
							ea_t fncea = get_long(obj->obj_ea + 4*k );
							if(isFunc(getFlags(fncea)))
							{
								//filter out nullsubs
								func_t * f = get_func(fncea);
								if (f->empty())
									continue;
								//TODO: speculative, it could be jmp xyz
								/*if (f->size() < 10)
									continue;*/

								if ( need_field_info && fields )
								{
									fields->function_adjustments[fncea] = uval_t(delta) + fields->current_offset;
								}
							}
						}
						is_ptr = true;
					}
				}
			}
		}
	}
	int idaapi visit_expr(cexpr_t *e)
	{
		scan_info_v2_t si;
		bool is_ptr = false;


		if(need_field_info && fields)
		{
			if ( e->op == cot_asg )
			{
				int new_var_idx = e->x->v.idx;
				int old_var_idx = e->y->v.idx;

				if ( e->x->op == cot_var  && !is_our(new_var_idx))
				{
					if ( e->y->op == cot_var  && is_our(old_var_idx))
					{
						//same type ?
						if(e->y->type == e->x->type)
						{
							idxs.insert(e->x->v.idx);
							offsets[new_var_idx] = offset_for_var_idx(old_var_idx);
							msg("[Hexrays-Tools] scanning also var %d\n", e->x->v.idx);
						}
					}
				}
			}
		}

		bool is_lvar = (e->op == cot_var && is_our(e->v.idx));
		bool is_glob_obj_ptr = (e->op == cot_obj && is_our(e->obj_ea));

		int index = 0;
		if (is_lvar)
		{
			index = e->v.idx;
		}
		if (is_glob_obj_ptr)
		{
			index = e->obj_ea;
		}		
		if ( is_lvar || is_glob_obj_ptr )
		{ // found our var. are we inside a pointer expression?
			bool is_array = false;
			uint64 delta = 0;			
			int cur_var_offset = offset_for_var_idx(index);
			
			bool delta_defined = true;
			int i = parents.size() - 1;
			if ( parents[i]->op == cot_add ) // possible delta
			{
				cexpr_t *d = ((cexpr_t*)parents[i])->theother(e);
				delta_defined = d->get_const_value(&delta);
				i--;
				if ( parents[i]->op == cot_add/* || parents[i]->op == cot_sub*/ )
				{					
					cexpr_t *d = ((cexpr_t*)parents[i])->theother((cexpr_t*)parents[i+1]);
					delta_defined = d->get_const_value(&delta);
					is_array = true;
					i--;
				}
			}
			/*
			// more additions or subtractions are allowed but spoil the delta
			while ( parents[i]->op == cot_add || parents[i]->op == cot_sub )
			{
			i--;
			delta_defined = false;
			}
			*/
			typestring t;
			handle_vtables(is_ptr, delta, i, t);
			if ( delta_defined )
			{
				// skip casts
				typerecord_t type;
				if(t.empty())
				{
					while ( parents[i]->op == cot_cast )
					{
						type.type = ((cexpr_t*)parents[i])->type;
						i--;
					}
					is_ptr = type.type.is_ptr();
					type.type = remove_pointer(type.type);
				}
				else
				{
					type.type = t;
					is_ptr = true;
				}
				if ( !need_field_info )
					return 1; // yes, can be converted to ptr
				if (!fields)
					return 0;
				add_type(is_ptr, delta, cur_var_offset, i, type, e, is_array);
			}
		}
		return 0;
	}

	ptr_checker_t(int i, bool nfi, field_info_t * fi) : need_field_info(nfi), fields(fi)
	{
		idxs.insert(i);
	}
};

/*
bool can_be_converted_to_ptr(cfunc_t *cfunc, int varidx, strtype_info_v2_t *strinfo)
{
lvars_t * lvars = cfunc->get_lvars();
if(!lvars)
return false;
lvar_t & lv = (*lvars)[varidx] ;

if ( lv.type().is_ptr() )
return false; // already ptr
if ( lv.width != inf.cc.size_i )
return false; // not quite correct but ok for now

field_info_t fields;
ptr_checker_t pc(varidx, strinfo != NULL, &fields);
if ( !pc.apply_to((citem_t*)&cfunc->body, NULL) && fields.empty() )
return false;

if ( strinfo != NULL )
{
int minalign = inf.cc.defalign;
uval_t off = 0;
strtype_info_v2_t &si = *strinfo;
for ( field_info_t::iterator p=fields.begin(); p != fields.end(); ++p )
{
if ( p->first < off )
continue; // skip overlapping fields
meminfo_v2_t &mi = si.push_back();
mi.offset = p->first;
while ( (mi.offset & (minalign-1)) != 0 )
minalign >>= 1;
const typestring & t = p->second.types.get_first_enabled();
mi.type = t;
mi.name = create_field_name.call(&mi.type, mi.offset); //170C8AA0 - int create_field_name(qstring *a1, typestring *a2, int offset)
mi.size = t.size();
//todo

mi.fields = dummy_plist_for.call(mi.type.u_str());
off = mi.offset + mi.size;
}
si.basetype = BTF_STRUCT;
si.N = (si.size() << 3);
if ( minalign < inf.cc.defalign )
si.N |= log2ceil(minalign)+1;
}
return true;
}*/

//-------------------------------------------------------------------------
// this functions assumes that the structure fields come one after another without gaps
void strtype_info_v2_t ::build_struct_type( typestring &outtype, typestring &outfields) const
{
	const strtype_info_v2_t &strinfo = *this;
	//QASSERT(50641, (strinfo.N >> 3) == strinfo.size());
	outtype.clear();
	outfields.clear();
	outtype.before(strinfo.basetype);
	outtype += make_dt/*.call*/(strinfo.N);

	int offset = 0;
	for (unsigned int i=0; i < strinfo.size(); i++ )
	{
		type_t buf[MAXSTR];
		memset(buf, 0, sizeof(buf));
		const meminfo_v2_t &mi = strinfo[i];
		make_dtname(buf, sizeof(buf), mi.name.c_str());
		/*char buff[200] = {0};
		{
			//char buff[200] = {0};
			//print_type_to_one_line(buff, sizeof(buff), idati, mi.type.c_str());
			//mi.type.print(buff, sizeof(buff));
		}*/
		outtype   += mi.type;
		outfields += buf;
		outfields += mi.fields;
		//QASSERT(50642, mi.offset == offset);
		offset = mi.offset + mi.size;
	}
}

//-------------------------------------------------------------------------
int strtype_info_v2_t::add_gaps()
{	
	strtype_info_v2_t &strinfo = *this;
	int cnt = 0;
	int offset = 0;
	for (unsigned int i=0; i < strinfo.size(); i++ )
	{
		const meminfo_v2_t &mi = strinfo[i];		
		if ( mi.offset != offset )
		{
			sval_t gapsize = mi.offset - offset;
			QASSERT(0, gapsize > 0);
			meminfo_v2_t gap;
			gap.size = gapsize;
			gap.offset = offset;
			gap.type = make_array(t_byte.c_str(), gapsize);
			gap.name.sprnt("f%X", offset);
			strinfo.insert(strinfo.begin()+i, gap);
			cnt++;
			offset += gapsize;
		}
		else
		{
			offset = mi.offset + mi.size;
		}
	}
	strinfo.N = (strinfo.N & 7) | (strinfo.size() << 3);
	return cnt;
}


//-------------------------------------------------------------------------
bool strtype_info_v2_t::build_udt_type(typestring &restype, typestring &resfields)
{
	if ( empty() )
		return false;
	add_gaps();
	build_struct_type(restype, resfields);
	return true;
}

static bool idx_for_struct_crawler(ctree_item_t & item, vdui_t &vu, int & idx, bool & is_global)
{
	lvar_t * lvar =  item.get_lvar();
	idx = -1;
	if(lvar)
	{		
		idx = get_idx_of_lvar(vu, lvar);
		is_global = false;
		return true;
	}
	if (item.citype == VDI_EXPR && item.e && item.e->op == cot_obj)
	{
		citem_t * parent = vu.cfunc->body.find_parent_of(item.e);
		if (!parent)
			return false;
		if (parent->op == cot_call && ((cexpr_t*)parent)->x == item.e )
			return false;		
		idx = vu.item.e->obj_ea;
		is_global = true;
		return true;
	}
	return false;
}

bool can_be_converted_to_ptr2(vdui_t &vu, ctree_item_t & item, field_info_t * fields)
{
	cfunc_t *cfunc = vu.cfunc;
	int varidx;
	bool is_global;
	if (!idx_for_struct_crawler(item, vu, varidx, is_global))
		return false;

	lvars_t * lvars = cfunc->get_lvars();
	if(!lvars)
		return false;

	if (!is_global)
	{
		lvar_t & lv = (*lvars)[varidx] ;
		if ( lv.type().is_ptr() )
			return false; // already ptr
		if ( lv.width != inf.cc.size_i )
			return false; // not quite correct but ok for now
	}
	else
	{
		//TODO: check for global symbol at ea varidx
		if (fields)
		{
			fields->global_pointers.add_unique(varidx);
			for(ea_t ea = get_first_dref_to(varidx); ea!=BADADDR; ea = get_next_dref_to(varidx, ea))
			{
				func_t * fnc = get_func(ea);
				if(fnc)
				{
					fields->function_adjustments[fnc->startEA] = 0;
				}
			}
		}
	}

	if (fields)
	{
		function_adjustments_t::iterator i = fields->function_adjustments.find(cfunc->entry_ea) ;
		if ( i !=  fields->function_adjustments.end())
		{
			if (i->second != fields->current_offset)
			{
				int answer = askyn_c(1, "[Hexrays-Tools] Do you want to set master offset to %d ? (instead of %d)", i->second, fields->current_offset);
				if (answer == -1)
					return false;
				if ( answer == 1)
				{
					fields->current_offset = i->second;
				}
				else
				{
					i->second = fields->current_offset;			
				}
			}
		}
		else
		{
			fields->function_adjustments[ cfunc->entry_ea ]  = fields->current_offset;	
		}
		msg("[Hexrays-Tools] scanning varidx: %08X\n", varidx);
	}
	ptr_checker_t pc(varidx, fields != NULL, fields);
	if ( !pc.apply_to((citem_t*)&cfunc->body, NULL) )
		if(!fields)
			return false;
	if(fields)
	{
		pc.collect_scanned_vars(cfunc);
		std::for_each(fields->scanned_variables.begin(),fields->scanned_variables.end(), [](std::pair<scanned_variables_t::key_type, scanned_variables_t::mapped_type> p)
		{
			typedef decltype(p.second) TMP;
			std::for_each(p.second.begin(), p.second.end(), [&](TMP::value_type & x)
			{
				msg("[Hexrays-Tools] ea: 0x%08X  loc: %08X defea: %08X offset: %d\n", p.first, x.second.location, x.second.defea, x.first);
			});		
		});
	}
	if (fields && fields->empty())
		return false;

	return true;
}

bool field_info_t::convert_to_strtype_info(strtype_info_v2_t *strinfo, field_info_t::iterator * bgn, field_info_t::iterator * end_)
{
	if ( strinfo != NULL )
	{

		field_info_t::iterator b = begin();
		field_info_t::iterator e = end();
		int offset_delta = 0;
		if(bgn)
		{
			b = *bgn;
			offset_delta = b->first;
		}
		if(end_)
			e = *end_;


		int minalign = inf.cc.defalign;
		if(minalign==0)
			minalign = 1;
		uval_t off = 0;
		strtype_info_v2_t &si = *strinfo;

		//TODO: check second.nesting_counter;
		for ( field_info_t::iterator p = b; p != e; ++p )
		{
			if ( p->first < off )
				continue; // skip overlapping fields

			typestring t;
			
			if (!p->second.types.get_first_enabled(t))
				continue;//skip completely disabled fields

			meminfo_v2_t &mi = si.push_back();
			mi.offset = p->first - offset_delta;
			while ( (mi.offset & (minalign-1)) != 0 &&  minalign)
				minalign >>= 1;			
			mi.type = t;			
			//TODO: there's a bug in heaxrays, but due to changes in array creation, we're not affected by it
			//t.remove_ptr_or_array();
			if (p->second.is_array)
			{
				auto q = p;
				bool enabled=false;
				//TODO: refactor	++q is this right?	nema to byt q++ ?
				while(!enabled && ++q!=e)
				{					
					typestring tmp;
					enabled = q->second.types.get_first_enabled(tmp);
				};
				if (q!=e)
				{
					mi.type = make_array(mi.type.c_str(), (q->first - p->first)/t.size());
#if IDA_SDK_VERSION >= 620
					t = mi.type;
#endif
				}			
			}
			mi.size = mi.type.size();
#if IDA_SDK_VERSION >= 630
			mi.name = create_field_name.call(&t, mi.offset); //170C8AA0 - int create_field_name(qstring *a1, typestring *a2, int offset)
			mi.fields = dummy_plist_for.call(mi.type.u_str());
#else
			mi.name = create_field_name( t,  mi.offset); //170C8AA0 - int create_field_name(qstring *a1, typestring *a2, int offset)
			mi.fields = dummy_plist_for(mi.type.u_str());
#endif

			off = mi.offset + mi.size + offset_delta;			
		}
		si.basetype = BTF_STRUCT;
		si.N = (si.size() << 3);
		if ( minalign < inf.cc.defalign )
			si.N |= log2ceil(minalign)+1;
	}
	return true;
}

bool structure_from_restype_resfields(qstring &varname, typestring &out_type, typestring &restype, typestring &resfields)
{	
	qstring strucname = "struct_";
	strucname += varname;
	int len = strucname.size()-1;
	for(int i=0;; ++i)
	{
		strucname[len] = 0;
		strucname.resize(len);
		strucname.cat_sprnt("_%d", i);
		if(!get_named_type(idati, strucname.c_str(), NTF_TYPE))
			break;
	}

	qtype type;
	qstring names;
	qstring result;
	qtype fields;
	qstring name;
	ssize_t succ = print_type_to_qstring(&result, 0, 5, 40, 12|3, idati, (type_t *)restype.c_str(), strucname.c_str(), 0, resfields.c_str(), 0);

	//TODO: remove this, it's for debugging only
	/*
	char buff[2000] = {0};
	ssize_t succ1 = print_type_to_one_line(buff, sizeof(buff), idati, (type_t *)restype.c_str());
	*/

	//too bad this is not implemented
	//result = restype.multiprint(resfields.c_str(), strucname.c_str(), 12|3);
	if(!result.size())
	{
		return false;
	}

	//msg("%s\n",  result.c_str());

	char answer[40*MAXSTR];
	char * declaration = (char *)result.c_str();
	while(1)
	{
		declaration = asktext(sizeof(answer), answer, declaration, "[Hexrays-Tools] The following new type will be created", strucname.c_str());
		if(!declaration)
		{
			return false;
		}

		if (!parse_decl(idati, declaration, &name, &type, &fields, 4))
		{
			continue;
		}

		if(set_named_type(idati, name.c_str(), 1, type.c_str(), fields.c_str()))
		{
			import_type(idati, -1, name.c_str());
			break;
		}
		else
		{
			warning("[Hexrays-Tools] Could not create %s, maybe it already exists?", name.c_str());
			continue;
		}
	}
	out_type = create_numbered_type_from_name(name.c_str());
	return true;
}


bool field_info_t::to_type(qstring varname, typestring & out_type, strtype_info_v2_t *sti, field_info_t::iterator * bgn, field_info_t::iterator * end )
{
	strtype_info_v2_t sti2;
	if (!sti)
		sti = &sti2;	

	if(!convert_to_strtype_info(sti, bgn, end))
		return false;
	typestring restype;
	typestring resfields;
	sti->build_udt_type(restype, resfields);
	return structure_from_restype_resfields(varname, out_type, restype, resfields);
}


bool field_info_t::flip_enabled_status(unsigned int idx, unsigned int position)
{
	if(size()==0)
		return false;
	field_info_t::iterator iter =  begin();
	if (iter == end())
		return false;
	if (!safe_advance(iter, end(), idx))
	{
		return false;	
	}
	

	if (iter == end())
		return false;

	scan_info_v2_t & si = iter->second;
	int offset = iter->first;

	if (si.types.size()>=0)
	{
		typevec_t::iterator iter1 = si.types.begin();
		if (!safe_advance(iter1, si.types.end(), position))
			return false;
		
		if (iter1 == si.types.end())
			return false;
		//msg("removing\n");
		//si.types.erase(iter1);
		if(iter1->enabled)
		{
			iter1->enabled = false;
			++si.types.disabled_count;
		}
		else
		{
			iter1->enabled = true;
			--si.types.disabled_count;		
		}
	}
	if (si.types.size() == 0)
	{
		this->erase(iter);
	}
	return true;
}

void field_info_t::clear()
{
	std::map<uval_t, scan_info_v2_t>::clear();
	current_offset = 0;
	function_adjustments.clear();
	visited_functions.clear();	
	argument_numbers.clear();
	global_pointers.clear();
	scanned_variables.clear();
	types_cache.clear();
}

unsigned int field_info_t::types_at_idx_qty(unsigned int idx) const
{
	if (size() == 0)
		return 0;
	field_info_t::const_iterator iter =  begin();
	if(iter == end())
		return 0;
	if (!safe_advance(iter, end(), idx))
	{
		return 0;	
	}
	if (iter == end())
		return 0;
	return (iter->second).types.size();
}


void field_info_t::update_max_offset(unsigned int current, unsigned int max_)
	{
		if (find(current) == end())
		{
			max_adjustments[current] = max_;
		}
		else
		{
			uval_t lastmax = max_adjustments[current];
			max_adjustments[current] = max(lastmax, max_);
		}	
	}