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


void configure_btrblocks(uint8_t max_depth) {
    BtrBlocksConfig::configure([&](BtrBlocksConfig &config){
        std::cout << "setting max cascade depth to " << max_depth << std::endl;
        config.integers.max_cascade_depth = max_depth;
        config.doubles.max_cascade_depth = max_depth;
        config.strings.max_cascade_depth = max_depth;
        config.doubles.schemes.enable(DoubleSchemeType::DOUBLE_BP);
    });
}


void emretestfunc() {
  cout << "This is emre's test func" << endl;
}

}  // namespace btrblocks
// ------------------------------------------------------------------------------
