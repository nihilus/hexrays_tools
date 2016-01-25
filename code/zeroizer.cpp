#include <Windows.h>
#include <hexrays.hpp>
#include <struct.hpp>
#include <bytes.hpp>
#include <kernwin.hpp>
#include <algorithm>
#include <pro.h>
#include "zeroizer.h"
#include <algorithm>
#include <iterator>

void convert_zeroes(cfunc_t *cfunc)
{
	/*
	RULES:
	variable is assigned zero
	variable is never referenced
	warn if variable is on stack

	replace every such variable with zero
	*/

	struct zeroizer_t : public ctree_parentee_t
	{
		std::set<int> vyrazene;
		std::set<int> navrzene;
		cfunc_t * cfunc;
		bool is_deleter;

		zeroizer_t(void) : ctree_parentee_t(false), is_deleter(false) {}

		int idaapi visit_expr(cexpr_t *e)
		{
			switch ( e->op )
			{
			case cot_var:
				{
					if(is_deleter)
					{
						if (navrzene.count(e->v.idx) == 1 && vyrazene.count(e->v.idx) == 0 )
						{
							cexpr_t *parent = parent_expr();
							if(parent /*&& parent->op == cot_asg */&& parent->x == e)
								goto skip;
							//e->cleanup();
							//TODO: fix nbytes
							lvars_t & lvs = *(cfunc->get_lvars());
							lvar_t &lv = lvs[e->v.idx];
							if (lv.cmt.find("POSSIBLE ZERO") == -1)
							{
								lv.cmt.append("POSSIBLE ZERO");
							}
							char funcname[MAXSTR];
							funcname[0]=0;
							get_func_name(cfunc->entry_ea, funcname, sizeof(funcname));
							msg("%08X %s: zeroing var %s \n", cfunc->entry_ea, funcname, lv.name.c_str());
							e->put_number(cfunc, 0, 4);
						}
skip:
						;
					}

				}
				break;
			case cot_preinc:
			case cot_postinc:
			case cot_predec:
			case cot_postdec:
			case cot_ref:
			case cot_asgbor:
			case cot_asgxor:
			case cot_asgband:
			case cot_asgadd:
			case cot_asgsub:
			case cot_asgmul:
			case cot_asgsshr:
			case cot_asgushr:
			case cot_asgshl:
			case cot_asgsdiv:
			case cot_asgudiv:
			case cot_asgsmod:
			case cot_asgumod:
				{
					if (e->x->op == cot_var)
					{
						vyrazene.insert(e->x->v.idx);
					}
				}
				break;

			case cot_call:
				{
					//todo check this is parent_cot_asg.x
					if (e->x && e->x->op == cot_helper)
					{
						//LODWORD(int64_var) = 23;
						for each (const carg_t & i  in *e->a )
						{
							if (i.op == cot_var)
							{
								vyrazene.insert(i.v.idx);
							}
						};
					}


				}
				break;

			case cot_asg:  // B
				{
					if (e->x->op == cot_var && !e->y->is_zero_const())
					{
						//vyradit idx
						vyrazene.insert(e->x->v.idx);
					}
					if (e->x->op == cot_var && e->y->is_zero_const())
					{
						navrzene.insert(e->x->v.idx);						
						if (is_deleter)
						{
						/*	cexpr_t *parent_exp = parent_expr();
							if (parent_exp->op == cit_expr)
							{
								auto i =  parents.end();
								i--;
								i--;
								if ((*i)->op == cit_block)
								{
									cblock_t * bl = (cblock_t *)*i;
									//std::remove(bl->begin(), bl->end(), (cinsn_t*)parent_exp);
								}
							}
							*/
						}
					}
				}
				break;
			}
			return 0; // continue walking the tree
		}
	};	
	//msg("looking for identical zeores\n");
	zeroizer_t zc;
	zc.cfunc = cfunc;
	zc.is_deleter = false;
	zc.apply_to(&cfunc->body, NULL);
	zc.is_deleter = true;
	zc.apply_to(&cfunc->body, NULL);

	//std::set<int> proslo;
	//std::set_difference(zc.vyrazene.begin(), zc.vyrazene.end(), zc.navrzene.begin(), zc.navrzene.end(), std::inserter(proslo, proslo.end()));
}