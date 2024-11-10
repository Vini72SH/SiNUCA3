#ifndef SINUCA3_CONFIG_SIMULATOR_BUILDER_HPP_
#define SINUCA3_CONFIG_SIMULATOR_BUILDER_HPP_

#include "../engine/engine.hpp"

namespace sinuca {
namespace config {

class SimulatorBuilder {
  private:
  public:
    engine::Engine* Instantiate(const char* configFile);
};

}  // namespace config
}  // namespace sinuca

#endif  // SINUCA3_CONFIG_SIMULATOR_BUILDER_HPP_
