/**
 * @file Log.cpp
 * Implements class Log.
 */

#include "Log.hpp"

#if LOGGING

#include "Forwards.hpp"

#include "Lib/ArrayMap.hpp"
#include "Lib/Backtrackable.hpp"
#include "Lib/DArray.hpp"
#include "Lib/DHMap.hpp"
#include "Lib/Environment.hpp"
#include "Lib/Exception.hpp"
#include "Lib/Int.hpp"
#include "Lib/ScopedPtr.hpp"
#include "Lib/Stack.hpp"
#include "Lib/System.hpp"
#include "Lib/Timer.hpp"

#include "Kernel/InferenceStore.hpp"
#include "Kernel/Clause.hpp"
#include "Kernel/Formula.hpp"
#include "Kernel/FormulaUnit.hpp"
#include "Kernel/Unit.hpp"


namespace Debug
{

using namespace Lib;

Logging::TagDeclTrigger Logging::s_trigger;

struct Logging::ChildInfo {
  TagInfo& child;
  unsigned depth;

  ChildInfo(TagInfo& child, unsigned depth)
   : child(child), depth(depth) {}
};

struct Logging::TagInfo : public TagInfoBase {

  CLASS_NAME("TagInfo");
  USE_ALLOCATOR(TagInfo);

  typedef Stack<ChildInfo> ChildStack;

  unsigned tagIndex;
  string doc;
  ChildStack children;


  /** Premises of units displayed through logUnit function will be printed */
  bool printUnitPremises;

  Stack<StatObserver*> statObservers;

  TagInfo(string name, unsigned index) : TagInfoBase(name), tagIndex(index), printUnitPremises(false) {}
};

struct Logging::StatObserver {
  CLASS_NAME("Logging::StatObserver");
  USE_ALLOCATOR_UNK;

  virtual ~StatObserver() {}

  virtual void onSimple() {}
  virtual void onInt(int num) {}
  virtual void onUnit(Kernel::Unit* unit) {}
  virtual void onFinalize() {}

  virtual void reset() {}
  /** display caption without final end of line */
  virtual void displayCaption(ostream& out) = 0;
  /** display data with the final end of line */
  virtual void displayData(ostream& out) = 0;

  virtual bool hasData() = 0;
};


class Logging::Impl
{
public:
  struct EnablingSpec {
    bool logEnable;
    bool logPrintUnitPrems;
    StatObserver* statObserver;

    EnablingSpec() : logEnable(false), logPrintUnitPrems(false), statObserver(0) {}
  };
private:
  struct SimpleObserver : public StatObserver {
    SimpleObserver(string name) : _name(name), _counter(0) {}

    virtual void onSimple() { _counter++; }
    virtual void onInt(int num) { _counter += num; }
    virtual void onUnit(Kernel::Unit* unit) { _counter++; }

    virtual void reset() { _counter = 0; }
    virtual void displayCaption(ostream& out) { out << _name; }
    virtual void displayData(ostream& out) {
      out << _counter << endl;
    }

    virtual bool hasData() { return _counter!=0; }

    string _name;
    int _counter;
  };
  struct AverageObserver : public StatObserver {
    AverageObserver(string name) : _name(name), _counter(0) {}

    virtual void onInt(int num) {
      _counter++;
      _sum+=num;
    }

    virtual void reset() { _counter = 0; _sum = 0; }
    virtual void displayCaption(ostream& out) { out << _name << " average"; }
    virtual void displayData(ostream& out)
    {
      CALL("Logging::Impl::AverageObserver::displayData");
      if(_counter) {
//	streamsize oldPrec = out.precision(3);
	out << (static_cast<float>(_sum))/_counter << endl;
//	out.precision(oldPrec);
      }
      else {
	out << 0 << endl;
      }
    }

    virtual bool hasData() { return _counter!=0; }

    string _name;
    int _counter;
    int _sum;
  };

  struct MetaObserver : public StatObserver {
    MetaObserver(StatObserver* inner, string captionSuffix)
    : _inner(inner), _captionSuffix(captionSuffix) {}

