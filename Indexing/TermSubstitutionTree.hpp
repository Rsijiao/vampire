
/*
 * File TermSubstitutionTree.hpp.
 *
 * This file is part of the source code of the software program
 * Vampire. It is protected by applicable
 * copyright laws.
 *
 * This source code is distributed under the licence found here
 * https://vprover.github.io/license.html
 * and in the source directory
 *
 * In summary, you are allowed to use Vampire for non-commercial
 * purposes but not allowed to distribute, modify, copy, create derivatives,
 * or use in competitions. 
 * For other uses of Vampire please contact developers for a different
 * licence, which we will make an effort to provide. 
 */
/**
 * @file TermSubstitutionTree.hpp
 * Defines class TermSubstitutionTree.
 */


#ifndef __TermSubstitutionTree__
#define __TermSubstitutionTree__


#include "Kernel/Renaming.hpp"
#include "Lib/SkipList.hpp"

#include "Index.hpp"
#include "TermIndexingStructure.hpp"
#include "SubstitutionTree.hpp"

namespace Indexing {

class TermSubstitutionTree
: public TermIndexingStructure, SubstitutionTree
{
public:
  CLASS_NAME(TermSubstitutionTree);
  USE_ALLOCATOR(TermSubstitutionTree);

  explicit TermSubstitutionTree(bool useC=false);

  void insert(TermList t, Literal* lit, Clause* cls) override;
  void remove(TermList t, Literal* lit, Clause* cls);

  bool generalizationExists(TermList t) override;


  TermQueryResultIterator getUnifications(TermList t,
	  bool retrieveSubstitutions) override;

  TermQueryResultIterator getUnificationsWithConstraints(TermList t,
          bool retrieveSubstitutions) override;

  TermQueryResultIterator getGeneralizations(TermList t,
	  bool retrieveSubstitutions) override;

  TermQueryResultIterator getInstances(TermList t,
	  bool retrieveSubstitutions) override;

#if VDEBUG
  virtual void markTagged() override{ SubstitutionTree::markTagged();}
#endif

private:
  void handleTerm(TermList t, Literal* lit, Clause* cls, bool insert);

  struct TermQueryResultFn;

  template<class Iterator>
  TermQueryResultIterator getResultIterator(Term* term,
	  bool retrieveSubstitutions,bool withConstraints);

  struct LDToTermQueryResultFn;
  struct LDToTermQueryResultWithSubstFn;
  struct LeafToLDIteratorFn;
  struct UnifyingContext;

  template<class LDIt>
  TermQueryResultIterator ldIteratorToTQRIterator(LDIt ldIt,
	  TermList queryTerm, bool retrieveSubstitutions,
          bool withConstraints);

  TermQueryResultIterator getAllUnifyingIterator(TermList trm,
	  bool retrieveSubstitutions,bool withConstraints);

  inline
  unsigned getRootNodeIndex(Term* t)
  {
    return t->functor();
  }


  typedef SkipList<LeafData,LDComparator> LDSkipList;
  LDSkipList _vars;
};

};// namespace Indexing

#endif // INDEXING_TERMSUBSTITUTIONTREE_HPP_
