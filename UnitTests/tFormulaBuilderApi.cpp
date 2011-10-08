
#include <iostream>
#include <sstream>
#include <map>

#include "Api/FormulaBuilder.hpp"
#include "Api/Problem.hpp"

#include "Lib/DHSet.hpp"

#include "Kernel/Term.hpp"

#include "Test/UnitTesting.hpp"

#define UNIT_ID fbapi
UT_CREATE;

using namespace std;
using namespace Api;



TEST_FUN(fbapi1)
{
  try {
    FormulaBuilder api(true);

    Var xv = api.var("X"); // variable x
    Var yv = api.var("Y"); // variable y
    Term x =  api.varTerm(xv); // term x
    Term y =  api.varTerm(yv); // term y
    Function f = api.function("f",1);
    Term fx = api.term(f,x); // f(x)
    Term fy = api.term(f,y); // f(y)
    Formula lhs = api.equality(fx,fy); // f(x) = f(y)
    Predicate p=api.predicate("p",3);
    Formula rhs = api.formula(p,x,fx,fy); // p(X0,f(X0),f(X1))

    Formula result = api.formula(FormulaBuilder::IMP,lhs,rhs); // f(X0) = f(X1) => p(X0,f(X0),f(X1))


    string formString=result.toString();

    stringstream sstr;
    sstr << result;
    ASS_EQ(sstr.str(), formString);

    cout << endl << "Should print something like \"f(X) = f(Y) => p(X,f(X),f(Y))\"" << endl;
    // example: output
    cout << formString << endl;

    AnnotatedFormula ares = api.annotatedFormula(result, FormulaBuilder::ASSUMPTION);
    cout << endl << "Should print something like \"fof(u1,hypothesis,( f(X) = f(Y) => p(X,f(X),f(Y)) )).\"" << endl;
    cout << ares << endl;

    AnnotatedFormula ares2 = api.annotatedFormula(result, FormulaBuilder::CONJECTURE, "conj123");
    cout << endl << "Should print something equivalent to \"fof(conj123,conjecture,( f(X) = f(Y) => p(X,f(X),f(Y)) )).\"" << endl;
    cout << ares2 << endl;
  }
  catch (FormulaBuilderException e)
  {
    cout<< "Exception: "<<e.msg()<<endl;
    throw;
  }
}

TEST_FUN(fbapiReflection)
{
  try {
    FormulaBuilder api(true);

    Var xv = api.var("X"); // variable x
    Var yv = api.var("Y"); // variable y
    Term x =  api.varTerm(xv); // term x
    Term y =  api.varTerm(yv); // term y
    Function fun = api.function("f",1);
    Term fx = api.term(fun,x); // f(x)
    Term fy = api.term(fun,y); // f(y)
    Formula f1 = api.equality(fx,fy); // f(x) = f(y)

    Formula f1neg = api.negation(f1);

    ASS(f1neg.isNegation());
    ASS(!f1neg.boundVars().hasNext());


    DHSet<string> vs;
    vs.loadFromIterator(f1neg.freeVars());
    ASS_EQ(vs.size(),2);
    ASS(vs.find("X"));
    ASS(vs.find("Y"));

    AnnotatedFormula af1neg = api.annotatedFormula(f1neg, FormulaBuilder::ASSUMPTION);
    ASS(!af1neg.boundVars().hasNext());

    vs.reset();
    vs.loadFromIterator(af1neg.freeVars());
    ASS_EQ(vs.size(),2);
    ASS(vs.find("X"));
    ASS(vs.find("Y"));

    AnnotatedFormula af1conj = api.annotatedFormula(f1neg, FormulaBuilder::CONJECTURE);
    ASS(!af1conj.freeVars().hasNext());

    vs.reset();
    vs.loadFromIterator(af1conj.boundVars());
    ASS_EQ(vs.size(),2);
    ASS(vs.find("X"));
    ASS(vs.find("Y"));


    ASS(api.trueFormula().isTrue());
    ASS(api.falseFormula().isFalse());

    Formula fnull;
    ASS(fnull.isNull());
    ASS(!fnull.freeVars().hasNext());

    Term tnull;
    ASS(tnull.isNull());

    AnnotatedFormula afnull;
    ASS(afnull.isNull());

    cout<<endl<<af1neg.toString()<<endl;
    cout<<af1neg.formula().toString()<<endl;
    cout<<af1conj.toString()<<endl;
    cout<<af1conj.formula().toString()<<endl;
    ASS_EQ(af1neg.annotation(),FormulaBuilder::ASSUMPTION);
    ASS_EQ(af1conj.annotation(),FormulaBuilder::CONJECTURE);
    ASS_EQ(af1neg.formula().connective(),FormulaBuilder::NOT);
    ASS_EQ(af1conj.formula().connective(),FormulaBuilder::FORALL);
    ASS_EQ(af1conj.formula().formulaArg(0).connective(),FormulaBuilder::NOT);
    ASS_EQ(af1conj.formula().formulaArg(0).formulaArg(0).connective(),FormulaBuilder::ATOM);
    ASS_EQ(af1conj.formula().formulaArg(0).formulaArg(0).predicate(),0);
    ASS_EQ(af1conj.formula().formulaArg(0).formulaArg(0).argCnt(),2);
    Term t = af1conj.formula().formulaArg(0).formulaArg(0).termArg(1);
    ASS(!t.isVar());
    ASS_EQ(t.functor(),fun);
    ASS_EQ(t.arity(),1);
    ASS(t.arg(0).isVar());
    ASS_NEQ(af1conj.formula().formulaArg(0).formulaArg(0).termArg(0).arg(0).var(), t.arg(0).var());
  }
  catch (FormulaBuilderException e)
  {
    cout<< "Exception: "<<e.msg()<<endl;
    throw;
  }
}

