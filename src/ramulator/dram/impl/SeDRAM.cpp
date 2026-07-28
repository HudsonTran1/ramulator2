// Include core DRAM interfaces and helper lambdas used by device models
#include "dram/dram.h"
#include "dram/lambdas.h"

// Open the Ramulator namespace for all device implementations
namespace Ramulator {

// SeDRAM (hybrid-bonded DRAM) device model class deriving from the generic IDRAM interface and Implementation helper
class SeDRAM : public IDRAM, public Implementation {
  RAMULATOR_REGISTER_IMPLEMENTATION(IDRAM, SeDRAM, "SeDRAM", "SeDRAM Hybrid-Bonded DRAM Model")

  public:
    // Organization presets for SeDRAM parts.
    // Hybrid bonding removes the TSV/microbump bottleneck, so we drop the
    // pseudochannel level entirely and widen the per-bank datapath to 256b.
    // Layout vector order matches m_levels: {channel, bankgroup, bank, row, column}.
    inline static const std::map<std::string, Organization> org_presets = {
      //   name          density   DQ    Ch  Bg  Ba   Ro       Co
      {"SeDRAM_2Gb",   {2<<10,   256,  {1,  4,  8,  1<<13,  1<<5}}}, // 2 Gb SeDRAM preset
      {"SeDRAM_4Gb",   {4<<10,   256,  {1,  4,  8,  1<<14,  1<<5}}}, // 4 Gb SeDRAM preset
      {"SeDRAM_8Gb",   {8<<10,   256,  {1,  4,  8,  1<<15,  1<<5}}}, // 8 Gb SeDRAM preset
    };

    // Timing presets for SeDRAM speed bins.
    // Hybrid bonds collapse the IO RC delay (~100 ps -> ~2 ps), so most
    // cycle-count latencies shrink even when the clock rate is held constant.
    // The 4 Gbps bin is the "native" SeDRAM preset; the 2 Gbps bin is provided
    // for apples-to-apples latency comparison against HBM3 at the same rate.
    inline static const std::map<std::string, std::vector<int>> timing_presets = {
      //   name             rate   nBL  nCL  nRCDRD  nRCDWR  nRP  nRAS  nRC  nWR  nRTPS  nRTPL  nCWL  nCCDS  nCCDL  nRRDS  nRRDL  nWTRS  nWTRL  nRTW  nFAW  nRFC  nRFCSB  nREFI  nREFISB  nRREFD  tCK_ps
      {"SeDRAM_4Gbps",     {4000,   2,   4,    5,      4,    10,   30,  38,  12,    2,     3,    1,    1,      2,     1,     2,     2,     3,    2,   20,   -1,    -1,   7800,    -1,      4,    500}},
      {"SeDRAM_2Gbps",     {2000,   4,   4,    5,      4,     5,   14,  17,   6,    2,     3,    1,    1,      2,     1,     2,     2,     3,    2,    8,   -1,   160,   3900,    -1,      4,   1000}},
      // TODO: Refine SeDRAM timing values as more published characterization data becomes available.
    };


  /************************************************
   *                Organization
   ***********************************************/
    // Internal prefetch matches nBL=2 in the native preset (256b * 2 = 512b array fetch).
    const int m_internal_prefetch_size = 2;

    // Hierarchy levels for SeDRAM. The pseudochannel level is removed: with
    // hybrid bonding every bank has its own dedicated datapath to the logic die,
    // so there is no shared bus that needs to be muxed.
    inline static constexpr ImplDef m_levels = {
      "channel", "bankgroup", "bank", "row", "column",
    };


  /************************************************
   *             Requests & Commands
   ***********************************************/
    // Commands supported by SeDRAM (identical to HBM3 - the command set is
    // unchanged; only the physical interconnect and timing scaling differ).
    inline static constexpr ImplDef m_commands = {
      "ACT",
      "PRE", "PREA",
      "RD",  "WR",  "RDA",  "WRA",
      "REFab", "REFsb",
      "RFMab", "RFMsb"
    };

