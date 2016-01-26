/*
*  This is a sample plugin module.
*  It demonstrates how to create a graph viewer with an aribtrary graph.
*
*  It can be compiled by the following compilers:
*
*      - Borland C++, CBuilder, free C++
*
*/

#include <hexrays.hpp>
#include <ida.hpp>
#include <idp.hpp>
#include <graph.hpp>
#include <loader.hpp>
#include <kernwin.hpp>
#include <struct.hpp>

#include "negative_cast.h"

//--------------------------------------------------------------------------
static qstrvec_t graph_text;
static int display_bits = 7;


tid_t node_to_id(int n)
{
	if(nonempty_back[n]==-1)
		return BADNODE;
	uval_t idx = nonempty_back[n];
	if(idx==-1)
		return BADNODE;
	return get_struc_by_idx(idx);
}

//--------------------------------------------------------------------------
static const char *get_node_name(int n)
{
	/*switch ( n )
	{
	case 0: return COLSTR("This", SCOLOR_MACRO);
	case 1: return COLSTR("is", SCOLOR_CNAME);
	case 2: return "a";
	case 3: return COLSTR("sample", SCOLOR_DNAME);
	case 4: return COLSTR("graph", SCOLOR_IMPNAME);
	case 5: return COLSTR("viewer", SCOLOR_ERROR);
	case 6: return COLSTR("window!", SCOLOR_DNUM) "\n(with colorful names)";
	}

	*/
	tid_t id = node_to_id(n);
	if(id == BADNODE)
		return "?";
	static qstring name;
	get_struc_name(&name, id);
	return name.c_str();
}
int last_clicked = 0;
//--------------------------------------------------------------------------
static int idaapi callback(void *, int code, va_list va)
{
	int result = 0;
	switch ( code )
	{
	case grcode_calculating_layout:
		// calculating user-defined graph layout
		// in: mutable_graph_t *g
		// out: 0-not implemented
		//      1-graph layout calculated by the plugin
		msg("[Hexrays-Tools] calculating graph layout...\n");
		break;

	case grcode_changed_current:
		// a new graph node became the current node
		// in:  graph_viewer_t *gv
		//      int curnode
		// out: 0-ok, 1-forbid to change the current node
		{
			graph_viewer_t *v = va_arg(va, graph_viewer_t *);
			int curnode       = va_argi(va, int);
			last_clicked = curnode;
			//msg("%p: current node becomes %d\n", v, curnode);
		}
		break;

	case grcode_clicked:      // a graph has been clicked
		// in:  graph_viewer_t *gv
		//      selection_item_t *current_item
		// out: 0-ok, 1-ignore click
		{
			graph_viewer_t *v   = va_arg(va, graph_viewer_t *);
			va_arg(va, selection_item_t *);
			graph_item_t *m = va_arg(va, graph_item_t *);
			//msg("clicked on ");
			switch ( m->type )
			{
			case git_none:
				//msg("background\n");
				break;
			case git_edge:
				msg("edge (%d, %d)\n", m->e.src, m->e.dst);
				break;
			case git_node:
			 {
				 //msg("node %d\n", m->n);


			 }
			 break;
			case git_tool:
				//msg("toolbutton %d\n", m->b);
				break;
			case git_text:
				msg("[Hexrays-Tools] text (x,y)=(%d,%d)\n", m->p.x, m->p.y);
				break;
			case git_elp:
				msg("[Hexrays-Tools] edge layout point (%d, %d) #%d\n", m->elp.e.src, m->elp.e.dst, m->elp.pidx);
				break;
			}
		}
		break;

	case grcode_dblclicked:   // a graph node has been double clicked
		// in:  graph_viewer_t *gv
		//      selection_item_t *current_item
		// out: 0-ok, 1-ignore click
		{
			graph_viewer_t *v   = va_arg(va, graph_viewer_t *);
			selection_item_t *s = va_arg(va, selection_item_t *);
			msg("%p: %sclicked on ", v, code == grcode_clicked ? "" : "dbl");
			if ( s == NULL )
				msg("background\n");
			else if ( s->is_node )
			{
				//msg("node %d\n", s->node);

				tid_t id = node_to_id(s->node);
				if(id!=BADNODE)
				{			   
					open_structs_window(id);
				}
			}
			else
				msg("[Hexrays-Tools] edge (%d, %d) layout point #%d\n", s->elp.e.src, s->elp.e.dst, s->elp.pidx);
		}
		break;

	case grcode_creating_group:
		// a group is being created
		// in:  mutable_graph_t *g
		//      intset_t *nodes
		// out: 0-ok, 1-forbid group creation
		{
			mutable_graph_t *g = va_arg(va, mutable_graph_t *);
			intset_t &nodes    = *va_arg(va, intset_t *);
			msg("[Hexrays-Tools] %p: creating group", g);
			/*
			for ( intset_t::iterator p=nodes.begin(); p != nodes.end(); ++p )
			msg(" %d", *p);
			msg("...\n");
			*/
		}
		break;

	case grcode_deleting_group:
		// a group is being deleted
		// in:  mutable_graph_t *g
		//      int old_group
		// out: 0-ok, 1-forbid group deletion
		{
			mutable_graph_t *g = va_arg(va, mutable_graph_t *);
			int group          = va_argi(va, int);
			msg("[Hexrays-Tools] %p: deleting group %d\n", g, group);
		}
		break;

	case grcode_group_visibility:
		// a group is being collapsed/uncollapsed
		// in:  mutable_graph_t *g
		//      int group
		//      bool expand
		// out: 0-ok, 1-forbid group modification
		{
			mutable_graph_t *g = va_arg(va, mutable_graph_t *);
			int group          = va_argi(va, int);
			bool expand        = va_argi(va, bool);
			msg("[Hexrays-Tools] %p: %scollapsing group %d\n", g, expand ? "un" : "", group);
		}
		break;

	case grcode_gotfocus:     // a graph viewer got focus
		// in:  graph_viewer_t *gv
		// out: must return 0
		{
			graph_viewer_t *g = va_arg(va, graph_viewer_t *);
			//msg("%p: got focus\n", g);
		}
		break;

	case grcode_lostfocus:    // a graph viewer lost focus
		// in:  graph_viewer_t *gv
		// out: must return 0
		{
			graph_viewer_t *g = va_arg(va, graph_viewer_t *);
			//msg("%p: lost focus\n", g);
		}
		break;

	case grcode_user_refresh: // refresh user-defined graph nodes and edges
		// in:  mutable_graph_t *g
		// out: success
		{
			mutable_graph_t *g = va_arg(va, mutable_graph_t *);
			msg("%p: refresh\n", g);
			g->reset();
			if ( g->empty() )
				g->resize(nonempty_cnt);			
			for(unsigned int i=0; i < graph_references.size(); i++)
			{
				struc_reference_t & sr = graph_references[i];
				if (display_bits & sr.type)
					g->add_edge( nonempty[ sr.idx_from ], nonempty[ sr.idx_to ] , NULL);
				else
				{
					g->del_edge(nonempty[ sr.idx_from ], nonempty[ sr.idx_to ]);
				}
			}

/*
			class single_nodes_hider: public  graph_visitor_t
			{
			protected:
				//abstract_graph_t *g;
				virtual int idaapi visit_node(int n, rect_t &r) {return 0;}
				virtual int idaapi visit_edge(edge_t e, edge_info_t *ei) {return 0;}
				//friend int abstract_graph_t::for_all_nodes_edges(graph_visitor_t &nev, bool visit_nodes);
			};


			single_nodes_hider snh;
			g->for_all_nodes_edges(snh, true);*/
			
			for(unsigned int i = 0; i<g->nodes.size(); i++)
			{				
				if (g->nsucc(i)==0 && g->npred(i)==0 )
				{
					g->del_node(i);
					//i--;
				}
			}
			g->redo_layout();
			result = true;
		}
		break;

	case grcode_user_gentext: // generate text for user-defined graph nodes
		// in:  mutable_graph_t *g
		// out: must return 0
		{
			mutable_graph_t *g = va_arg(va, mutable_graph_t *);
			//msg("%p: generate text for graph nodes\n", g);
			graph_text.resize(g->size());
			//std::for_each(g->begin(), g->end(), [=] (int &n){graph_text[n] = get_node_name(n);});
			for ( node_iterator p=g->begin(); p != g->end(); ++p )
			{
				int n = *p;
				graph_text[n] = get_node_name(n);
			}
			result = true;
		}
		break;

	case grcode_user_text:    // retrieve text for user-defined graph node
		// in:  mutable_graph_t *g
		//      int node
		//      const char **result
		//      bgcolor_t *bg_color (maybe NULL)
		// out: must return 0, result must be filled
		// NB: do not use anything calling GDI!
		{
			mutable_graph_t *g = va_arg(va, mutable_graph_t *);
			int node           = va_arg(va, int);
			const char **text  = va_arg(va, const char **);
			bgcolor_t *bgcolor = va_arg(va, bgcolor_t *);
			*text = graph_text[node].c_str();
			if ( bgcolor != NULL )
				*bgcolor = DEFCOLOR;
			result = true;
			qnotused(g);
		}
		break;

	case grcode_user_size:    // calculate node size for user-defined graph
		// in:  mutable_graph_t *g
		//      int node
		//      int *cx
		//      int *cy
		// out: 0-did not calculate, ida will use node text size
		//      1-calculated. ida will add node title to the size
		//msg("calc node size - not implemented\n");
		// ida will calculate the node size based on the node text
		break;

	case grcode_user_title:   // render node title of a user-defined graph
		// in:  mutable_graph_t *g
		//      int node
		//      rect_t *title_rect
		//      int title_bg_color
		//      HDC dc
		// out: 0-did not render, ida will fill it with title_bg_color
		//      1-rendered node title
		// ida will draw the node title itself
		break;

	case grcode_user_draw:    // render node of a user-defined graph
		// in:  mutable_graph_t *g
		//      int node
		//      rect_t *node_rect
		//      HDC dc
		// out: 0-not rendered, 1-rendered
		// NB: draw only on the specified DC and nowhere else!
		// ida will draw the node text itself
		break;

	case grcode_user_hint:    // retrieve hint for the user-defined graph
		// in:  mutable_graph_t *g
		//      int mousenode
		//      int mouseedge_src
		//      int mouseedge_dst
		//      char **hint
		// 'hint' must be allocated by qalloc() or qstrdup()
		// out: 0-use default hint, 1-use proposed hint
		{
			mutable_graph_t *g = va_arg(va, mutable_graph_t *);
			int mousenode      = va_argi(va, int);
			int mouseedge_src  = va_argi(va, int);
			int mouseedge_dst  = va_argi(va, int);
			char **hint        = va_arg(va, char **);
			char buf[MAXSTR];
			buf[0] = '\0';
			if ( mousenode != -1 )
			{
				qsnprintf(buf, sizeof(buf), "%s", graph_text[mousenode].c_str());
			}
			else if ( mouseedge_src != -1 )
				qsnprintf(buf, sizeof(buf), "%s  -> %s", graph_text[mouseedge_src].c_str(), graph_text[mouseedge_dst].c_str());
			if ( buf[0] != '\0' )
				*hint = qstrdup(buf);
			result = true; // use our hint
			qnotused(g);
		}
		break;
	}
	return result;
}

