/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#include "DeleteRangeTxn.h"
#include "nsIDOMRange.h"
#include "nsIDOMCharacterData.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMSelection.h"
#include "DeleteTextTxn.h"
#include "DeleteElementTxn.h"
#include "TransactionFactory.h"
#include "nsISupportsArray.h"

#ifdef NS_DEBUG
#include "nsIDOMElement.h"
#endif

static NS_DEFINE_IID(kDeleteTextTxnIID,     DELETE_TEXT_TXN_IID);
static NS_DEFINE_IID(kDeleteElementTxnIID,  DELETE_ELEMENT_TXN_IID);

#ifdef NS_DEBUG
static PRBool gNoisy = PR_FALSE;
#else
static const PRBool gNoisy = PR_FALSE;
#endif

// note that aEditor is not refcounted
DeleteRangeTxn::DeleteRangeTxn()
  : EditAggregateTxn()
{
}

NS_IMETHODIMP DeleteRangeTxn::Init(nsIEditor *aEditor, nsIDOMRange *aRange)
{
  if (aEditor && aRange)
  {
    mEditor = do_QueryInterface(aEditor);
    nsresult result = aRange->GetStartParent(getter_AddRefs(mStartParent));
    NS_ASSERTION((NS_SUCCEEDED(result)), "GetStartParent failed.");
    result = aRange->GetEndParent(getter_AddRefs(mEndParent));
    NS_ASSERTION((NS_SUCCEEDED(result)), "GetEndParent failed.");
    result = aRange->GetStartOffset(&mStartOffset);
    NS_ASSERTION((NS_SUCCEEDED(result)), "GetStartOffset failed.");
    result = aRange->GetEndOffset(&mEndOffset);
    NS_ASSERTION((NS_SUCCEEDED(result)), "GetEndOffset failed.");
    result = aRange->GetCommonParent(getter_AddRefs(mCommonParent));
    NS_ASSERTION((NS_SUCCEEDED(result)), "GetCommonParent failed.");

#ifdef NS_DEBUG
  {
    PRUint32 count;
    nsCOMPtr<nsIDOMCharacterData> textNode;
    textNode = do_QueryInterface(mStartParent, &result);
    if (textNode)
      textNode->GetLength(&count);
    else
    {
      nsCOMPtr<nsIDOMNodeList> children;
      result = mStartParent->GetChildNodes(getter_AddRefs(children));
      NS_ASSERTION(((NS_SUCCEEDED(result)) && children), "bad start child list");
      children->GetLength(&count);
    }
    NS_ASSERTION(mStartOffset<=count, "bad start offset");

    textNode = do_QueryInterface(mEndParent, &result);
    if (textNode)
      textNode->GetLength(&count);
    else
    {
      nsCOMPtr<nsIDOMNodeList> children;
      result = mEndParent->GetChildNodes(getter_AddRefs(children));
      NS_ASSERTION(((NS_SUCCEEDED(result)) && children), "bad end child list");
      children->GetLength(&count);
    }
    NS_ASSERTION(mEndOffset<=count, "bad end offset");
    if (gNoisy)
      printf ("DeleteRange: %d of %p to %d of %p\n", 
               mStartOffset, (void *)mStartParent, mEndOffset, (void *)mEndParent);
  }
#endif
    return result;
  }
  else
    return NS_ERROR_NULL_POINTER;
}

DeleteRangeTxn::~DeleteRangeTxn()
{
}

NS_IMETHODIMP DeleteRangeTxn::Do(void)
{
  if (!mStartParent || !mEndParent || !mCommonParent)
    return NS_ERROR_NULL_POINTER;

  nsresult result; 

  // build the child transactions

  if (mStartParent==mEndParent)
  { // the selection begins and ends in the same node
    result = CreateTxnsToDeleteBetween(mStartParent, mStartOffset, mEndOffset);
  }
  else
  { // the selection ends in a different node from where it started
    // delete the relevant content in the start node
    result = CreateTxnsToDeleteContent(mStartParent, mStartOffset, nsIEditor::eLTR);
    if (NS_SUCCEEDED(result))
    {
      // delete the intervening nodes
      result = CreateTxnsToDeleteNodesBetween(mCommonParent, mStartParent, mEndParent);
      if (NS_SUCCEEDED(result))
      {
        // delete the relevant content in the end node
        result = CreateTxnsToDeleteContent(mEndParent, mEndOffset, nsIEditor::eRTL);
      }
    }
  }

  // if we've successfully built this aggregate transaction, then do it.
  if (NS_SUCCEEDED(result)) {
    result = EditAggregateTxn::Do();
  }

  if (NS_SUCCEEDED(result)) {
    // set the resulting selection
    nsCOMPtr<nsIDOMSelection> selection;
    result = mEditor->GetSelection(getter_AddRefs(selection));
    if (NS_SUCCEEDED(result)) {
      result = selection->Collapse(mStartParent, mStartOffset);
    }
  }

  return result;
}

