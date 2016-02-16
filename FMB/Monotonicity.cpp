/**
 * @file Monotonicity.cpp
 * Implements class Monotonicity.
 *
 */

#include "Forwards.hpp"

#include "Lib/Stack.hpp"
#include "Lib/DHMap.hpp"
#include "Lib/Environment.hpp"
#include "Lib/List.hpp"

#include "Kernel/Unit.hpp"
#include "Kernel/Term.hpp"
#include "Kernel/Clause.hpp"
#include "Kernel/SortHelper.hpp"

#include "SAT/SATSolver.hpp"
#include "SAT/SATLiteral.hpp"
#include "SAT/SATClause.hpp"
#include "SAT/MinisatInterfacing.hpp"

#include "Monotonicity.hpp"

namespace FMB
{

Monotonicity::Monotonicity(ClauseList* clauses, unsigned srt) : _srt(srt)
{
 CALL("Monotonicity::Monotonicity");

  _solver = new MinisatInterfacing(*env.options,true);

 // create pt and pf per predicate and add the constraint -pf | -pt
 for(unsigned p=1;p<env.signature->predicates();p++){
   _pT.insert(p,SATLiteral(_solver->newVar(),true));
   _pF.insert(p,SATLiteral(_solver->newVar(),true));

   Stack<SATLiteral> slits;
   slits.push(_pT.get(p).opposite()); 
   slits.push(_pF.get(p).opposite()); 
   _solver->addClause(SATClause::fromStack(slits));
 }

 ClauseIterator cit = pvi(ClauseList::Iterator(clauses));
 while(cit.hasNext()){
   Clause* c = cit.next();
   Clause::Iterator lit(*c);
   while(lit.hasNext()){
     Literal* l = lit.next();
     monotone(c,l);
   }
 }

 SATSolver::Status status = _solver->solve();
 ASS(status!=SATSolver::Status::UNKNOWN);
 _result = (status == SATSolver::Status::SATISFIABLE);
}


void Monotonicity::monotone(Clause* c, Literal* l)
{
  CALL("Monotonicity::monotone");

  // if we have equality
  if(l->isEquality()){
    TermList* t1 = l->nthArgument(0); 
    TermList* t2 = l->nthArgument(1); 
    // t1 = t2
    if(l->polarity()){
      // add a clause for each
      safe(c,l,t1);
      safe(c,l,t2);
    }
    // t1 != t2
    else{
      // the true clause, skip
    }
  }
  else{
  // not equality
    unsigned p = l->functor();
    SATLiteral add = (l->polarity() ? _pF.get(p) : _pT.get(p)).opposite();
    for(unsigned i=0;i<l->arity();i++){
      TermList* t = l->nthArgument(i);
      safe(c,l,t,add);
    }
  }
}

void Monotonicity::safe(Clause* c, Literal* parent, TermList* t){
  Stack<SATLiteral> slits;
  safe(c,parent,t,slits);
}
void Monotonicity::safe(Clause* c, Literal* parent, TermList* t,SATLiteral add){
  Stack<SATLiteral> slits;
  slits.push(add);
  safe(c,parent,t,slits);
}

void Monotonicity::safe(Clause* c, Literal* parent, TermList* t,Stack<SATLiteral>& slits)
{
  CALL("Monotonicity::safe");
  if(t->isVar()){
    unsigned var = t->var();
    unsigned s = SortHelper::getVariableSort(*t,parent);
    if(s==_srt){
      Clause::Iterator lit(*c);
      while(lit.hasNext()){
        Literal* l = lit.next(); 
        if(guards(l,var,slits)){
          // if guards returns true it means true will be added to the clause
          // so don't bother creating it
          return;
        } 
      } 
      _solver->addClause(SATClause::fromStack(slits));
    }
  }
  // otherwise true clause, so ignore
}

bool Monotonicity::guards(Literal* l, unsigned var, Stack<SATLiteral>& slits)
{
  CALL("Monotonicyt::guards");

  if(l->isEquality()){
    // check for X != f(...) or f(...) != X
    // i.e. a negative equality with X on one side
    if(!l->polarity()){
      TermList* t1 = l->nthArgument(0);
      TermList* t2 = l->nthArgument(1);
      if(t1->isVar() && t1->var()==var) return true; 
      if(t2->isVar() && t2->var()==var) return true; 
    }
  }
  else{
    // check if l contains X 
    unsigned p = l->functor();
    for(unsigned i=0;i<l->arity();i++){
      TermList* t = l->nthArgument(i);
      if(t->isVar() && t->var()==var){
        SATLiteral slit = l->polarity() ? _pT.get(p) : _pF.get(p);
        slits.push(slit);
        return false; // not the true literal
      }
    }
  }
  return false; // not the true literal
}


void Monotonicity::addSortPredicates(ClauseList*& clauses)
{
  CALL("Monotonicity::addSortPredicates");

  // First compute the monotonic sorts
  DArray<bool> isMonotonic(env.sorts->sorts());
  for(unsigned s=0;s<env.sorts->sorts();s++){
    if(env.property->usesSort(s)){
      Monotonicity m(clauses,s);
      bool monotonic = m.check();
      isMonotonic[s] = monotonic;
      //if(!monotonic){ cout << env.sorts->sortName(s) << " NOT M" << endl; }
    }
    else{ isMonotonic[s] = true; } // We are monotonic in a sort we do not use!!
  }

  // Now create a sort predicate per non-monotonic sort
  DArray<unsigned> sortPredicates(env.sorts->sorts());
  for(unsigned s=0;s<env.sorts->sorts();s++){
    if(!isMonotonic[s]){
      vstring name = "sortPredicate_"+env.sorts->sortName(s);
      unsigned p = env.signature->addFreshPredicate(1,name.c_str());
      env.signature->getPredicate(p)->setType(new PredicateType(s));
      sortPredicates[s] = p;
    }
    else{ sortPredicates[s]=0; }
  }

  // The newAxioms clause list
  ClauseList* newAxioms = 0;

  // Now add the relevant axioms for these new sort predicates i.e.
  // 1) ?[X] : p(X) (need skolem constant) = p(sk)
  // 2) for each function f with return sort s 
  //    !args : p(f(args))
  for(unsigned s=0;s<env.sorts->sorts();s++){
    if(isMonotonic[s]) continue;

    unsigned p = sortPredicates[s];
    ASS(p>0);

    // First the function axioms
    for(unsigned f=0; f < env.signature->functions(); f++){

      if(env.signature->getFunction(f)->fnType()->result() != s) continue;

      unsigned arity = env.signature->functionArity(f);
      static Stack<TermList> vars;
      vars.reset();
      for(unsigned x=0;x<arity;x++) vars.push(TermList(x,false)); 

      Term* fX = Term::create(f,arity,vars.begin());
      Literal* pfX = Literal::create1(p,true,TermList(fX));
      Clause* fINs = new(1) Clause(1,Unit::InputType::AXIOM, new Inference(Inference::INPUT));
      (*fINs)[0] = pfX;
      ClauseList::push(fINs,newAxioms);
    } 

    // Next the non-empty constraint
    unsigned skolemConstant = env.signature->addSkolemFunction(0);
    env.signature->getFunction(skolemConstant)->setType(new FunctionType(s));
    Literal* psk = Literal::create1(p,true,TermList(Term::createConstant(skolemConstant)));
    Clause* nonEmpty = new(1) Clause(1,Unit::InputType::AXIOM, new Inference(Inference::INPUT));
    (*nonEmpty)[0] = psk;
    ClauseList::push(nonEmpty,newAxioms);

  }


  // Go through previous clauses and
  // i) keep a clause if it only has variables of monotonic sort
  // ii) replace clauses with variables of non-monotic sort by adding literal(s) ~p(X)
   ClauseList::DelIterator it(clauses);
   while(it.hasNext()){
     Clause* cl = it.next();
     // pair(variable,variableSort)
     static Stack<std::pair<unsigned,unsigned>> sortedVariables;
     sortedVariables.reset();

     DHMap<unsigned,unsigned> varSorts;
     SortHelper::collectVariableSorts(cl,varSorts); 
     for(unsigned v=0;v<cl->varCnt();v++){
       unsigned vsrt;
       if(varSorts.find(v,vsrt)){
         if(!isMonotonic[vsrt]) sortedVariables.push(make_pair(v,vsrt));
       }
     }
     
     if(!sortedVariables.isEmpty()){

       Stack<Literal*> literals; 
       literals.loadFromIterator(Clause::Iterator(*cl)); 

       Stack<std::pair<unsigned,unsigned>>::Iterator vit(sortedVariables);
       while(vit.hasNext()){
         std::pair<unsigned,unsigned> pair = vit.next();
         unsigned var = pair.first;
         unsigned varSort = pair.second;
         unsigned p = sortPredicates[varSort];
         ASS(p>0);
         ASS(!isMonotonic[varSort]);
         Literal* guard = Literal::create1(p,false,TermList(var,false));
         literals.push(guard);
       }

       Clause* replacement = Clause::fromStack(literals,cl->inputType(),
                                   new Inference1(Inference::ADD_SORT_PREDICATES, cl)); 
       ClauseList::push(replacement,newAxioms);
       //cout << replacement->toString() << endl;
       it.del(); 
     }
   }

   clauses = ClauseList::concat(clauses,newAxioms);
}

}