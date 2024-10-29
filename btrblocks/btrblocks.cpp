// ------------------------------------------------------------------------------
#include "btrblocks.hpp"
#include "scheme/SchemePool.hpp"
// ------------------------------------------------------------------------------
namespace btrblocks {

void BtrBlocksConfig::configure(const std::function<void(BtrBlocksConfig&)>& f) {
  auto& instance = BtrBlocksConfig::get();
  f(instance);
  SchemePool::refresh();
}
void emretestfunc() {
  cout << "This is emre's test func" << endl;
}

}  // namespace btrblocks
// ------------------------------------------------------------------------------