NS_IMETHODIMP DeleteRangeTxn::Undo(void)
{
  if (!mStartParent || !mEndParent || !mCommonParent)
    return NS_ERROR_NULL_POINTER;

  nsresult result = EditAggregateTxn::Undo();

  if (NS_SUCCEEDED(result)) {
    // set the resulting selection
    nsCOMPtr<nsIDOMSelection> selection;
    result = mEditor->GetSelection(getter_AddRefs(selection));
    if (NS_SUCCEEDED(result)) {
      result = selection->Collapse(mStartParent, mStartOffset);
    }
  }

  return result;
}

NS_IMETHODIMP DeleteRangeTxn::Redo(void)
{
  if (!mStartParent || !mEndParent || !mCommonParent)
    return NS_ERROR_NULL_POINTER;

  nsresult result = EditAggregateTxn::Redo();

  if (NS_SUCCEEDED(result)) {
    // set the resulting selection
    nsCOMPtr<nsIDOMSelection> selection;
    result = mEditor->GetSelection(getter_AddRefs(selection));
    if (NS_SUCCEEDED(result)) {
      result = selection->Collapse(mStartParent, mStartOffset);
    }
  }

  return result;
}

NS_IMETHODIMP DeleteRangeTxn::Merge(PRBool *aDidMerge, nsITransaction *aTransaction)
{
  if (nsnull!=aDidMerge)
    *aDidMerge=PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP DeleteRangeTxn::Write(nsIOutputStream *aOutputStream)
{
  return NS_OK;
}

NS_IMETHODIMP DeleteRangeTxn::GetUndoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Insert Range: ";
  }
  return NS_OK;
}

NS_IMETHODIMP DeleteRangeTxn::GetRedoString(nsString **aString)
{
  if (nsnull!=aString)
  {
    **aString="Remove Range: ";
  }
  return NS_OK;
}

NS_IMETHODIMP DeleteRangeTxn::CreateTxnsToDeleteBetween(nsIDOMNode *aStartParent, 
                                                   PRUint32    aStartOffset, 
                                                   PRUint32    aEndOffset)
{
  nsresult result;
  // see what kind of node we have
  nsCOMPtr<nsIDOMCharacterData> textNode;
  textNode =  do_QueryInterface(aStartParent);
  if (textNode)
  { // if the node is a text node, then delete text content
    DeleteTextTxn *txn;
    result = TransactionFactory::GetNewTransaction(kDeleteTextTxnIID, (EditTxn **)&txn);
    if (nsnull!=txn)
    {
      PRInt32 numToDel;
      if (aStartOffset==aEndOffset)
        numToDel = 1;
      else
        numToDel = aEndOffset-aStartOffset;
      txn->Init(mEditor, textNode, aStartOffset, numToDel);
      AppendChild(txn);
    }
  }
  else
  {
    PRUint32 childCount;
    nsCOMPtr<nsIDOMNodeList> children;
    result = aStartParent->GetChildNodes(getter_AddRefs(children));
    if ((NS_SUCCEEDED(result)) && children)
    {
      children->GetLength(&childCount);
      NS_ASSERTION(aEndOffset<childCount, "bad aEndOffset");
      PRUint32 i;
      for (i=aStartOffset; i<=aEndOffset; i++)
      {
        nsCOMPtr<nsIDOMNode> child;
        result = children->Item(i, getter_AddRefs(child));
        if ((NS_SUCCEEDED(result)) && child)
        {
          DeleteElementTxn *txn;
          result = TransactionFactory::GetNewTransaction(kDeleteElementTxnIID, (EditTxn **)&txn);
          if (nsnull!=txn)
          {
            txn->Init(child);
            AppendChild(txn);
          }
          else
            return NS_ERROR_NULL_POINTER;
        }
      }
    }
  }
  return result;
}

