// Crypto++ normally instantiates these in its full dll.cpp translation unit.
// Limit this offline tool to the integer algebra needed by its common support.
#include <algebra.cpp>
namespace CryptoPP
{
    template class AbstractGroup<Integer>;
    template class AbstractRing<Integer>;
    template class AbstractEuclideanDomain<Integer>;
}
