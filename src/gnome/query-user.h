#ifndef __QUERY_USER_H__
#define __QUERY_USER_H__

#include "gnc-common.h"
#include <guile/gh.h>

enum
{
  GNC_QUERY_YES = -1,
  GNC_QUERY_NO = -2,
  GNC_QUERY_CANCEL = -3
};

void gnc_info_dialog( const char *message );
void gnc_warning_dialog(const char *message);

SCM gnc_choose_item_from_list_dialog(const char *title, SCM list_items);

#endif
