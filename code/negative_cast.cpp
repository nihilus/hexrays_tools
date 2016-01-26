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

#include "choosers.h"
#include "structures.h"
#include "negative_cast.h"
#include "helpers.h"


static bool cache_dirty = false;

void set_cache_dirty()
{
	cache_dirty = true;
}
/*
tid_t get_struc_id_my(const char *name)
{
	tid_t struct_id = get_struc_id(name);
	if (struct_id == BADADDR)
	{
		if(strncmp(name, "struct ", 7)==0)
		{
			struct_id = get_struc_id(name + 7);
		}
	}
	return struct_id;
}
*/

srvec_t graph_references;

intvec_t nonempty;
intvec_t nonempty_back;
int nonempty_cnt;

sbrvecvec_t backreferences;

void fill_nonempty()
{
	int j=0;
	for(unsigned int i=0; i < nonempty.size(); i++)
	{
		if(nonempty[i])
		{
			nonempty[i] = j++;
		}
		else
		{
			nonempty[i] = -1;
		}
	}
	nonempty_back.clear();
	nonempty_back.resize(nonempty.size(), -1);
	for(unsigned int i=0; i < nonempty.size(); i++)
	{
		if(nonempty[i]!=-1)
			nonempty_back[ nonempty[i] ] = i;
	}
	nonempty_cnt = j;
}

/*
void debug_adding(char t, int idx1, int idx2)
{
	char str1[MAXSTR];
	char str2[MAXSTR];
	get_struc_name(get_struc_by_idx(idx1), str1, MAXSTR);
	get_struc_name(get_struc_by_idx(idx2), str2, MAXSTR);	
	msg("%c: adding link from %s (%d) to %s (%d)\n", t, str1, idx1, str2, idx2);
}*/

void graphreference_add(unsigned int idx_from,unsigned  int idx_to, struc_reference_type_t type)
{
	struc_reference_t br;
	br.idx_from = idx_from;
	br.idx_to = idx_to;
	br.type = type;

	if(idx_from>=0 && idx_from < nonempty.size())
		nonempty[idx_from]=1;

	if(idx_to>=0 && idx_to < nonempty.size())
		nonempty[idx_to]=1;

	graph_references.add_unique(br);
	//graph_references.push_back(br);
}

bool fill_graphreferences()
{
	DWORD t1 = GetTickCount();

	graph_references.clear();
	
	nonempty.clear();
	if (get_last_struc_idx() == -1)
		return false;
	nonempty.resize((get_last_struc_idx()+1)*2, 0);
	
	int count = 0;
	for( uval_t idx = get_first_struc_idx(); idx != BADNODE; idx = get_next_struc_idx(idx) )
	{
		tid_t id = get_struc_by_idx(idx);
		struc_t * struc = get_struc(id);
		
		for(unsigned int i = 0; i < struc->memqty; i++ )
		{
			member_t * member = &struc->members[i];//get_member(struc, off);
			if( member  )
			{
				struc_t * child = get_sptr(member);
				
				if (child) 
				{
					if (!is_union(child->id))
					{
						uval_t indx = get_struc_idx(child->id);
						if(indx!=-1)
						{
							graphreference_add(idx, indx, srt_substruct);
						}
					}
				}
								
				if ( isOff( member->flag, OPND_ALL) )
				{
					opinfo_t info;
					if(retrieve_member_info(member, &info))
					{	
						//if(info.ri.type() == REF_OFF32)
						{
							tid_t referenced = info.ri.target;
							if (referenced!=BADNODE)
							{
								uval_t idx_ref = get_struc_idx(info.path.ids[0]);
								if(idx_ref != -1)
								{
									graphreference_add(idx, idx_ref, srt_struct_reference);
								}
							}
						}					
					}
				}

				{
					tinfo_t t;
					if(get_member_type(member, &t))
					{
						tid_t referenced = get_struc_from_typestring(t);
						if (referenced!=BADNODE)
						{
							uval_t idx_ref = get_struc_idx(referenced);
							if(idx_ref != -1)
							{
								graphreference_add(idx, idx_ref, srt_type_reference);
							}
						}					
					}				
				}
			}
			//off = get_struc_next_offset(struc, off);
		}
		count++;
	}
	
	fill_nonempty();
	DWORD t2 = GetTickCount();
	msg("[Hexrays-Tools] %d graphreferences refreshed in %d ms\n", graph_references.size(), t2 - t1);
	cache_dirty = false;
	return true;
}