//--------------------------------------------------------------------------
static const char form1[] =
"Select which vertices to display:\n"
"<substructures	         :C>\n"
"<structure offsets      :C>\n"
"<typestring offsets     :C>>\n"
"\n"
"\n";


bool idaapi choose_types(void *ud)
{    
	int bits = display_bits;	
	if (AskUsingForm_c(form1, &bits))
	{
		display_bits = bits;		
		graph_viewer_t *gv = (graph_viewer_t *)ud;
		mutable_graph_t *g = get_viewer_graph(gv);
		refresh_viewer(gv);
		return true;
	}
	return false;
}

bool idaapi save_to_file(void *ud)
{    
		
	char * path;
	if (path = askfile_c(1, "*.gdl", "[Hexrays-Tools] Choose filename to save the graph to."))
	{
		graph_viewer_t *gv = (graph_viewer_t *)ud;
		mutable_graph_t *g = get_viewer_graph(gv);
		//gen_gdl(g, path);
		gen_gdl(g, path);
		return true;
	}
	return false;
}

extern bool show_VT_from_tid(tid_t id);

bool idaapi menu_callback(void *ud)
{
	graph_viewer_t *gv = (graph_viewer_t *)ud;
	mutable_graph_t *g = get_viewer_graph(gv);
	/*int code = askbuttons_c("Circle", "Tree", "Digraph", 1, "Please select layout type");
	bgcolor_t c = 0x44FF55;
	set_node_info(g->gid, 7, &c, NULL, "Hello from plugin!");
	g->current_layout = code + 2;
	g->circle_center = point_t(200, 200);
	g->circle_radius = 200;
	g->redo_layout();
	refresh_viewer(gv);
	*/

	tid_t id = node_to_id(last_clicked);
	if(id!=BADNODE)
	{
		show_VT_from_tid(id);
	}
	return true;
}

//--------------------------------------------------------------------------
void idaapi run_graph(int /*arg*/)
{
	HWND hwnd = NULL;
	TForm *form = create_tform("[Hexrays-Tools] Structures graph", &hwnd);
	if ( hwnd != NULL )
	{
		// get a unique graph id
		netnode id;
		id.create();
		graph_viewer_t *gv = create_graph_viewer(form,  id, callback, NULL, 0);
#if IDA_SDK_VERSION >= 630
	open_tform(form, FORM_TAB|FORM_MENU|FORM_QWIDGET);
#else
	open_tform(form, FORM_TAB|FORM_MENU);
#endif	
		if ( gv != NULL )
		{
			viewer_fit_window(gv);
			viewer_add_menu_item(gv, "[Hexrays-Tools] Show VT", menu_callback, gv, NULL, 0);
			viewer_add_menu_item(gv, "[Hexrays-Tools] Choose visible types", choose_types, gv, NULL, 0);
			viewer_add_menu_item(gv, "[Hexrays-Tools] Save to file", save_to_file, gv, NULL, 0);
		}
	}
	else
	{
		close_tform(form, 0);
	}
}
