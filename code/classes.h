#pragma once

struct class_t
{
	tid_t tid;	
	eavec_t functions_ea;
	eavec_t parents_tid;
	ea_t virt_table_ea;
	flags_t flags;//reserved
	int age;
	int position;
};


extern void classes_init();
extern bool classes_term();

extern class_t * get_class(tid_t tid);
extern bool save_class(class_t * clas);
extern class_t * get_class(tid_t tid);
extern tid_t get_class_by_idx(int index);
extern size_t get_class_qty(void);
extern uval_t get_first_class_idx(void);
extern bool add_class_at_idx(tid_t clas, size_t idx);
extern tid_t add_class(tid_t tid_of_struc);

extern uval_t get_first_class_idx(void);
extern uval_t get_last_class_idx(void);
extern size_t get_class_qty(void);
extern uval_t get_next_class_idx(uval_t idx);
extern uval_t get_class_idx(tid_t id);        // get internal number of the class
extern ssize_t get_class_name(tid_t id, char *buf, size_t bufsize);
extern uval_t get_prev_class_idx(uval_t idx);
extern uval_t get_last_class_idx(void);
