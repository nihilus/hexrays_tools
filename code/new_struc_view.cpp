/* Custom viewer sample plugin.
* Copyright (c) 2007 by Ilfak Guilfanov, ig@hexblog.com
* Feel free to do whatever you want with this code.
*
* This sample plugin demonstates how to create and manipulate a simple
* custom viewer in IDA Pro v5.1
*
* Custom viewers allow you to create a view which displays colored lines.
* These colored lines are dynamically created by callback functions.
*
* Custom viewers are used in IDA Pro itself to display
* the disassembly listng, structure, and enumeration windows.
*
* This sample plugin just displays several sample lines on the screen.
* It displays a hint with the current line number.
* The right-click menu contains one sample command.
* It reacts to one hotkey.
*
* This plugin uses the simpleline_place_t class for the locations.
* Custom viewers can use any decendant of the place_t class.
* The place_t is responsible for supplying data to the viewer.
*/

//---------------------------------------------------------------------------
#ifdef __NT__
 #define WIN32_LEAN_AND_MEAN
 #include <windows.h>
#else
 #define VK_ESCAPE 27
#endif
#include <hexrays.hpp>
#include <ida.hpp>
#include <idp.hpp>
#include <loader.hpp>
#include <kernwin.hpp>
#include <struct.hpp>
//#include <moves.hpp>
#include "new_struct.h"
#include "new_struc_place.h"
#include "helpers.h"
#include "choosers.h"


bool idaapi change_item_type_cb(void *ud);
bool idaapi make_array_cb(void *ud);
bool idaapi pack_cb(void *ud);
// Structure to keep all information about the our sample view
struct new_struc_info_t
{
	TForm *form;
	TCustomControl *cv;
	field_info_t * fi;
	new_struc_info_t(TForm *f) : form(f), cv(NULL) {}
};

//---------------------------------------------------------------------------
// get the word under the (keyboard or mouse) cursor
static bool get_current_word(TCustomControl *v, bool mouse, qstring &word)
{
	// query the cursor position
	int x, y;
	if ( get_custom_viewer_place(v, mouse, &x, &y) == NULL )
		return false;

	// query the line at the cursor
	char buf[MAXSTR];
	const char *line = get_custom_viewer_curline(v, mouse);
	tag_remove(line, buf, sizeof(buf));
	if ( x >= (int)strlen(buf) )
		return false;

	// find the beginning of the word
	char *ptr = buf + x;
	while ( ptr > buf && !isspace(ptr[-1]) )
		ptr--;

	// find the end of the word
	char *begin = ptr;
	ptr = buf + x;
	while ( !isspace(*ptr) && *ptr != '\0' )
		ptr++;

	word = qstring(begin, ptr-begin);
	return true;
}

//------------------------------------------------------------
// The user double clicked
static bool idaapi cb_dblclick(TCustomControl *cv, int shift, void *ud)
{
	new_struc_info_t *si = (new_struc_info_t *)ud;
	return false;
}


//---------------------------------------------------------------------------
// Sample callback function for the right-click menu
static bool idaapi remove_one(new_struc_info_t *nsi)
{
	new_struc_place_t * place; 
	place = (new_struc_place_t *)get_custom_viewer_place(nsi->cv, false, NULL, NULL);
	if(!place)
		return false;


	if(!nsi->fi->flip_enabled_status(place->idx, place->subtype))
	{
		return false;	
	}	
	refresh_custom_viewer(nsi->cv);
	return true;
}

static bool idaapi adjust_substruct(new_struc_info_t *nsi, bool add, bool fine)
{
	new_struc_place_t * place; 
	place = (new_struc_place_t *)get_custom_viewer_place(nsi->cv, false, NULL, NULL);
	if(!place)
		return false;
	field_info_t::iterator iter =  nsi->fi->begin();
	
	if (!safe_advance(iter, nsi->fi->end(), place->idx))
		return false;


	//cannot have negative nesting counter
	if (!add)
	{
		field_info_t::iterator iter2 = iter;
		while(iter2!= nsi->fi->end())
		{
			if (iter2->second.nesting_counter == 0)
				return false;
			if (fine)
				break;
			++iter2;
		}
	}

	while(iter != nsi->fi->end())
	{
		if (add)
			++iter->second.nesting_counter;
		else
			--iter->second.nesting_counter;	
		if (fine)
			break;
		++iter;
	}
	refresh_custom_viewer(nsi->cv);
	return true;
}