    virtual void onFinalize() { _inner->onFinalize(); }

    virtual void reset() { _inner->reset(); }
    virtual void displayCaption(ostream& out) {
      _inner->displayCaption(out);
      out << " " << _captionSuffix;
    };
    virtual void displayData(ostream& out) {
      _inner->displayData(out);
    }

    virtual bool hasData() { return _inner->hasData(); }

    ScopedPtr<StatObserver> _inner;
    string _captionSuffix;
  };

  struct UnitWeightObserver : public MetaObserver {
    UnitWeightObserver(StatObserver* inner) : MetaObserver(inner,"weight") {}

    virtual void onUnit(Kernel::Unit* unit) {
      CALL("Logging::Impl::UnitWeightObserver::onUnit");
      unsigned weight;
      if(unit->isClause()) {
	weight = static_cast<Kernel::Clause*>(unit)->weight();
      }
      else {
	weight = static_cast<Kernel::FormulaUnit*>(unit)->formula()->weight();
      }
      _inner->onInt(weight);
    }
  };

  struct ClauseLengthObserver : public MetaObserver {
    ClauseLengthObserver(StatObserver* inner) : MetaObserver(inner,"clause length") {}

    virtual void onUnit(Kernel::Unit* unit) {
      CALL("Logging::Impl::ClauseLengthObserver::onUnit");
      if(unit->isClause()) {
	_inner->onInt(static_cast<Kernel::Clause*>(unit)->length());
      }
    }
  };

  struct TimedObserver : public MetaObserver {
    TimedObserver(StatObserver* inner, unsigned intervalMS)
    : MetaObserver(inner,"t"+Int::toString(intervalMS)),
      _interval(intervalMS),
      _segmentCnt(0)
    {
      CALL("Logging::Impl::TimedObserver::TimedObserver");
      _startTime = env.timer->elapsedMilliseconds();
      _currSegmentStartTime = _startTime;
      _currSegmentEndTime = _currSegmentStartTime + _interval;
    }

    virtual void onSimple() {
      CALL("Logging::Impl::TimedObserver::onSimple");
      finishPassed();
      _inner->onSimple();
    }
    virtual void onInt(int num) {
      CALL("Logging::Impl::TimedObserver::onInt");
      finishPassed();
      _inner->onInt(num);
    }
    virtual void onUnit(Kernel::Unit* unit) {
      CALL("Logging::Impl::TimedObserver::onUnit");
      finishPassed();
      _inner->onUnit(unit);
    }

    virtual void onFinalize() {
      CALL("Logging::Impl::TimedObserver::onUnit");

      finishPassed();
      nextSegment();
    }

    virtual void reset() {
      NOT_IMPLEMENTED;
    }
    virtual void displayCaption(ostream& out) {
      out << "overview of ";
      _inner->displayCaption(out);
      out << " " << _captionSuffix;
    };
    virtual void displayData(ostream& out) {
      out << _segmentCnt <<" segments recorded" << endl;
    }

    virtual bool hasData() {
      //TimedObserver doesn't have interesting data to be output
      //because it prints them out gradually over time
      return false;
    }

  private:

    void finishPassed()
    {
      CALL("Logging::Impl::TimedObserver::finishPassed");
      unsigned currTime = env.timer->elapsedMilliseconds();
      while(currTime>_currSegmentEndTime) {
	nextSegment();
      }
    }

    void nextSegment()
    {
      CALL("Logging::Impl::TimedObserver::nextSegment");

      _inner->onFinalize();
      _inner->displayCaption(cerr);
      cerr << " " << _captionSuffix << " at " << (_currSegmentStartTime-_startTime) <<": ";
      _inner->displayData(cerr);
      _inner->reset();
      _segmentCnt++;

      _currSegmentStartTime = _currSegmentEndTime;
      _currSegmentEndTime = _currSegmentStartTime + _interval;
    }

    unsigned _interval;

    unsigned _segmentCnt;
    unsigned _startTime;
    unsigned _currSegmentStartTime;
    unsigned _currSegmentEndTime;
  };

