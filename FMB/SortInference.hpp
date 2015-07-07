/**
 * @file SortInference.hpp
 * Defines class SortInference.
 */

#ifndef __SortInference__
#define __SortInference__

#include "Forwards.hpp"

#include "Lib/DHMap.hpp"
#include "Kernel/Signature.hpp"

namespace FMB {
using namespace Kernel;
using namespace Shell;
using namespace Lib;


struct SortedSignature{
    unsigned sorts;
    DArray<Stack<unsigned>> sortedConstants;
    DArray<Stack<unsigned>> sortedFunctions;

    DArray<DArray<unsigned>> functionBounds;
    DArray<DArray<unsigned>> predicateBounds;
    
};

class SortInference {
public:
  CLASS_NAME(SortInference);
  USE_ALLOCATOR(SortInference);    
  
  static SortedSignature*  apply(ClauseIterator cit,
                                 DArray<unsigned> del_f,
                                 DArray<unsigned> del_p);

};

}

#endif // __SortInference__