static bool idaapi set_current_offset(new_struc_info_t *nsi)
{
	new_struc_place_t * place; 
	place = (new_struc_place_t *)get_custom_viewer_place(nsi->cv, false, NULL, NULL);
	if(!place)
		return false;

	field_info_t::iterator iter =  nsi->fi->begin();
	if (!safe_advance(iter, nsi->fi->end(), place->idx))
	{
		return false;
	}
	if (iter == nsi->fi->end())
		return false;

	nsi->fi->current_offset = iter->first;
	refresh_custom_viewer(nsi->cv);
	return true;
}

bool idaapi jump_to_next_function_cb(void *ud);

//---------------------------------------------------------------------------
// Keyboard callback
static bool idaapi ct_keyboard(TCustomControl * /*v*/, int key, int shift, void *ud)
{
	//if ( shift == 0 )
	{
		new_struc_info_t *si = (new_struc_info_t *)ud;
		switch ( key )    
		{
		case 'X':
			return jump_to_next_function_cb(ud);
			break;
		case 'N':
			return false;
			break;
		case 'R':
			return make_array_cb(ud);			
			break;
		case 'Y':
			return change_item_type_cb(ud);
		case VK_ADD:
		case '+':
			return adjust_substruct(si, true, shift );
			break;

		case VK_MULTIPLY:
			return set_current_offset(si);
			break;

		case 'P':
			return pack_cb(ud);
			break;

		case VK_SUBTRACT:  
		case '-':
			return adjust_substruct(si, false, shift );
			break;

		case VK_ESCAPE:
			close_tform(si->form, FORM_SAVE);		
			return true;

		case VK_DELETE:
			{
				return remove_one(si);			  
			}
			break;

		case VK_RETURN:
			break;
		}
	}
	return false;
}

//----------------------------------------------------------------------------------------------------
//show_function_list_cb

bool idaapi show_function_list_cb(void *ud)
{
	if(!ud)
		return false;
	new_struc_info_t &si = *(new_struc_info_t *)ud;

	field_info_t & fi = *si.fi;


	function_list fl;
	for(function_adjustments_t::iterator i = fi.function_adjustments.begin(); i != fi.function_adjustments.end(); i++ )
	{
		ea_t ea = i->first;
		if (fi.visited_functions.find(ea) == fi.visited_functions.end())
			fl.functions.add_unique(ea);
	}
	if(fl.functions.size())
	{
		fl.choose("[Hexrays-Tools] Detected functions");
		if (fl.choosed>0)
			fi.visited_functions.insert(fl.functions[fl.choosed-1]);
		fl.open_pseudocode();		
	}
	return true;
}


struct ida_local glob_vars_locator_t : public ctree_parentee_t
{
	field_info_t * fields;
	cexpr_t * found_expr;
	bool is_our(int idx)
	{
		return fields->global_pointers.find(idx)!=fields->global_pointers.end();
	}
	int idaapi visit_expr(cexpr_t *e)
	{
		bool is_glob_obj_ptr = (e->op == cot_obj && is_our(e->obj_ea));
		if (is_glob_obj_ptr)	
		{
			found_expr = e;
			return 1;
		}			
		return 0;
	}
	glob_vars_locator_t(field_info_t * fi) : fields(fi), found_expr(NULL)
	{
	}
};

