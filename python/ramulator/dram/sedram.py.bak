import math

from ramulator.dram.spec import DRAMStandard, TimingConstraint


class SeDRAM(DRAMStandard):
    """SeDRAM: a hybrid-bonded DRAM device model.

    Hybrid bonding gives every bank a dedicated datapath straight to the
    logic die, so (unlike DDR5/HBM3) there is no shared pseudochannel bus to
    mux -- the PseudoChannel level is simply omitted. The 256b-wide native
    datapath is expressed via `dq`/`channel_width` in the org presets, and
    the ~100ps -> ~2ps drop in IO RC delay from hybrid bonding is reflected
    directly in the (much smaller) cycle counts of the timing presets.
    """

    name = "SeDRAM"
    internal_prefetch_size = 2       # nBL=2 in the native 4 Gbps preset (256b * 2 = 512b/access)
    read_latency = "nCL + nBL"

    # ---- Hierarchy (level name -> init state) ----
    # No Rank, no PseudoChannel: hybrid bonding removes the shared-bus levels
    # that DDR5/HBM3 need to arbitrate.
    levels = {
        "Channel":   "N_A",
        "BankGroup": "N_A",
        "Bank":      "Closed",
        "Row":       "Closed",
        "Column":    "N_A",
    }

    # ---- Commands ----
    commands = [
        "ACT",
        "PREpb", "PREab",
        "RD", "WR", "RDA", "WRA",
        "REFab", "REFpb",
        "RFMab", "RFMpb",
    ]

    # ---- CA bus cycle count per command ----
    # ACT carries a wide row address and occupies the row command bus for
    # 2 CK (matches the original device's 2-cycle ACT). All other row/column
    # commands are 1 CK. The +-1 adjustment JEDEC needs around ACT (e.g. the
    # nRRDS+1 / nRREFD-1 fixups in the old hand-written model) is now handled
    # automatically by to_config() from this single declaration.
    command_cycles = {"ACT": 2}

    # ---- Bus classification (dual command bus, like HBM3) ----
    # SeDRAM has no shared column-command resource across banks (hybrid
    # bonding gives each bank its own datapath), so only the row bus needs
    # the 2-cycle ACT occupancy constraint auto-generated for it.
    row_commands = ["ACT", "PREpb", "PREab", "REFab", "REFpb", "RFMab", "RFMpb"]
    column_commands = ["RD", "WR", "RDA", "WRA"]

    # ---- States ----
    states = ["Opened", "Closed", "N_A"]

    # ---- Timing parameters ----
    timing_params = [
        "rate",
        "nBL", "nCL", "nRCDRD", "nRCDWR", "nRP", "nRAS", "nRC", "nWR", "nRTPS", "nRTPL", "nCWL",
        "nCCDS", "nCCDL",
        "nRRDS", "nRRDL",
        "nWTRS", "nWTRL",
        "nRTW",
        "nFAW",
        "nRFC", "nRFCpb", "nREFI", "nREFIpb", "nRREFD",
        "tCK_ps",
    ]

    # ---- External request types ----
    supported_requests = {
        "Read": "RD",
        "Write": "WR",
        "AllBankRefresh": "REFab",
        "PerBankRefresh": "REFpb",
        "AllBankRFM": "RFMab",
        "PerBankRFM": "RFMpb",
    }

    # ---- Timing constraints ----
    timing_constraints = [
        # ============================================================
        # Channel
        # ============================================================
        # RAS <-> RAS
        TimingConstraint(level="Channel", preceding=["ACT"], following=["ACT"], latency="nRRDS"),
        # 4-activation window restriction
        TimingConstraint(level="Channel", preceding=["ACT"], following=["ACT"], latency="nFAW", window=4),

        # ACT happens on the 2nd cycle of ACT, so +1 cycle to nRRDS
        TimingConstraint(level="Channel", preceding=["ACT"], following=["REFpb", "RFMpb"], latency="nRRDS + 1"),
        # nRREFD is the latency between REFpb <-> REFpb to *different* banks
        TimingConstraint(level="Channel", preceding=["REFpb", "RFMpb"], following=["REFpb", "RFMpb"], latency="nRREFD"),
        # nRREFD is the latency between REFpb <-> ACT to *different* banks. -1 as ACT happens on its 2nd cycle
        TimingConstraint(level="Channel", preceding=["REFpb", "RFMpb"], following=["ACT"], latency="nRREFD - 1"),

        # RAS <-> PREab / PREab <-> RAS at channel scope
        TimingConstraint(level="Channel", preceding=["ACT"], following=["PREab"], latency="nRAS"),
        TimingConstraint(level="Channel", preceding=["PREab"], following=["ACT"], latency="nRP"),

        # RAS <-> REF
        TimingConstraint(level="Channel", preceding=["ACT"], following=["REFab", "RFMab"], latency="nRC"),
        TimingConstraint(level="Channel", preceding=["PREpb", "PREab"], following=["REFab", "RFMab"], latency="nRP"),
        TimingConstraint(level="Channel", preceding=["RDA"], following=["REFab", "RFMab"], latency="nRP + nRTPS"),
        TimingConstraint(level="Channel", preceding=["WRA"], following=["REFab", "RFMab"], latency="nCWL + nBL + nWR + nRP"),
        TimingConstraint(level="Channel", preceding=["REFab", "RFMab"], following=["ACT", "REFpb", "RFMpb"], latency="nRFC"),

        # CAS drain before precharge-all
        TimingConstraint(level="Channel", preceding=["RD"], following=["PREab"], latency="nRTPS"),
        TimingConstraint(level="Channel", preceding=["WR"], following=["PREab"], latency="nCWL + nBL + nWR"),

        # ============================================================
        # BankGroup
        # ============================================================
        # CAS <-> CAS
        TimingConstraint(level="BankGroup", preceding=["RD", "RDA"], following=["RD", "RDA"], latency="nCCDL"),
        TimingConstraint(level="BankGroup", preceding=["WR", "WRA"], following=["WR", "WRA"], latency="nCCDL"),
        TimingConstraint(level="BankGroup", preceding=["WR", "WRA"], following=["RD", "RDA"], latency="nCWL + nBL + nWTRL"),
        # RAS <-> RAS
        TimingConstraint(level="BankGroup", preceding=["ACT"], following=["ACT"], latency="nRRDL"),
        TimingConstraint(level="BankGroup", preceding=["ACT"], following=["REFpb", "RFMpb"], latency="nRRDL + 1"),
        TimingConstraint(level="BankGroup", preceding=["REFpb", "RFMpb"], following=["ACT"], latency="nRRDL - 1"),

        # ============================================================
        # Bank
        # ============================================================
        TimingConstraint(level="Bank", preceding=["RD"], following=["PREpb"], latency="nRTPS"),

        TimingConstraint(level="Bank", preceding=["RD", "RDA"], following=["RD", "RDA"], latency="nBL"),
        TimingConstraint(level="Bank", preceding=["WR", "WRA"], following=["WR", "WRA"], latency="nBL"),
        TimingConstraint(level="Bank", preceding=["RD", "RDA"], following=["WR", "WRA"], latency="nCL + nBL + 2 - nCWL"),
        TimingConstraint(level="Bank", preceding=["WR", "WRA"], following=["RD", "RDA"], latency="nCWL + nBL + nWTRS"),

        TimingConstraint(level="Bank", preceding=["ACT"], following=["ACT"], latency="nRC"),
        TimingConstraint(level="Bank", preceding=["ACT"], following=["RD", "RDA"], latency="nRCDRD"),
        TimingConstraint(level="Bank", preceding=["ACT"], following=["WR", "WRA"], latency="nRCDWR"),
        TimingConstraint(level="Bank", preceding=["ACT"], following=["PREpb"], latency="nRAS"),
        TimingConstraint(level="Bank", preceding=["PREpb"], following=["ACT"], latency="nRP"),
        TimingConstraint(level="Bank", preceding=["RD"], following=["PREpb"], latency="nRTPL"),
        TimingConstraint(level="Bank", preceding=["WR"], following=["PREpb"], latency="nCWL + nBL + nWR"),
        TimingConstraint(level="Bank", preceding=["RDA"], following=["ACT", "REFpb", "RFMpb"], latency="nRTPL + nRP"),
        TimingConstraint(level="Bank", preceding=["WRA"], following=["ACT", "REFpb", "RFMpb"], latency="nCWL + nBL + nWR + nRP"),
    ]

    # ---- Secondary timing resolution ----
    # tRFC/tRFCpb/tREFIpb are density-dependent (JEDEC-style table lookup),
    # so they're resolved here rather than baked into every timing preset --
    # unless a preset supplies its own value, in which case that's kept.
    _tRFC_NS_TABLE    = {2048: 160,  4096: 260,  8192: 350,  16384: 450}
    _tRFCpb_NS_TABLE  = {2048: 160,  4096: 160,  8192: 260,  16384: 350}
    _tREFIpb_NS_TABLE = {2048: 4875, 4096: 4875, 8192: 2438, 16384: 2438}

    @classmethod
    def resolve_secondary_timings(cls, timing_dict, org_dict):
        tCK_ps = timing_dict["tCK_ps"]
        density = org_dict["density"]

        if timing_dict.get("nRFC", -1) == -1:
            timing_dict["nRFC"] = cls._lookup_ns(cls._tRFC_NS_TABLE, density, tCK_ps)
        if timing_dict.get("nRFCpb", -1) == -1:
            timing_dict["nRFCpb"] = cls._lookup_ns(cls._tRFCpb_NS_TABLE, density, tCK_ps)
        if timing_dict.get("nREFIpb", -1) == -1:
            timing_dict["nREFIpb"] = cls._lookup_ns(cls._tREFIpb_NS_TABLE, density, tCK_ps)

    @staticmethod
    def _lookup_ns(table, density, tCK_ps):
        if density not in table:
            raise ValueError(f"SeDRAM: no timing table entry for density {density} Mb")
        return math.ceil(table[density] * 1000 / tCK_ps)