NS_IMETHODIMP DeleteRangeTxn::CreateTxnsToDeleteContent(nsIDOMNode *aParent, 
                                                        PRUint32    aOffset, 
                                                        nsIEditor::Direction aDir)
{
  nsresult result;
  // see what kind of node we have
  nsCOMPtr<nsIDOMCharacterData> textNode;
  textNode = do_QueryInterface(aParent);
  if (textNode)
  { // if the node is a text node, then delete text content
    PRUint32 start, numToDelete;
    if (nsIEditor::eLTR==aDir)
    {
      start=aOffset;
      textNode->GetLength(&numToDelete);
      numToDelete -= aOffset;
    }
    else
    {
      start=0;
      numToDelete=aOffset;
    }
    DeleteTextTxn *txn;
    result = TransactionFactory::GetNewTransaction(kDeleteTextTxnIID, (EditTxn **)&txn);
    if (nsnull!=txn)
    {
      txn->Init(mEditor, textNode, start, numToDelete);
      AppendChild(txn);
    }
    else
      return NS_ERROR_NULL_POINTER;
  }
  else
  { // we have an interior node, so delete some of its children
    if (nsIEditor::eLTR==aDir)
    { // delete from aOffset to end
      PRUint32 childCount;
      nsCOMPtr<nsIDOMNodeList> children;
      result = aParent->GetChildNodes(getter_AddRefs(children));
      if ((NS_SUCCEEDED(result)) && children)
      {
        children->GetLength(&childCount);
        PRUint32 i;
        for (i=aOffset; i<childCount; i++)
        {
          nsCOMPtr<nsIDOMNode> child;
          result = children->Item(i, getter_AddRefs(child));
          if ((NS_SUCCEEDED(result)) && child)
          {
            DeleteElementTxn *txn;
            result = TransactionFactory::GetNewTransaction(kDeleteElementTxnIID, (EditTxn **)&txn);
            if (nsnull!=txn)
            {
              txn->Init(child);
              AppendChild(txn);
            }
            else
              return NS_ERROR_NULL_POINTER;
          }
        }
      }
    }
    else
    { // delete from 0 to aOffset
      nsCOMPtr<nsIDOMNode> child;
      result = aParent->GetFirstChild(getter_AddRefs(child));
      for (PRUint32 i=0; i<aOffset; i++)
      {
        if ((NS_SUCCEEDED(result)) && child)
        {
          DeleteElementTxn *txn;
          result = TransactionFactory::GetNewTransaction(kDeleteElementTxnIID, (EditTxn **)&txn);
          if (nsnull!=txn)
          {
            txn->Init(child);
            AppendChild(txn);
          }
          else
            return NS_ERROR_NULL_POINTER;
        }
        nsCOMPtr<nsIDOMNode> temp = child;
        result = temp->GetNextSibling(getter_AddRefs(child));
      }
    }    
  }

  return result;
}