bool fill_backreferences_();

struct backreferences_thread_t : public exec_request_t
{
	
	int idaapi execute(void)
	{		
		fill_backreferences_();
		return 0;
	}
	
	backreferences_thread_t()
	{
	}
};

backreferences_thread_t bt;

bool fill_backreferences()
{
	execute_sync(bt, MFF_WRITE);
	return 0;
}

bool fill_backreferences_()
{
	DWORD t1 = GetTickCount();
	
	if (get_last_struc_idx() == -1)
	{
		return false;	
	}

	//TODO: better check for freshness
	if(!cache_dirty)
	{
		if (get_last_struc_idx() + 1 == backreferences.size()  )
		{
			return false;
		}
	}

	

	backreferences.clear();
	backreferences.resize( get_last_struc_idx() +1 );
	
	int count = 0;
	for( uval_t idx = get_first_struc_idx(); idx != BADNODE; idx=get_next_struc_idx(idx) )
	{
		tid_t id = get_struc_by_idx(idx);
		struc_t * struc = get_struc(id);

//it turned out, that this is much slower on big databases, than my initial approach
#if 0
		for ( ea_t i = get_first_dref_to(id); i != BADNODE; i = get_next_dref_to(id, i) )
		{
			if (isEnabled(i))
				continue;

			char member_name[MAXNAMESIZE];
			if(!get_member_fullname(i, member_name, MAXNAMESIZE))
				continue;
			struc_t * struc_place;
			member_t * member = get_member_by_fullname(member_name, &struc_place);
			if(!member)
				continue;

			struc_backreference_t br;			
			br.idx = get_struc_idx(struc_place->id);
			br.offset = member->get_soff();
			if (br.idx != BADNODE)
				backreferences[idx].add_unique(br);
		}
#else
		//asize_t off = 0;
		
		for(unsigned int i = 0; i < struc->memqty; i++ )		
		{
			member_t * member = &struc->members[i];//get_member(struc, off);
			if( member  )
			{
				struc_t * child = get_sptr(member);
				
				if (child) 
				{
					if (!is_union(child->id))
					{
						struc_backreference_t br;
						uval_t indx = get_struc_idx(child->id);
						br.idx = idx;
						br.offset = member->get_soff();
						backreferences[indx].add_unique(br);
					}				
				}
			}			
		}
#endif
		//get_member_fullname();
		count++;
	}
	DWORD t2 = GetTickCount();
	bool again;
	int again_counter = 0;
	do
	{
		again = false;
		for (unsigned int i = 0; i <= get_last_struc_idx(); i++)
		{
			//iterate over all structs j such that i as child of j
			for (unsigned int j=0; j < backreferences[i].size(); j++)			
			{
				int intermediate = backreferences[i][j].idx;
				//iterate over all structs k such that j as child of k
				for (unsigned int k=0; k < backreferences[intermediate].size(); k++)
				{					
					struc_backreference_t br;
					br.idx = backreferences[intermediate][k].idx;
					br.offset = backreferences[i][j].offset + backreferences[intermediate][k].offset;
					//br.offset = backreferences[i][j].offset + backreferences[j][k].offset;
					again |= backreferences[i].add_unique(br);			
				}
			}
		}
		again_counter++;
	} while(again);
	DWORD t3 = GetTickCount();
	msg("[Hexrays-Tools] backreferences refreshed in %d ms, expand took %d ms, again counter: %d\n", t3 - t1, t3-t2, again_counter);
	cache_dirty = false;
	return true;
}


static const char HELPERNAME[] = "container_of";//"NEGATIVE_STRUCT_CAST";
/*
class negative_cast_t
{
public:
	ea_t func_ea;
	typestring cast_to;

	negative_cast_t(void)
	{		
		func_ea = BADADDR;
		cast_to = typestring();
	}

	negative_cast_t(ea_t fea, typestring t)
	{
		func_ea = fea;
		cast_to = t;
	}

	DECLARE_COMPARISONS(negative_cast_t)
	
	{
	
		if (func_ea < r.func_ea)
			return -1;

		if (func_ea > r.func_ea)
			return 1;

	
		if (cast_to == r.cast_to)
			return 0;
	
		return -2;
	};
};
*/