static bool idaapi jump_to_next_function_cb(void *ud)
{
	if(!ud)
		return false;
	new_struc_info_t &si = *(new_struc_info_t *)ud;

	field_info_t & fi = *si.fi;

	function_list fl;
	ea_t fnc_ea = BADADDR;
	for(function_adjustments_t::iterator i = fi.function_adjustments.begin(); i != fi.function_adjustments.end(); i++ )
	{
		ea_t ea = i->first;
		if (fi.visited_functions.find(ea) == fi.visited_functions.end())
		{
			fnc_ea = ea;
			break;
		}
	}
	if((fnc_ea != BADADDR) && isEnabled(fnc_ea))
	{	
		vdui_t * ui;
		if(ui = open_pseudocode(fnc_ea, 1))
		{	
			fi.visited_functions.insert(fnc_ea);

			arguments_t::iterator argnum = fi.argument_numbers.find(fnc_ea);
			

			if(argnum != fi.argument_numbers.end())
			{
				//TODO: this doesnt work, because rgas is just empty class in hexrays.hpp
				//if (argnum->second.arg_cnt == ui->cfunc->rgas.size() + ui->cfunc->stas.size())

				uval_t argcnt = -1;
				{
					func_type_info_t fti;
					typestring type;
					qtype fields;
					if (ui->cfunc->get_func_type(type, fields))
					{
						if ( -1 != build_funcarg_info(idati, type.c_str(), fields.c_str(), &fti, 0) )
						{
							argcnt = fti.size();
							msg("%s\n", fti[argnum->second.arg_num].name.c_str());
						}
					}					
				}
				{
				 	if(argnum->second.arg_cnt == argcnt)
						msg("[Hexrays-Tools] function @ %08X argument nr %d\n", argnum->first, argnum->second.arg_num);
					else
						msg("[Hexrays-Tools] function @ %08X has different arguments count\n", argnum->first);
				}
			}
			//jump to global variable
			if(fi.global_pointers.size()>0)
			{
				history_item_t hist;
				hist.ea = ui->cfunc->entry_ea;
				glob_vars_locator_t locator(&fi);				
				if (locator.apply_to((citem_t*)&ui->cfunc->body, NULL)==1)
				{
					qstring ptr;
					ptr.sprnt("%08X", locator.found_expr);

					{
#if IDA_SDK_VERSION >= 630
						//auto & strvec = ui->sv;
						auto strvec = ui->cfunc->get_pseudocode();						
#else
						auto & strvec = ui->sv;
#endif						
						for(unsigned int i =0; i<strvec.size();i++)
						{
							size_t position;
							auto & SV = strvec[i];
							if ((position = SV.line.find(ptr, 0)) != SV.line.npos)
							{
								hist.lnnum = i;
								size_t len = SV.line.size();
								char buff[MAXSTR];
								memset(buff, 0, sizeof(buff));
								//-1 because of COLOR_ADDR tag, we want to skip
								qstrncpy(buff, SV.line.c_str(), position-1);
								bool jump_custom_viewer(TCustomControl *custom_viewer, int line, int x, int y);
								jump_custom_viewer(ui->ct, i, tag_strlen(buff), 0);
							}
						}
					}
					
				}
			}

		}
		return true;
	}
	return false;
}

//---------------------------------------------------------------------------
static bool idaapi change_item_type_cb(void *ud)
{
	if(!ud)
		return false;
	new_struc_info_t &si = *(new_struc_info_t *)ud;

	new_struc_place_t * place; 
	place = (new_struc_place_t *)get_custom_viewer_place(si.cv, false, NULL, NULL);
	if(!place)
		return false;


	field_info_t & fi = *si.fi;
	//place->idx
	if ( fi.size() <= place->idx )
		return false;
	field_info_t::iterator b = fi.begin();

	if (!safe_advance(b, fi.end(), place->idx))
		return false;
	
	typevec_t & types = b->second.types;


	if (types.size()==0)
		return false;
	
	typevec_t::iterator iter1 = types.begin();
	if (!safe_advance(iter1, types.end(), place->subtype))
	{
		return false;
	}
	if (iter1 == types.end())
		return false;

	qtype fields;  
	typestring type = iter1->type;
	char strtype[MAXSTR];
	print_type_to_one_line(strtype, MAXSTR, idati, type.c_str(), "dummy", 0, 0, 0);
	char declaration[MAXSTR];
	char * answer;
	while(answer = askstr(HIST_TYPE, strtype, "[Hexrays-Tools] Enter type"))
	{
		qstrncpy(declaration, answer, MAXSTR);
		//notice:
		//there is always some 0 in memory
		//so no need to check non-nullness of strchr result
		if( *(strchr(declaration, 0)-1)!= ';')
			qstrncat(declaration, ";", MAXSTR);
		if (parse_decl(idati, declaration, NULL, &type, &fields, PT_VAR))
		{
			iter1->type = type;
			refresh_custom_viewer(si.cv);
			return true;			
		}
	}
	return false;
}

