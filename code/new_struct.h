#pragma once
#include "typestring.h"
#include <pro.h>

struct meminfo_v2_t 
{
	qstring name;
	uint64 offset;
	typestring type;
	uint64 size;
	typestring fields;
};

struct typerecord_t
{
	typestring type;
	bool enabled;

	typerecord_t():enabled(true),type(){}

	DECLARE_COMPARISONS(typerecord_t)	
	{	
		return type.compare(r.type);
	};
};

struct typevec_t:qvector<typerecord_t>
{
	unsigned int disabled_count;
	bool get_first_enabled(typestring & type)
	{		
		for(iterator i=begin(); i!=end(); i++)
		{
			if (i->enabled)
			{
				type = i->type;
				return true;
			}
		}
		return false;
	}
	bool in_conflict() const
	{
		return size() > 1+disabled_count;	
	}
	typevec_t():disabled_count(0){}

	void clear()
	{
		qvector<typerecord_t>::clear();
		disabled_count = 0;		
	}

};        // vector of types

struct scan_info_v2_t
{	
	//typestring type;
	typevec_t types;
	bool is_array /*= false*/; //MSVC2010 is not fully C++0x compliant, sigh
	uint32 nesting_counter;	
	scan_info_v2_t():types(), nesting_counter(0), is_array(false){}
};

struct strtype_info_v2_t:qvector<meminfo_v2_t>
{
	type_t basetype;
	unsigned int N;
	bool build_udt_type(typestring &restype, typestring &resfields);
	int add_gaps();	
	void build_struct_type( typestring &outtype, typestring &outfields) const;

	bool getat(int offset, typestring & t)
	{
		iterator i = std::find_if(begin(), end(), [=](value_type & t){return t.offset == offset;});
		if (i == end())
			return false;
		t = i->type;
		return true;	
	}	
	
};


struct argument_t
{
	uval_t arg_num;
	uval_t arg_cnt;
};

typedef std::map<ea_t, uval_t> function_adjustments_t;

typedef std::map<ea_t, argument_t> arguments_t;


typedef std::map<uval_t, uval_t> max_adjustments_t;
typedef std::set<ea_t> visited_functions_t;
typedef qvector<ea_t> global_pointers_t;
typedef std::pair<int,lvar_locator_t> scanned_variable_t;
typedef std::map<ea_t,  qvector<scanned_variable_t>> scanned_variables_t;
typedef std::map<int,  typestring> types_cache_t;

//mapping offset -> set of possible types
struct field_info_t: std::map<uval_t, scan_info_v2_t>
{
	int current_offset;
	//uval_t max_offset_last_scan;
	max_adjustments_t max_adjustments;
	function_adjustments_t function_adjustments;
	visited_functions_t visited_functions;
	arguments_t argument_numbers;
	global_pointers_t global_pointers;
	scanned_variables_t scanned_variables;
	types_cache_t types_cache;

	field_info_t::field_info_t():current_offset(0){}

	void update_max_offset(unsigned int current, unsigned int max);
	unsigned int types_at_idx_qty(unsigned int idx) const;
	bool flip_enabled_status(unsigned int idx, unsigned int position);
	bool to_type(qstring varname, typestring & out_type, strtype_info_v2_t *sti = nullptr, field_info_t::iterator * bgn = nullptr, field_info_t::iterator * end = nullptr );
	bool convert_to_strtype_info(strtype_info_v2_t *strinfo, field_info_t::iterator * bgn = nullptr, field_info_t::iterator * end_ = nullptr );
	void clear();	
};


//extern bool can_be_converted_to_ptr(cfunc_t *cfunc, int varidx, strtype_info_v2_t *strinfo);
extern bool can_be_converted_to_ptr2(vdui_t &vu, ctree_item_t & item, field_info_t * fields);
extern bool structure_from_restype_resfields(qstring &varname, typestring &out_type, typestring &restype, typestring &resfields);