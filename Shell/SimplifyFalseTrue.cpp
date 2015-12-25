/**
 * @file SimplifyFalseTrue.cpp
 * Implements class SimplifyFalseTrue implementing simplification
 * of formulas containing true or false.
 *
 * @since 11/12/2004 Manchester
 */

#include "Kernel/Inference.hpp"
#include "Kernel/FormulaUnit.hpp"
#include "Kernel/Signature.hpp"

#include "Lib/Environment.hpp"

#include "SimplifyFalseTrue.hpp"

using namespace Kernel;
using namespace Shell;
using namespace Lib;

/**
 * Simplify the unit.
 *
 * @since 11/12/2004 Manchester
 * @since 14/04/2005 Manchester, return value changed to boolean.
 * @return the simplified unit, coincides with the input unit if not changed
 * @warning the unit must contain a formula
 * @since 09/06/2007 Manchester, changed to new datastructures
 */
FormulaUnit* SimplifyFalseTrue::simplify (FormulaUnit* unit)
{
  CALL("SimplifyFalseTrue::simplify(Unit*)");
  ASS(! unit->isClause());

  Formula* f = unit->formula();
  Formula* g = simplify(f);
  if (f == g) { // not simplified
    return unit;
  }

  FormulaUnit* res = new FormulaUnit(g,
			 new Inference1(Inference::REDUCE_FALSE_TRUE,unit),
			 unit->inputType());
  if(unit->included()) {
    res->markIncluded();
  }
  return res;
} // SimplifyFalseTrue::simplify


/**
 * Simplify subformula.
 *
 * @since 30/08/2002 Torrevieja, return type changed to void
 * @since 23/01/2004 Manchester, changed to include info about positions
 * @since 11/12/2004 Manchester, true and false added
 * @since 09/06/2007 Manchester, changed to new datastructures
 * @since 27/03/2008 Torrevieja, AND/OR case changed considerably
 */