TEST_FUN(fbapiSubst)
{
  try {
    FormulaBuilder api(true);

    Var xv = api.var("X"); // variable x
    Var yv = api.var("Y"); // variable y
    Term x =  api.varTerm(xv); // term x
    Term y =  api.varTerm(yv); // term y
    Function fun = api.function("f",1);
    Function cfun = api.function("c",0);
    Term c = api.term(cfun); // c
    Term fx = api.term(fun,x); // f(x)
    Term fy = api.term(fun,y); // f(y)
    Term fc = api.term(fun,c); // f(c)
    Term ffc = api.term(fun,fc); // f(f(c))
    Formula f1 = api.equality(fx,fy); // f(x) = f(y)
    Formula f2 = api.equality(fc,ffc); // f(c) = f(f(c))

    Formula f1neg = api.negation(f1);
    AnnotatedFormula af1neg = api.annotatedFormula(f1neg, FormulaBuilder::ASSUMPTION);
    AnnotatedFormula af1conj = api.annotatedFormula(f1neg, FormulaBuilder::CONJECTURE);

    cout<<f1neg.toString()<<endl;
    cout<<api.substitute(f1neg, xv, fx).toString()<<endl;
    cout<<api.substitute(api.substitute(f1neg, xv, fx), xv, fx).toString()<<endl;
    cout<<api.substitute(api.substitute(af1neg, xv, fx), xv, fx).toString()<<endl;
    cout<<api.substitute(api.substitute(fx, xv, fx), xv, fx).toString()<<endl;

    Formula f2neg = api.negation(f2);
    AnnotatedFormula af2neg = api.annotatedFormula(f2neg, FormulaBuilder::ASSUMPTION);
    AnnotatedFormula af2conj = api.annotatedFormula(f2neg, FormulaBuilder::CONJECTURE);
    cout<<af2neg.toString()<<endl;
    cout<<api.replaceConstant(af2neg, c, fx).toString()<<endl;
    cout<<api.replaceConstant(ffc, c, y).toString()<<endl;

  }
  catch (ApiException e)
  {
    cout<< "Exception: "<<e.msg()<<endl;
    throw;
  }
}

