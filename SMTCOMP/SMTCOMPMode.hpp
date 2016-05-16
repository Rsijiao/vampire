/**
 * @file SMTCOMPMode.hpp
 * Defines class SMTCOMPMode.
 */

#ifndef __SMTCOMPMode__
#define __SMTCOMPMode__

#include <utility>

#include "Forwards.hpp"

#include "Lib/DHSet.hpp"
#include "Lib/Portability.hpp"
#include "Lib/ScopedPtr.hpp"
#include "Lib/Stack.hpp"

#include "Lib/VString.hpp"

#include "Lib/Sys/SyncPipe.hpp"

#include "Kernel/Problem.hpp"

#include "Shell/Property.hpp"
#include "Shell/SineUtils.hpp"

namespace SMTCOMP {

using namespace std;
using namespace Lib;
using namespace Kernel;



#if COMPILER_MSVC

class SMTCOMPMode
{
public:
  static bool perform() { USER_ERROR("multi-core smtcomp mode is not supported on Windows"); }
};

#else

class SMTCOMPMode
{
public:

  SMTCOMPMode() : _syncSemaphore(1), _outputPrinted(false)
{
  //add the privileges into the semaphore
  _syncSemaphore.set(0,1);
}

  static bool perform();
private:
  static ostream& lineOutput();
  static ostream& coutLineOutput();

  typedef Set<vstring> StrategySet;
  typedef Stack<vstring> Schedule;
  static void getSchedules(Property& prop, Schedule& quick, Schedule& fallback);

  bool searchForProof();

  bool runSchedule(Schedule&,StrategySet& remember,bool fallback,int terminationTime);
  unsigned getSliceTime(vstring sliceCode,vstring& chopped);

  bool performStrategy(Shell::Property* property);
  bool waitForChildAndCheckIfProofFound();

  static ofstream* writerFileStream;
  static void terminatingSignalHandler(int sigNum) __attribute__((noreturn));
  void runSlice(vstring slice, unsigned milliseconds) __attribute__((noreturn));
  void runSlice(Options& strategyOpt) __attribute__((noreturn));

  static vstring problemFinishedString;

#if VDEBUG
  DHSet<pid_t> childIds;
#endif

  /**
   * Problem that is being solved.
   *
   * Note that in the current process this child object is the only one that
   * will be using the problem object.
   */
  ScopedPtr<Problem> prb;

  Semaphore _syncSemaphore; // semaphore for synchronizing following variable 
  volatile bool _outputPrinted;

  /**
   * Assumes semaphore object with 1 semaphores (at index 0).
   * Locks on demand (once) and releases the lock on destruction.
   */
  struct ScopedSemaphoreLocker { //
    Semaphore& _sem;
    bool locked;
    ScopedSemaphoreLocker(Semaphore& sem) : _sem(sem), locked(false) {}

    void lock() {
      if (!locked) {
        _sem.dec(0);
        locked = true;
      }
    }

    ~ScopedSemaphoreLocker() {
      if (locked) {
        _sem.inc(0);
      }
    }
  };
};

#endif //!COMPILER_MSVC

}

#endif // __SMTCOMPMode__