//---------------------------------------------------------------------------
static bool idaapi make_array_cb(void *ud)
{
	if(!ud)
		return false;
	new_struc_info_t &si = *(new_struc_info_t *)ud;

	new_struc_place_t * place; 
	place = (new_struc_place_t *)get_custom_viewer_place(si.cv, false, NULL, NULL);
	if(!place)
		return false;


	field_info_t & fi = *si.fi;	
	
	//place->idx
	if ( fi.size() <= place->idx )
		return false;
	field_info_t::iterator b = fi.begin();

	if (!safe_advance(b, fi.end(), place->idx))
		return false;
	
	typevec_t & types = b->second.types;

#if !MANY_IS_ARRAY_BOOL
	b->second.is_array = !b->second.is_array;
	refresh_custom_viewer(si.cv);
	return true;
#else
	/*
	uval_t current_offset = b->first;

	if (types.size()==0)
		return false;
	
	typevec_t::iterator iter1 = types.begin();
	
	if (!safe_advance(iter1, types.end(), place->subtype))
	{
		return false;
	}
	if (iter1 == types.end())
		return false;

	b++;
	if (b==fi.end())
		return false;

	uval_t next_offset = b->first;
	qtype fields;
	typestring type = iter1->type;
	size_t sz = get_type_size0(idati, type.c_str());
	if (sz==0)
		return false;
	iter1->type = make_array(type, (next_offset-current_offset)/sz);
	refresh_custom_viewer(si.cv);
	return false;*/
#endif
}

//---------------------------------------------------------------------------
static bool idaapi pack_cb(void *ud)
{
	if(!ud)
		return false;
	new_struc_info_t &si = *(new_struc_info_t *)ud;

	new_struc_place_t * place; 
	place = (new_struc_place_t *)get_custom_viewer_place(si.cv, false, NULL, NULL);
	if(!place)
		return false;


	field_info_t & fi = *si.fi;
	//place->idx
	if ( fi.size() <= place->idx )
		return false;
	field_info_t::iterator b = fi.begin();

	if(!safe_advance(b, fi.end(), place->idx))
	{
		return false;	
	}
	int c = b->second.nesting_counter;

	field_info_t::iterator last = b;
	while((b != fi.end()) && (b!=fi.begin())  && (b->second.nesting_counter == c))
	{
		last = b;
		--b;
	}
	
	b = last;
	field_info_t::iterator e = b;
	while(e!=fi.end() && e->second.nesting_counter == c)
		++e;

	qstring name = "dummy";
	typestring outtype;
	
	if (!fi.to_type(name, outtype, nullptr, &b, &e))
		return false;

	scan_info_v2_t & sci = b->second;
	sci.types.clear();
	typerecord_t tr;
	tr.type = outtype;
	tr.enabled = true;
	//
	fi.types_cache[b->first] = outtype;

	sci.types.add_unique(tr);
	--sci.nesting_counter;

	++b;
	if(b!=fi.end())
	{
		if(b!=e)
			fi.erase(b, e);
	}
	refresh_custom_viewer(si.cv);
	return true;
}

//---------------------------------------------------------------------------
#define add_popup add_custom_viewer_popup_item