  struct TagEnableBO : public BacktrackObject {

    CLASS_NAME("TagEnableBO");
    USE_ALLOCATOR(TagEnableBO);

    TagInfo& _ti;
    TagInfo _backup;

    /**
     * Create BacktrackObject that will restore the state of TagInfo into the current state
     */
    TagEnableBO(TagInfo& ti)
    : _ti(ti), _backup(ti)
    {}

    virtual void backtrack() {
      _ti = _backup;
    }
  };

  DHMap<std::string, unsigned> _tagNums;
  Stack<TagInfo*> _tags;

  Stack<BacktrackData> _stateStack;

  Stack<StatObserver*> _observers;

  void enableOneTag(TagInfo& ti, const EnablingSpec& eSpec) {
    if(_stateStack.isNonEmpty()) {
      _stateStack.top().addBacktrackObject(new TagEnableBO(ti));
    }

    if(eSpec.logEnable) {
      ti.logEnabled = true;
      if(eSpec.logPrintUnitPrems) {
	ti.printUnitPremises = true;
      }
    }
    if(eSpec.statObserver) {
      ti.statsEnabled = true;
      ti.statObservers.push(eSpec.statObserver);
    }
  }

public:
  ~Impl() {
    while(_stateStack.isNonEmpty()) {
      _stateStack.pop().drop();
    }
    while(_tags.isNonEmpty()) {
      delete _tags.pop();
    }
    while(_observers.isNonEmpty()) {
      delete _observers.pop();
    }
  }

  unsigned tag2idx(const char* tag) const {
    CALL("Logging::Impl::tag2idx");

    unsigned res;
    if(!_tagNums.find(tag, res)) {
      ASS_REP(false,tag);
      throw Exception("Tag \""+string(tag)+"\" does not exist.");
    }
    return res;
  }

  TagInfo& tag2ti(const char* tag) const {
    CALL("Logging::Impl::tag2ti");

    unsigned idx = tag2idx(tag);
    return *_tags[idx];
  }

  /**
   * Declare new tag
   *
   * A tag of given name can be declared at most once
   */
  void declareTag(const char* tag)
  {
    CALL("Logging::Impl::declareTag");

    if(_tagNums.find(tag)) {
      ASS_REP2(false,"tag already declared",tag);
      throw Exception("Tag \""+string(tag)+"\" already declared.");
    }

    unsigned idx = _tags.size();
    _tags.push(new TagInfo(tag, idx));
    _tagNums.insert(tag,idx);
  }

  /**
   * Add documentation string to a tag
   */
  void addDoc(const char* tag, const char* doc)
  {
    CALL("Logging::Impl::declareTag");

    TagInfo& ti = tag2ti(tag);
    ti.doc = doc;
  }

  void addParent(const char* child, const char* parent, unsigned depth)
  {
    CALL("Logging::Impl::addParent");

    TagInfo& childTI = tag2ti(child);
    TagInfo& parTI = tag2ti(parent);

    parTI.children.push(ChildInfo(childTI, depth));
  }


  void pushTagStates()
  {
    CALL("Logging::Impl::pushTagStates");

    _stateStack.push(BacktrackData());
  }

  void popTagStates()
  {
    CALL("Logging::Impl::popTagStates");

    _stateStack.top().backtrack();
    _stateStack.pop();
  }

  /**
   * Enable tag @c tag and all its child tags up to the @c depthLimit.
   */
  void enableTag(const char* tag, const EnablingSpec& eSpec, unsigned depthLimit=UINT_MAX)
  {
    CALL("Logging::Impl::enableTag");

    static ArrayMap<unsigned> enabledDepths;
    enabledDepths.ensure(_tags.size());
    enabledDepths.reset();

    static Stack<ChildInfo> todo;
    ASS(todo.isEmpty());

    todo.push(ChildInfo(tag2ti(tag), depthLimit));
    while(todo.isNonEmpty()) {
      ChildInfo cur = todo.pop();
      TagInfo& ti = cur.child;

      unsigned prevDepth;
      if(enabledDepths.find(ti.tagIndex, prevDepth) && prevDepth>=cur.depth) {
	continue;
      }
      enabledDepths.set(ti.tagIndex, cur.depth);

      enableOneTag(cur.child, eSpec);

      TagInfo::ChildStack::Iterator cit(ti.children);
      while(cit.hasNext()) {
	ChildInfo ci = cit.next();
	if(ci.depth<=cur.depth) {
	  unsigned childDepthLimit = (depthLimit==UINT_MAX) ? UINT_MAX : (cur.depth-ci.depth);
	  todo.push(ChildInfo(ci.child, childDepthLimit));
	}
      }
    }
  }

