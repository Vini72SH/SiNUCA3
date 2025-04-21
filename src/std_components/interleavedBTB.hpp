#ifndef SINUCA3_INTERLEAVED_BTB_HPP
#define SINUCA3_INTERLEAVED_BTB_HPP

/**
 * @file interleavedBTB.hpp
 * @brief Implementation of Interleaved Branch Target Buffer.
 * @details The Interleaved BTB is a structure that stores branches and has a
 * certain interleaving factor that allows multiple queries in a single cycle.
 * In this implementation, it is mapped directly and each entry has a tag for
 * recognizing the block of instructions and vectors assimilated to the block,
 * where each element of the vector corresponds to information from one of the
 * instructions in the block. Although the parameters are configurable, for
 * optimization reasons, the BTB is limited to using parameters that are
 * multiples of 2, allowing bitwise operations to optimize the BTB's overall
 * running time. When defining the parameters, the BTB itself internally defines
 * the parameters as multiples of 2, choosing the log2 floor of the parameter
 * passed in and using it to self-generate.
 */

#include "../engine/component.hpp"
#include "../utils/bimodalPredictor.hpp"

namespace sinuca {

enum branchType { NONE, CONDITIONAL_BRANCH, UNCONDITIONAL_BRANCH };

enum TypeBTBMessage {
    BTB_REQUEST,
    UNALLOCATED_ENTRY,
    ALLOCATED_ENTRY,
    BTB_ALLOCATION_REQUEST,
    BTB_UPDATE_REQUEST
};

struct BTBMessage {
    int channelID;
    unsigned int fetchAddress;
    unsigned int nextBlock;
    unsigned int* fetchTargets;
    bool* validBits;
    bool* executedInstructions;
    TypeBTBMessage messageType;
};

struct btb_entry {
  private:
    unsigned int numBanks;             /**<The number of banks. */
    unsigned long entryTag;            /**<The entry tag. */
    unsigned long* targetArray;        /**<The target address array. */
    branchType* branchTypes;           /**<The branch types array. */
    BimodalPredictor* predictorsArray; /**<The array of predictors. */

  public:
    btb_entry();

    /**
     * @brief Allocates the BTB entry.
     * @param numBanks The number of banks in an entry.
     * @return 0 if successfuly, 1 otherwise.
     */
    int Allocate(unsigned int numBanks);

    /**
     * @brief Register a new entry.
     * @param tag The tag of this entry's instruction block.
     * @param bank The bank with which the branch will be registered.
     * @param targetAdress The branch's target address.
     * @param type The branch's type.
     * @return 0 if successfuly, 1 otherwise.
     */
    int NewEntry(unsigned long tag, unsigned int bank,
                 unsigned long targetAddress, branchType type);

    /**
     * @brief Update the BTB entry.
     * @param bank The bank containing the branch to be updated.
     * @param branchState The state of the branch, whether it has been taken or
     * not.
     * @return 0 if successfuly, 1 otherwise.
     */
    int UpdateEntry(unsigned long bank, bool branchState);

    /**
     * @brief Gets the tag of the entry
     */
    long GetTag();

    /**
     * @brief Gets the branch target address.
     * @param bank The bank containing the branch.
     */
    long GetTargetAddress(unsigned int bank);

    /**
     * @brief Gets the branch type.
     * @param bank The bank containing the branch.
     */
    branchType GetBranchType(unsigned int bank);

    /**
     * @brief Gets the branch prediction.
     * @param bank The bank containing the branch.
     */
    bool GetPrediction(unsigned int bank);

    ~btb_entry();
};

class BranchTargetBuffer : public sinuca::Component<BTBMessage> {
  private:
    btb_entry** btb; /**<The pointer to BTB struct. */
    unsigned int
        interleavingFactor /**<The interleaving factor, defining the number of
                              banks in which the BTB is interleaved. */
        ;
    unsigned int numEntries;       /**<The number of BTB entries. */
    unsigned int interleavingBits; /**< InterleavingFactor in bits. */
    unsigned int entriesBits;      /**<Number of entries in bits. */

    /**
     * @brief Calculate the bank based on address.
     * @param address The address that will be used to calculate the bank.
     * @return Which bank the instruction is in.
     */
    unsigned int CalculateBank(unsigned long address);

    /**
     * @brief Calculate the tag based on address.
     * @param address The address that will be used to calculate the tag.
     * @return The tag of instruction block.
     */
    unsigned long CalculateTag(unsigned long address);

    /**
     * @brief Calculate the index based on address.
     * @param address The address that will be used to calculate the index.
     * @return The index of branch, In which BTB entry is the branch located.
     */
    unsigned long CalculateIndex(unsigned long address);

    /**
     * @brief Register a new branch in BTB.
     * @param address The branch address.
     * @param targetAddress The target of the branch.
     * @param type The type of branch.
     * @return 0 if successfuly, 1 otherwise.
     */
    int RegisterNewBranch(unsigned long address, unsigned long targetAddress,
                          branchType type);

    /**
     * @brief Update a BTB entry.
     * @param address The branch address.
     * @param branchState The Information on whether the branch has been taken or not.
     * @return 0 if successfuly, 1 otherwise. 
     */
    int UpdateBranch(unsigned long address, bool branchState);

  public:
    BranchTargetBuffer();

    virtual int SetConfigParameter(const char* parameter,
                                   sinuca::config::ConfigValue value);
    virtual int FinishSetup();

    virtual void Clock();
    virtual void Flush();
    ~BranchTargetBuffer();
};

}  // namespace sinuca

#endif