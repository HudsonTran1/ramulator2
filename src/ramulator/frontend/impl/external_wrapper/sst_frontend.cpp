// Goes at: /sst-elements/src/sst/elements/memHierarchy/membackend/ramulator2/sst_frontend.cpp

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
    void init() override {};

    void tick() override {
      m_memory_system->tick();
    };

// Updated 6-parameter override matching frontend.h exactly
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