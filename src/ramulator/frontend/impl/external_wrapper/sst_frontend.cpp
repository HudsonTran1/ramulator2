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

    // 4-parameter convenience overload for legacy SST backends
    bool receive_external_requests(int req_type_id, Addr_t addr, int source_id, std::function<void(Request&)> callback) {
      return receive_external_requests(req_type_id, addr, source_id, callback, 64);
    }

    // 5-parameter override matching i_frontend.h
    bool receive_external_requests(int req_type_id, Addr_t addr, int source_id, std::function<void(Request&)> callback, int size_bytes) override {
      Request req{addr, req_type_id, source_id, callback};
      return m_memory_system->send(req);
    }

    // 6-parameter override matching i_frontend.h
    bool receive_external_requests(int req_type_id, Addr_t addr, int source_id, int ingress_id, std::function<void(Request&)> callback, int size_bytes) override {
      Request req{addr, req_type_id, source_id, callback};
      return m_memory_system->send(req);
    }

  private:
    bool is_finished() override { return true; };
};

}        // namespace Ramulator
