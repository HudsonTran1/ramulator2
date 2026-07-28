#include "ramulator/frontend/i_frontend.h"

namespace Ramulator {

class SST : public IFrontEnd, public Implementation {
  RAMULATOR_REGISTER_IMPLEMENTATION(IFrontEnd, SST, "SST")

public:
  void init() override {};

  void tick() override {
    m_memory_system->tick();
  };

  bool receive_external_requests(int req_type_id, Addr_t addr, int source_id,
                                  int ingress_id, std::function<void(Request&)> callback,
                                  int size_bytes) override {
    Request req(addr, req_type_id, source_id, std::move(callback));
    req.ingress_id = ingress_id;
    req.size_bytes = size_bytes;
    return m_memory_system->send(req);
  }

private:
  bool is_finished() override { return true; };
};

}  // namespace Ramulator