//---------------------------------------------------------------------------
static void idaapi ct_popup(TCustomControl *v, void *ud)
{
	// dynamically create custom popup items
	set_custom_viewer_popup_menu(v, NULL);
	// Create right-click menu on the fly
	add_popup(v, "[Hexrays-Tools] pack substruct", "P", pack_cb, ud);
	add_popup(v, "[Hexrays-Tools] change type", "Y", change_item_type_cb, ud);	
	add_popup(v, "[Hexrays-Tools] make array", "R", make_array_cb, ud);

	new_struc_info_t  * si = (new_struc_info_t *)ud;  
	if (si && si->fi && ( si->fi->function_adjustments.size() >0) )
	{
		add_popup(v, "[Hexrays-Tools] show function list", "", show_function_list_cb, ud);
	}
	if (si && si->fi && ( (si->fi->function_adjustments.size() - si->fi->visited_functions.size()) >0))
	{
		add_popup(v, "[Hexrays-Tools] jump to next function in function list", "X", jump_to_next_function_cb, ud);
	}
	//function_list
	//add_popup(v, "add constructor / destructor", "", add_constructor_destuctor, ud); 
}

//---------------------------------------------------------------------------
// This callback will be called each time the keyboard cursor position
// is changed
static void idaapi ct_curpos(TCustomControl *v, void *)
{
	qstring word;
	/*
	if ( get_current_word(v, false, word) )
	msg("Current word is: %s\n", word.c_str());
	*/
}

//--------------------------------------------------------------------------
static int idaapi ui_callback(void *ud, int code, va_list va)
{
	new_struc_info_t *si = (new_struc_info_t *)ud;
	switch ( code )
	{
		// how to implement a simple hint callback
	case ui_get_custom_viewer_hint:
		{
			TCustomControl *viewer = va_arg(va, TCustomControl *);
			place_t *place         = va_arg(va, place_t *);
			int *important_lines   = va_arg(va, int *);
			qstring &hint          = *va_arg(va, qstring *);
			if ( si->cv == viewer ) // our viewer
			{
				if ( place == NULL )
					return 0;
				new_struc_place_t *spl = (new_struc_place_t *)place;
				hint.sprnt("[Hexrays-Tools] Hint for line %d", spl->idx);
				*important_lines = 1;
				return 1;
			}
			break;
		}
	case ui_tform_invisible:
		{
			TForm *f = va_arg(va, TForm *);
			if ( f == si->form )
			{
				delete si;
				unhook_from_notification_point(HT_UI, ui_callback, NULL);
			}
		}
		break;
	}
	return 0;
}

//---------------------------------------------------------------------------
// Create a custom view window
bool idaapi show_new_struc_view(field_info_t * fi)
{	
	//TODO: this only permits one instance of field_info_t to be used in ida
	//in future we want to be able to have more instances
	//so add TForm * member to field_info_t and set it here
	TForm *form = find_tform("[Hexrays-Tools] new structure");
	if ( form != NULL )
	{
		
		switchto_tform(form, true);
		return true;
	}
	HWND hwnd = NULL;
	form = create_tform("[Hexrays-Tools] new structure", &hwnd);
	// allocate block to hold info about our sample view
	new_struc_info_t *si = new new_struc_info_t(form);
	// create two place_t objects: for the minimal and maximal locations
	new_struc_place_t  s1;  
	new_struc_place_t s2(fi->size()-1);
	si->fi = fi;
	si->cv = create_custom_viewer("", (TWinControl *)form, &s1, &s2, &s1, 0, si->fi);
	set_custom_viewer_handlers(si->cv, ct_keyboard, ct_popup, NULL, cb_dblclick, ct_curpos, NULL, si);
	hook_to_notification_point(HT_UI, ui_callback, si);	
#if IDA_SDK_VERSION >= 630
	open_tform(form, FORM_MENU|FORM_RESTORE|FORM_QWIDGET);
#else
	open_tform(form, FORM_MDI|FORM_MENU|FORM_RESTORE);
#endif	
	return true;
}