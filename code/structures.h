#pragma once
extern bool extract_substruct(uval_t idx, uval_t begin, uval_t end);
extern bool idaapi struct_get_member(struc_t * str, asize_t offset, member_t * * out_member = NULL );
extern bool idaapi struct_has_member(struc_t * str, asize_t offset);
extern void print_struct_member_name(struc_t * str, asize_t offset, char * name,  size_t len);
extern bool which_struct_matches_here(uval_t idx, uval_t begin, uval_t end);
extern bool compare_structs(struc_t * str1, asize_t begin, struc_t * str2 );
extern bool unpack_this_member(uval_t idx, uval_t offset);