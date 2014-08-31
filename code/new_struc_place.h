/*

*/
#include <ida.hpp>
#include <kernwin.hpp>
#include "new_struct.h"

define_place_exported_functions(new_struc_place_t)
class new_struc_place_t : public place_t
{          // a place pointer
public:
	//size_t n; // line number in strvec_t	
	uval_t idx;
	unsigned int subtype;
	
	new_struc_place_t(void):idx(0),subtype(0)  {lnnum = 0;}
	//new_struc_place_t(uval_t i, uval_t o, short ln) : place_t(ln), idx(i), offset(o) {}
	//new_struc_place_t(size_t n_) : n(n_) {}
	new_struc_place_t(uval_t idx_) : idx(idx_),subtype(0) { lnnum = 0; }
	new_struc_place_t(uval_t idx_, unsigned int st) : idx(idx_),subtype(st) { lnnum = 0; }
	define_place_virtual_functions(new_struc_place_t);
};