TEST_FUN(fbapiStrConv)
{
  try {
    FormulaBuilder api(true, true);

    Var xv = api.var("X");
    Var yv = api.var("Y");
    Function ct = api.function("c",0);
    Function f = api.function("f",1);
    Function g = api.function("g",2);
    Predicate p = api.predicate("p",1);

    Term x = api.varTerm(xv); // c
    Term y = api.varTerm(yv); // c
    Term c = api.term(ct); // c
    Term fc = api.term(f,c); // f(c)
    Term ffc = api.term(f,fc); // f(c)
    Term fffc = api.term(f,ffc); // f(c)

    Term gxfffc = api.term(g,x,fffc); // g(X,f(f(f(c))))
    ASS_EQ(gxfffc.toString(), "g(X,f(f(f(c))))");

    Term fgxfffc = api.term(f,gxfffc); // f(g(X,f(f(f(c)))))

    Term gfgxfffcfgxfffc = api.term(g,fgxfffc,fgxfffc); // g(f(g(X,f(f(f(c))))),f(g(X,f(f(f(c))))))
    ASS_EQ(gfgxfffcfgxfffc.toString(), "g(f(g(X,f(f(f(c))))),f(g(X,f(f(f(c))))))");

    Formula f1=api.equality(gxfffc,y);
    ASS_NEQ(f1.toString().find("Y"), string::npos);
    ASS_NEQ(f1.toString().find("g(X,f(f(f(c))))"), string::npos);
    ASS_NEQ(f1.toString().find("="), string::npos);

    Formula f2=api.atom(p,&gfgxfffcfgxfffc, false);
    ASS_EQ(f2.toString(), "~p(g(f(g(X,f(f(f(c))))),f(g(X,f(f(f(c)))))))");

    Formula f3=api.formula(FormulaBuilder::AND, api.negation(f1), api.formula(FormulaBuilder::EXISTS,xv,f2));
    ASS_REP2(f3.toString().find(f1.toString())!=string::npos, f3.toString(),f1.toString());
    ASS_REP2(f3.toString().find(f2.toString())!=string::npos, f3.toString(),f2.toString());
    ASS_REP(f3.toString().find("[X]")!=string::npos, f3.toString());

    try{
      Formula f4=api.formula(FormulaBuilder::EXISTS,xv,f3); //binding bound variable
      ASSERTION_VIOLATION;
    } catch(FormulaBuilderException) {
    }
  }
  catch (FormulaBuilderException e)
  {
    cout<< "Exception: "<<e.msg()<<endl;
    throw;
  }


}

TEST_FUN(fbapiErrors)
{
  FormulaBuilder api(true, true);

  try {
    api.var("x"); // lowercase variable name
    ASSERTION_VIOLATION;
  } catch (InvalidTPTPNameException e)
  {
    ASS_EQ(e.name(), "x");
  }

  try {
    api.function("F",1); // uppercase function name
    ASSERTION_VIOLATION;
  } catch (InvalidTPTPNameException e)
  {
    ASS_EQ(e.name(), "F");
  }

  try {
    api.predicate("P",1); // uppercase predicate name
    ASSERTION_VIOLATION;
  } catch (InvalidTPTPNameException e)
  {
    ASS_EQ(e.name(), "P");
  }

  Var x = api.var("X");
  Term xt = api.varTerm(x);
  Function f = api.function("e_f",4);
  Predicate p = api.predicate("e_p",4);
  Predicate q = api.predicate("e_q",1);

  try {
    api.term(f,xt,xt,xt); // invalid function arity
    ASSERTION_VIOLATION;
  } catch (FormulaBuilderException e)
  {
  }

  try {
    api.formula(p,xt,xt,xt); // invalid predicate arity
    ASSERTION_VIOLATION;
  } catch (FormulaBuilderException e)
  {
  }

  try{
    FormulaBuilder api2(true);
    Formula f2=api2.negation(api.formula(q,xt)); //mixing formulas from diferent FormulaBuilders
    ASSERTION_VIOLATION;
  }
  catch (FormulaBuilderException e)
  {
  }

  try{
    Formula f1=api.formula(FormulaBuilder::FORALL,x,api.formula(q,xt));
    Formula f2=api.formula(FormulaBuilder::FORALL,x,f1); //binding bound variable
    ASSERTION_VIOLATION;
  }
  catch (FormulaBuilderException e)
  {
  }
}