    // Map each command to the level it targets.
    inline static const ImplLUT m_command_scopes = LUT (
      m_commands, m_levels, {
        {"ACT",   "row"},
        {"PRE",   "bank"},    {"PREA",   "channel"},
        {"RD",    "column"},  {"WR",     "column"}, {"RDA",   "column"}, {"WRA",   "column"},
        {"REFab", "channel"}, {"REFsb",  "bank"},
        {"RFMab", "channel"}, {"RFMsb",  "bank"},
      }
    );

    // Metadata for each command describing whether it opens/closes rows, performs an access, or is a refresh.
    inline static const ImplLUT m_command_meta = LUT<DRAMCommandMeta> (
      m_commands, {
                // open?   close?   access?  refresh?
        {"ACT",   {true,   false,   false,   false}},
        {"PRE",   {false,  true,    false,   false}},
        {"PREA",  {false,  true,    false,   false}},
        {"RD",    {false,  false,   true,    false}},
        {"WR",    {false,  false,   true,    false}},
        {"RDA",   {false,  true,    true,    false}},
        {"WRA",   {false,  true,    true,    false}},
        {"REFab", {false,  false,   false,   true }},
        {"REFsb", {false,  false,   false,   true }},
        {"RFMab", {false,  false,   false,   true }},
        {"RFMsb", {false,  false,   false,   true }},
      }
    );

    // High-level request types and their translation to device commands.
    inline static constexpr ImplDef m_requests = {
      "read", "write", "all-bank-refresh", "per-bank-refresh", "all-bank-rfm", "per-bank-rfm"
    };

    inline static const ImplLUT m_request_translations = LUT (
      m_requests, m_commands, {
        {"read", "RD"}, {"write", "WR"}, {"all-bank-refresh", "REFab"}, {"per-bank-refresh", "REFsb"},
        {"all-bank-rfm", "RFMab"}, {"per-bank-rfm", "RFMsb"},
      }
    );


  /************************************************
   *                   Timing
   ***********************************************/
    // Ordered list of timing field names; indices correspond to m_timing_vals.
    inline static constexpr ImplDef m_timings = {
      "rate",
      "nBL", "nCL", "nRCDRD", "nRCDWR", "nRP", "nRAS", "nRC", "nWR", "nRTPS", "nRTPL", "nCWL",
      "nCCDS", "nCCDL",
      "nRRDS", "nRRDL",
      "nWTRS", "nWTRL",
      "nRTW",
      "nFAW",
      "nRFC", "nRFCSB", "nREFI", "nREFISB", "nRREFD",
      "tCK_ps"
    };


  /************************************************
   *                 Node States
   ***********************************************/
    inline static constexpr ImplDef m_states = {
       "Opened", "Closed", "N/A", "Refreshing"
    };

    // Initial state per level. The pseudochannel entry is gone along with the level.
    inline static const ImplLUT m_init_states = LUT (
      m_levels, m_states, {
        {"channel",   "N/A"},
        {"bankgroup", "N/A"},
        {"bank",      "Closed"},
        {"row",       "Closed"},
        {"column",    "N/A"},
      }
    );

  public:
    struct Node : public DRAMNodeBase<SeDRAM> {
      Node(SeDRAM* dram, Node* parent, int level, int id) : DRAMNodeBase<SeDRAM>(dram, parent, level, id) {};
    };
    std::vector<Node*> m_channels;

    FuncMatrix<ActionFunc_t<Node>>  m_actions;
    FuncMatrix<PreqFunc_t<Node>>    m_preqs;
    FuncMatrix<RowhitFunc_t<Node>>  m_rowhits;
    FuncMatrix<RowopenFunc_t<Node>> m_rowopens;


  public:
    void tick() override {
      m_clk++;
    };

    void init() override {
      RAMULATOR_DECLARE_SPECS();
      set_organization();
      set_timing_vals();

      set_actions();
      set_preqs();
      set_rowhits();
      set_rowopens();

      create_nodes();
    };