  /**
   * Build a StatObserver, also put it into the stat observer stack,
   * so that the collected statistic is printed at the end.
   */
  StatObserver* buildStatObserver(char* tagName, char* observerString)
  {
    CALL("Logging::Impl::buildStatObserver");

    StatObserver* res = 0;

    char* curr = observerString;
    bool hasNext;
    do {
      char* specStart = curr;
      while(*curr && *curr!=':') {
	curr++;
      }
      if(*curr==':') {
	*curr = 0;
	curr++;
	hasNext = true;
      }
      else {
	hasNext = false;
      }
      string spec(specStart);
      if(spec=="c") {
	//simple observer
	if(res!=0) {
	  USER_ERROR("simple observer must be the first in the chain");
	}
	res = new SimpleObserver(tagName);
      }
      else if(spec=="a") {
	if(res!=0) {
	  USER_ERROR("simple observer must be the first in the chain");
	}
	res = new AverageObserver(tagName);
      }
      else if(spec=="w") {
	if(res==0) {
	  USER_ERROR("unit weight observer cannot be the first in the chain");
	}
	res = new UnitWeightObserver(res);
      }
      else if(spec=="l") {
	if(res==0) {
	  USER_ERROR("clause length observer cannot be the first in the chain");
	}
	res = new ClauseLengthObserver(res);
      }
      else if(*specStart=='t') {
	if(res==0) {
	  USER_ERROR("timed observer cannot be the first in the chain");
	}
	unsigned interval;
	if(!Int::stringToUnsignedInt(specStart+1, interval)) {
	  USER_ERROR("invalid timed observer specification: \""+spec+"\"");
	}
	res = new TimedObserver(res, interval);
      }
      else {
	USER_ERROR("unknown observer specification: \""+spec+"\"");
      }
    } while(hasNext);

    _observers.push(res);
    return res;
  }

  /**
   * Perform a single tag-enabling command.
   *
   * Function can modify the content of @c str, so this should not
   * affect the caller.
   */
  void processSingleTraceSpecString(char* str)
  {
    CALL("Logging::Impl::processSingleTraceSpecString");

    char* curr = str;

    char* tagNameStart = curr;
    while(*curr && *curr!=':' && *curr!='^' && *curr!='@') {
      curr++;
    }
    bool printPrems = false;
    if(*curr=='^') {
      *curr = 0;
      curr++;
      printPrems = true;
    }
    char* depthStart = 0;
    if(*curr==':') {
      *curr = 0;
      curr++;
      depthStart = curr;
      while(*curr && isdigit(*curr)) {
	curr++;
      }
    }
    char* statSpecStart = 0;
    if(*curr=='@') {
      *curr = 0;
      curr++;
      statSpecStart = curr;
      while(*curr) { //The statistics string is the last, so we just go to the end of string. Later this may change.
	curr++;
      }
    }
    ASS_EQ(*curr,0);

    unsigned depth;
    if(depthStart) {
      if(!Int::stringToUnsignedInt(depthStart, depth)) {
	USER_ERROR("Tag depth specification \""+string(depthStart)+"\" is not valid.");
      }
    }
    else {
      depth = UINT_MAX;
    }

    StatObserver* statObserver = 0;
    if(statSpecStart) {
      statObserver = buildStatObserver(tagNameStart, statSpecStart);
    }

    EnablingSpec eSpec;
    if(statObserver) {
      eSpec.statObserver = statObserver;
      if(printPrems) {
	USER_ERROR("premise printing cannot be specified for tag declaring statistics");
      }
    }
    else {
      eSpec.logEnable = true;
      eSpec.logPrintUnitPrems = printPrems;
    }

    enableTag(tagNameStart, eSpec, depth);
  }

