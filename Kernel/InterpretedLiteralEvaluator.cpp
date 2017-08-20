/**
 * @file InterpretedLiteralEvaluator.cpp
 * Implements class InterpretedLiteralEvaluator.
 */

#include "Lib/Environment.hpp"

#include "Signature.hpp"
#include "Sorts.hpp"
#include "TermIterators.hpp"
#include "Term.hpp"
#include "Theory.hpp"

#include "InterpretedLiteralEvaluator.hpp"

namespace Kernel
{
using namespace Lib;

/**
 * We use descendants of this class to evaluate various functions.
 *
 * One function can be evaluated only by one Evaluator object.
 */
class InterpretedLiteralEvaluator::Evaluator
{
public:
  CLASS_NAME(InterpretedLiteralEvaluator::Evaluator);
  USE_ALLOCATOR(InterpretedLiteralEvaluator::Evaluator);
  
  virtual ~Evaluator() {}

  bool canEvaluateFunc(unsigned func)
  {
    CALL("InterpretedLiteralEvaluator::Evaluator::canEvaluateFunc");

    if (!theory->isInterpretedFunction(func)) {
      return false;
    }
    Interpretation interp = theory->interpretFunction(func);
    return canEvaluate(interp);
  }

  bool canEvaluatePred(unsigned pred)
  {
    CALL("InterpretedLiteralEvaluator::Evaluator::canEvaluatePred");
        
    if (!theory->isInterpretedPredicate(pred)) {
      return false;
    }
    Interpretation interp = theory->interpretPredicate(pred);
    return canEvaluate(interp);
  }

  virtual bool canEvaluate(Interpretation interp) = 0;
  virtual bool tryEvaluateFunc(Term* trm, TermList& res) { return false; }
  virtual bool tryEvaluatePred(Literal* trm, bool& res)  { return false; }
};


/**
 * Interpreted equality has to be treated specially. We do not have separate
 * predicate symbols for different kinds of equality so the sorts must be 
 * detected and the correct intepretation of constants carried out.
 *
 * Equality is only decided between constant terms.
 *
 * @author Giles
 * @since 10/11/14
 */
class InterpretedLiteralEvaluator::EqualityEvaluator
  : public Evaluator
{

  virtual bool canEvaluate(Interpretation interp)
  {
    return interp==Interpretation::EQUAL; 
  }

  virtual bool tryEvaluateFunc(Term* trm, TermList& res)
  {
    ASSERTION_VIOLATION; // EQUAL is a predicate, not a function!
  }

  template<typename T>
  bool checkEquality(Literal* lit, bool& res)
  { 
    CALL("InterpretedLiteralEvaluator::EqualityEvaluator::checkEquality");
    T arg1;
    if(!theory->tryInterpretConstant(lit->nthArgument(0)->term(),arg1)){
      return false; 
    }
    T arg2;
    if(!theory->tryInterpretConstant(lit->nthArgument(1)->term(),arg2)){
      return false;
    }
    
    res = (arg1 == arg2);

    return true;
  }

  bool tryEvaluatePred(Literal* lit, bool& res)
  {
    CALL("InterpretedLiteralEvaluator::EqualityEvaluator::tryEvaluatePred");
            
    // Return if this is not an equality between theory terms
    if(!theory->isInterpretedPredicate(lit)){ return false; }

    try{

      Interpretation itp = theory->interpretPredicate(lit);
      ASS(itp==Interpretation::EQUAL);
      ASS(theory->getArity(itp)==2);
    
      // We try and interpret the equality as a number of different sorts
      // If it is not an equality between two constants of the same sort the
      // checkEquality function will return false, otherwise res will contain
      // the result of the equality check
      bool okay = checkEquality<IntegerConstantType>(lit,res)  ||
                  checkEquality<RationalConstantType>(lit,res) ||
                  checkEquality<RealConstantType>(lit,res) || checkEquality<BitVectorConstantType>(lit,res);

      if(!okay) return false;

      if(lit->isNegative()){ res = !res; }
      cout<<endl<<"tryEvaluatePred becomes true"<<endl;
      return true;

    }
    catch(ArithmeticException&)
    {
      return false;
    }

  }

};

/**
 * An evaluator for dealing with conversions between sorts
 *
 */
class InterpretedLiteralEvaluator::ConversionEvaluator
  : public Evaluator
{
public:
  virtual bool canEvaluate(Interpretation interp)
  {
    CALL("InterpretedLiteralEvaluator::ConversionEvaluator::canEvaluate");
    return theory->isConversionOperation(interp);
  }

  virtual bool tryEvaluateFunc(Term* trm, TermList& res)
  {
    CALL("InterpretedLiteralEvaluator::ConversionEvaluator::tryEvaluateFunc");
    ASS(theory->isInterpretedFunction(trm));

    try {
      Interpretation itp = theory->interpretFunction(trm);
      ASS(theory->isFunction(itp));
      ASS(theory->isConversionOperation(itp));
      ASS_EQ(theory->getArity(itp), 1);

      TermList argTrm = *trm->nthArgument(0);
      switch(itp) {
      case Theory::INT_TO_RAT:
	{
	  IntegerConstantType arg;
	  if (!theory->tryInterpretConstant(argTrm, arg)) { 
	    return false;
	  }
	  RationalConstantType resNum(arg,1);
	  res = TermList(theory->representConstant(resNum));
	  return true;
	}
      case Theory::INT_TO_REAL:
	{
	  IntegerConstantType arg;
	  if (!theory->tryInterpretConstant(argTrm, arg)) {
	    return false;
	  }
	  RealConstantType resNum(RationalConstantType(arg,1));
	  res = TermList(theory->representConstant(resNum));
	  return true;
	}
      case Theory::RAT_TO_INT:
	{
	  RationalConstantType arg;
	  if (!theory->tryInterpretConstant(argTrm, arg)) {
	    return false;
	  }
	  IntegerConstantType resNum = IntegerConstantType::floor(arg);
	  res = TermList(theory->representConstant(resNum));
	  return true;
	}
      case Theory::RAT_TO_REAL:
	{
	  RationalConstantType arg;
	  if (!theory->tryInterpretConstant(argTrm, arg)) {
	    return false;
	  }
	  RealConstantType resNum(arg);
	  res = TermList(theory->representConstant(resNum));
	  return true;
	}
      case Theory::REAL_TO_INT:
	{
	  RealConstantType arg;
	  if (!theory->tryInterpretConstant(argTrm, arg)) {
	    return false;
	  }
	  IntegerConstantType resNum = IntegerConstantType::floor(RationalConstantType(arg));
	  res = TermList(theory->representConstant(resNum));
	  return true;
	}
      case Theory::REAL_TO_RAT:
	{
	  //this is correct only as long as we only represent rational real numbers
	  RealConstantType arg;
	  if (!theory->tryInterpretConstant(argTrm, arg)) {
	    return false;
	  }
	  RationalConstantType resNum(arg);
	  res = TermList(theory->representConstant(resNum));
	return true;
      }

      default:
	ASSERTION_VIOLATION;
      }
    }
    catch(ArithmeticException&)
    {
      return false;
    }
  }
};

/**
 * Evaluates constant theory expressions
 *
 * Evaluators for each sort implement tryEvaluate(Unary/Binary)(Func/Pred) 
 * 
 */
template<class T>
class InterpretedLiteralEvaluator::TypedEvaluator : public Evaluator
{
public:
  typedef T Value;