    void issue_command(int command, const AddrVec_t& addr_vec) override {
      int channel_id = addr_vec[m_levels["channel"]];
      m_channels[channel_id]->update_timing(command, addr_vec, m_clk);
      m_channels[channel_id]->update_states(command, addr_vec, m_clk);
    };

    int get_preq_command(int command, const AddrVec_t& addr_vec) override {
      int channel_id = addr_vec[m_levels["channel"]];
      return m_channels[channel_id]->get_preq_command(command, addr_vec, m_clk);
    };

    bool check_ready(int command, const AddrVec_t& addr_vec) override {
      int channel_id = addr_vec[m_levels["channel"]];
      return m_channels[channel_id]->check_ready(command, addr_vec, m_clk);
    };

    bool check_rowbuffer_hit(int command, const AddrVec_t& addr_vec) override {
      int channel_id = addr_vec[m_levels["channel"]];
      return m_channels[channel_id]->check_rowbuffer_hit(command, addr_vec, m_clk);
    };

    bool check_node_open(int command, const AddrVec_t& addr_vec) override {
      int channel_id = addr_vec[m_levels["channel"]];
      return m_channels[channel_id]->check_node_open(command, addr_vec, m_clk);
    };

  private:
    void set_organization() {
      // Channel width default: 256b per bank, no pseudochannel splitting needed
      // since each bank has its own dedicated hybrid-bond datapath.
      m_channel_width = param_group("org").param<int>("channel_width").default_val(256);

      // Organization
      m_organization.count.resize(m_levels.size(), -1);

      // Load organization preset if provided
      if (auto preset_name = param_group("org").param<std::string>("preset").optional()) {
        if (org_presets.count(*preset_name) > 0) {
          m_organization = org_presets.at(*preset_name);
        } else {
          throw ConfigurationError("Unrecognized organization preset \"{}\" in {}!", *preset_name, get_name());
        }
      }

      // Override the preset with any provided settings
      if (auto dq = param_group("org").param<int>("dq").optional()) {
        m_organization.dq = *dq;
      }

      for (int i = 0; i < m_levels.size(); i++){
        auto level_name = m_levels(i);
        if (auto sz = param_group("org").param<int>(level_name).optional()) {
          m_organization.count[i] = *sz;
        }
      }

      if (auto density = param_group("org").param<int>("density").optional()) {
        m_organization.density = *density;
      }

      // Sanity check: is the calculated channel density the same as the provided one?
      // Pseudochannel term has been removed since the level no longer exists.
      size_t _density = size_t(m_organization.count[m_levels["bankgroup"]]) *
                        size_t(m_organization.count[m_levels["bank"]]) *
                        size_t(m_organization.count[m_levels["row"]]) *
                        size_t(m_organization.count[m_levels["column"]]) *
                        size_t(m_organization.dq);
      _density >>= 20;
      if (m_organization.density != _density) {
        throw ConfigurationError(
            "Calculated {} channel density {} Mb does not equal the provided density {} Mb!",
            get_name(),
            _density,
            m_organization.density
        );
      }

    };