class negative_cast_t
{
public:
	typestring cast_from;
	typestring cast_to;
	int offset;

	int mem_ptr_offset;
	bool is_mem_ptr;

	negative_cast_t(void)
	{		
		cast_from = typestring();
		cast_to = typestring();
		int offset = 0;
		is_mem_ptr = false;
		mem_ptr_offset = 0;
	}

	negative_cast_t(typestring from, typestring to, int off)
	{
		cast_from = from;
		cast_to = to;
		offset = off;
	}

	DECLARE_COMPARISONS(negative_cast_t)
	/* compare function guts */
	{
		/* greater/less than by ea */
		if (cast_from < r.cast_from)
			return -1;

		if (cast_from > r.cast_from)
			return 1;

		if (offset != r.offset)
			return offset - r.offset;

		if (cast_to < r.cast_to)
			return -1;

		if (cast_to > r.cast_to)
			return 1;

		if (is_mem_ptr != r.is_mem_ptr)
		{
			if (is_mem_ptr)
				return 1;
			else
				return -1;
		}

		if (mem_ptr_offset < r.mem_ptr_offset)
			return -1;

		if (mem_ptr_offset > r.mem_ptr_offset)
			return 1;
			
		if (mem_ptr_offset == r.mem_ptr_offset)
			return 0;


		/* not equal, return error 
		(nothing cares about the error of course) */
		return -2;
	};
};

typedef qvector<negative_cast_t>  nctvec_t;
static nctvec_t negative_casts;

negative_cast_t * find_cached_cast(typestring from, int offset )
{
	nctvec_t::iterator i = negative_casts.begin();
	while (i != negative_casts.end())
	{
		negative_cast_t & p = (*i);
		if (p.cast_from == from && p.offset == offset)
		{
			return &p;	
		}
		i++;
	}
	return NULL;
}

negative_cast_t * find_cached_cast_2(typestring from, int offset )
{	
	nctvec_t::iterator i = negative_casts.begin();
	while (i != negative_casts.end())
	{
		negative_cast_t & p = (*i);
		if (p.cast_from == from && !p.is_mem_ptr && (p.offset > offset) )
		{
			return &p;	
		}
		i++;
	}
	return NULL;
}


bool find_negative_struct_cast_parent_r(struc_t * struc_candidate, tid_t child_id, asize_t offset)
{
		member_t * member = get_member(struc_candidate, offset);	
		if ( member )
		{
			struc_t * membstr = get_sptr(member);
			if ( membstr )
			{
				if ( member->get_soff() == offset )
				{
					if (membstr->id == child_id)
					{
						return true;
					}
					//because structure can be member of structure at the same offset
					return find_negative_struct_cast_parent_r(membstr, child_id, offset);
				}
				else
				{
					return find_negative_struct_cast_parent_r(membstr, child_id, offset-member->get_soff());
				}
			}
			
		}
		return false;
}

bool find_negative_struct_cast_parent(tid_t child_id, asize_t offset, char * parent_name, int parent_name_len)
{
	for( uval_t idx = get_first_struc_idx(); idx!=BADNODE; idx=get_next_struc_idx(idx) )
	{
		tid_t id = get_struc_by_idx(idx);
		struc_t * struc_candidate = get_struc(id);

		if(get_struc_size(struc_candidate) < offset)
			continue;

		if(!struc_candidate)
			continue;

		if (is_union(id))
			continue;

		if (find_negative_struct_cast_parent_r(struc_candidate, child_id, offset))
		{
			qstring tmpname;
			get_struc_name(&tmpname, id);
			qstrncpy(parent_name, tmpname.c_str(), parent_name_len);
			return true;
		}
	}	
	return false;
}



