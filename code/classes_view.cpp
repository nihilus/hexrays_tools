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


#include "classplace.h"
#include "classes.h"
#include "helpers.h"

// Structure to keep all information about the our sample view
struct sample_info_t
{
  TForm *form;
  TCustomControl *cv;
  strvec_t sv;
  sample_info_t(TForm *f) : form(f), cv(NULL) {}
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

//---------------------------------------------------------------------------
// Sample callback function for the right-click menu
static bool idaapi change_fnc_name_cb(void *ud)
{
  sample_info_t *si = (sample_info_t *)ud;

/*  qstring word;
  if ( !get_current_word(si->cv, false, word) )
    return false;

  info("The current word is: %s", word.c_str());
  */
  class_place_t * place; 
  place = (class_place_t *)get_custom_viewer_place(si->cv, false, NULL, NULL);
  if(!place)
	  return false;
  tid_t tid = get_class_by_idx(place->idx);
  if(tid==BADNODE)
	  return false;

  class_t * clas = get_class(tid);
  if(!clas)
	  return false;

  if (place->subsection >= clas->functions_ea.size())
	  return false;
  
  ea_t ea = clas->functions_ea[place->subsection];

  char name[MAXNAMESIZE];

  get_true_name(BADADDR, ea, name, MAXNAMESIZE);

  char * answer;
  while(answer = askstr(HIST_TYPE, name, "[Hexrays-Tools] Enter new function name"))
  {
	  qstrncpy(name, answer, MAXNAMESIZE);
	  if ( set_name(ea, name) )
		  break;
  }

  return true;
}


static bool set_first_parameter_for_one_place(class_place_t *place)
{
	if (place->section != 2)
		return false;

	tid_t tid = get_class_by_idx(place->idx);
	if(tid==BADNODE)
		return false;

	class_t * clas = get_class(tid);
	if(!clas)
		return false;
	if (place->subsection >= clas->functions_ea.size())
		return false;
	ea_t ea = clas->functions_ea[place->subsection];
	func_t * func = get_func(ea);	
	qtype type;
	qtype fields;
	if(!get_tinfo(ea, &type, &fields))
	{
		if(guess_func_tinfo(func,&type, &fields)!=GUESS_FUNC_OK)
			return false;
	}

	func_type_data_t fti;
	
	if(!build_funcarg_info(idati, type.c_str(), fields.c_str(), &fti, 0))
		return false;

	char class_name[MAXNAMELEN];
	get_class_name(tid, class_name, MAXNAMELEN);
	
	tinfo_t ptr =  make_pointer(create_numbered_type_from_name(class_name));//make_pointer(create_typedef(class_name));
	if (fti.size())
	{
		fti[0].type = ptr;
	}

	fields.clear();
	type.clear();
	build_func_type(&type, &fields, fti);
	if ( !apply_tinfo(idati, ea, type.c_str(), fields.c_str(), 1) )
		return false;

	//	vu.refresh_view(true);
	return true;
}

static bool idaapi set_first_parameter_cb(void *ud)
{
  sample_info_t *si = (sample_info_t *)ud;
  twinpos_t b,e;
  if (!readsel2(si->cv, &b, &e))
  {
	  //just one
	  class_place_t * place;
	  place = (class_place_t *)get_custom_viewer_place(si->cv, false, NULL, NULL);
	  if(!place)
		  return false;
	  return set_first_parameter_for_one_place(place);
  };
  class_place_t * sb = (class_place_t *)b.at;
  class_place_t * se = (class_place_t *)e.at;
  do
  {
	  if (sb->section == 2)
	  {
		  set_first_parameter_for_one_place(sb);
	  }
	if (!sb->next(0))
		return false;
  }	while(sb->compare(se)!=1);
  return true;
}

static bool idaapi changetype_cb(void *ud)
{
  sample_info_t *si = (sample_info_t *)ud;

/*  qstring word;
  if ( !get_current_word(si->cv, false, word) )
    return false;

  info("The current word is: %s", word.c_str());
  */
  class_place_t * place;  
  place = (class_place_t *)get_custom_viewer_place(si->cv, false, NULL, NULL);
  
  if(!place)
	  return false;
  tid_t tid = get_class_by_idx(place->idx);
  if(tid==BADNODE)
	  return false;

  class_t * clas = get_class(tid);
  if(!clas)
	  return false;

  if (place->subsection >= clas->functions_ea.size())
	  return false;
  
  ea_t ea = clas->functions_ea[place->subsection];

  char name[MAXSTR];

  get_true_name(BADADDR, ea, name, MAXSTR);

  qtype type;
  qtype fields;  
  if (!get_tinfo(ea, &type, &fields))
  {
	  if (!guess_func_tinfo(get_func(ea), &type, &fields))
		  goto pokracuj; 
  }
  print_type_to_one_line(name, MAXSTR, idati, type.c_str(), name, 0, fields.c_str(), 0);

pokracuj:
  char declaration[MAXSTR];
  char * answer;
  while(answer = askstr(HIST_TYPE, name, "[Hexrays-Tools] Enter new function type"))
  {
	  qstrncpy(declaration, answer, MAXSTR);
	  //notice:
	  //there is always some 0 in memory
	  //so no need to check non-nullness of strchr result
	  if( *(strchr(declaration, 0)-1)!= ';')
		  qstrncat(declaration, ";", MAXSTR);

	  if (parse_decl(idati, declaration, NULL, &type, &fields, PT_VAR))
	  {
		  if (apply_tinfo(idati, ea, type.c_str(), fields.c_str(), 1))
		  {
			  return true;		  
		  }
	  }
  }
  return false;
}

extern void show_function_with_this_struct_tid_as_parameter(tid_t goal);

static bool idaapi scan_cb(void *ud)
{
  sample_info_t *si = (sample_info_t *)ud;
  
  class_place_t * place;  
  place = (class_place_t *)get_custom_viewer_place(si->cv, false, NULL, NULL);
  if(!place)
	  return false;
  tid_t tid = get_class_by_idx(place->idx);
  if(tid==BADNODE)
	  return false;
  show_function_with_this_struct_tid_as_parameter(tid);
  return true;
}

bool is_at_function(Controls::TCustomControl * cv, class_place_t ** place_out = 0)
{
	class_place_t * place; 
	place = (class_place_t *)get_custom_viewer_place(cv, false, NULL, NULL);
	if(!place)
		return false;
	if (place_out)
		*place_out = place;

	return place->section == 2;
}

bool is_at_vftable(Controls::TCustomControl * cv)
{
	class_place_t * place; 
	place = (class_place_t *)get_custom_viewer_place(cv, false, NULL, NULL);
	if(!place)
		return false;
	return place->section == 1;	
}

bool is_at_classname(Controls::TCustomControl * cv)
{
	class_place_t * place; 
	place = (class_place_t *)get_custom_viewer_place(cv, false, NULL, NULL);
	if(!place)
		return false;
	return place->section == 0;	
}

static bool class_add_constructor(class_t * clas)
{
	if(!clas)
		return false;
	if(clas->virt_table_ea == BADADDR)
		return false;
	int cnt = 0;
	for(ea_t ea = get_first_dref_to(clas->virt_table_ea); ea!=BADADDR; ea = get_next_dref_to(clas->virt_table_ea, ea))
	{
		func_t * fnc = get_func(ea);
		if(fnc)
		{
			clas->functions_ea.add_unique(fnc->startEA);
			cnt++;
		}
	}
	return cnt>0;
}

static bool idaapi add_constructor_destuctor_cb(void *ud)
{
	sample_info_t * si = (sample_info_t *)ud;
  
	class_place_t * place; 
	place = (class_place_t *)get_custom_viewer_place(si->cv, false, NULL, NULL);
	if(!place)
		return false;
	tid_t tid = get_class_by_idx(place->idx);
	if(tid==BADNODE)
		return false;

	class_t * clas = get_class(tid);
	class_add_constructor(clas);
	save_class(clas);
	return true;
}

static bool idaapi add_from_structs_cb(void *ud)
{
	for( uval_t idx = get_first_struc_idx(); idx!=BADNODE; idx=get_next_struc_idx(idx) )
	{
		tid_t id = get_struc_by_idx(idx);
		struc_t * struc = get_struc(id);
		if (!struc->memqty)
			continue;
		char name[MAXNAMESIZE];
		if(!get_member_name(struc->members[0].id,  name, MAXNAMESIZE))
			continue;
		
		if((strnicmp(name, "VT", 2)==0) || (stristr(name, "vt_")!=0))
		{
			//msg("adding class %p\n", id);
			add_class(id);
			tinfo_t t;
			if( get_member_type(&struc->members[0], &t) )
			{
				tid_t child = get_struc_from_typestring(t);
				struc_t * vt = get_struc(child);
				if(vt)
				{
					char comment[MAXSTR];
					get_struc_cmt(vt->id, true, comment, sizeof(comment));
					char * strpos = strstr(comment, "@0x");
					if ( strpos )
					{
						ea_t ea;
						if ( my_atoea( strpos + 1, &ea) )
						{
							class_t * clas =  get_class(id);
							if(!clas)
								continue;
							//TODO: class_set_vft();
							clas->virt_table_ea = ea;
							//msg("adding vt %p to class %p\n", ea, id);
							save_class(clas);
							{
								while (1)
								{
									ea_t fncea = get_long(ea);
									flags_t fnc_flags = getFlags(fncea);
									if ( isFunc(fnc_flags)  )
									{
										clas->functions_ea.add_unique(fncea);
										func_t * func = get_func(fncea);
										if (func && (func->flags & FUNC_THUNK))
										{
											ea_t target;
											ea_t target2;
											if ((target2 = calc_thunk_func_target(func, &target))!=BADADDR)
											{
												clas->functions_ea.add_unique(target2);
												if (target!=BADADDR)
													clas->functions_ea.add_unique(target);
											}
										}
									}
									ea += 4;
									//this doesn't work if vtable is already declared as structure
									//ea = next_head(ea, BADADDR);
									flags_t ea_flags = getFlags(ea);
									if (has_any_name(ea_flags))
										break;
								}
								class_add_constructor(clas);
								save_class(clas);
							}
						}
					}
				}
			}
		}
	}
	return true;
}

static bool jumping(sample_info_t *si, int shift)
{
	class_place_t * place; 
	place = (class_place_t *)get_custom_viewer_place(si->cv, false, NULL, NULL);
	switch (place->section)
	{
	case 0:
		{
			tid_t id = get_class_by_idx( place->idx );
			open_structs_window(id);
			msg("[Hexrays-Tools] Enter has been pressed");
		}
		break;
	case 1:				  
		{
			//TODO
			/*tid_t id = get_class_by_idx( place->idx );
			open_structs_window(id);
			msg("Enter has been pressed");
			*/
		}
		break;

	case 2:
		{
			tid_t id = get_class_by_idx( place->idx );
			if (id==BADADDR)
				return false;
			class_t * clas = get_class(id);
			if (!clas)
				return false;
			if (place->subsection >= clas->functions_ea.size())
				return false;
			if(shift)
			{
				jumpto(clas->functions_ea[place->subsection]);
			}
			else
			{
				open_pseudocode(clas->functions_ea[place->subsection], -1);
			}

		}
		break;
	}
	return true;
}

//------------------------------------------------------------
// The user double clicked
static bool idaapi cb_dblclick(TCustomControl *cv, int shift, void *ud)
{
	sample_info_t *si = (sample_info_t *)ud;
	return jumping(si, shift);
}

//---------------------------------------------------------------------------
// Keyboard callback
static bool idaapi ct_keyboard(TCustomControl * /*v*/, int key, int shift, void *ud)
{
  //if ( shift == 0 )
  {
    sample_info_t *si = (sample_info_t *)ud;
    switch ( key )
    {
      case 'N':
		  if (is_at_function(si->cv))
		  {
			  change_fnc_name_cb(si);
			  return true;
		  }

        msg("[Hexrays-Tools] The hotkey 'N' has been pressed\n");
        return false;

	  case 'Y':
		  if (is_at_function(si->cv))
		  {
			  changetype_cb(si);
			  return true;
		  }

		  return false;

      case VK_ESCAPE:
        close_tform(si->form, FORM_SAVE);
		
        return true;

	  case VK_DELETE:
		  {
			  class_place_t * class_pl;
			  if (is_at_function(si->cv, &class_pl))
			  {
				  tid_t tid = get_class_by_idx(class_pl->idx);
				  class_t * clas = get_class(tid);
				  eavec_t::iterator i = clas->functions_ea.begin();
				  i += class_pl->subsection;
				  clas->functions_ea.erase(i);
				  save_class(clas);
				  refresh_custom_viewer(si->cv);
				  return true;			  
			  }		  
		  }
		  break;

	  case VK_RETURN:
		  return jumping(si, shift);			  		  
    }
  }
  return false;
}

//---------------------------------------------------------------------------
#define add_popup add_custom_viewer_popup_item

bool is_something_selected(TCustomControl *v)
{
	twinpos_t b,e;
	return readsel2(v, &b, &e);	
}

//---------------------------------------------------------------------------
static void idaapi ct_popup(TCustomControl *v, void *ud)
{  
  set_custom_viewer_popup_menu(v, NULL);
  // Create right-click menu on the fly
  add_popup(v, "[Hexrays-Tools] Import from Structures", "", add_from_structs_cb, ud);
  if ( is_at_function(v) )
  {
	  if (!is_something_selected(v))
	  {
		  add_popup(v, "[Hexrays-Tools] Rename function", "N", change_fnc_name_cb, ud);
		  add_popup(v, "[Hexrays-Tools] Retype function", "Y", changetype_cb, ud);
	  }
	  add_popup(v, "[Hexrays-Tools] Set first argument type", "", set_first_parameter_cb, ud);
  }
  add_popup(v, "[Hexrays-Tools] Scan for class functions", "S", scan_cb, ud);
  add_popup(v, "[Hexrays-Tools] add constructor / destructor", "", add_constructor_destuctor_cb, ud);   
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
  sample_info_t *si = (sample_info_t *)ud;
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
		  class_place_t *spl = (class_place_t *)place;
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
bool idaapi show_classes_view(void *ud)
{
  HWND hwnd = NULL;
  TForm *form = find_tform("[Hexrays-Tools] Classes");
  if ( form != NULL )
  {
	  switchto_tform(form, true);
	  return true;
  }  
  form = create_tform("[Hexrays-Tools] Classes", &hwnd);
  // allocate block to hold info about our sample view
  sample_info_t *si = new sample_info_t(form);
  
  // create two place_t objects: for the minimal and maximal locations
  class_place_t s1;
  class_t * clas = get_class(get_class_by_idx(get_last_class_idx()));  
  class_place_t s2(get_last_class_idx(), 2, 100);
  // create a custom viewer
  si->cv = create_custom_viewer("", (TWinControl *)form, &s1, &s2, &s1, 0, &si->sv);
  // set the handlers so we can communicate with it
  set_custom_viewer_handlers(si->cv, ct_keyboard, ct_popup, NULL, cb_dblclick, ct_curpos, NULL, si);
  // also set the ui event callback
  hook_to_notification_point(HT_UI, ui_callback, si);
  // finally display the form on the screen
  open_tform(form, FORM_TAB|FORM_MENU|FORM_RESTORE|FORM_QWIDGET);
  return true;
}