    void set_timing_vals() {
      m_timing_vals.resize(m_timings.size(), -1);

      // Load timing preset if provided
      bool preset_provided = false;
      if (auto preset_name = param_group("timing").param<std::string>("preset").optional()) {
        if (timing_presets.count(*preset_name) > 0) {
          m_timing_vals = timing_presets.at(*preset_name);
          preset_provided = true;
        } else {
          throw ConfigurationError("Unrecognized timing preset \"{}\" in {}!", *preset_name, get_name());
        }
      }

      // Check for rate (in MT/s), and if provided, calculate and set tCK (in picosecond)
      if (auto dq = param_group("timing").param<int>("rate").optional()) {
        if (preset_provided) {
          throw ConfigurationError("Cannot change the transfer rate of {} when using a speed preset !", get_name());
        }
        m_timing_vals("rate") = *dq;
      }
      int tCK_ps = 1E6 / (m_timing_vals("rate") / 2);
      m_timing_vals("tCK_ps") = tCK_ps;

      // Refresh timings
      // tRFC table (unit is nanosecond!)
      constexpr int tRFC_TABLE[1][4] = {
      //  2Gb   4Gb   8Gb  16Gb
        { 160,  260,  350,  450},
      };

      // tREFISB table: per-bank refresh interval (unit is nanosecond!)
      constexpr int tREFISB_TABLE[1][4] = {
      //  2Gb    4Gb    8Gb    16Gb
        { 4875,  4875,  2438,  2438},
      };

      // tRFCSB table: per-bank refresh cycle duration (unit is nanosecond!)
      constexpr int tRFCSB_TABLE[1][4] = {
      //  2Gb   4Gb   8Gb   16Gb
        { 160,  160,  260,  350},
      };

      int density_id = [](int density_Mb) -> int {
        switch (density_Mb) {
          case 2048:  return 0;
          case 4096:  return 1;
          case 8192:  return 2;
          case 16384: return 3;
          default:    return -1;
        }
      }(m_organization.density);

      m_timing_vals("nRFC")    = JEDEC_rounding(tRFC_TABLE[0][density_id], tCK_ps);
      m_timing_vals("nREFISB") = JEDEC_rounding(tREFISB_TABLE[0][density_id], tCK_ps);
      m_timing_vals("nRFCSB")  = JEDEC_rounding(tRFCSB_TABLE[0][density_id], tCK_ps);

      // Overwrite timing parameters with any user-provided value
      // Rate and tCK should not be overwritten
      for (int i = 1; i < m_timings.size() - 1; i++) {
        auto timing_name = std::string(m_timings(i));

        if (auto provided_timing = param_group("timing").param<int>(timing_name).optional()) {
          // Check if the user specifies in the number of cycles (e.g., nRCD)
          m_timing_vals(i) = *provided_timing;
        } else if (auto provided_timing = param_group("timing").param<float>(timing_name.replace(0, 1, "t")).optional()) {
          // Check if the user specifies in nanoseconds (e.g., tRCD)
          m_timing_vals(i) = JEDEC_rounding(*provided_timing, tCK_ps);
        }
      }

      // Check if there is any uninitialized timings
      for (int i = 0; i < m_timing_vals.size(); i++) {
        if (m_timing_vals(i) == -1) {
          throw ConfigurationError("In \"{}\", timing {} is not specified!", get_name(), m_timings(i));
        }
      }

      // Set read latency
      m_read_latency = m_timing_vals("nCL") + m_timing_vals("nBL");

      // Populate the timing constraints
      #define V(timing) (m_timing_vals(timing))
      populate_timingcons(this, {
          /*** Channel ***/
          /// 2-cycle ACT command (for row commands)
          {.level = "channel", .preceding = {"ACT"}, .following = {"ACT", "PRE", "PREA", "REFab", "REFsb", "RFMab", "RFMsb"}, .latency = 2},

          /*** Channel-level RAS/refresh constraints (formerly pseudochannel level) ***/
          // With hybrid bonding the pseudochannel level is gone; RAS-side
          // constraints that limited row activations across the (formerly shared)
          // pseudochannel now apply at the channel level. The cross-bank CAS
          // data-bus occupancy constraints (nBL, nCCDS, RD->WR, WR->RD, CAS<->PREA)
          // are NOT replicated here: SeDRAM has no shared DQ bus across banks,
          // so column-command spacing reduces to per-bank constraints (already
          // expressed at bank and bankgroup levels below).

          // RAS <-> RAS
          {.level = "channel", .preceding = {"ACT"}, .following = {"ACT"}, .latency = V("nRRDS")},
          /// 4-activation window restriction
          {.level = "channel", .preceding = {"ACT"}, .following = {"ACT"}, .latency = V("nFAW"), .window = 4},

          /// ACT actually happens on the 2-nd cycle of ACT, so +1 cycle to nRRD
          {.level = "channel", .preceding = {"ACT"}, .following = {"REFsb", "RFMsb"}, .latency = V("nRRDS") + 1},
          /// nRREFD is the latency between REFsb <-> REFsb to *different* banks
          {.level = "channel", .preceding = {"REFsb", "RFMsb"}, .following = {"REFsb", "RFMsb"}, .latency = V("nRREFD")},
          /// nRREFD is the latency between REFsb <-> ACT to *different* banks. -1 as ACT happens on its 2nd cycle
          {.level = "channel", .preceding = {"REFsb", "RFMsb"}, .following = {"ACT"}, .latency = V("nRREFD") - 1},

          /// RAS <-> PREA / PREA <-> RAS at channel scope
          {.level = "channel", .preceding = {"ACT"},  .following = {"PREA"}, .latency = V("nRAS")},
          {.level = "channel", .preceding = {"PREA"}, .following = {"ACT"},  .latency = V("nRP")},

          /// RAS <-> REF
          {.level = "channel", .preceding = {"ACT"},          .following = {"REFab", "RFMab"},             .latency = V("nRC")},
          {.level = "channel", .preceding = {"PRE", "PREA"},  .following = {"REFab", "RFMab"},             .latency = V("nRP")},
          {.level = "channel", .preceding = {"RDA"},          .following = {"REFab", "RFMab"},             .latency = V("nRP") + V("nRTPS")},
          {.level = "channel", .preceding = {"WRA"},          .following = {"REFab", "RFMab"},             .latency = V("nCWL") + V("nBL") + V("nWR") + V("nRP")},
          {.level = "channel", .preceding = {"REFab", "RFMab"}, .following = {"ACT", "REFsb", "RFMsb"},    .latency = V("nRFC")},

          /*** Same Bank Group ***/
          /// CAS <-> CAS
          {.level = "bankgroup", .preceding = {"RD", "RDA"}, .following = {"RD", "RDA"}, .latency = V("nCCDL")},
          {.level = "bankgroup", .preceding = {"WR", "WRA"}, .following = {"WR", "WRA"}, .latency = V("nCCDL")},
          {.level = "bankgroup", .preceding = {"WR", "WRA"}, .following = {"RD", "RDA"}, .latency = V("nCWL") + V("nBL") + V("nWTRL")},
          /// RAS <-> RAS
          {.level = "bankgroup", .preceding = {"ACT"}, .following = {"ACT"}, .latency = V("nRRDL")},
          {.level = "bankgroup", .preceding = {"ACT"}, .following = {"REFsb", "RFMsb"}, .latency = V("nRRDL") + 1},
          {.level = "bankgroup", .preceding = {"REFsb", "RFMsb"}, .following = {"ACT"}, .latency = V("nRRDL") - 1},

          {.level = "bank", .preceding = {"RD"},  .following = {"PRE"}, .latency = V("nRTPS")},

          /*** Channel — CAS drain before precharge-all ***/
          {.level = "channel", .preceding = {"RD"}, .following = {"PREA"}, .latency = V("nRTPS")},
          {.level = "channel", .preceding = {"WR"}, .following = {"PREA"}, .latency = V("nCWL") + V("nBL") + V("nWR")},

          /*** Bank ***/
          /*** Bank — CAS-to-CAS (per-bank datapath; shared bus gone, array timing remains) ***/
          {.level = "bank", .preceding = {"RD", "RDA"}, .following = {"RD", "RDA"}, .latency = V("nBL")},
          {.level = "bank", .preceding = {"WR", "WRA"}, .following = {"WR", "WRA"}, .latency = V("nBL")},
          {.level = "bank", .preceding = {"RD", "RDA"}, .following = {"WR", "WRA"}, .latency = V("nCL") + V("nBL") + 2 - V("nCWL")},
          {.level = "bank", .preceding = {"WR", "WRA"}, .following = {"RD", "RDA"}, .latency = V("nCWL") + V("nBL") + V("nWTRS")},

          {.level = "bank", .preceding = {"ACT"}, .following = {"ACT"}, .latency = V("nRC")},
          {.level = "bank", .preceding = {"ACT"}, .following = {"RD", "RDA"}, .latency = V("nRCDRD")},
          {.level = "bank", .preceding = {"ACT"}, .following = {"WR", "WRA"}, .latency = V("nRCDWR")},
          {.level = "bank", .preceding = {"ACT"}, .following = {"PRE"}, .latency = V("nRAS")},
          {.level = "bank", .preceding = {"PRE"}, .following = {"ACT"}, .latency = V("nRP")},
          {.level = "bank", .preceding = {"RD"},  .following = {"PRE"}, .latency = V("nRTPL")},
          {.level = "bank", .preceding = {"WR"},  .following = {"PRE"}, .latency = V("nCWL") + V("nBL") + V("nWR")},
          {.level = "bank", .preceding = {"RDA"}, .following = {"ACT", "REFsb", "RFMsb"}, .latency = V("nRTPL") + V("nRP")},
          {.level = "bank", .preceding = {"WRA"}, .following = {"ACT", "REFsb", "RFMsb"}, .latency = V("nCWL") + V("nBL") + V("nWR") + V("nRP")},
        }
      );
      #undef V

    };