  TypedEvaluator() {}

  virtual bool isZero(T arg) = 0;
  virtual TermList getZero() = 0;
  virtual bool isOne(T arg) = 0;

  virtual bool isAddition(Interpretation interp) = 0;
  virtual bool isProduct(Interpretation interp) = 0;
  virtual bool isDivision(Interpretation interp) = 0;

  virtual bool canEvaluate(Interpretation interp)
  {
    CALL("InterpretedLiteralEvaluator::TypedEvaluator::canEvaluate");
     
    //only interpreted operations with non-single argument sort are array operations
    if (theory->isArrayOperation(interp))
    {
        unsigned opSort = theory->getArrayOperationSort(interp);
        return opSort==T::getSort();
    }
    
    if (theory->isBitVectorOperation(interp) && T::getSort()==1500 )
        return true;
    
                
    // This is why we cannot evaluate Equality here... we cannot determine its sort
    if (!theory->hasSingleSort(interp)) { return false; } //To skip conversions and EQUAL

    unsigned opSort = theory->getOperationSort(interp);
    return opSort==T::getSort();
  }

  virtual bool tryEvaluateFunc(Term* trm, TermList& res)
  {
    CALL("InterpretedLiteralEvaluator::tryEvaluateFunc");
    ASS(theory->isInterpretedFunction(trm));

    //cout << "try evaluate " << trm->toString() << endl;
    
    try {
      Interpretation itp = theory->interpretFunction(trm);
      ASS(theory->isFunction(itp));
      unsigned arity = theory->getArity(itp);

      if (arity!=1 && arity!=2) {
	INVALID_OPERATION("unsupported arity of interpreted operation: "+Int::toString(arity));
      }
      T resNum;
      TermList arg1Trm = *trm->nthArgument(0);
      T arg1;
      if (arity==1) {
        if (theory->tryInterpretConstant(arg1Trm, arg1)){
          if (!tryEvaluateUnaryFunc(itp, arg1, resNum)) { return false;}
        }
        else{ return false;}
      }
      else if(arity==2){
        // If one argument is not a constant and the other is zero or one then
        // we have some special cases
          
        T arg2;
        TermList arg2Trm = *trm->nthArgument(1);

        bool specialCase = true;
        T conArg;
        TermList nonConTerm;
        if (theory->tryInterpretConstant(arg1Trm, arg1) && (isZero(arg1) || isOne(arg1)) && 
            !theory->tryInterpretConstant(arg2Trm, arg2)) {
         conArg = arg1;
         nonConTerm = arg2Trm;
        }
        else if(theory->tryInterpretConstant(arg2Trm, arg2) && (isZero(arg2) || isOne(arg2)) && 
            !theory->tryInterpretConstant(arg1Trm, arg1)) {
         conArg = arg2;
         nonConTerm = arg1Trm;
        }
        else{
          specialCase = false;
        }
        if(specialCase){
 
          //Special case where itp is division and arg2 is '1'
          //   Important... this is a non-symmetric case!
          if(theory->tryInterpretConstant(arg2Trm, arg2) && isOne(arg2) && isDivision(itp)){
            res = arg1Trm;
            return true;
          }
          //Special case where itp is addition and conArg is '0'
          if(isZero(conArg) && isAddition(itp)){
            res = nonConTerm;
            return true;
          }
          //Special case where itp is multiplication and conArg  is '1'
          if(isOne(conArg) && isProduct(itp)){
            res = nonConTerm;
            return true;
          }
          //Special case where itp is multiplication and conArg is '0'
          if(isZero(conArg) && isProduct(itp)){
            res = getZero();
            return true;
          }
        }
        if(theory->tryInterpretConstant(arg1Trm, arg1) && theory->tryInterpretConstant(arg2Trm, arg2)){
	  if (!tryEvaluateBinaryFunc(itp, arg1, arg2, resNum)) { return false;}
        }
        else{ return false;}
      }
      res = TermList(theory->representConstant(resNum));
      return true;
    }
    catch(ArithmeticException)
    {
       //cout << "ArithmeticException" << endl;
      return false;
    }
  }

  virtual bool tryEvaluatePred(Literal* lit, bool& res)
  {
    CALL("InterpretedLiteralEvaluator::tryEvaluatePred");
    ASS(theory->isInterpretedPredicate(lit));
   
    try {
      Interpretation itp = theory->interpretPredicate(lit);
      ASS(!theory->isFunction(itp));
      unsigned arity = theory->getArity(itp);

      if (arity!=1 && arity!=2) {
	INVALID_OPERATION("unsupported arity of interpreted operation: "+Int::toString(arity));
      }
      TermList arg1Trm = *lit->nthArgument(0);
      T arg1;
      if (!theory->tryInterpretConstant(arg1Trm, arg1)) {return false;}
      if (arity==1) {
	if (!tryEvaluateUnaryPred(itp, arg1, res)) {return false;}
      }
      else {
	TermList arg2Trm = *lit->nthArgument(1);
	T arg2;
	if (!theory->tryInterpretConstant(arg2Trm, arg2)) {return false;}
	if (!tryEvaluateBinaryPred(itp, arg1, arg2, res)) {return false;}
      }
      if (lit->isNegative()) {
	res = !res;
      }
      return true;
    }
    catch(ArithmeticException&)
    {
      return false;
    }

  }
protected:
  virtual bool tryEvaluateUnaryFunc(Interpretation op, const T& arg, T& res)
  { return false; }
  virtual bool tryEvaluateBinaryFunc(Interpretation op, const T& arg1, const T& arg2, T& res)
  { return false; }

  virtual bool tryEvaluateUnaryPred(Interpretation op, const T& arg1, bool& res)
  { return false; }
  virtual bool tryEvaluateBinaryPred(Interpretation op, const T& arg1, const T& arg2, bool& res)
  { return false; }
};

/**
 * Evaluates integer functions
 */
class InterpretedLiteralEvaluator::IntEvaluator : public TypedEvaluator<IntegerConstantType>
{
protected:

  virtual bool isZero(IntegerConstantType arg){ return arg.toInner()==0;}
  virtual TermList getZero(){ return TermList(theory->representConstant(IntegerConstantType(0))); }
  virtual bool isOne(IntegerConstantType arg){ return arg.toInner()==1;}

  virtual bool isAddition(Interpretation interp){ return interp==Theory::INT_PLUS; }
  virtual bool isProduct(Interpretation interp){ return interp==Theory::INT_MULTIPLY;}
  virtual bool isDivision(Interpretation interp){ 
    return interp==Theory::INT_QUOTIENT_E || interp==Theory::INT_QUOTIENT_T || 
           interp==Theory::INT_QUOTIENT_F; 
  }

  virtual bool tryEvaluateUnaryFunc(Interpretation op, const Value& arg, Value& res)
  {
    CALL("InterpretedLiteralEvaluator::IntEvaluator::tryEvaluateUnaryFunc");

    switch(op) {
    case Theory::INT_UNARY_MINUS:
      res = -arg;
      return true;
    case Theory::INT_ABS:
      if (arg < 0) {
        res = -arg;
      } else {
        res = arg;
      }
      return true;
    case Theory::INT_SUCCESSOR:
      res = arg+1;
      return true;
    case Theory::INT_FLOOR:
    case Theory::INT_CEILING:
    case Theory::INT_TRUNCATE:
    case Theory::INT_ROUND:
       // For integers these do nothing
      res = arg;
      return true;
    default:
      return false;
    }
  }

  virtual bool tryEvaluateBinaryFunc(Interpretation op, const Value& arg1,
      const Value& arg2, Value& res)
  {
    CALL("InterpretedLiteralEvaluator::IntEvaluator::tryEvaluateBinaryFunc");

    switch(op) {
    case Theory::INT_PLUS:
      res = arg1+arg2;
      return true;
    case Theory::INT_MINUS:
      res = arg1-arg2;
      return true;
    case Theory::INT_MULTIPLY:
      res = arg1*arg2;
      return true;
    case Theory::INT_QUOTIENT_E:
      res = arg1.quotientE(arg2); // should be equivalent to arg1/arg2
      return true;
    case Theory::INT_QUOTIENT_T:
      res = arg1.quotientT(arg2);
      return true;
    case Theory::INT_QUOTIENT_F:
      res = arg1.quotientF(arg2);
      return true;
    // The remainder is left - (quotient * right)
    case Theory::INT_REMAINDER_E:
      res = arg1 - (arg1.quotientE(arg2)*arg2);
      return true;
    case Theory::INT_REMAINDER_T:
      res = arg1 - (arg1.quotientT(arg2)*arg2);
      return true;
    case Theory::INT_REMAINDER_F:
      res = arg1 - (arg1.quotientF(arg2)*arg2);
      return true;
    default:
      return false;
    }
  }

  virtual bool tryEvaluateBinaryPred(Interpretation op, const Value& arg1,
      const Value& arg2, bool& res)
  {
    CALL("InterpretedLiteralEvaluator::IntEvaluator::tryEvaluateBinaryPred");

    switch(op) {
    case Theory::INT_GREATER:
      res = arg1>arg2;
      return true;
    case Theory::INT_GREATER_EQUAL:
      res = arg1>=arg2;
      return true;
    case Theory::INT_LESS:
      res = arg1<arg2;
      return true;
    case Theory::INT_LESS_EQUAL:
      res = arg1<=arg2;
      return true;
    case Theory::INT_DIVIDES:
      res = (arg2%arg1)==0;
      return true;
    default:
      return false;
    }
  }
};

/**
 * Evaluations rational functions
 */
class InterpretedLiteralEvaluator::RatEvaluator : public TypedEvaluator<RationalConstantType>
{
protected:
  virtual bool isZero(RationalConstantType arg){ return arg.isZero();}
  virtual TermList getZero(){ return TermList(theory->representConstant(RationalConstantType(0,1))); }
  virtual bool isOne(RationalConstantType arg) { return arg.numerator()==arg.denominator();}

  virtual bool isAddition(Interpretation interp){ return interp==Theory::RAT_PLUS; }
  virtual bool isProduct(Interpretation interp){ return interp==Theory::RAT_MULTIPLY;}
  virtual bool isDivision(Interpretation interp){ 
    return interp==Theory::RAT_QUOTIENT || interp==Theory::RAT_QUOTIENT_E || 
           interp==Theory::RAT_QUOTIENT_T || interp==Theory::RAT_QUOTIENT_F;
  }

  virtual bool tryEvaluateUnaryFunc(Interpretation op, const Value& arg, Value& res)
  {
    CALL("InterpretedLiteralEvaluator::RatEvaluator::tryEvaluateUnaryFunc");

    switch(op) {
    case Theory::RAT_UNARY_MINUS:
      res = -arg;
      return true;
    case Theory::RAT_FLOOR:
      res = arg.floor();
      return true;
    case Theory::RAT_CEILING:
      res = arg.ceiling();
      return true;
    case Theory::RAT_TRUNCATE:
      res = arg.truncate();
      return true;
    default:
      return false;
    }
  }

  virtual bool tryEvaluateBinaryFunc(Interpretation op, const Value& arg1,
      const Value& arg2, Value& res)
  {
    CALL("InterpretedLiteralEvaluator::RatEvaluator::tryEvaluateBinaryFunc");

    switch(op) {
    case Theory::RAT_PLUS:
      res = arg1+arg2;
      return true;
    case Theory::RAT_MINUS:
      res = arg1-arg2;
      return true;
    case Theory::RAT_MULTIPLY:
      res = arg1*arg2;
      return true;
    case Theory::RAT_QUOTIENT:
      res = arg1/arg2;
      return true;
    case Theory::RAT_QUOTIENT_E:
      res = arg1.quotientE(arg2);
      return true;
    case Theory::RAT_QUOTIENT_T:
      res = arg1.quotientT(arg2);
      return true;
    case Theory::RAT_QUOTIENT_F:
      res = arg1.quotientF(arg2);
      return true;
    // The remainder is left - (quotient * right)
    case Theory::RAT_REMAINDER_E:
      res = arg1 - (arg1.quotientE(arg2)*arg2);
      return true;
    case Theory::RAT_REMAINDER_T:
      res = arg1 - (arg1.quotientT(arg2)*arg2);
      return true;
    case Theory::RAT_REMAINDER_F:
      res = arg1 - (arg1.quotientF(arg2)*arg2);
      return true;
    default:
      return false;
    }
  }

