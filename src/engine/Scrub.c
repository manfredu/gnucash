/********************************************************************\
 * Scrub.c -- convert single-entry accounts into clean double-entry *
 *                                                                  *
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, contact:                        *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 59 Temple Place - Suite 330        Fax:    +1-617-542-2652       *
 * Boston, MA  02111-1307,  USA       gnu@gnu.org                   *
 *                                                                  *
\********************************************************************/

/*
 * FILE:
 * Scrub.c
 *
 * FUNCTION:
 * Provides a set of functions and utilities for scrubbing clean 
 * single-entry accounts so that they can be promoted into 
 * self-consistent, clean double-entry accounts.
 *
 * HISTORY:
 * Created by Linas Vepstas December 1998
 * Copyright (c) 1998, 1999, 2000 Linas Vepstas
 */

#include "config.h"

#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "Account.h"
#include "Group.h"
#include "GroupP.h"
#include "Scrub.h"
#include "Transaction.h"
#include "TransactionP.h"
#include "gnc-engine-util.h"
#include "messages.h"

static short module = MOD_SCRUB;
static Account * GetOrMakeAccount (AccountGroup *root, Transaction *trans,
                                   const char *name_root);

/* ================================================================ */

void
xaccGroupScrubOrphans (AccountGroup *grp)
{
  GList *list;
  GList *node;

  if (!grp)
    return;

  list = xaccGroupGetAccountList (grp);

  for (node = list; node; node = node->next)
  {
    Account *account = node->data;

    xaccAccountTreeScrubOrphans (account);
  }
}

void
xaccAccountTreeScrubOrphans (Account *acc)
{
  if (!acc)
    return;

  xaccGroupScrubOrphans (xaccAccountGetChildren(acc));
  xaccAccountScrubOrphans (acc);
}

void
xaccAccountScrubOrphans (Account *acc)
{
  GList *node;
  const char *str;

  if (!acc)
    return;

  str = xaccAccountGetName (acc);
  str = str ? str : "(null)";
  PINFO ("Looking for orphans in account %s \n", str);

  for (node = xaccAccountGetSplitList(acc); node; node = node->next)
  {
    Split *split = node->data;

    xaccTransScrubOrphans (xaccSplitGetParent (split),
                           xaccGetAccountRoot (acc));
  }
}

void
xaccTransScrubOrphans (Transaction *trans, AccountGroup *root)
{
  GList *node;

  if (!trans)
    return;

  for (node = xaccTransGetSplitList (trans); node; node = node->next)
  {
    Split *split = node->data;
    Account *account;
    Account *orph;

    account = xaccSplitGetAccount (split);
    if (account)
      continue;

    DEBUG ("Found an orphan \n");

    orph = GetOrMakeAccount (root, trans, _("Orphan"));

    xaccAccountBeginEdit (orph);
    xaccAccountInsertSplit (orph, split);
    xaccAccountCommitEdit (orph);
  }
}

/* ================================================================ */

void
xaccGroupScrubSplits (AccountGroup *group)
{
  GList *list;
  GList *node;

  if (!group) return;

  list = xaccGroupGetAccountList (group);

  for (node = list; node; node = node->next)
  {
    Account *account = node->data;

    xaccAccountTreeScrubSplits (account);
  }
}

void
xaccAccountTreeScrubSplits (Account *account)
{
  xaccGroupScrubSplits (xaccAccountGetChildren(account));
  xaccAccountScrubSplits (account);
}

void
xaccAccountScrubSplits (Account *account)
{
  GList *node;

  for (node = xaccAccountGetSplitList (account); node; node = node->next)
    xaccSplitScrub (node->data);
}

void
xaccTransScrubSplits (Transaction *trans)
{
  GList *node;

  if (!trans)
    return;

  for (node = trans->splits; node; node = node->next)
    xaccSplitScrub (node->data);
}

void
xaccSplitScrub (Split *split)
{
  Account *account;
  Transaction *trans;
  gnc_numeric value;
  gboolean trans_was_open;
  gnc_commodity *commodity;
  gnc_commodity *currency;
  int scu;

  if (!split)
    return;

  trans = xaccSplitGetParent (split);
  if (!trans)
    return;

  account = xaccSplitGetAccount (split);
  if (!account)
  {
    value = xaccSplitGetValue (split);

    if (gnc_numeric_same (xaccSplitGetAmount (split),
                          xaccSplitGetValue (split),
                          value.denom, GNC_RND_ROUND))
      return;

    xaccSplitSetAmount (split, value);

    return;
  }

  commodity = xaccAccountGetCommodity (account);
  currency = xaccTransGetCurrency (trans);

  if (!commodity || !gnc_commodity_equiv (commodity, currency))
    return;

  scu = MIN (xaccAccountGetCommoditySCU (account),
             gnc_commodity_get_fraction (currency));

  value = xaccSplitGetValue (split);

  if (gnc_numeric_same (xaccSplitGetAmount (split),
                        value, scu, GNC_RND_ROUND))
    return;

  PINFO ("split with mismatched values");

  trans_was_open = xaccTransIsOpen (trans);

  if (!trans_was_open)
    xaccTransBeginEdit (trans);

  xaccSplitSetAmount (split, value);

  if (!trans_was_open)
    xaccTransCommitEdit (trans);
}

/* ================================================================ */

void
xaccGroupScrubImbalance (AccountGroup *grp)
{
  GList *list;
  GList *node;

  if (!grp) return;

  list = xaccGroupGetAccountList (grp);

  for (node = list; node; node = node->next)
  {
    Account *account = node->data;

    xaccAccountTreeScrubImbalance (account);
  }
}

