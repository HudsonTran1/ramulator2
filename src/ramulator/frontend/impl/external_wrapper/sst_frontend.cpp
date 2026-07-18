#include <filesystem>
#include <iostream>
#include <fstream>
#include <functional>
#include <stdexcept>

#include "base/factory.h"
#include "frontend/i_frontend.h"

namespace Ramulator {

class SST : public IFrontEnd, public Implementation {
  RAMULATOR_REGISTER_IMPLEMENTATION(IFrontEnd, SST, "SST")

  public:
    // FIX: Match Ramulator 2's expected 4-argument Factory constructor signature
    SST(const ConfigNode& config, std::string name, std::string id, Implementation* parent)
        : Implementation(config, "frontend", name, parent) 
    {
      // Safely link the internal implementation tracker to this class
      m_impl = this;
    }

    void init() override {};

    void tick() override {
      // Driven explicitly in the SST backend file to maintain precise sync.
    };

    bool receive_external_requests(int req_type_id, Addr_t addr, int source_id, 
                                   int size_bytes, std::function<void(Request&)> callback, 
                                   bool is_sst) override {
      Request req{addr, req_type_id, source_id, callback};
      req.size_bytes = size_bytes; 
      return m_memory_system->send(req);
    }

  private:
    bool is_finished() override { return true; };
};

}        // namespace Ramulator