  virtual bool tryEvaluateBinaryPred(Interpretation op, const Value& arg1,
      const Value& arg2, bool& res)
  {
    CALL("InterpretedLiteralEvaluator::RatEvaluator::tryEvaluateBinaryPred");

    switch(op) {
    case Theory::RAT_GREATER:
      res = arg1>arg2;
      return true;
    case Theory::RAT_GREATER_EQUAL:
      res = arg1>=arg2;
      return true;
    case Theory::RAT_LESS:
      res = arg1<arg2;
      return true;
    case Theory::RAT_LESS_EQUAL:
      res = arg1<=arg2;
      return true;
    default:
      return false;
    }
  }

  virtual bool tryEvaluateUnaryPred(Interpretation op, const Value& arg1,
      bool& res)
  {
    CALL("InterpretedLiteralEvaluator::RatEvaluator::tryEvaluateBinaryPred");

    switch(op) {
    case Theory::RAT_IS_INT:
      res = arg1.isInt();
      return true;
    default:
      return false;
    }
  }
};

/**
 * Evaluates real functions. 
 * As reals are represented as rationals the operations are for reals.
 * See Kernel/Theory.hpp for how these operations are defined
 */
class InterpretedLiteralEvaluator::RealEvaluator : public TypedEvaluator<RealConstantType>
{
protected:
  virtual bool isZero(RealConstantType arg){ return arg.isZero();}
  virtual TermList getZero(){ return TermList(theory->representConstant(RealConstantType(RationalConstantType(0, 1)))); }
  virtual bool isOne(RealConstantType arg) { return arg.numerator()==arg.denominator();}

  virtual bool isAddition(Interpretation interp){ return interp==Theory::REAL_PLUS; }
  virtual bool isProduct(Interpretation interp){ return interp==Theory::REAL_MULTIPLY;}
  virtual bool isDivision(Interpretation interp){ 
    return interp==Theory::REAL_QUOTIENT || interp==Theory::REAL_QUOTIENT_E ||
           interp==Theory::REAL_QUOTIENT_T || interp==Theory::REAL_QUOTIENT_F;
  }

  virtual bool tryEvaluateUnaryFunc(Interpretation op, const Value& arg, Value& res)
  {
    CALL("InterpretedLiteralEvaluator::RealEvaluator::tryEvaluateUnaryFunc");

    switch(op) {
    case Theory::REAL_UNARY_MINUS:
      res = -arg;
      return true;
    case Theory::REAL_FLOOR:
      res = arg.floor();
      return true;
    case Theory::REAL_CEILING:
      res = arg.ceiling();
      return true;
    case Theory::REAL_TRUNCATE:
      res = arg.truncate();
      return true;
    default:
      return false;
    }
  }

  virtual bool tryEvaluateBinaryFunc(Interpretation op, const Value& arg1,
      const Value& arg2, Value& res)
  {
    CALL("InterpretedLiteralEvaluator::RealEvaluator::tryEvaluateBinaryFunc");

    switch(op) {
    case Theory::REAL_PLUS:
      res = arg1+arg2;
      return true;
    case Theory::REAL_MINUS:
      res = arg1-arg2;
      return true;
    case Theory::REAL_MULTIPLY:
      res = arg1*arg2;
      return true;
    case Theory::REAL_QUOTIENT:
      res = arg1/arg2;
      return true;
    case Theory::REAL_QUOTIENT_E:
      res = arg1.quotientE(arg2);
      return true;
    case Theory::REAL_QUOTIENT_T:
      res = arg1.quotientT(arg2);
      return true;
    case Theory::REAL_QUOTIENT_F:
      res = arg1.quotientF(arg2);
      return true;
    // The remainder is left - (quotient * right)
    case Theory::REAL_REMAINDER_E:
      res = arg1 - (arg1.quotientE(arg2)*arg2);
      return true;
    case Theory::REAL_REMAINDER_T:
      res = arg1 - (arg1.quotientT(arg2)*arg2);
      return true;
    case Theory::REAL_REMAINDER_F:
      res = arg1 - (arg1.quotientF(arg2)*arg2);
      return true;
    default:
      return false;
    }
  }

  virtual bool tryEvaluateBinaryPred(Interpretation op, const Value& arg1,
      const Value& arg2, bool& res)
  {
    CALL("InterpretedLiteralEvaluator::RealEvaluator::tryEvaluateBinaryPred");

    switch(op) {
    case Theory::REAL_GREATER:
      res = arg1>arg2;
      return true;
    case Theory::REAL_GREATER_EQUAL:
      res = arg1>=arg2;
      return true;
    case Theory::REAL_LESS:
      res = arg1<arg2;
      return true;
    case Theory::REAL_LESS_EQUAL:
      res = arg1<=arg2;
      return true;
    default:
      return false;
    }
  }

  virtual bool tryEvaluateUnaryPred(Interpretation op, const Value& arg1,
      bool& res)
  {
    CALL("InterpretedLiteralEvaluator::RealEvaluator::tryEvaluateBinaryPred");

    switch(op) {
    case Theory::REAL_IS_INT:
      res = arg1.isInt();
      return true;
    case Theory::REAL_IS_RAT:
      //this is true as long as we can evaluate only rational reals.
      res = true;
      return true;
    default:
      return false;
    }
  }

};

 

class InterpretedLiteralEvaluator::BitVectorEvaluator : public TypedEvaluator<BitVectorConstantType> 
{
  protected:

  virtual bool isZero(BitVectorConstantType arg){return false;}
  virtual TermList getZero(){ return TermList(); }
  virtual bool isOne(BitVectorConstantType arg){return false;}