void
xaccAccountTreeScrubImbalance (Account *acc)
{
  xaccGroupScrubImbalance (xaccAccountGetChildren(acc));
  xaccAccountScrubImbalance (acc);
}

void
xaccAccountScrubImbalance (Account *acc)
{
  GList *node;
  const char *str;

  str = xaccAccountGetName(acc);
  str = str ? str : "(null)";
  PINFO ("Looking for imbalance in account %s \n", str);

  for(node = xaccAccountGetSplitList(acc); node; node = node->next)
  {
    Split *split = node->data;
    Transaction *trans = xaccSplitGetParent(split);

    xaccTransScrubImbalance (trans, xaccGetAccountRoot (acc), NULL);
  }
}

void
xaccTransScrubImbalance (Transaction *trans, AccountGroup *root,
                         Account *parent)
{
  Split *balance_split = NULL;
  gnc_numeric imbalance;

  if (!trans)
    return;

  xaccTransScrubSplits (trans);

  {
    Account *account;
    GList *node;

    imbalance = xaccTransGetImbalance (trans);
    if (gnc_numeric_zero_p (imbalance))
      return;

    if (!parent)
      account = GetOrMakeAccount (root, trans, _("Imbalance"));
    else
      account = parent;

    for (node = xaccTransGetSplitList (trans); node; node = node->next)
    {
      Split *split = node->data;

      if (xaccSplitGetAccount (split) == account)
      {
        balance_split = split;
        break;
      }
    }

    /* put split into account before setting split value */
    if (!balance_split)
    {
      balance_split = xaccMallocSplit ();

      xaccAccountBeginEdit (account);
      xaccAccountInsertSplit (account, balance_split);
      xaccAccountCommitEdit (account);
    }
  }

  PINFO ("unbalanced transaction");

  {
    const gnc_commodity *currency;
    const gnc_commodity *commodity;
    gboolean trans_was_open;
    gnc_numeric new_value;
    Account *account;

    trans_was_open = xaccTransIsOpen (trans);

    if (!trans_was_open)
      xaccTransBeginEdit (trans);

    currency = xaccTransGetCurrency (trans);
    account = xaccSplitGetAccount (balance_split);

    new_value = xaccSplitGetValue (balance_split);

    new_value = gnc_numeric_sub (new_value, imbalance,
                                 new_value.denom, GNC_RND_ROUND);

    xaccSplitSetValue (balance_split, new_value);

    commodity = xaccAccountGetCommodity (account);
    if (gnc_commodity_equiv (currency, commodity))
      xaccSplitSetAmount (balance_split, new_value);

    if (!parent && gnc_numeric_zero_p (new_value))
    {
      xaccSplitDestroy (balance_split);
      balance_split = NULL;
    }

    if (balance_split)
      xaccTransAppendSplit (trans, balance_split);

    xaccSplitScrub (balance_split);

    if (!trans_was_open)
      xaccTransCommitEdit (trans);
  }
}

/* ================================================================ */

void
xaccTransScrubCurrency (Transaction *trans)
{
  gnc_commodity *currency;

  if (!trans) return;

  currency = xaccTransGetCurrency (trans);
  if (currency) return;

  currency = xaccTransFindOldCommonCurrency (trans);
  if (currency)
  {
    xaccTransBeginEdit (trans);
    xaccTransSetCurrency (trans, currency);
    xaccTransCommitEdit (trans);
  }
  else
  {
    PWARN ("no common transaction currency found");
  }
}

/* ================================================================ */

void
xaccAccountScrubCommodity (Account *account)
{
  gnc_commodity *commodity;

  if (!account) return;

  commodity = xaccAccountGetCommodity (account);
  if (commodity) return;

  commodity = DxaccAccountGetSecurity (account);
  if (commodity)
  {
    xaccAccountSetCommodity (account, commodity);
    return;
  }

  commodity = DxaccAccountGetCurrency (account);
  if (commodity)
  {
    xaccAccountSetCommodity (account, commodity);
    return;
  }

  PERR ("account with no commodity");
}

/* ================================================================ */

static gboolean
scrub_trans_currency_helper (Transaction *t, void *unused)
{
  xaccTransScrubCurrency (t);
  return TRUE;
}

static gpointer
scrub_account_commodity_helper (Account *account, gpointer unused)
{
  xaccAccountScrubCommodity (account);
  xaccAccountDeleteOldData (account);
  return NULL;
}

void
xaccGroupScrubCommodities (AccountGroup *group)
{
  if (!group) return;

  xaccAccountGroupBeginEdit (group);

  xaccGroupForEachTransaction (group, scrub_trans_currency_helper, NULL);

  xaccGroupForEachAccount (group, scrub_account_commodity_helper, NULL, TRUE);

  xaccAccountGroupCommitEdit (group);
}

/* ================================================================ */

static Account *
GetOrMakeAccount (AccountGroup *root, Transaction *trans,
                  const char *name_root)
{
  gnc_commodity * currency;
  char * accname;
  Account * acc;

  /* build the account name */
  currency = xaccTransGetCurrency (trans);

  accname = g_strconcat (name_root, "-",
                         gnc_commodity_get_mnemonic (currency), NULL);

  /* see if we've got one of these going already ... */
  acc = xaccGetAccountFromName (root, accname);

  if (acc == NULL)
  {
    /* guess not. We'll have to build one */
    acc = xaccMallocAccount ();
    xaccAccountBeginEdit (acc);
    xaccAccountSetName (acc, accname);
    xaccAccountSetCommodity (acc, currency);
    xaccAccountSetType (acc, BANK);

    /* hang the account off the root */
    xaccGroupInsertAccount (root, acc);
    xaccAccountCommitEdit (acc);
  }

  g_free (accname);

  return acc;
}

/* ==================== END OF FILE ==================== */