  void processTraceSpecString(std::string str)
  {
    CALL("Logging::Impl::processTraceSpecString");

    if(str.empty()) { return; }
    if(str=="help") {
      displayHelp();
      exit(0);
    }
    if(str=="help+") {
      //with this command we only display help but don't exit
      displayHelp();
    }

    DArray<char> chars;
    chars.initFromArray(str.size()+1, str.c_str());
    char* curr = chars.array();
    while(true) {
      bool lastTag;

      char* tagStart = curr;
      while(*curr && *curr!=',') {
	curr++;
      }
      if(tagStart==curr) {
	USER_ERROR("Tag control string \""+string(str)+"\" is not valid.");
      }

      if(*curr) {
	ASS_EQ(*curr,',');
	*curr = 0;
	lastTag = false;
	curr++;
      }
      else {
	lastTag = true;
      }

      processSingleTraceSpecString(tagStart);

      if(lastTag) {
	break;
      }
      if(!*curr) {
	USER_ERROR("Tag control string \""+string(str)+"\" is not valid.");
      }
    }
  }

  bool isTagEnabled(const char* tag)
  {
    CALL("Logging::Impl::isTagEnabled");

#if !VDEBUG
    if(!_tagNums.find(tag)) { return false; }
#endif

    TagInfo& ti = tag2ti(tag);
    return ti.logEnabled;
  }

  void displayHelp()
  {
    CALL("Logging::Impl::displayHelp");

    ostream& out = cerr;

    out << "Vampire trace output" << endl
	<< "Usage:" << endl
	<< "  " << System::guessExecutableName() << " -tr <trace string>" << endl
	<< "Trace string:" << endl
	<< "help" << endl
	<< "  ... show this help" << endl
	<< "[trace_name1[^][:depth_limit1][,trace_name2[:depth_limit2][,...]]]" << endl
	<< "  ... enable specified traces with child traces up to given depth or without limit" << endl
	<< "  ... if star is specified next to a tag, premises will be shown for logged units" << endl
	<< endl
	<< "Traces:" << endl
	<< "(with each trace we specify its child traces together with their distance "
	   "from the parent that can be used for the depth limit)" << endl;


    unsigned tagCnt = _tags.size();
    for(unsigned i=0; i<tagCnt; ++i) {
      TagInfo& cur = *_tags[i];
      out << cur.name << endl;
      if(!cur.doc.empty()) {
	out << "  " << cur.doc << endl;
      }
      if(cur.children.isNonEmpty()) {
	out << "  children: ";
	TagInfo::ChildStack::Iterator cit(cur.children);
	while(cit.hasNext()) {
	  ChildInfo ci = cit.next();
	  out << ci.child.name << "(" << ci.depth << ")";
	  if(cit.hasNext()) {
	    out << ", ";
	  }
	}
	out << endl;
      }
    }
  }