# ---- SeDRAM organization presets ----
# layout: density (Mb), dq/channel_width (b, native 256b hybrid-bonded datapath),
# bankgroup, bank, row, column counts.
SeDRAM.org_presets = {
    "SeDRAM_2Gb": {"density": 2048, "dq": 256, "channel_width": 256, "bankgroup": 4, "bank": 8, "row": 1 << 13, "column": 1 << 5},
    "SeDRAM_4Gb": {"density": 4096, "dq": 256, "channel_width": 256, "bankgroup": 4, "bank": 8, "row": 1 << 14, "column": 1 << 5},
    "SeDRAM_8Gb": {"density": 8192, "dq": 256, "channel_width": 256, "bankgroup": 4, "bank": 8, "row": 1 << 15, "column": 1 << 5},
}

# ---- SeDRAM speed-bin timing presets ----
# nRFC / nRFCpb / nREFIpb left as -1 to be resolved from the JEDEC-style
# density tables in resolve_secondary_timings(); SeDRAM_2Gbps supplies its
# own nRFCpb directly, which resolve_secondary_timings() will leave alone.
SeDRAM.timing_presets = {
    # 4 Gbps: the "native" SeDRAM bin used for latency comparison at the same clock as HBM3.
    "SeDRAM_4Gbps": {
        "rate": 4000, "nBL": 2, "nCL": 4, "nRCDRD": 5, "nRCDWR": 4, "nRP": 10, "nRAS": 30, "nRC": 38,
        "nWR": 12, "nRTPS": 2, "nRTPL": 3, "nCWL": 1, "nCCDS": 1, "nCCDL": 2, "nRRDS": 1, "nRRDL": 2,
        "nWTRS": 2, "nWTRL": 3, "nRTW": 2, "nFAW": 20, "nRFC": -1, "nRFCpb": -1, "nREFI": 7800,
        "nREFIpb": -1, "nRREFD": 4, "tCK_ps": 500,
    },
    # 2 Gbps: apples-to-apples comparison bin at the same rate as HBM3.
    "SeDRAM_2Gbps": {
        "rate": 2000, "nBL": 4, "nCL": 4, "nRCDRD": 5, "nRCDWR": 4, "nRP": 5, "nRAS": 14, "nRC": 17,
        "nWR": 6, "nRTPS": 2, "nRTPL": 3, "nCWL": 1, "nCCDS": 1, "nCCDL": 2, "nRRDS": 1, "nRRDL": 2,
        "nWTRS": 2, "nWTRL": 3, "nRTW": 2, "nFAW": 8, "nRFC": -1, "nRFCpb": 160, "nREFI": 3900,
        "nREFIpb": -1, "nRREFD": 4, "tCK_ps": 1000,
    },
    # TODO: Refine SeDRAM timing values as more published characterization data becomes available.
}