bool can_be_recast(void * ud)
{
	vdui_t &vu = *(vdui_t *)ud;	
	if ( !vu.item.is_citem() )
		return false;
	cexpr_t *e = vu.item.e;

	if (e->op == cot_helper)
	{
		return qstrcmp(HELPERNAME, e->helper) == 0;
	}

	if (e->op != cot_sub)
		return false;
	

	cexpr_t *var = e->x;
	cexpr_t * cast = 0;

	if (var->op == cot_cast)
	{
		cast = var;
		var = var->x;			
	}

	cexpr_t * num = e->y;

	if(var->op != cot_var || num->op != cot_num)
		return false;
	
	tinfo_t vartype = var->type;
	if (!vartype.is_ptr())
		return false;
	vartype.remove_ptr_or_array();
	if (!vartype.is_struct())
		return false;

	tid_t var_struct_id = get_struc_from_typestring(vartype);
	
	if (var_struct_id == BADADDR)
		return false;

	if (is_union(var_struct_id))
		return false;

	return true;	
}


void convert_test(cexpr_t *e)
{
	if (e->op == cot_sub)
	{
		cexpr_t *var = e->x;
		cexpr_t * cast = 0;

		if (var->op == cot_cast)
		{
			cast = var;
			var = var->x;			
		}

		cexpr_t * num = e->y;
		if(var->op == cot_var && num->op == cot_num)
		{

			//typestring t = e->type;//make_pointer( create_typedef("_DWORD"));
			//cexpr_t * ce = create_helper(t, "testXXX");//make_num(10);//create_helper(t, "pokus");//call_helper( t,  carglist, "pokus" );
			//e->type = make_pointer( create_typedef("_DWORD") );
			bool mem_ptr = false;
			int mem_ptr_offset = 0;


			tinfo_t vartype = var->type;

			int offset = num->numval();
			tinfo_t t;
			negative_cast_t * cache = find_cached_cast(vartype, offset);
			if (cache)
			{
				t = cache->cast_to;
				mem_ptr = cache->is_mem_ptr;
				mem_ptr_offset = cache->mem_ptr_offset;
			}
			else
			{				
				if (!vartype.is_ptr())
					return;				
				tid_t var_struct_id = get_struc_from_typestring(vartype);
				if ( var_struct_id  == BADADDR)
					return;
					
				uval_t struc_idx = get_struc_idx( var_struct_id  );
				bool found = false;

				sbrvec_t & vec = backreferences[struc_idx];
				for(unsigned int i=0; i < vec.size(); i++)
				{
					if( vec[i].offset >= offset)
					{
						tid_t parent_struc_tid = get_struc_by_idx(vec[i].idx);
						if(parent_struc_tid == BADNODE)
							continue;

						struc_t * parent_struc = get_struc(parent_struc_tid);
						if( ! parent_struc )
							continue;

						qstring parent_struc_name;
						get_struc_name(&parent_struc_name, parent_struc_tid);
						
						t = make_pointer(create_numbered_type_from_name(parent_struc_name.c_str()));
						//t = make_pointer( create_typedef(parent_struc_name) );								
						mem_ptr_offset = vec[i].offset - offset;
						mem_ptr = mem_ptr_offset != 0;

						negative_cast_t cast;
						cast.cast_from = vartype;
						cast.cast_to = t;
						cast.offset = offset;
						cast.is_mem_ptr = mem_ptr;
						cast.mem_ptr_offset = mem_ptr_offset;
						negative_casts.push_back(cast);
						
						found = true;
						break;
					}						
				}
				if(!found)
				{
					qstring var_struct_name;
					get_struc_name(&var_struct_name, var_struct_id);

					msg("[Hexrays-Tools] negative casts: !find_negative_struct_cast_parent: %s, offset: %d\n", var_struct_name.c_str(), offset);
					return;
				}
			}			

			carglist_t * arglist = new carglist_t();
			carg_t * arg1 = new carg_t();
			arg1->consume_cexpr(var);
			arglist->push_back(*arg1);
			
			if (1)
			{
				carg_t * arg2 = new carg_t();
				arg2->consume_cexpr(num);
				arglist->push_back(*arg2);
			}
			else			
			{
				carg_t * arg3 = new carg_t();
				char type_text[MAXSTR];
				typestring t2 = t;
				t2.remove_ptr_or_array();	
				t2.print(type_text, MAXSTR);

				cexpr_t * hlpr = create_helper(true, t2, "%s", type_text);
				arg3->consume_cexpr(hlpr);
				arglist->push_back(*arg3);
				
				print_struct_member_name(get_struc( get_struc_from_typestring(t2) ), offset+mem_ptr_offset, type_text, MAXSTR );
				const char * tn = qstrchr(type_text, '.');
				if (!tn)
					tn = type_text;
				else
					tn++;
			
				carg_t * arg4 = new carg_t();												
				arg4->consume_cexpr( create_helper(true, t, "%s", tn));
				arglist->push_back(*arg4);
			}			

			cexpr_t * call = call_helper(t, arglist, "%s", HELPERNAME);
			call->ea = e->ea;
			call->x->ea = e->ea;

			if (mem_ptr)
			{
				cexpr_t * mptr = new cexpr_t(cot_add, call, make_num(mem_ptr_offset) );
				e->replace_by(mptr);				
			}
			else
			{
				e->replace_by(call);
			}
		}
	}
}

