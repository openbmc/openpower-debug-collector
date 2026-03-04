#pragma once

#include "targeting_iface.hpp"
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace openpower::dump::phal::chipop {

// ============================================================================
// Dump Data Container (Backend-Neutral)
// ============================================================================

/**
 * @brief RAII container for dump data
 * 
 * Supports both malloc-based (old PHAL) and vector-based (new PHAL) storage
 */
class DumpData {
public:
    DumpData() = default;
    ~DumpData() = default;
    
    // Non-copyable
    DumpData(const DumpData&) = delete;
    DumpData& operator=(const DumpData&) = delete;
    
    // Movable
    DumpData(DumpData&&) noexcept = default;
    DumpData& operator=(DumpData&&) noexcept = default;
    
    /**
     * @brief Create from malloc'd buffer (old PHAL)
     * @param p Pointer to malloc'd buffer (ownership transferred)
     * @param len Length of buffer
     */
    static DumpData fromMalloc(uint8_t* p, uint32_t len);
    
    /**
     * @brief Create from vector (new PHAL)
     * @param v Vector of bytes (moved)
     */
    static DumpData fromVector(std::vector<uint8_t>&& v);
    
    /**
     * @brief Get byte span for reading
     */
    std::span<const uint8_t> bytes() const;
    
    /**
     * @brief Get size in bytes
     */
    uint32_t size() const;

private:
    struct MallocBuf {
        uint8_t* p = nullptr;
        uint32_t len = 0;
        ~MallocBuf() { if (p) free(p); }
        
        // Non-copyable
        MallocBuf(const MallocBuf&) = delete;
        MallocBuf& operator=(const MallocBuf&) = delete;
        
        // Movable
        MallocBuf() = default;
        MallocBuf(MallocBuf&& other) noexcept : p(other.p), len(other.len) {
            other.p = nullptr;
            other.len = 0;
        }
        MallocBuf& operator=(MallocBuf&& other) noexcept {
            if (this != &other) {
                if (p) free(p);
                p = other.p;
                len = other.len;
                other.p = nullptr;
                other.len = 0;
            }
            return *this;
        }
    };
    
    std::variant<std::monostate, MallocBuf, std::vector<uint8_t>> storage_;
};

// ============================================================================
// Chip-Op Error
// ============================================================================

struct ChipOpError : public std::exception {
    enum class Type {
        NotAllowed,     // SBE not ready for chip-ops
        Timeout,        // Command timeout
        Failed,         // Command failed
        NoFfdc,         // No FFDC data available
        InternalFfdc,   // Internal FFDC (not chip-op failure)
        Unknown         // Unknown error
    };
    
    Type type{Type::Unknown};
    std::string msg;
    
    ChipOpError(Type t, std::string m) : type(t), msg(std::move(m)) {}
    
    const char* what() const noexcept override { return msg.c_str(); }
};

// ============================================================================
// Chip Operations
// ============================================================================

/**
 * @brief Collect dump from SBE
 *
 * Old: openpower::phal::sbe::getDump()
 * Next: sbei::getDump(fapi2::Target<...>)
 *
 * @param chip Target handle (proc or OCMB)
 * @param dumpType Type of dump to collect
 * @param clockState Clock state (ON/OFF)
 * @param collectFastArray Whether to collect fast array data
 * @return DumpData containing collected dump
 * @throws ChipOpError on failure
 */
DumpData getDump(targeting::TargetHandle chip,
                 uint8_t dumpType,
                 uint8_t clockState,
                 uint8_t collectFastArray);

/**
 * @brief Stop threads on processor (prepare for hostboot dump)
 *
 * Old:  openpower::phal::sbe::threadStopProc()
 * Next: MPIPL sequence via sbei::enterMpiplChipOp() →
 *       sbei::continueMpiplChipOp() → sbei::stopClocksChipOp()
 *       (phal/src/sbei/sbe_cmd_impl.H)
 *
 * @param proc Processor target handle
 * @throws ChipOpError on failure (type=Failed for each MPIPL step)
 */
void threadStopProc(targeting::TargetHandle proc);

// ============================================================================
// SBE Dump Collection HWPs (legacy: libipl/libphal; next: stubs)
// ============================================================================

/**
 * @brief SBE type identifier for collectSBEDump operations
 */
constexpr int SBE_TYPE_PROC = 0xA;   ///< Normal P10 SBE
constexpr int SBE_TYPE_OCMB = 0xB;   ///< Odyssey OCMB SBE

/**
 * @brief Initialize pdbg + EKB libraries for SBE dump collection
 *
 * Old: openpower::phal::pdbg::initWithEkb() / initializePdbgLibEkb()
 * Next: stub (throws runtime_error)
 *
 * @throws std::runtime_error on failure
 */
void initSbeCollection();

/**
 * @brief Get the proc/OCMB target for a given failing unit ID and SBE type
 *
 * Old: iterates pdbg targets by index matching failingUnit
 * Next: stub (throws runtime_error)
 *
 * @param failingUnit  Chip position of the failing unit
 * @param sbeTypeId    SBE_TYPE_PROC or SBE_TYPE_OCMB
 * @return TargetHandle for the matching target
 * @throws std::runtime_error if target not found
 */
targeting::TargetHandle getTargetForSBEDump(uint32_t failingUnit,
                                             int sbeTypeId);

/**
 * @brief Probe a sub-target (pib or fsi) under a proc/OCMB target
 *
 * Old: pdbg_target_probe() on "pib" or "fsi" child
 * Next: stub (throws runtime_error)
 *
 * @param proc       Parent proc/OCMB target
 * @param subTarget  "pib" or "fsi"
 * @param sbeTypeId  SBE_TYPE_PROC or SBE_TYPE_OCMB
 * @return TargetHandle for the probed sub-target
 * @throws std::runtime_error on probe failure
 */
targeting::TargetHandle probeSbeTarget(targeting::TargetHandle proc,
                                        const std::string& subTarget,
                                        int sbeTypeId);

/**
 * @brief Check SBE state before dump collection
 *
 * Old:  openpower::phal::sbe::checkSbeState()
 * Next: sbei::getSbeCapabilitiesChipOp() — probes SBE readiness;
 *       failure → ChipOpError::NotAllowed
 *
 * @param pibFsiTarget  Probed pib/fsi target (old) / proc target (next)
 * @param sbeTypeId     SBE_TYPE_PROC or SBE_TYPE_OCMB
 * @throws ChipOpError if SBE is not in a collectable state
 */
void checkSbeState(targeting::TargetHandle pibFsiTarget, int sbeTypeId);

/**
 * @brief Execute SBE extract-RC HWP to retrieve SBE error RC
 *
 * Old:  openpower::phal::sbe::extractRC()
 * Next: sbei::getTiInfoChipOp() — retrieves TI (Terminate Immediately)
 *       data, the phal_next equivalent of the SBE error RC
 *
 * @param proc       Proc/OCMB target
 * @param dumpPath   Path to write extracted RC data
 * @param sbeTypeId  SBE_TYPE_PROC or SBE_TYPE_OCMB
 * @throws ChipOpError on HWP failure
 */
void sbeExtractRC(targeting::TargetHandle proc,
                  const std::filesystem::path& dumpPath,
                  int sbeTypeId);

/**
 * @brief Collect local register dump from SBE
 *
 * Old: openpower::phal::sbe::collectLocalRegDump()
 * Next: stub (throws runtime_error)
 *
 * @param proc          Proc/OCMB target
 * @param dumpPath      Path to write dump files
 * @param baseFilename  Base filename prefix
 * @param sbeTypeId     SBE_TYPE_PROC or SBE_TYPE_OCMB
 * @throws ChipOpError on HWP failure
 */
void collectLocalRegDump(targeting::TargetHandle proc,
                          const std::filesystem::path& dumpPath,
                          const std::string& baseFilename,
                          int sbeTypeId);

/**
 * @brief Collect PIB/MS register dump from SBE
 *
 * Old: openpower::phal::sbe::collectPIBMSRegDump()
 * Next: stub (throws runtime_error)
 *
 * @param proc          Proc/OCMB target
 * @param dumpPath      Path to write dump files
 * @param baseFilename  Base filename prefix
 * @param sbeTypeId     SBE_TYPE_PROC or SBE_TYPE_OCMB
 * @throws ChipOpError on HWP failure
 */
void collectPIBMSRegDump(targeting::TargetHandle proc,
                          const std::filesystem::path& dumpPath,
                          const std::string& baseFilename,
                          int sbeTypeId);

/**
 * @brief Collect PIB memory dump from SBE
 *
 * Old: openpower::phal::sbe::collectPIBMEMDump()
 * Next: stub (throws runtime_error)
 *
 * @param proc          Proc/OCMB target
 * @param dumpPath      Path to write dump files
 * @param baseFilename  Base filename prefix
 * @param sbeTypeId     SBE_TYPE_PROC or SBE_TYPE_OCMB
 * @throws ChipOpError on HWP failure
 */
void collectPIBMEMDump(targeting::TargetHandle proc,
                        const std::filesystem::path& dumpPath,
                        const std::string& baseFilename,
                        int sbeTypeId);

/**
 * @brief Collect PPE state from SBE
 *
 * Old: openpower::phal::sbe::collectPPEState()
 * Next: stub (throws runtime_error)
 *
 * @param proc          Proc/OCMB target
 * @param dumpPath      Path to write dump files
 * @param baseFilename  Base filename prefix
 * @param sbeTypeId     SBE_TYPE_PROC or SBE_TYPE_OCMB
 * @throws ChipOpError on HWP failure
 */
void collectPPEState(targeting::TargetHandle proc,
                     const std::filesystem::path& dumpPath,
                     const std::string& baseFilename,
                     int sbeTypeId);

/**
 * @brief Finalize SBE dump collection
 *
 * Old:  openpower::phal::sbe::finalizeCollection()
 * Next: sbei::quiesceSbeChipOp() — signals SBE that collection is
 *       complete; skipped on failure path
 *
 * @param pibFsiTarget  Probed pib/fsi target (old) / proc target (next)
 * @param dumpPath      Path where dump files were written
 * @param success       true = collection succeeded, false = failed
 * @param sbeTypeId     SBE_TYPE_PROC or SBE_TYPE_OCMB
 * @throws ChipOpError on HWP failure
 */
void finalizeSbeCollection(targeting::TargetHandle pibFsiTarget,
                            const std::filesystem::path& dumpPath,
                            bool success,
                            int sbeTypeId);

} // namespace openpower::dump::phal::chipop