TEST_FUN(fbapiProblem)
{
  Problem prb;
  stringstream stm("cnf(a,axiom,p(X) | q(Y) | q(X)).");

  prb.addFromStream(stm);

  AnnotatedFormulaIterator fit=prb.formulas();

  ASS(fit.hasNext());
  AnnotatedFormula af=fit.next();
  ASS(!fit.hasNext());

  int i=0;
  StringIterator sit=af.freeVars();
  while(sit.hasNext()) {
    sit.next();
    i++;
  }
  ASS_EQ(i,2);

  sit=af.freeVars();
  DHSet<string> sset;
  sset.loadFromIterator(sit);
  ASS_EQ(sset.size(), 2);

}

TEST_FUN(fbapiClausifySmall)
{
  try {
    FormulaBuilder api;

    Var xv = api.var("Var");
    Term x = api.varTerm(xv);
    Predicate p=api.predicate("p",1);
    Predicate q=api.predicate("q",0);

    Formula fpx=api.formula(p,x);
    Formula fq=api.formula(q);
    Formula fQpx=api.formula(FormulaBuilder::FORALL, xv, fpx);
    Formula fQpxOq=api.formula(FormulaBuilder::OR, fQpx, fq);

    AnnotatedFormula af=api.annotatedFormula(fQpxOq,FormulaBuilder::CONJECTURE, "conj1");
    Problem prb;
    prb.addFormula(af);
    prb.output(cout);

    Problem cprb=prb.clausify(0,false,Problem::INL_OFF,false);
    cprb.output(cout);

  } catch (ApiException e) {
    cout<<"Exception: "<<e.msg()<<endl;
    throw;
  }
}


TEST_FUN(fbapiClausify)
{
  try {
    FormulaBuilder api;

    Var xv = api.var("Var");
    Var yv = api.var("Var2");
    Term x = api.varTerm(xv);
    Term y = api.varTerm(yv);
    Predicate p=api.predicate("p",1);
    Predicate q=api.predicate("q",0);

    Formula fpx=api.formula(p,x);
    Formula fpy=api.formula(p,y);
    Formula fq=api.formula(q);
    Formula fpxOq=api.formula(FormulaBuilder::OR, fpx, fq);
    Formula fFpxOq=api.formula(FormulaBuilder::FORALL, xv, fpxOq);
    Formula fpyAFpxOq=api.formula(FormulaBuilder::AND, fpy, fFpxOq);

    AnnotatedFormula af=api.annotatedFormula(fpyAFpxOq,FormulaBuilder::CONJECTURE, "abc123");

    cout<<endl<<"FOF:"<<endl;
    cout<<af<<endl;

    Problem prb;
    prb.addFormula(af);

    Problem sprb=prb.skolemize(0,false,Problem::INL_OFF,false);
    cout<<"Skolemized:"<<endl;
    AnnotatedFormulaIterator afit=sprb.formulas();
    while(afit.hasNext()) {
      cout<<afit.next()<<endl;
    }

    Problem cprb=prb.clausify(0,false,Problem::INL_OFF,false);
    cout<<"CNF:"<<endl;
    afit=cprb.formulas();
    while(afit.hasNext()) {
      cout<<afit.next()<<endl;
    }

    cprb=sprb.clausify(0,false,Problem::INL_OFF,false);
    cout<<"CNF from skolemized:"<<endl;
    afit=cprb.formulas();
    while(afit.hasNext()) {
      cout<<afit.next()<<endl;
    }
  } catch (ApiException e) {
    cout<<"Exception: "<<e.msg()<<endl;
    throw;
  }
}

TEST_FUN(fbapiClausifyDefinitions)
{
  try {
    Problem prb;
    stringstream stm("fof(a,axiom,(? [X]: p(X)&p(a2)) | (p(b1)&p(b2)) | (p(c1)&p(c2)) | (p(d1)&p(d2)) | (p(e1)&p(e2))).");
    prb.addFromStream(stm);

    cout<<"Problem:"<<endl;
    AnnotatedFormulaIterator afit=prb.formulas();
    while(afit.hasNext()) {
      cout<<afit.next()<<endl;
    }

    Problem cprb = prb.clausify(4, true, Problem::INL_OFF, false);
    cout<<"Clausified, naming_threshold=4:"<<endl;
    afit=cprb.formulas();
    while(afit.hasNext()) {
      cout<<afit.next()<<endl;
    }
  } catch (ApiException e) {
    cout<<"Exception: "<<e.msg()<<endl;
    throw;
  }
}

