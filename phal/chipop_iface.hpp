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

namespace openpower::dump::phal::chipop
{

// ============================================================================
// Dump Data Container (Backend-Neutral)
// ============================================================================

/**
 * @brief RAII container for dump data
 *
 * Supports both malloc-based (legacy PHAL) and vector-based (new PHAL) storage
 */
class DumpData
{
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
     * @brief Create from malloc'd buffer (legacy PHAL)
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
    struct MallocBuf
    {
        uint8_t* p = nullptr;
        uint32_t len = 0;
        ~MallocBuf()
        {
            if (p)
                free(p);
        }

        // Non-copyable
        MallocBuf(const MallocBuf&) = delete;
        MallocBuf& operator=(const MallocBuf&) = delete;

        // Movable
        MallocBuf() = default;
        MallocBuf(MallocBuf&& other) noexcept : p(other.p), len(other.len)
        {
            other.p = nullptr;
            other.len = 0;
        }
        MallocBuf& operator=(MallocBuf&& other) noexcept
        {
            if (this != &other)
            {
                if (p)
                    free(p);
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

struct ChipOpError : public std::exception
{
    enum class Type
    {
        NotAllowed,   // SBE not ready for chip-ops
        Timeout,      // Command timeout
        Failed,       // Command failed
        NoFfdc,       // No FFDC data available
        InternalFfdc, // Internal FFDC (not chip-op failure)
        Unknown       // Unknown error
    };

    Type type{Type::Unknown};
    std::string msg;

    ChipOpError(Type t, std::string m) : type(t), msg(std::move(m)) {}

    const char* what() const noexcept override
    {
        return msg.c_str();
    }
};

// ============================================================================
// Chip Operations
// ============================================================================

/**
 * @brief Collect dump from SBE
 *
 * Legacy:  openpower::phal::sbe::getDump()
 * Next: hostfw::dump::getDump() — wrapper around sbei::getDump()
 *
 * @param chip Target handle (proc or OCMB)
 * @param dumpType Type of dump to collect
 * @param clockState Clock state (ON/OFF)
 * @param collectFastArray Whether to collect fast array data
 * @return DumpData containing collected dump
 * @throws ChipOpError on failure
 */
DumpData getDump(targeting::TargetHandle chip, uint8_t dumpType,
                 uint8_t clockState, uint8_t collectFastArray);

/**
 * @brief Stop threads on processor (prepare for hostboot dump)
 *
 * Legacy:  openpower::phal::sbe::threadStopProc()
 * Next: Not implemented (stub)
 *
 * @param proc Processor target handle
 * @throws ChipOpError on failure
 */
void threadStopProc(targeting::TargetHandle proc);

// ============================================================================
// SBE Dump Collection HWPs (legacy: libipl/libphal; next: stubs)
// ============================================================================

/**
 * @brief SBE type identifier for collectSBEDump operations
 */
constexpr int SBE_TYPE_PROC = 0xA; ///< Normal P10 SBE
constexpr int SBE_TYPE_OCMB = 0xB; ///< Odyssey OCMB SBE

/**
 * @brief Initialize pdbg + EKB libraries for SBE dump collection
 *
 * Legacy:  openpower::phal::pdbg::initWithEkb() / initializePdbgLibEkb()
 * Next: No-op (targeting already initialized by init())
 *
 * @throws std::runtime_error on failure
 */
void initSbeCollection();

/**
 * @brief Get the proc/OCMB target for a given failing unit ID and SBE type
 *
 * Legacy:  iterates pdbg targets by index matching failingUnit
 * Next: Not used (targets enumerated via targeting_next.cpp)
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
 * Legacy:  pdbg_target_probe() on "pib" or "fsi" child
 * Next: Not used (no pdbg probe concept in next backend)
 *
 * @param proc       Parent proc/OCMB target
 * @param subTarget  "pib" or "fsi"
 * @param sbeTypeId  SBE_TYPE_PROC or SBE_TYPE_OCMB
 * @return TargetHandle for the probed sub-target
 * @throws std::runtime_error on probe failure
 */
targeting::TargetHandle probeSbeTarget(
    targeting::TargetHandle proc, const std::string& subTarget, int sbeTypeId);

/**
 * @brief Check SBE state before dump collection
 *
 * Legacy:  openpower::phal::sbe::checkSbeState()
 * Next: Not implemented (stub)
 *
 * @param pibFsiTarget  Probed pib/fsi target (legacy) / proc target (next)
 * @param sbeTypeId     SBE_TYPE_PROC or SBE_TYPE_OCMB
 * @throws ChipOpError if SBE is not in a collectable state
 */
void checkSbeState(targeting::TargetHandle pibFsiTarget, int sbeTypeId);

/**
 * @brief Execute SBE extract-RC HWP to retrieve SBE error RC
 *
 * Legacy:  openpower::phal::sbe::extractRC()
 * Next: Not implemented (stub)
 *
 * @param proc       Proc/OCMB target
 * @param dumpPath   Path to write extracted RC data
 * @param sbeTypeId  SBE_TYPE_PROC or SBE_TYPE_OCMB
 * @throws ChipOpError on HWP failure
 */
void sbeExtractRC(targeting::TargetHandle proc,
                  const std::filesystem::path& dumpPath, int sbeTypeId);

/**
 * @brief Collect local register dump from SBE
 *
 * Legacy:  openpower::phal::sbe::collectLocalRegDump()
 * Next: Not used (collected via getDump() in next backend)
 *
 * @param proc          Proc/OCMB target
 * @param dumpPath      Path to write dump files
 * @param baseFilename  Base filename prefix
 * @param sbeTypeId     SBE_TYPE_PROC or SBE_TYPE_OCMB
 * @throws std::runtime_error (not supported)
 */
void collectLocalRegDump(targeting::TargetHandle proc,
                         const std::filesystem::path& dumpPath,
                         const std::string& baseFilename, int sbeTypeId);

/**
 * @brief Collect PIB/MS register dump from SBE
 *
 * Legacy:  openpower::phal::sbe::collectPIBMSRegDump()
 * Next: Not used (collected via getDump() in next backend)
 *
 * @param proc          Proc/OCMB target
 * @param dumpPath      Path to write dump files
 * @param baseFilename  Base filename prefix
 * @param sbeTypeId     SBE_TYPE_PROC or SBE_TYPE_OCMB
 * @throws std::runtime_error (not supported)
 */
void collectPIBMSRegDump(targeting::TargetHandle proc,
                         const std::filesystem::path& dumpPath,
                         const std::string& baseFilename, int sbeTypeId);

/**
 * @brief Collect PIB memory dump from SBE
 *
 * Legacy:  openpower::phal::sbe::collectPIBMEMDump()
 * Next: Not used (collected via getDump() in next backend)
 *
 * @param proc          Proc/OCMB target
 * @param dumpPath      Path to write dump files
 * @param baseFilename  Base filename prefix
 * @param sbeTypeId     SBE_TYPE_PROC or SBE_TYPE_OCMB
 * @throws std::runtime_error (not supported)
 */
void collectPIBMEMDump(targeting::TargetHandle proc,
                       const std::filesystem::path& dumpPath,
                       const std::string& baseFilename, int sbeTypeId);

/**
 * @brief Collect PPE state from SBE
 *
 * Legacy:  openpower::phal::sbe::collectPPEState()
 * Next: Not used (collected via getDump() in next backend)
 *
 * @param proc          Proc/OCMB target
 * @param dumpPath      Path to write dump files
 * @param baseFilename  Base filename prefix
 * @param sbeTypeId     SBE_TYPE_PROC or SBE_TYPE_OCMB
 * @throws std::runtime_error (not supported)
 */
void collectPPEState(targeting::TargetHandle proc,
                     const std::filesystem::path& dumpPath,
                     const std::string& baseFilename, int sbeTypeId);

/**
 * @brief Finalize SBE dump collection
 *
 * Legacy:  openpower::phal::sbe::finalizeCollection()
 * Next: Not implemented (stub)
 *
 * @param pibFsiTarget  Probed pib/fsi target (legacy) / proc target (next)
 * @param dumpPath      Path where dump files were written
 * @param success       true = collection succeeded, false = failed
 * @param sbeTypeId     SBE_TYPE_PROC or SBE_TYPE_OCMB
 * @throws ChipOpError on HWP failure
 */
void finalizeSbeCollection(targeting::TargetHandle pibFsiTarget,
                           const std::filesystem::path& dumpPath, bool success,
                           int sbeTypeId);

} // namespace openpower::dump::phal::chipop