NS_IMETHODIMP DeleteRangeTxn::CreateTxnsToDeleteNodesBetween(nsIDOMNode *aCommonParent, 
                                                        nsIDOMNode *aFirstChild,
                                                        nsIDOMNode *aLastChild)
{
  nsresult result;
  PRBool needToProcessLastChild=PR_TRUE;  // set to false if we discover we can delete all required nodes by just walking up aFirstChild's parent list
  nsCOMPtr<nsIDOMNode> parent;    // the current parent in the iteration up the ancestors
  nsCOMPtr<nsIDOMNode> child;     // the current child of parent
  nsISupportsArray *ancestorList; // the ancestorList of the other endpoint, used to gate deletion
  NS_NewISupportsArray(&ancestorList);
  if (nsnull==ancestorList)
    return NS_ERROR_NULL_POINTER;

  // Walk up the parent list of aFirstChild to aCommonParent,
  // deleting all siblings to the right of the ancestors of aFirstChild.
  BuildAncestorList(aLastChild, ancestorList); 
  child = do_QueryInterface(aFirstChild);
  result = child->GetParentNode(getter_AddRefs(parent));
  while ((NS_SUCCEEDED(result)) && parent)
  {
    while ((NS_SUCCEEDED(result)) && child)
    { // this loop starts with the first sibling of an ancestor of aFirstChild
      nsCOMPtr<nsIDOMNode> temp = child;
      result = temp->GetNextSibling(getter_AddRefs(child));
      if ((NS_SUCCEEDED(result)) && child)
      {
        if (child.get()==aLastChild)
        { // aFirstChild and aLastChild have the same parent, and we've reached aLastChild
          needToProcessLastChild = PR_FALSE;
          break;  
        }
        // test if child is an ancestor of the other node.  If it is, don't process this parent anymore
        PRInt32 index;
        nsISupports *childAsISupports;
        child->QueryInterface(nsISupports::GetIID(), (void **)&childAsISupports);
        index = ancestorList->IndexOf(childAsISupports);
        if (-1!=index)
          break;
#ifdef NS_DEBUG
        // begin debug output
        nsCOMPtr<nsIDOMElement> childElement;
        childElement = do_QueryInterface(child, &result);
        nsAutoString childElementTag="text node";
        if (childElement)
          childElement->GetTagName(childElementTag);
        nsCOMPtr<nsIDOMElement> parentElement;
        parentElement = do_QueryInterface(parent, &result);
        nsAutoString parentElementTag="text node";
        if (parentElement)
          parentElement->GetTagName(parentElementTag);
        char *c, *p;
        c = childElementTag.ToNewCString();
        p = parentElementTag.ToNewCString();
        if (c&&p)
        {
          if (gNoisy)
            printf("DeleteRangeTxn (1):  building txn to delete child %s (pointer address %p) from parent %s\n", 
                   c, childAsISupports, p); 
          delete [] c;
          delete [] p;
        }
        // end debug output
#endif
        DeleteElementTxn *txn;
        result = TransactionFactory::GetNewTransaction(kDeleteElementTxnIID, (EditTxn **)&txn);
        if (nsnull!=txn)
        {
          txn->Init(child);
          AppendChild(txn);
        }
        else
          return NS_ERROR_NULL_POINTER;
      }
    }
    if (parent.get()==aCommonParent)
      break;
    child = parent;
    nsCOMPtr<nsIDOMNode> temp=parent;
    result = temp->GetParentNode(getter_AddRefs(parent));
  }
  
  // Walk up the parent list of aLastChild to aCommonParent,
  // deleting all siblings to the left of the ancestors of aLastChild.
  BuildAncestorList(aFirstChild, ancestorList);
  if (PR_TRUE==needToProcessLastChild)
  {
    child = do_QueryInterface(aLastChild);
    result = child->GetParentNode(getter_AddRefs(parent));
    while ((NS_SUCCEEDED(result)) && parent)
    {
      if (parent.get()==aCommonParent)
        break; // notice that when we go up the aLastChild parent chain, we don't touch aCommonParent
      while ((NS_SUCCEEDED(result)) && child)
      { // this loop starts with the first sibling of an ancestor of aFirstChild
        nsCOMPtr<nsIDOMNode> temp = child;
        result = temp->GetPreviousSibling(getter_AddRefs(child));
        if ((NS_SUCCEEDED(result)) && child)
        {
          // test if child is an ancestor of the other node.  If it is, don't process this parent anymore
          PRInt32 index;
          index = ancestorList->IndexOf((nsISupports*)child);
          if (-1!=index)
            break;
#ifdef NS_DEBUG
          // begin debug output
          nsCOMPtr<nsIDOMElement> childElement;
          childElement = do_QueryInterface(child, &result);
          nsAutoString childElementTag="text node";
          if (childElement)
            childElement->GetTagName(childElementTag);
          nsCOMPtr<nsIDOMElement> parentElement;
          parentElement = do_QueryInterface(parent, &result);
          nsAutoString parentElementTag="text node";
          if (parentElement)
            parentElement->GetTagName(parentElementTag);
          char *c, *p;
          c = childElementTag.ToNewCString();
          p = parentElementTag.ToNewCString();
          if (c&&p)
          {
            if (gNoisy)
              printf("DeleteRangeTxn (2):  building txn to delete child %s from parent %s\n", c, p); 
            delete [] c;
            delete [] p;
          }
          // end debug output
#endif
          DeleteElementTxn *txn;
          result = TransactionFactory::GetNewTransaction(kDeleteElementTxnIID, (EditTxn **)&txn);
          if (nsnull!=txn)
          {
            txn->Init(child);
            AppendChild(txn);
          }
          else
            return NS_ERROR_NULL_POINTER;
        }
      }
      child = parent;
      nsCOMPtr<nsIDOMNode> temp=parent;
      result = temp->GetParentNode(getter_AddRefs(parent));
    }
  }
  NS_RELEASE(ancestorList);
  return result;
}

// XXX: probably want to move this to editor as a standard support method
NS_IMETHODIMP DeleteRangeTxn::BuildAncestorList(nsIDOMNode *aNode, nsISupportsArray *aList)
{
  nsresult result=NS_OK;
  if (nsnull!=aNode && nsnull!=aList)
  {
    aList->Clear();
    nsCOMPtr<nsIDOMNode> parent;
    nsCOMPtr<nsIDOMNode> child(do_QueryInterface(aNode));
    result = child->GetParentNode(getter_AddRefs(parent));
    while ((NS_SUCCEEDED(result)) && child && parent)
    {
      nsISupports * parentAsISupports;
      parent->QueryInterface(nsISupports::GetIID(), (void **)&parentAsISupports);
      aList->AppendElement(parentAsISupports);
#ifdef NS_DEBUG
        // begin debug output
        nsCOMPtr<nsIDOMElement> parentElement;
        parentElement = do_QueryInterface(parent, &result);
        nsAutoString parentElementTag="text node";
        if (parentElement)
          parentElement->GetTagName(parentElementTag);
        char *p;
        p = parentElementTag.ToNewCString();
        if (p)
        {
          if (gNoisy)
            printf("BuildAncestorList:  adding parent %s (pointer address %p)\n", p, parentAsISupports); 
          delete [] p;
        }
        // end debug output
#endif
      child = parent;
      nsCOMPtr<nsIDOMNode> temp=parent;
      result = temp->GetParentNode(getter_AddRefs(parent));
    }
  }
  else
    result = NS_ERROR_NULL_POINTER;

  return result;
}