string getId(Term t)
{
  static std::map<string,string> idMap;

  stringstream newIdStr;
  newIdStr<<"t_"<<idMap.size();
  string newId=newIdStr.str();

  string id=(*idMap.insert(make_pair(t.toString(), newId)).first).second;
  return id;
}

TEST_FUN(fbapiIds)
{
  FormulaBuilder api;

  Var xv = api.var("X");
  Term x = api.varTerm(xv);
  Function f = api.function("f",1);
  Term t=x;
  for(int i=0;i<5;i++) {
    cout<<t.toString()<<" "<<getId(t)<<endl;
    t=api.term(f,t);
  }
  t=x;
  for(int i=0;i<5;i++) {
    cout<<t.toString()<<" "<<getId(t)<<endl;
    t=api.term(f,t);
  }
}

TEST_FUN(fbapiSorts)
{
  try {
    FormulaBuilder api;

    Sort s1 = api.sort("sort1");
    Sort s2 = api.sort("sort2");
    cout<<s1<<" "<<s2<<" "<<api.defaultSort()<<endl;
    Var xv = api.var("VarS1", s1);
    Var yv = api.var("VarS2", s2);
    Var zv = api.var("VarDef");
    Function cSym=api.function("c_s1",0,s1,0);
    Function dSym=api.function("d_s2",0,s2,0);
    Term x = api.varTerm(xv);
    Term y = api.varTerm(yv);
    Term z = api.varTerm(zv);
    Term c = api.term(cSym);
    Term d = api.term(dSym);

    ASS_EQ(x.sort(), s1);
    ASS_EQ(y.sort(), s2);
    ASS_EQ(z.sort(), api.defaultSort());
    ASS_EQ(c.sort(), s1);
    ASS_EQ(d.sort(), s2);

    Predicate p=api.predicate("p_s1",1,&s1);
    Predicate r=api.predicate("r_s2",1,&s2);
    Sort qSorts[] = {s1, s2, api.defaultSort()};
    Predicate q=api.predicate("q_s1_s2_i",3,qSorts);

    Formula fpx=api.formula(p,x);
    Formula fpc=api.formula(p,c);
    Formula fry=api.formula(r,y);
    Formula frd=api.formula(r,d);
    Formula fqxyz=api.formula(q,x,y,z);
    Formula fqcdz=api.formula(q,c,d,z);
    Formula fxEQx=api.equality(x,x);
    Formula fxEQc=api.equality(x,c);
    Formula fxEQc2=api.equality(x,c,s1);
    Formula fzEQz=api.equality(z,z);
    Formula fOr=api.formula(FormulaBuilder::OR, fqxyz, frd);
    Formula fEx=api.formula(FormulaBuilder::EXISTS, xv, fOr);
    AnnotatedFormula af = api.annotatedFormula(fEx, FormulaBuilder::AXIOM, "ax1");

    Formula fAnd=api.formula(FormulaBuilder::AND, fpx, fry);
    Formula fOr2=api.formula(FormulaBuilder::OR, fAnd, fAnd);
    Formula fOr4=api.formula(FormulaBuilder::OR, fOr2, fOr2);
    Formula fOr8=api.formula(FormulaBuilder::OR, fOr4, fOr4);
    AnnotatedFormula af2 = api.annotatedFormula(fOr8, FormulaBuilder::AXIOM, "ax2");

    OutputOptions::setSortedEquality(true);
    cout<<fxEQc<<" ";
    OutputOptions::setSortedEquality(false);
    cout<<fxEQc<<endl;

    OutputOptions::setSortedEquality(true);
    cout<<fxEQx<<" ";
    OutputOptions::setSortedEquality(false);
    cout<<fxEQx<<endl;

    OutputOptions::setSortedEquality(true);
    cout<<fzEQz<<" ";
    OutputOptions::setSortedEquality(false);
    cout<<fzEQz<<endl;

    Problem prb;
    prb.addFormula(af);
    prb.addFormula(af2);
    cout<<"Orig:"<<endl<<af<<af2<<endl;
    prb.outputTypeDefinitions(cout);

    OutputOptions::setTffFormulas(true);
    cout<<"Clausified:"<<endl;
    Problem cprb = prb.clausify(4,true,Problem::INL_OFF,false);
    AnnotatedFormulaIterator afit=cprb.formulas();
    while(afit.hasNext()) {
      cout<<afit.next()<<endl;
    }
    prb.outputTypeDefinitions(cout, true);
    OutputOptions::setTffFormulas(false);

    try{
      api.equality(x,y);
      ASSERTION_VIOLATION;
    } catch (SortMismatchException e)
    {}

    try{
      api.equality(x,c, s2);
      ASSERTION_VIOLATION;
    } catch (SortMismatchException e)
    {}

    try{
      api.formula(q,x,y,c);
      ASSERTION_VIOLATION;
    } catch (SortMismatchException e)
    {}

    try{
      api.predicate("p1234",1,&s1);
      api.predicate("p1234",1,&s2);
      ASSERTION_VIOLATION;
    } catch (FormulaBuilderException e)
    {}

    try{
      api.var("Var1234",s1);
      api.var("Var1234",s2);
      ASSERTION_VIOLATION;
    } catch (FormulaBuilderException e)
    {}

  } catch (ApiException e) {
    cout<<"Exception: "<<e.msg()<<endl;
    throw;
  }
}