    void set_actions() {
      m_actions.resize(m_levels.size(), std::vector<ActionFunc_t<Node>>(m_commands.size()));

      // Channel Actions
      m_actions[m_levels["channel"]][m_commands["PREA"]] = Lambdas::Action::Channel::PREab<SeDRAM>;

      // Bank actions
      m_actions[m_levels["bank"]][m_commands["ACT"]] = Lambdas::Action::Bank::ACT<SeDRAM>;
      m_actions[m_levels["bank"]][m_commands["PRE"]] = Lambdas::Action::Bank::PRE<SeDRAM>;
      m_actions[m_levels["bank"]][m_commands["RDA"]] = Lambdas::Action::Bank::PRE<SeDRAM>;
      m_actions[m_levels["bank"]][m_commands["WRA"]] = Lambdas::Action::Bank::PRE<SeDRAM>;
    };

    void set_preqs() {
      m_preqs.resize(m_levels.size(), std::vector<PreqFunc_t<Node>>(m_commands.size()));

      // Channel Actions
      m_preqs[m_levels["channel"]][m_commands["REFab"]] = Lambdas::Preq::Channel::RequireAllBanksClosed<SeDRAM>;

      // Bank actions
      m_preqs[m_levels["bank"]][m_commands["REFsb"]] = Lambdas::Preq::Bank::RequireBankClosed<SeDRAM>;
      m_preqs[m_levels["bank"]][m_commands["RD"]] = Lambdas::Preq::Bank::RequireRowOpen<SeDRAM>;
      m_preqs[m_levels["bank"]][m_commands["WR"]] = Lambdas::Preq::Bank::RequireRowOpen<SeDRAM>;
    };

    void set_rowhits() {
      m_rowhits.resize(m_levels.size(), std::vector<RowhitFunc_t<Node>>(m_commands.size()));

      m_rowhits[m_levels["bank"]][m_commands["RD"]] = Lambdas::RowHit::Bank::RDWR<SeDRAM>;
      m_rowhits[m_levels["bank"]][m_commands["WR"]] = Lambdas::RowHit::Bank::RDWR<SeDRAM>;
    }


    void set_rowopens() {
      m_rowopens.resize(m_levels.size(), std::vector<RowhitFunc_t<Node>>(m_commands.size()));

      m_rowopens[m_levels["bank"]][m_commands["RD"]] = Lambdas::RowOpen::Bank::RDWR<SeDRAM>;
      m_rowopens[m_levels["bank"]][m_commands["WR"]] = Lambdas::RowOpen::Bank::RDWR<SeDRAM>;
    }


    void create_nodes() {
      int num_channels = m_organization.count[m_levels["channel"]];
      for (int i = 0; i < num_channels; i++) {
        Node* channel = new Node(this, nullptr, 0, i);
        m_channels.push_back(channel);
      }
    };
};


}        // namespace Ramulator