  virtual bool isAddition(Interpretation interp){return false;}
  virtual bool isProduct(Interpretation interp){return false;}
  virtual bool isDivision(Interpretation interp){return false; 
  }
  virtual bool tryEvaluateFunc(Term* trm, TermList& res)
  {
      Interpretation itp = theory->interpretFunction(trm);
      ASS(theory->isFunction(itp));
      unsigned arity = theory->getArity(itp);
      Theory::StructuredSortInterpretation ssi = theory->convertToStructured(itp);
      // certain interpretations need special attention 
      if (arity == 1)
      {
          TermList arg1Trm = *trm->nthArgument(0);
          BitVectorConstantType arg1,resNum;
          if (theory->tryInterpretConstant(arg1Trm, arg1))
          {if (!tryEvaluateUnaryFunc(itp, arg1, resNum)) 
                {return false;}}
          else
            {return false;}
          
          
          res = TermList(theory->representConstant(resNum));
          return true;
      
      }
      if (ssi == Theory::StructuredSortInterpretation::BV_ROTATE_RIGHT || 
              ssi == Theory::StructuredSortInterpretation::BV_ROTATE_LEFT || 
              ssi == Theory::StructuredSortInterpretation::BV_SIGN_EXTEND || 
              ssi == Theory::StructuredSortInterpretation::BV_ZERO_EXTEND)
      {
              TermList arg1Trm = *trm->nthArgument(0);
              TermList arg2Trm = *trm->nthArgument(1);
              
              IntegerConstantType rotateBy;
              if (!theory->tryInterpretConstant(arg1Trm, rotateBy))
              {
                  return false;
              }
              BitVectorConstantType argBv;
              if (!theory->tryInterpretConstant(arg2Trm, argBv))
              {
                  return false;
              }
              
              
              // if sign extend or zero extend size accordingly
              unsigned resSize = argBv.size();
              if (ssi == Theory::StructuredSortInterpretation::BV_SIGN_EXTEND || ssi == Theory::StructuredSortInterpretation::BV_ZERO_EXTEND)
                  resSize = argBv.size() + rotateBy.toInner();
              BitVectorConstantType resNum(resSize);
              if (ssi == Theory::StructuredSortInterpretation::BV_ROTATE_RIGHT)
                BitVectorConstantType::rotate_right(rotateBy.toInner(),argBv,resNum);
              else if (ssi == Theory::StructuredSortInterpretation::BV_ROTATE_LEFT)
                BitVectorConstantType::rotate_left(rotateBy.toInner(),argBv,resNum);
              else if (ssi == Theory::StructuredSortInterpretation::BV_SIGN_EXTEND)
                BitVectorConstantType::sign_extend(rotateBy.toInner(),argBv,resNum);  
              else if (ssi == Theory::StructuredSortInterpretation::BV_ZERO_EXTEND)
                BitVectorConstantType::zero_extend(rotateBy.toInner(),argBv,resNum);   
              res = TermList(theory->representConstant(resNum));
              return true;
              
      }
      // TODO: error handling 
      else if (ssi == Theory::StructuredSortInterpretation::EXTRACT)
      {
              TermList arg1Trm = *trm->nthArgument(0);
              TermList arg2Trm = *trm->nthArgument(1);
              TermList arg3Trm = *trm->nthArgument(2);
              
              IntegerConstantType from;
              IntegerConstantType to;
              BitVectorConstantType argBv;
              
              if (!theory->tryInterpretConstant(arg1Trm, argBv))
              {
                  
                  return false;
              }
              
              if (!theory->tryInterpretConstant(arg2Trm, from))
              {
                  return false;
              }
              
              
              if (!theory->tryInterpretConstant(arg3Trm, to))
              {
                  return false;
              }
              
              
              // if sign extend or zero extend size accordingyl
              unsigned resSize = from.toInner()-to.toInner()+1;
              BitVectorConstantType resNum(resSize);
              BitVectorConstantType::extract(from.toInner(),to.toInner(),argBv,resNum);
              res = TermList(theory->representConstant(resNum));
              return true;
      }
      else //for bvand and such
      {
          TermList arg1Trm = *trm->nthArgument(0);
          TermList arg2Trm = *trm->nthArgument(1);
          BitVectorConstantType argBv1;
          BitVectorConstantType argBv2;
          if (!theory->tryInterpretConstant(arg1Trm, argBv1))
          {
              return false;
          }
          if (!theory->tryInterpretConstant(arg2Trm, argBv2))
          {
              return false;
          }
          // here check what operation is done... according to that determine the size 
          unsigned resSize = argBv1.size();
          if (ssi == Theory::StructuredSortInterpretation::CONCAT)
              resSize = argBv1.size() + argBv2.size();
          else if (ssi == Theory::StructuredSortInterpretation::BVCOMP )
              resSize = 1;
          BitVectorConstantType resNum(resSize);
          
          if (!tryEvaluateBinaryFunc(itp, argBv1, argBv2, resNum)) 
          { 
              return false;
          }
          
          
          res = TermList(theory->representConstant(resNum));
          return true;
          
      }
      return false; // hav to do representConstant in every if else branch  
      
      
  }
  // return BVCT representing one with the given size 
  BitVectorConstantType getOne(unsigned size)
  {
      BitVectorConstantType res;
      DArray<bool> resArray(size);
      resArray[0] = true;
      for (int i = 1 ; i<size;++i){
          resArray[i] = false;
      }
      res.setBinArray(resArray);
      return res;
  }

 
  
  virtual bool tryEvaluateUnaryFunc(Interpretation op, const Value& arg, Value& res)
  {
      
      Theory::StructuredSortInterpretation ssi = theory->convertToStructured(op);
      switch(ssi){
          case Theory::StructuredSortInterpretation::BVNEG:
              res.prepareBinArray(arg.size());
              BitVectorConstantType::bvneg(arg, res);
              return true;
          case Theory::StructuredSortInterpretation::BVNOT:
              res.prepareBinArray(arg.size());
              BitVectorConstantType::bvnot(arg, res);
              return true;
          default:
              USER_ERROR("Add here1");
              return false;
      }
      
  }