TEST_FUN(fbapiSine)
{
  try {
    Problem prb;
    stringstream stm("fof(a1,axiom,a|b).fof(a2,axiom,b|c).fof(a3,axiom,b|d).fof(a4,axiom,d).fof(a4,axiom,d|e).fof(a5,conjecture,a).");
    prb.addFromStream(stm);
    Problem::PreprocessingOptions opts;
    opts.mode = Problem::PM_SELECTION_ONLY;
    opts.sineSelection = true;
    Problem prb1 = prb.preprocess(opts);
    prb1.output(cout, false);
    cout<<"------\n";
    opts.mode = Problem::PM_CLAUSIFY;
    opts.unusedPredicateDefinitionRemoval = false;
    opts.sineTolerance = 3;
    opts.traceClausification = true;
    Problem prb2 = prb.preprocess(opts);
    prb2.output(cout, false);

  } catch (ApiException e) {
    cout<<"Exception: "<<e.msg()<<endl;
    throw;
  }
}

TEST_FUN(fbapiAttributes)
{
  try {
    FormulaBuilder api;

    Function c=api.function("c",0);
    Predicate p=api.predicate("p",1);

    api.addAttribute(p, "a1", "v1");
    api.addAttribute(p, "a2", "v2");
    api.addAttribute(p, "a3", "v3");
    api.addAttribute(p, "a3", "v3");
    api.addAttribute(p, "a3", "v3");

    ASS_EQ(api.attributeCount(p),3);
    ASS_EQ(api.getAttributeName(p,0),"a1");
    ASS_EQ(api.getAttributeName(p,1),"a2");
    ASS_EQ(api.getAttributeName(p,2),"a3");
    ASS_EQ(api.getAttributeValue(p,"a2"),"v2");

    try{
      api.getAttributeValue(p,"a4");
      ASSERTION_VIOLATION;
    } catch (FormulaBuilderException e)
    {}

    try{
      api.getAttributeValue(p,4);
      ASSERTION_VIOLATION;
    } catch (FormulaBuilderException e)
    {}

    api.addAttribute(c, "b1", "v1");
    ASS_EQ(api.getAttributeValue(c,"b1"),"v1");

    Sort s = api.sort("srt");
    api.addAttribute(s, "strAttr", "val");
    api.addAttribute(s, "strAttr2", "val2");

    Problem prb;
    Term ctrm = api.term(c);
    Formula f=api.formula(p, ctrm);
    AnnotatedFormula af = api.annotatedFormula(f, FormulaBuilder::AXIOM, "ax1");
    prb.addFormula(af);
    prb.output(cout,true, true);


  } catch (ApiException e) {
    cout<<"Exception: "<<e.msg()<<endl;
    throw;
  }
}

