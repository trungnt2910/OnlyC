#ifndef ONLYC_FAKES_DOMMOCK_H
#define ONLYC_FAKES_DOMMOCK_H

#include "core/Song.h"
#include "platform/web/Browser.h"

const char* test_document(void);
const char* test_input_value(const char* id);
const char* test_subtree(const char* selector);
int markdown_render(const char* source, Buffer* out);
void test_set_width(int value);
const char* test_style(const char* selector, const char* property);
extern int test_prevented, test_shift, test_active_index, test_file_bytes, test_read_requests;
extern char test_key[32], test_event_id[80];
extern int test_has_files, test_drop_count, test_drop_bytes, test_drop_reads, test_plain_click;
extern char test_drop_filename[1024];
extern char test_discussion_term[96], test_discussion_description[8192],
    test_discussion_backlink[2048];
#endif
