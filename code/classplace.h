/*

*/
#include <ida.hpp>
#include <kernwin.hpp>

define_place_exported_functions(class_place_t)
class class_place_t : public place_t
{          // a place pointer
public:
	//size_t n; // line number in strvec_t	
	uval_t idx;
	uval_t section;//head, ea, function list
	uval_t subsection; 
	
	class_place_t(void):idx(0), section(0), subsection(0) {lnnum = 0;}
	//class_place_t(uval_t i, uval_t o, short ln) : place_t(ln), idx(i), offset(o) {}
	//class_place_t(size_t n_) : n(n_) {}
	class_place_t(uval_t idx_) : idx(idx_), section(0), subsection(0) { lnnum = 0; }
	class_place_t(uval_t idx_, uval_t sctn, uval_t ssctn) : idx(idx_), section(sctn), subsection(ssctn) {lnnum = 0;}
	define_place_virtual_functions(class_place_t);
};