Formula* SimplifyFalseTrue::simplify (Formula* f)
{
  CALL("SimplifyFalseTrue::simplify(Formula*)");

  Connective con = f->connective();
  switch (con) {
  case TRUE:
  case FALSE:
    return f;

  case BOOL_TERM:
    {
      TermList ts = simplify(f->getBooleanTerm());
      if (ts.isTerm()) {
        for (bool constant : { true, false }) {
          if (env.signature->isFoolConstantSymbol(constant, ts.term()->functor())) {
            return new Formula(constant);
          }
        }
      }
      return new BoolTermFormula(ts);
    }

  case LITERAL:
    {
      Literal* literal = f->literal();
      if (literal->isEquality()) {
        TermList arguments[2];
        for (unsigned argument : { 0u, 1u }) {
          arguments[argument] = simplify(*literal->nthArgument(argument));
        }

        for (unsigned argument : { 0u, 1u }) {
          if (!arguments[argument].isTerm()) continue;
          for (bool constant : { true, false }) {
            if (env.signature->isFoolConstantSymbol(constant, arguments[argument].term()->functor())) {
              TermList counterpart = arguments[argument == 0 ? 1 : 0];
              if (counterpart.isTerm()) {
                for (bool counterpartConstant : { true, false }) {
                  if (env.signature->isFoolConstantSymbol(counterpartConstant, counterpart.term()->functor())) {
                    // Lets say we have a boolean equality A = B with a sign C
                    // Its value is A xor B xor C
                    return new Formula(constant ^ counterpartConstant ^ (bool)literal->polarity());
                  }
                }
              }

              Formula* g = new BoolTermFormula(counterpart);
              if (literal->polarity() != constant) {
                g = new NegatedFormula(g);
              }
              return simplify(g);
            }
          }
        }
      }

      if (!literal->shared()) {
        Stack<TermList> arguments;
        Term::Iterator lit(literal);
        while (lit.hasNext()) {
          arguments.push(simplify(lit.next()));
        }
        Literal* processedLiteral = Literal::create(literal, arguments.begin());
        return new AtomicFormula(processedLiteral);
      }

      return f;
    }

  case AND:
  case OR: 
    {
      int length = 0;  // the length of the result
      bool changed = false;
      FormulaList* fs = f->args();
      DArray<Formula*> gs(fs->length());

      FormulaList::Iterator it(fs);
      while (it.hasNext()) {
	Formula* h = it.next();
	Formula* g = simplify(h);
	switch (g->connective()) {
	case TRUE:
	  if (con == OR) {
	    return g;
	  }
	  if (con == AND) {
	    changed = true;
	    break;
	  }
	  gs[length++] = g;
	  if (h != g) {
	    changed = true;
	  }
	  break;

	case FALSE:
	  if (con == AND) {
	    return g;
	  }
	  if (con == OR) {
	    changed = true;
	    break;
	  }
	  gs[length++] = g;
	  if (h != g) {
	    changed = true;
	  }
	  break;

	default:
	  gs[length++] = g;
	  if (h != g) {
	    changed = true;
	  }
	  break;
	}
      }
      if (! changed) {
	return f;
      }
      switch (length) {
      case 0:
	return new Formula(con == OR ? false : true);
      case 1:
	return gs[0];
      default:
	FormulaList* res = FormulaList::empty();
	for (int l = length-1;l >= 0;l--) {
	  FormulaList::push(gs[l],res);
	}
	return new JunctionFormula(con,res);
      }
    }

  case IMP:
    {
      Formula* right = simplify(f->right());
      if (right->connective() == TRUE) {
	return right;
      }
      Formula* left = simplify(f->left());

      switch (left->connective()) {
      case TRUE: // T -> R
	return right;
      case FALSE: // F -> R
	return new Formula(true);
      default: // L -> R;
	break;
      }

      if (right->connective() == FALSE) {
	return new NegatedFormula(left);
      }
      if (left == f->left() && right == f->right()) {
	return f;
      }
      return new BinaryFormula(con,left,right);
    }

  case IFF:
  case XOR: 
    {
      Formula* left = simplify(f->left());
      Formula* right = simplify(f->right());

      Connective lc = left->connective();
      Connective rc = right->connective();

      switch (lc) {
      case FALSE: // F * _
	switch (rc) {
	case FALSE: // F * F
	  return con == XOR
	         ? right
	         : new Formula(true);
	case TRUE: // F * T
	  return con == XOR
	         ? right
     	         : left;
	default: // F * R
	  return con == XOR
	         ? right
 	         : new NegatedFormula(right);
	}
      case TRUE: // T * _
	switch (rc) {
	case FALSE: // T * F
	  return con == XOR
	         ? left
	         : right;
	case TRUE: // T * T
	  return con == XOR
 	         ? new Formula(false)
     	         : left;
	default: // T * R
	  return con == XOR
 	         ? new NegatedFormula(right)
     	         : right;
	}
      default: // L * _
	switch (rc) {
	case FALSE: // L * F
	  return con == XOR
	         ? left
 	         : new NegatedFormula(left);
	case TRUE:  // L * T
	  return con == XOR
 	         ? new NegatedFormula(left)
     	         : left;
	default:    // L * R
	  if (left == f->left() && right == f->right()) {
	    return f;
	  }
	  return new BinaryFormula(con,left,right);
	}
      }
    }
    
  case NOT: 
    {
      Formula* arg = simplify(f->uarg());
      switch (arg->connective()) {
      case FALSE:
	return new Formula(true);
      case TRUE:
	return new Formula(false);
      default:
	return arg == f->uarg() ? f : new NegatedFormula(arg);
      }
    }
    
  case FORALL:
  case EXISTS: 
    {
      Formula* arg = simplify(f->qarg());
      switch (arg->connective()) {
      case FALSE:
      case TRUE:
	return arg;
      default:
	return arg == f->qarg()
               ? f
               : new QuantifiedFormula(con,f->vars(),f->sorts(),arg);
      }
    }

#if VDEBUG
  default:
    ASSERTION_VIOLATION;
#endif
  }
} // SimplifyFalseTrue::simplify ()


TermList SimplifyFalseTrue::simplify(TermList ts)
{
  CALL("SimplifyFalseTrue::simplify(TermList)");

  if (ts.isVar()) {
    return ts;
  }

  if (ts.term()->shared()) {
    return ts;
  }

  Term* term = ts.term();

  if (term->isSpecial()) {
    Term::SpecialTermData* sd = term->getSpecialData();
    switch (sd->getType()) {
      case Term::SF_FORMULA: {
        Formula* simplifiedFormula = simplify(sd->getFormula());
        switch (simplifiedFormula->connective()) {
          case TRUE:
            return TermList(Term::foolTrue());
          case FALSE:
            return TermList(Term::foolFalse());
          default:
            return TermList(Term::createFormula(simplifiedFormula));
        }
      }
      case Term::SF_ITE: {
        Formula* condition  = simplify(sd->getCondition());
        TermList thenBranch = simplify(*term->nthArgument(0));
        TermList elseBranch = simplify(*term->nthArgument(1));
        unsigned sort = sd->getSort();
        return TermList(Term::createITE(condition, thenBranch, elseBranch, sort));
      }
      case Term::SF_LET: {
        unsigned functor = sd->getFunctor();
        IntList* variables = sd->getVariables();
        TermList binding = simplify(sd->getBinding());
        TermList body = simplify(*term->nthArgument(0));
        unsigned sort = sd->getSort();
        return TermList(Term::createLet(functor, variables, binding, body, sort));
      }
      default:
        ASSERTION_VIOLATION_REP(term->toString());
    }
  }

  Stack<TermList> arguments;
  Term::Iterator it(term);
  while (it.hasNext()) {
    arguments.push(simplify(it.next()));
  }

  return TermList(Term::create(term, arguments.begin()));
} // SimplifyFalseTrue::simplify(TermList)