TEST_FUN(fbapiTff)
{
  Problem prb;
  stringstream stm("fof(a,axiom,p(X) | q(Y) | q(X)).");
  prb.addFromStream(stm);

  OutputOptions::setTffFormulas(true);
  prb.output(cout);
  OutputOptions::setTffFormulas(false);
}

TEST_FUN(fbapiInts)
{
  FormulaBuilder api;
  Sort iSort = api.integerSort();
  Function one = api.integerConstant("1");
  Function two = api.integerConstant(2);
  Term oneT = api.term(one);
  Term twoT = api.term(two);
  Predicate leqP = api.interpretedPredicate(FormulaBuilder::INT_LESS_EQUAL);
  Formula eq = api.equality(oneT, twoT, iSort, true);
  cout << eq.toString() << endl;
  Formula leq = api.formula(leqP, oneT, twoT);
  cout << leq.toString() << endl;

  Problem prb;
  prb.outputTypeDefinitions(cout, true);
}

TEST_FUN(fbapiDummyNames)
{
  try {
    FormulaBuilder api(true, false, true, true);

    Sort s1 = api.sort("sort1");
    Sort s2 = api.sort("sort2");
    cout<<s1<<" "<<s2<<" "<<api.defaultSort()<<endl;
    Var xv = api.var("VarS1", s1);
    Var yv = api.var("VarS2", s2);
    Var zv = api.var("VarDef");
    Function cSym=api.function("c_s1",0,s1,0);
    Function dSym=api.function("d_s2",0,s2,0);
    Term x = api.varTerm(xv);
    Term y = api.varTerm(yv);
    Term z = api.varTerm(zv);
    Term c = api.term(cSym);
    Term d = api.term(dSym);

    ASS_EQ(x.sort(), s1);
    ASS_EQ(y.sort(), s2);
    ASS_EQ(z.sort(), api.defaultSort());
    ASS_EQ(c.sort(), s1);
    ASS_EQ(d.sort(), s2);

    Predicate p=api.predicate("p_s1",1,&s1);
    Predicate r=api.predicate("r_s2",1,&s2);
    Sort qSorts[] = {s1, s2, api.defaultSort()};
    Predicate q=api.predicate("q_s1_s2_i",3,qSorts);

    Formula fpx=api.formula(p,x);
    Formula fpc=api.formula(p,c);
    Formula fry=api.formula(r,y);
    Formula frd=api.formula(r,d);
    Formula fqxyz=api.formula(q,x,y,z);
    Formula fqcdz=api.formula(q,c,d,z);
    Formula fxEQx=api.equality(x,x);
    Formula fxEQc=api.equality(x,c);
    Formula fxEQc2=api.equality(x,c,s1);
    Formula fzEQz=api.equality(z,z);
    Formula fOr=api.formula(FormulaBuilder::OR, fqxyz, frd);
    Formula fEx=api.formula(FormulaBuilder::EXISTS, xv, fOr);
    AnnotatedFormula af = api.annotatedFormula(fEx, FormulaBuilder::AXIOM, "ax1");

    Formula fAnd=api.formula(FormulaBuilder::AND, fpx, fry);
    Formula fOr2=api.formula(FormulaBuilder::OR, fAnd, fAnd);
    Formula fOr4=api.formula(FormulaBuilder::OR, fOr2, fOr2);
    Formula fOr8=api.formula(FormulaBuilder::OR, fOr4, fOr4);
    AnnotatedFormula af2 = api.annotatedFormula(fOr8, FormulaBuilder::AXIOM, "ax2");

    Problem prb;
    prb.addFormula(af);
    prb.addFormula(af2);

    OutputOptions::setTffFormulas(true);
    cout<<"Clausified:"<<endl;
    Problem cprb = prb.clausify(4,true,Problem::INL_OFF,false);
    cprb.output(cout, true);
    OutputOptions::setTffFormulas(false);

  } catch (ApiException e) {
    cout<<"Exception: "<<e.msg()<<endl;
    throw;
  }
}