  void displayStats(ostream& stm)
  {
    CALL("Logging::Impl::displayStats");

    Stack<StatObserver*>::BottomFirstIterator oit(_observers);
    while(oit.hasNext()) {
      StatObserver* obs = oit.next();
      obs->onFinalize();
      if(obs->hasData()) {
	obs->displayCaption(stm);
	stm<<": ";
	obs->displayData(stm);
      }
    }
  }
};

Logging::Impl& Logging::impl()
{
  CALL("Logging::impl");

  static Impl impl;
  return impl;
}

Logging::TagInfoBase& Logging::getTagInfo(const char* tag)
{
  return impl().tag2ti(tag);
}

void Logging::declareTag(const char* tag) {
  impl().declareTag(tag);
}
void Logging::addDoc(const char* tag, const char* doc) {
  impl().addDoc(tag,doc);
}
void Logging::addParent(const char* child, const char* parent, unsigned depth) {
  impl().addParent(child,parent,depth);
}
void Logging::enableTag(const char* tag, unsigned depthLimit)
{
  CALL("Logging::enableTag");

  Impl::EnablingSpec eSpec;
  eSpec.logEnable = true;
  impl().enableTag(tag, eSpec, depthLimit);
}
void Logging::processTraceSpecString(std::string str) {
  impl().processTraceSpecString(str);
}

void Logging::pushTagStates() {
  impl().pushTagStates();
}
void Logging::popTagStates() {
  impl().popTagStates();
}

bool Logging::isTagEnabled(const char* tag) {
  return impl().isTagEnabled(tag);
}


void Logging::logUnit(TagInfoBase& tib, Kernel::Unit* u)
{
  CALL("Logging::Impl::logUnit");

  TagInfo& ti = static_cast<TagInfo&>(tib);

  using namespace Kernel;

  tout << ti.name << ": " << u->toString() << std::endl;
  if(ti.printUnitPremises) {
    UnitSpecIterator pit = InferenceStore::instance()->getParents(UnitSpec(u));
    while(pit.hasNext()) {
      UnitSpec us = pit.next();
      tout << ti.name << " premise: " << us.toString() << std::endl;
    }
  }
}

void Logging::logSimpl(TagInfoBase& tib, Kernel::Unit* src, Kernel::Unit* tgt, const char* doc)
{
  CALL("Logging::logSimpl");

  TagInfo& ti = static_cast<TagInfo&>(tib);
  tout << ti.name << " simplification:" << endl
       << "   <- " << src->toString() << endl
       << "   -> " << tgt->toString() << endl;
  if(doc) {
    tout << "      (" << doc << ")" << endl;
  }
}

void Logging::logSimpl2(TagInfoBase& tib, Kernel::Unit* prem1, Kernel::Unit* prem2, Kernel::Unit* tgt, const char* doc)
{
  CALL("Logging::logSimpl2");

  TagInfo& ti = static_cast<TagInfo&>(tib);
  tout << ti.name << " simplification:" << endl
       << "   <- " << prem1->toString() << endl
       << "   <- " << prem2->toString() << endl
       << "   -> " << tgt->toString() << endl;
  if(doc) {
    tout << "      (" << doc << ")" << endl;
  }
}

void Logging::logTaut(TagInfoBase& tib, Kernel::Unit* u, const char* doc)
{
  CALL("Logging::logSimpl");

  TagInfo& ti = static_cast<TagInfo&>(tib);
  tout << ti.name << " discovered tautology:" << endl
       << "    " << u->toString() << endl;
  if(doc) {
    tout << "    (" << doc << ")" << endl;
  }
}


void Logging::statSimple(TagInfoBase& tib)
{
  CALL("Logging::statSimple");

  TagInfo& ti = static_cast<TagInfo&>(tib);
  Stack<StatObserver*>::ConstIterator oit(ti.statObservers);
  while(oit.hasNext()) {
    StatObserver* obs = oit.next();
    obs->onSimple();
  }
}

void Logging::statInt(TagInfoBase& tib, int val)
{
  CALL("Logging::statInt");

  TagInfo& ti = static_cast<TagInfo&>(tib);
  Stack<StatObserver*>::ConstIterator oit(ti.statObservers);
  while(oit.hasNext()) {
    StatObserver* obs = oit.next();
    obs->onInt(val);
  }
}

void Logging::statUnit(TagInfoBase& tib, Kernel::Unit* u)
{
  CALL("Logging::statUnit");

  TagInfo& ti = static_cast<TagInfo&>(tib);
  Stack<StatObserver*>::ConstIterator oit(ti.statObservers);
  while(oit.hasNext()) {
    StatObserver* obs = oit.next();
    obs->onUnit(u);
  }
}

void Logging::displayStats(std::ostream& stm)
{
  CALL("Logging::displayStats");
  impl().displayStats(stm);
}

using namespace Lib;

/**
 * Return current process id for the purpose of log outputs
 */
size_t LOG_getpid()
{
  return System::getPID();
}

}

#endif