//--------------------------------------------------------------------------
void convert_negative_offset_casts(cfunc_t *cfunc)
{
	struct zero_converter_t : public ctree_parentee_t
	{
		zero_converter_t(void) : ctree_parentee_t(false) {}
		int idaapi visit_expr(cexpr_t *e)
		{
			switch ( e->op )
			{

			case cot_sub:  // B
				{			  
					convert_test(e);
					if (recalc_parent_types())
						return 1;
				}
				break;

			}
			return 0; // continue walking the tree
		}
	};

	fill_backreferences();
	zero_converter_t zc;
	zc.apply_to(&cfunc->body, NULL);
}


bool idaapi change_negative_cast_callback(void *ud)
{
	vdui_t &vu = *(vdui_t *)ud;
	vu.get_current_item(USE_KEYBOARD);
	if (!vu.item.is_citem())
		return false;
	cexpr_t *e = vu.item.e;

	tinfo_t from;
	int offset = 0;

	if (e->op == cot_helper)
	{
		if (qstrcmp(HELPERNAME, e->helper) != 0)
			return false;

		const citem_t *p = vu.cfunc->body.find_parent_of(e);
		if (p->op == cot_call)
		{
			//TODO: fix this
			return false;
			cexpr_t * call = (cexpr_t * )p;
			carglist_t & arglist = *call->a;
			if (arglist.size()!= 3)
				return false;			
			from = arglist[0].type;
			if (arglist[1].op != cot_num)
				return false;
			offset = arglist[1].numval();
		}
	}
	else
	{
		if (e->op != cot_sub)
			return false;

		if (e->op != cot_sub)
		return false;
	
		cexpr_t *var = e->x;
		cexpr_t * cast = 0;

		if (var->op == cot_cast)
		{
			cast = var;
			var = var->x;			
		}
		cexpr_t * num = e->y;
		if(var->op != cot_var || num->op != cot_num)
			return false;
		from = var->type;
		offset = num->numval();
		if (cast)
		{
			typestring t = cast->type;
			t.remove_ptr_or_array();
			offset *= t.size();
		}
	}

	typestring t;
	negative_cast_t * cache = find_cached_cast(from, offset);
	if (cache)
	{
		t = cache->cast_to;
	}

	char definition[MAXSTR];
	t.print(definition, MAXSTR);
	qtype qt;

	char declaration[MAXSTR];
	char * answer;

	while(answer = askstr(HIST_TYPE, definition, "[Hexrays-Tools] Gimme new negative cast type"))
	{
		qstrncpy(declaration, answer, MAXSTR);
		//notice:
		//there is always some 0 in memory
		//so no need to check non-nullness of strchr result
		if( *(strchr(declaration, 0)-1)!= ';')
			qstrncat(declaration, ";", MAXSTR);

		if (parse_decl(idati, declaration, NULL, &qt, NULL, PT_TYP))
		{
			t = qt;
		}
		else
		{
			continue;
		}
	}
	if(!answer)
		return false;
	negative_cast_t cast;

	if (cache)
	{
		cache->cast_to = t;
		cache->is_mem_ptr = false;
	}
	else
	{
		cast.cast_to = t;
		cast.cast_from = from;
		cast.offset = offset;
		negative_casts.push_back(cast);
	}
	
	vu.refresh_view(false);
	return true;
}