  virtual bool tryEvaluateBinaryFunc(Interpretation op, const Value& arg1,
      const Value& arg2, Value& res)
  {
     Theory::StructuredSortInterpretation ssi = theory->convertToStructured(op);
     switch(ssi){
          case Theory::StructuredSortInterpretation::BVAND:
              BitVectorConstantType::bvand(arg1, arg2,res);
              return true;
          case Theory::StructuredSortInterpretation::BVNAND:
              BitVectorConstantType::bvnand(arg1, arg2,res);
              return true;
          case Theory::StructuredSortInterpretation::BVXOR:
              BitVectorConstantType::bvxor(arg1, arg2,res);
              return true;
          case Theory::StructuredSortInterpretation::BVXNOR:
              BitVectorConstantType::bvxnor(arg1, arg2,res);
              return true;    
          case Theory::StructuredSortInterpretation::BVADD:
              BitVectorConstantType::bvadd(arg1, arg2,res);
              return true;
          case Theory::StructuredSortInterpretation::BVSHL:
              BitVectorConstantType::bvshl(arg1, arg2,res);
              return true;
          case Theory::StructuredSortInterpretation::BVLSHR:
              BitVectorConstantType::bvlshr(arg1, arg2,res);
              return true;
          case Theory::StructuredSortInterpretation::BVASHR:
              BitVectorConstantType::bvashr(arg1, arg2,res);
              return true;    
          case Theory::StructuredSortInterpretation::BVSUB:
              BitVectorConstantType::bvsub(arg1, arg2,res);
              return true;
          case Theory::StructuredSortInterpretation::BVUDIV:
              BitVectorConstantType::bvudiv(arg1, arg2,res);
              return true;
          case Theory::StructuredSortInterpretation::BVSDIV:
              BitVectorConstantType::bvsdiv(arg1, arg2,res);
              return true;    
          case Theory::StructuredSortInterpretation::BVUREM:
              BitVectorConstantType::bvurem(arg1, arg2,res);
              return true; 
          case Theory::StructuredSortInterpretation::BVSREM:
              BitVectorConstantType::bvsrem(arg1, arg2,res);
              return true;
          case Theory::StructuredSortInterpretation::BVSMOD:
              BitVectorConstantType::bvsmod(arg1, arg2,res);
              return true;    
           case Theory::StructuredSortInterpretation::BVCOMP:
              BitVectorConstantType::bvcomp(arg1, arg2,res);
              return true; 
           case Theory::StructuredSortInterpretation::CONCAT:
              BitVectorConstantType::concat(arg1, arg2,res);
              return true;
           case Theory::StructuredSortInterpretation::BVMUL:
              BitVectorConstantType::bvmul(arg1, arg2,res);
              return true;   
          default:
              USER_ERROR("Add here"); // remove this
              return false;
      }
  }

  
  virtual bool tryEvaluateBinaryPred(Interpretation op, const Value& arg1,
      const Value& arg2, bool& res)
  {
      Theory::StructuredSortInterpretation ssi = theory->convertToStructured(op);
      if (ssi==Theory::StructuredSortInterpretation::BVUGE)
      {       
             BitVectorConstantType::bvuge(arg1, arg2,res);
             return true;
      }
      if (ssi==Theory::StructuredSortInterpretation::BVUGT)
      {       
             BitVectorConstantType::bvugt(arg1, arg2,res);
             return true;
      }
      if (ssi==Theory::StructuredSortInterpretation::BVULE)
      {       
             BitVectorConstantType::bvule(arg1, arg2,res);
             return true;
      }
      if (ssi==Theory::StructuredSortInterpretation::BVULT)
      {       
             BitVectorConstantType::bvult(arg1, arg2,res);
             return true;
      }
      if (ssi==Theory::StructuredSortInterpretation::BVSLT)
      {
          BitVectorConstantType::bvslt(arg1, arg2,res);
          return true;
      }
      if (ssi==Theory::StructuredSortInterpretation::BVSLE)
      {
          BitVectorConstantType::bvsle(arg1, arg2,res);
          return true;
      }
      if (ssi==Theory::StructuredSortInterpretation::BVSGT)
      {
          BitVectorConstantType::bvsgt(arg1, arg2,res);
          return true;
      }
      if (ssi==Theory::StructuredSortInterpretation::BVSGE)
      {
          BitVectorConstantType::bvsge(arg1, arg2,res);
          return true;
      }
      
      USER_ERROR("Add here2");
      return false;
      
  }
    
};


////////////////////////////////
// InterpretedLiteralEvaluator
//
// This is where the evaluators defined above are used.

InterpretedLiteralEvaluator::InterpretedLiteralEvaluator()
{
  CALL("InterpretedLiteralEvaluator::InterpretedLiteralEvaluator");

  // For an evaluator to be used it must be pushed onto _evals
  // We search this list, calling canEvaluate on each evaluator
  // An invariant we want to maintain is that for any literal only one
  //  Evaluator will return true for canEvaluate
  _evals.push(new IntEvaluator());
  _evals.push(new RatEvaluator());
  _evals.push(new RealEvaluator());
  _evals.push(new ConversionEvaluator());
  _evals.push(new EqualityEvaluator());
  _evals.push(new BitVectorEvaluator());
  
  _funEvaluators.ensure(0);
  _predEvaluators.ensure(0);

}

InterpretedLiteralEvaluator::~InterpretedLiteralEvaluator()
{
  CALL("InterpretedEvaluation::LiteralSimplifier::~LiteralSimplifier");

  while (_evals.isNonEmpty()) {
    delete _evals.pop();
  }
}

/**
 * This checks if a literal is 'balancable' i.e. can be put into the form term=constant or term=var
 * 
 * This is still an experimental process and will be expanded/reworked later
 *
 * @author Giles
 * @since 11/11/14
 */
bool InterpretedLiteralEvaluator::balancable(Literal* lit)
{
  CALL("InterpretedLiteralEvaluator::balancable");
  // Check that lit is compatible with this balancing operation
  // One thing that we cannot check, but assume is that it has already been simplified once
  // balance applies further checks

  // lit must be an interpretted predicate
  if(!theory->isInterpretedPredicate(lit->functor())) return false;

  // the perdicate must be binary
  Interpretation ip = theory->interpretPredicate(lit->functor());
  if(theory->getArity(ip)!=2) return false;

  // one side must be a constant and the other interpretted
  // the other side can contain at most one variable or uninterpreted subterm 
  // but we do not check this second condition here, instead we detect it in balance
  TermList t1 = *lit->nthArgument(0);
  TermList t2 = *lit->nthArgument(1);

  bool t1Number = theory->isInterpretedNumber(t1);
  bool t2Number = theory->isInterpretedNumber(t2);

  if(!t1Number && !t2Number){ return false; } // cannot balance
  if(t1Number && t2Number){ return true; } // already balanced

  // so here exactly one of t1Number and t2Number is true

  if(t1Number){
    if(t2.isVar()){ return false;} // already balanced
    if(!theory->isInterpretedFunction(t2)){ return false;} // cannot balance
  }
  if(t2Number){
    if(t1.isVar()){ return false;} // already balanced
    if(!theory->isInterpretedFunction(t1)){ return false;} // cannot balance
  }

  return true;
}

/**
 * This attempts to 'balance' a literal i.e. put it into the form term=constant
 *
 * The current implementation is only applicable to a restricted set of cases.
 *
 * This is still an experimental process and will be expanded/reworked later
 *
 * @author Giles
 * @since 11/11/14
 */
bool InterpretedLiteralEvaluator::balance(Literal* lit,Literal*& resLit,Stack<Literal*>& sideConditions)
{
  CALL("InterpretedLiteralEvaluator::balance");
  ASS(balancable(lit));

  cout << "try balance " << lit->toString() << endl;

  ASS(theory->isInterpretedPredicate(lit->functor()));

  Interpretation predicate = theory->interpretPredicate(lit->functor());
  // at the end this tells us if the predicate needs to be swapped, only applies if non-equality
  bool swap = false; 

  // only want lesseq and equality
  if(lit->arity()!=2) return false;

  TermList t1;
  TermList t2;
  // ensure that t1 is the constant
  if(theory->isInterpretedNumber(*lit->nthArgument(0))){
    t1 = *lit->nthArgument(0); t2 = *lit->nthArgument(1);
  }else{
    t1 = *lit->nthArgument(1); t2 = *lit->nthArgument(0);
    swap=true;
  }
  // so we have t1 a constant and t2 something that has an interpreted function at the top

  Signature::Symbol* conSym = env.signature->getFunction(t1.term()->functor());
  unsigned srt = 0;
  if(conSym->integerConstant()) srt = Sorts::SRT_INTEGER;
  else if(conSym->rationalConstant()) srt = Sorts::SRT_RATIONAL;
  else if(conSym->realConstant()) srt = Sorts::SRT_REAL;
  else{
     ASSERTION_VIOLATION_REP(t1);
    return false;// can't work out the sort, that's odd!
  }

  // unwrap t2, applying the wrappings to t1 until we get to something we can't unwrap
  // this updates t1 and t2 as we go

  // This relies on the fact that a simplified literal with a single non-constant
  // subterm will look like f(c,f(c,f(c,t)))=c
  // If we cannot invert an interpreted function f then we fail

  bool modified = false;

  while(theory->isInterpretedFunction(t2)){
    TermList* args = t2.term()->args();
    
    // find which arg of t2 is the non_constant bit, this is what we are unwrapping 
    TermList* to_unwrap=0;
    while(args->isNonEmpty()){
      if(!theory->isInterpretedNumber(*args)){
        if(to_unwrap){
          return false; // If there is more than one non-constant term this will not work
        }
        to_unwrap=args;
      } 
      args= args->next();
    }
    //Should not happen if balancable passed and it was simplified
    if(!to_unwrap){ return false;} 
    
    // Now we do a case on the functor of t2
    Term* t2term = t2.term();
    Interpretation t2interp = theory->interpretFunction(t2term->functor());
    TermList result;
    bool okay=true;
    switch(t2interp){
      case Theory::INT_PLUS:
        okay=balancePlus(Theory::INT_PLUS,Theory::INT_UNARY_MINUS,t2term,to_unwrap,t1,result);
        break;
      case Theory::RAT_PLUS:
        okay=balancePlus(Theory::RAT_PLUS,Theory::RAT_UNARY_MINUS,t2term,to_unwrap,t1,result);
        break;
      case Theory::REAL_PLUS:
        okay=balancePlus(Theory::REAL_PLUS,Theory::REAL_UNARY_MINUS,t2term,to_unwrap,t1,result);
        break;

      case Theory::INT_MULTIPLY: 
      {
        okay=balanceIntegerMultiply(t2term,to_unwrap,t1,result,predicate,swap,sideConditions);
        break;
      }
      case Theory::RAT_MULTIPLY:
      {
        RationalConstantType zero(0,1);
        okay=balanceMultiply(Theory::RAT_QUOTIENT,zero,t2term,to_unwrap,t1,result,predicate,swap,sideConditions);
        break;
      }
      case Theory::REAL_MULTIPLY:
      {
        RealConstantType zero(RationalConstantType(0, 1));
        okay=balanceMultiply(Theory::REAL_QUOTIENT,zero,t2term,to_unwrap,t1,result,predicate,swap,sideConditions);
        break;
       }

      case Theory::RAT_QUOTIENT:
        okay=balanceDivide(Theory::RAT_MULTIPLY,t2term,to_unwrap,t1,result,predicate,swap,sideConditions);
        break;
      case Theory::REAL_QUOTIENT:
        okay=balanceDivide(Theory::REAL_MULTIPLY,t2term,to_unwrap,t1,result,predicate,swap,sideConditions);
        break;

      default:
        okay=false;
        break;
    }
    if(!okay){
        // cannot invert this one, finish here, either by giving up or going to the end
        if(!modified) return false;
        goto endOfUnwrapping; 
    }

    // update t1
    t1=result;
    // set t2 to the non-constant argument
    t2 = *to_unwrap;
    modified = true;
  }
endOfUnwrapping:

  //Evaluate t1
  // We have rearranged things so that t2 is a non-constant term and t1 is a number
  // of interprted functions applied to some constants. By evaluating t1 we will
  //  get a constant (unless evaluation is not possible)

  // don't swap equality
  if(lit->functor()==0){
   resLit = TermTransformer::transform(Literal::createEquality(lit->polarity(),t2,t1,srt));
  }
  else{
    // important, need to preserve the ordering of t1 and t2 in the original!
    if(swap){
      resLit = TermTransformer::transform(Literal::create2(lit->functor(),lit->polarity(),t2,t1));
    }else{
      resLit = TermTransformer::transform(Literal::create2(lit->functor(),lit->polarity(),t1,t2));
    }
  }
  return true;
}


bool InterpretedLiteralEvaluator::balancePlus(Interpretation plus, Interpretation unaryMinus, 
                                              Term* AplusB, TermList* A, TermList C, TermList& result)
{
  CALL("InterpretedLiteralEvaluator::balancePlus");

    unsigned um = env.signature->getInterpretingSymbol(unaryMinus);
    unsigned ip = env.signature->getInterpretingSymbol(plus);
    TermList* B = 0;
    if(AplusB->nthArgument(0)==A){
      B = AplusB->nthArgument(1);
    }
    else{
      ASS(AplusB->nthArgument(1)==A);
      B = AplusB->nthArgument(0);
    }

    TermList mB(Term::create1(um,*B));
    result = TermList(Term::create2(ip,C,mB));
    return true;
}

template<typename ConstantType>
bool InterpretedLiteralEvaluator::balanceMultiply(Interpretation divide,ConstantType zero, 
                                                  Term* AmultiplyB, TermList* A, TermList C, TermList& result,
                                                  Interpretation under, bool& swap,
                                                  Stack<Literal*>& sideConditions)
{
    CALL("InterpretedLiteralEvaluator::balanceMultiply");
    unsigned srt = theory->getOperationSort(divide); 
    ASS(srt == Sorts::SRT_REAL || srt == Sorts::SRT_RATIONAL); 

    unsigned div = env.signature->getInterpretingSymbol(divide);
    TermList* B = 0;
    if(AmultiplyB->nthArgument(0)==A){
      B = AmultiplyB->nthArgument(1);
    }
    else{
      ASS(AmultiplyB->nthArgument(1)==A);
      B = AmultiplyB->nthArgument(0);
    }
    result = TermList(Term::create2(div,C,*B));

    ConstantType bcon;
    if(theory->tryInterpretConstant(*B,bcon)){
      if(bcon.isZero()) return false;
      if(bcon.isNegative()){ swap=!swap; } // switch the polarity of an inequality if we're under one
      return true;
    }
    // Unsure exactly what the best thing to do here, so for now give up
    // This means we only balance when we have a constant on the variable side
    return false;

    // if B is not a constant we need to ensure that B!=0
    //Literal* notZero = Literal::createEquality(false,B,zero,srt);
    //sideConditions.push(notZero);
    //result = TermList(Term::create2(div,C,*B);
    //return true;
}

bool InterpretedLiteralEvaluator::balanceIntegerMultiply(
                                                  Term* AmultiplyB, TermList* A, TermList C, TermList& result,
                                                  Interpretation under, bool& swap,
                                                  Stack<Literal*>& sideConditions)
{
    CALL("InterpretedLiteralEvaluator::balanceIntegerMultiply");

    // only works if we in the end divid a number by a number
    IntegerConstantType ccon;
    if(!theory->tryInterpretConstant(C,ccon)){ return false; }

    // we are going to use rounding division but ensure that it is non-rounding
    unsigned div = env.signature->getInterpretingSymbol(Theory::INT_QUOTIENT_E);
    TermList* B = 0;
    if(AmultiplyB->nthArgument(0)==A){
      B = AmultiplyB->nthArgument(1);
    }
    else{
      ASS(AmultiplyB->nthArgument(1)==A);
      B = AmultiplyB->nthArgument(0);
    }
    result = TermList(Term::create2(div,C,*B));

    IntegerConstantType bcon;
    if(theory->tryInterpretConstant(*B,bcon)){
      if(bcon.isZero()){ return false; }
      if(ccon.toInner() % bcon.toInner() !=0){ return false; } 
      if(bcon.isNegative()){ swap=!swap; } // switch the polarity of an inequality if we're under one
      return true;
    }
    return false;
}

bool InterpretedLiteralEvaluator::balanceDivide(Interpretation multiply, 
                       Term* AoverB, TermList* A, TermList C, TermList& result,
                       Interpretation under, bool& swap, Stack<Literal*>& sideConditions)
{
    CALL("InterpretedLiteralEvaluator::balanceDivide");
    unsigned srt = theory->getOperationSort(multiply); 
    ASS(srt == Sorts::SRT_REAL || srt == Sorts::SRT_RATIONAL);

    unsigned mul = env.signature->getInterpretingSymbol(multiply);
    if(AoverB->nthArgument(0)!=A)return false;

    TermList* B = AoverB->nthArgument(1);

    result = TermList(Term::create2(mul,C,*B));

    RationalConstantType bcon;
    if(theory->tryInterpretConstant(*B,bcon)){
      ASS(!bcon.isZero());
      if(bcon.isNegative()){ swap=!swap; } // switch the polarity of an inequality if we're under one
      return true;
    }
    // Unsure exactly what the best thing to do here, so for now give up
    // This means we only balance when we have a constant on the variable side
    return false;    
}

/**
 * Used to evaluate a literal, setting isConstant, resLit and resConst in the process
 *
 * Returns true if it has been evaluated, in which case resLit is set 
 * isConstant is true if the literal predicate evaluates to a constant value
 * resConst is set iff isConstant and gives the constant value (true/false) of resLit 
 */
bool InterpretedLiteralEvaluator::evaluate(Literal* lit, bool& isConstant, Literal*& resLit, bool& resConst,Stack<Literal*>& sideConditions)
{
  CALL("InterpretedLiteralEvaluator::evaluate");

  //cout << "evaluate " << lit->toString() << endl;

  // This tries to transform each subterm using tryEvaluateFunc (see transform Subterm below)
  resLit = TermTransformer::transform(lit);

  //cout << "transformed " << resLit->toString() << endl;

  // If it can be balanced we balance it
  // A predicate on constants will not be balancable
  if(balancable(resLit)){
      Literal* new_resLit=resLit;
      bool balance_result = balance(resLit,new_resLit,sideConditions);
      ASS(balance_result || resLit==new_resLit);
      resLit=new_resLit;
  }
  //else{ cout << "NOT" << endl; }

  // If resLit contains variables the predicate cannot be interpreted
  VariableIterator vit(lit);
  if(vit.hasNext()){
    isConstant=false;
    return (lit!=resLit);
  }
  //cout << resLit->toString()<< " is variable free, evaluating..." << endl;

  unsigned pred = resLit->functor();

  // Now we try and evaluate the predicate
  Evaluator* predEv = getPredEvaluator(pred);
  if (predEv) {
    if (predEv->tryEvaluatePred(resLit, resConst)) {
        //cout << "pred evaluated " << resConst << endl;
	isConstant = true;
	return true;
    }
  }
  if (resLit!=lit) {
    isConstant = false;
    return true;
  }
  return false;
}

/**
 * This attempts to evaluate each subterm.
 * See Kernel/TermTransformer for how it is used.
 * Terms are evaluated bottom-up
 */
TermList InterpretedLiteralEvaluator::transformSubterm(TermList trm)
{
  CALL("InterpretedLiteralEvaluator::transformSubterm");

  //cout << "transformSubterm for " << trm.toString() << endl;

  if (!trm.isTerm()) { return trm; }
  Term* t = trm.term();
  unsigned func = t->functor();

  Evaluator* funcEv = getFuncEvaluator(func);
  if (funcEv) {
    TermList res;
    if (funcEv->tryEvaluateFunc(t, res)) {
	return res;
    }
  }
  return trm;
}

/**
 * This searches for an Evaluator for a function
 */
InterpretedLiteralEvaluator::Evaluator* InterpretedLiteralEvaluator::getFuncEvaluator(unsigned func)
{
  CALL("InterpretedLiteralEvaluator::getFuncEvaluator");

  if (func>=_funEvaluators.size()) {
    unsigned oldSz = _funEvaluators.size();
    unsigned newSz = func+1;
    _funEvaluators.expand(newSz);
    for (unsigned i=oldSz; i<newSz; i++) {
	EvalStack::Iterator evit(_evals);
	while (evit.hasNext()) {
	  Evaluator* ev = evit.next();
	  if (ev->canEvaluateFunc(i)) {
	    ASS_EQ(_funEvaluators[i], 0); //we should have only one evaluator for each function
            _funEvaluators[i] = ev;
	  }
	}
    }
  }
  return _funEvaluators[func];
}

/**
 * This searches for an Evaluator for a predicate
 */
InterpretedLiteralEvaluator::Evaluator* InterpretedLiteralEvaluator::getPredEvaluator(unsigned pred)
{
  CALL("InterpretedLiteralEvaluator::getPredEvaluator");

  if (pred>=_predEvaluators.size()) {
    unsigned oldSz = _predEvaluators.size();
    unsigned newSz = pred+1;
    _predEvaluators.expand(newSz);
    for (unsigned i=oldSz; i<newSz; i++) {
      EvalStack::Iterator evit(_evals);
      while (evit.hasNext()) {
	Evaluator* ev = evit.next();
	if (ev->canEvaluatePred(i)) {
	  ASS_EQ(_predEvaluators[i], 0); //we should have only one evaluator for each predicate
	  _predEvaluators[i] = ev;
	}
      }
    }
  }
  return _predEvaluators[pred];
}

}
