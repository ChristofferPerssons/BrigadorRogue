// dllmain.cpp : Defines the entry point for the DLL application.
#include "pch.h"
#include "windows.h"
#include "stdlib.h"
#include "stdio.h"
#include "fstream"
#include "iostream"
#include <string> 
#include "TlHelp32.h"
#include <cstdio>
#include <stdint.h>
#include <Psapi.h>
#include <random>
#include <cmath>
#include "utils.h"
#include "mo.h"
#include "BrigadorRogue.h"

using namespace std;

//#define DEBUG
void DebugBox(LPCSTR lpText) {
#ifdef DEBUG
    MessageBoxA(NULL, lpText, "Debug", MB_OK);
#endif // DEBUG
}

//Some globals follow but you should use a struct created in the main loop to access them.
//They are only global since I find it easier to match them to their enum values in this way.

//Used to fetch index of added button pressed
const enum buttons {
    M_Repair
    , M_PosOvercharge
    , M_PosForwardSpeed
    , P_PosCapacity
    , P_PosFireRate
    , P_PosProjectilesNegAccuracy
    , P_PosPropMult
    , S_PosCapacity
    , S_PosFireRate
    , S_PosProjectilesNegAccuracy
    , S_PosPropMult
    , RandomMech
    , RandomPrimary
    , RandomSecondary
};

//Must correspond to buttons enum
const char* addedButtonStrings[] = {
    {"Repair +%dhp $%lldk"}
    , {"Overcharge +%.1fx $%lldk"}
    , {"Forward Speed +%.1fx $%lldk"}
    , {"P: Capacity +%.1fx $%lldk"}
    , {"P: Fire Rate +%.1fx $%lldk"}
    , {"P: Projectiles +%d Accuracy -%d $%lldk"}
    , {"P: Structure Damage +%.1fx $%lldk"}
    , {"S: Capacity +%.1fx $%lldk"}
    , {"S: Fire Rate +%.1fx $%lldk"}
    , {"S: Projectiles +%d Accuracy -%d $%lldk"}
    , {"S: Structure +%.1fx $%lldk"}
    , {"%s $%0.1fm"}
    , {"P: %s $%0.1fm"}
    , {"S: %s $%0.1fm"}

};
#define addedButtons sizeof(addedButtonStrings) / sizeof(addedButtonStrings[0])

//Cost relate to the button with the same index in addedButtonStrings
double baseButtonCosts[addedButtons]{
    500000
    , 1000000
    , 1000000
    , 1000000
    , 1000000
    , 1000000
    , 1000000
    , 1000000
    , 1000000
    , 1000000
    , 1000000
    , 0 
    , 0
    , 0
};

//Values used to determine upgrade magnitude
#define repairHealthPoints 50
#define overchargeMult 1.2
#define forwardSpeedMult 1.2
#define pCapacityMult 1.2
#define sCapacityMult 1.2
#define pFireRateMult 1.2
#define sFireRateMult 1.2
#define pProjectiles 1
#define pAccuracy 5
#define sProjectiles 1
#define sAccuracy 5
#define pPropMult 1.2
#define sPropMult 1.2

//Arrays below use a tuple to store: Offset, Default Value, Modded Value
//Mech
const enum mechVarsIdx {
    MaxOverchargeIdx,
    MaxHealthIdx
};

uthruple deployedMechOffsetsAndVals[] = {
    {maxOverchargeOffset, 0x0, 0x0} //Max Overcharge
    ,{ maxHealthOffset, 0x0, 0x0 } 
};
#define mechVars sizeof(deployedMechOffsetsAndVals) / sizeof(deployedMechOffsetsAndVals[0])

//MechLegs
const enum mechLegsVarsIdx {
    MaxForwardSpeedIdx,
};

uthruple deployedMechLegsOffsetsAndVals[] = {
    {maxForwardSpeedOffset, 0x0, 0x0} //Max Forward Speed
};
#define mechLegsVars sizeof(deployedMechLegsOffsetsAndVals) / sizeof (deployedMechLegsOffsetsAndVals[0])

//Weapons variables. Must correspond to to same index in weaponVarsTemplate definition.
const enum weaponVarIdx {
    CapacityIdx,
    CooldownIdx,
    ShotCountIdx,
    AccuracyIdx
};

#define weaponVarsTemplate  {{weaponVarsOffset + weaponCapacityOffset, 0x0, 0x0}, { weaponVarsOffset + weaponCooldownOffset, 0x0, 0x0 },{ weaponVarsOffset + weaponShotCountOffset, 0x0, 0x0 },{ weaponVarsOffset + weaponAccuracyOffset, 0x0, 0x0 }}

uthruple deployedWeaponOffsetsAndValsTemplate[] = weaponVarsTemplate;

#define weaponVars sizeof(deployedWeaponOffsetsAndValsTemplate) / sizeof(deployedWeaponOffsetsAndValsTemplate[0])

uthruple deployedWeaponsOffsetsAndVals[maxWeapons][weaponVars] = {
    weaponVarsTemplate,
    weaponVarsTemplate,
    weaponVarsTemplate,
    weaponVarsTemplate,
    weaponVarsTemplate,
    weaponVarsTemplate,
    weaponVarsTemplate,
    weaponVarsTemplate,
    weaponVarsTemplate
};

//Weapons variables. Must correspond to to same index in bulletVarsTemplate definition.
const enum bulletVars {
    PropMultIdx
};

#define bulletVarsTemplate  {{bulletPropMultOffset, 0x0, 0x0}}

uthruple deployedBulletOffsetsAndValsTemplate[] = bulletVarsTemplate;

#define bulletVars sizeof(deployedBulletOffsetsAndValsTemplate) / sizeof(deployedBulletOffsetsAndValsTemplate[0])

uthruple deployedBulletsOffsetsAndVals[maxWeapons][bulletVars] = {
    bulletVarsTemplate,
    bulletVarsTemplate,
    bulletVarsTemplate,
    bulletVarsTemplate,
    bulletVarsTemplate,
    bulletVarsTemplate,
    bulletVarsTemplate,
    bulletVarsTemplate,
    bulletVarsTemplate
};

struct varBaseAddresses {
    uint64_t mech;
    uint64_t mechLegs;
    uint64_t* weapons;
    uint64_t* bullets;
};

struct offsetsAndVals {
    uthruple* mech;
    uthruple* mechLegs;
    uthruple (*weapons)[weaponVars];
    uthruple (*bullets)[bulletVars];
};

struct varStruct {
    varBaseAddresses baseAddresses;
    offsetsAndVals offsetsNVals;
    int primaryIndex;
    int secondaryIndex;
    bool shouldResetResources;
    bool resourceMemUpdateNecessary;
    bool shouldKeepPreviousValues;
};

struct upgrades {
    char* upgradeText[maxAvailableUpgrades];
    buttons buttonIndex[maxAvailableUpgrades];
};

struct resourceList {
    uint64_t* addresses;
    uint64_t len;
    uint64_t maxLen;
};

//Used to store data that should be injected into the save file
//struct saveStruct {
//    unsigned char mechJSONBuf[64];
//    uint32_t weaponCount;
//    uint32_t socketID[maxWeapons];
//    uint32_t socketGroup[maxWeapons];
//    unsigned char weaponJSONBufs[maxWeapons][64];
//    unsigned char socketStringBufs[maxWeapons][64];
//};
//Used to store data that should be injected into the save file
struct saveStruct {
    uint64_t mechAddress;
    uint32_t weaponCount;
    uint32_t socketID[maxWeapons];
    uint32_t socketGroup[maxWeapons];
    uint64_t weaponAddresses[maxWeapons];
    char* socketsStringAddresses[maxWeapons];
    bool shouldReset;
};

struct upgradeStruct {
    upgrades* availableUpgrades;
    char (*formattedButtonStrings)[maxButtonStringLength];
    double* upgradesCost;
    const uint64_t alwaysAvailableCount;
    const buttons* alwaysAvailableButtons;
    const uint64_t availableCount;
    const uint64_t freeUpgradesPerLevel;
    const uint64_t consumeMax;
    uint64_t consumed;
    uint64_t removedUpgrades;
    bool randomizeUpgrades;
    float repairAmount;
    float maxHealth;
    uint64_t randomizedMechAddress;
    uint64_t randomizedWeaponAddresses[maxWeapons];
    resourceList mechResources;
    resourceList weaponResources;
    saveStruct* saveUpt;
};

struct rngStruct {
    std::mt19937 gen;
    std::uniform_int_distribution<> distrib;
};

//Is global to make but could be avoided with the altering of patch deployment method. addButtonsChooseDistrict and 
upgrades upgradeList;

asmHook addButtonsChooseDistrict{
    "addButtonsChooseDistrictV3",
    294,
    120,
    0x68e28,
    16,
    NULL,
    26,
    false,
    { {132} },
    { {0x68d12} }, // True loop start at 0x68d12
    { {270}, {278} }, //stringArrayAddres, buttonsToAdd
    { {(uint64_t)upgradeList.upgradeText}, {0} }
};

// Uses set string in createUIButtonChooseDistrictOrupdateSomeUI2 from addButtonsChooseDistrict if it is not null
asmHook createUIButtonUseSetString{
    "createUIButtonUseSetString",
    173,
    120,
    0x754d0,
    14,
    NULL,
    8,
    false,
    {},
    {},
    { {165}}, //stringToPrintAddressLocation
    { {0x0}}  //Must be set before deploying. Should be address to location where addButtonsChooseDistrict stores the address of the nextStringToPrintAddress
};

//Sets up a list of all resource addresses
//Code derived from source function at +0xdbf10 but reduced in scope using assumptions which hopefully hold
void setupResourceList(resourceList* resList, uint64_t* param_2, uint32_t* param4content, int elems) {
    char buffer[256];

    //Reset resource list
    resList->len = 0;

    LPVOID(*getResourceAddress)(long long*, uint64_t) = getResourceAddressFunction;

    char local_128[64] = { 0 };

    //uint64_t* param_2 = fetchMechDictAddress;
    //uint64_t param4content = 0xE;
    uint64_t lVar16;
    uint64_t lVar12;
    uint64_t* plVar8;
    uint64_t* local_140 = param_2;
    char* stringKey;
    uint64_t dictIndex;
    uint64_t resAddress;

    for (int i = 0; i < elems; i++) {
        lVar16 = param4content[i];
        if (local_128[lVar16] == '\0') {
            local_128[lVar16] = '\x01';
            lVar12 = 0;
            plVar8 = param_2 + (lVar16 + 4) * 2;
            lVar16 = *plVar8;
            param_2 = local_140;
            if (0 < lVar16) {
                do {
                    stringKey = *(char**)(*(uint64_t*)(*local_140 + 0x18) + (uint64_t) * (int*)(plVar8[1] + lVar12 * 4) * 8);
                    //snprintf(buffer, 100, "mech resource string key address = %I64x, count = %d", stringKey, lVar16);
                    //DebugBox(buffer);

                    uint64_t stringMatchAddress = (uint64_t)bsearch(&stringKey, *(void**)(*param_2 + 0x18), (uint64_t) * (int*)(*param_2 + 0x10), 8, stringKeyComparisonFunction);

                    dictIndex = ((uint64_t)stringMatchAddress - *(uint64_t*)(*param_2 + 0x18) >> 3);
                    //snprintf(buffer, 100, "offset = %I64x", dictIndex);
                    //DebugBox(buffer);

                    resAddress = (uint64_t)getResourceAddress((long long*)param_2, *(uint64_t*)(*(uint64_t*)(*param_2 + 0x28) + dictIndex * 0x8));

                    //snprintf(buffer, 100, "mechAddress = %I64x", mechAddress);
                    //DebugBox(buffer);

                    //Increase size of list if necessary 
                    if (resList->len + lVar12 >= resList->maxLen) {
                        uint64_t* newListAddress = new uint64_t[resList->maxLen + arbitraryResourceCount];

                        memcpy(newListAddress, resList->addresses, resList->maxLen * sizeof(uint64_t));

                        delete[] resList->addresses;
                        resList->maxLen += arbitraryResourceCount;
                        resList->addresses = newListAddress;
                    }

                    //Append resource to list
                    resList->addresses[resList->len + lVar12] = resAddress;

                    //Incremet counter
                    lVar12 = lVar12 + 1;
                    //param_4 = local_160;
                    param_2 = local_140;
                } while (lVar12 < lVar16);
                resList->len += lVar12;
                //snprintf(buffer, 100, "mechCount = %d", lVar12);
                //DebugBox(buffer);
            }
        }
    }
}


#define RESOURCES_MUST_BE_VALID
void setRandomPartsAddresses(upgradeStruct* upgradeState, rngStruct* rng) {
    char buffer[256];
    std::uniform_int_distribution<> mechDistrib(0, upgradeState->mechResources.len - 1);
#ifdef RESOURCES_MUST_BE_VALID
    while (true) {
#endif // RESOURCES_MUST_BE_VALID
        int randomIndex = mechDistrib(rng->gen);
        upgradeState->randomizedMechAddress = upgradeState->mechResources.addresses[randomIndex];
#ifdef RESOURCES_MUST_BE_VALID
        if (text_lookup((char*)(upgradeState->randomizedMechAddress + mechNameOffset)) != NULL) {
            break;
        }
    }
#endif // RESOURCES_MUST_BE_VALID

    std::uniform_int_distribution<> weaponDistrib(0, upgradeState->weaponResources.len - 1);
    for (int i = 0; i < maxWeapons; i++) {
#ifdef RESOURCES_MUST_BE_VALID
        while (true) {
#endif // RESOURCES_MUST_BE_VALID
            int randomIndex = weaponDistrib(rng->gen);
            upgradeState->randomizedWeaponAddresses[i] = upgradeState->weaponResources.addresses[randomIndex];
#ifdef RESOURCES_MUST_BE_VALID
            if (text_lookup((char*)(upgradeState->randomizedWeaponAddresses[i] + weaponNameOffset)) != NULL && fetchWeaponCapacity(upgradeState->weaponResources.addresses[randomIndex])>0) {
                break;
            }
        }
#endif // RESOURCES_MUST_BE_VALID
    }
}

//Return index to first weapon with the searched for weaponGroup
int getWeaponIndexOfWeaponGroup(weaponGroups weaponGroup) {
    char buffer[256];
    for (int i = 0; i < maxWeapons; i++) {
        weaponGroups curWeaponGroup = (weaponGroups)fetchDeployedWeaponSocketGroup(i);
        //snprintf(buffer, 100, "weaponGroup = %d, searched for = %d", curWeaponGroup, weaponGroup);
        //DebugBox(buffer);
        if (weaponGroup == curWeaponGroup) {
            return i;
        }
    }
    return -1;
}

bool ammoTypeHasBullet(unsigned char* ammoTypeAddress) {
    if (ammoTypeAddress == NULL) {
        return false;
    }
    ammoTypes ammoType = (ammoTypes)*ammoTypeAddress;
    if (ammoType == Bullet || ammoType == Artillery || ammoType == Cannon) {
        return true;
    }
    return false;
}

void updateVars(uint64_t baseAddress, uthruple* offsetsNVals, uint64_t varCount) {
    for (int i = 0; i < varCount; i++) {
        *(uint32_t*)(baseAddress + offsetsNVals[i].offset) = offsetsNVals[i].val;
    }
}

//Copies stored weapon var values to game memory
void updateResources(varStruct* vars) {
    char buffer[256];
    updateVars(vars->baseAddresses.mech, vars->offsetsNVals.mech, mechVars);
    //if (vars->resourceMemUpdateNecessary) {
    //
    //    DebugBox("updateResources1");
    //}
    updateVars(vars->baseAddresses.mechLegs, vars->offsetsNVals.mechLegs, mechLegsVars);
    //if (vars->resourceMemUpdateNecessary) {
    //
    //    DebugBox("updateResources2");
    //}
    for (int i = 0; i < maxWeapons; i++) {
        if (vars->baseAddresses.weapons[i] != NULL) {
            //if (vars->resourceMemUpdateNecessary) {
            //
            //    snprintf(buffer, 100, "updateResources3 %d", i);
            //    DebugBox(buffer);
            //}
            updateVars(vars->baseAddresses.weapons[i], vars->offsetsNVals.weapons[i], weaponVars);
            //if (vars->resourceMemUpdateNecessary) {
            //    snprintf(buffer, 100, "updateResources3.5 %d", i);
            //    DebugBox(buffer);
            //}
            if (ammoTypeHasBullet(fetchDeployedWeaponAmmoTypeAddress(i))) {
                //if (vars->resourceMemUpdateNecessary) {
                //    snprintf(buffer, 100, "updateResources3.9 %d", i);
                //    DebugBox(buffer);
                //}
                updateVars(vars->baseAddresses.bullets[i], vars->offsetsNVals.bullets[i], bulletVars);
            }
            //if (vars->resourceMemUpdateNecessary) {
            //
            //    snprintf(buffer, 100, "updateResources4 %d", i);
            //    DebugBox(buffer);
            //}
            
        }
    }
}

void resetVars(uint64_t* baseAddress, uthruple* offsetsNVals, uint64_t varCount, uint64_t deployedResourceAddress) {
    char buffer[256];
    for (int i = 0; i < varCount; i++) {
        if (*baseAddress != NULL) {
            *(uint32_t*)(*baseAddress + offsetsNVals[i].offset) = offsetsNVals[i].org;
        }
        if (deployedResourceAddress != NULL) {
            offsetsNVals[i].org = *(uint32_t*)(deployedResourceAddress + offsetsNVals[i].offset);
            offsetsNVals[i].val = offsetsNVals[i].org;
        }
        else {
            offsetsNVals[i].org = 0;
            offsetsNVals[i].val = 0;
        }

    }
    *baseAddress = deployedResourceAddress;
}

//Resets weapon var structs in case of weapon change or game state is in a menu where upgrades should reset
void resetResources(varStruct* vars){
    char buffer[256];
    states currentState = fetchCurrentState;
    bool shouldReset = (
        currentState == MainMenu ||
        currentState == Campaign ||
        currentState == Freelancer ||
        currentState == Aquisitions ||
        currentState == LoseScreen
        );

    if (vars->shouldResetResources) {
        //Get a stable copy of mech, primary and secondary weapon data 
        uint64_t mechAddress = NULL;
        uint64_t mechLegsAddress = NULL;
        uthruple mechOffsetsNVals[mechVars] = { 0 };
        uthruple mechLegsOffsetsNVals[mechLegsVars] = { 0 };
        uint64_t primaryAddress = NULL;
        uint64_t primaryBulletAddress = NULL;
        uthruple primaryOffsetsNVals[weaponVars] = { 0 };
        uthruple primaryBulletOffsetsNVals[bulletVars] = { 0 };
        uint64_t secondaryAddress = NULL;
        uint64_t secondaryBulletAddress = NULL;
        uthruple secondaryOffsetsNVals[weaponVars] = { 0 };
        uthruple secondaryBulletOffsetsNVals[bulletVars] = { 0 };
        if (vars->shouldKeepPreviousValues) {
            mechAddress = vars->baseAddresses.mech;
            mechLegsAddress = vars->baseAddresses.mechLegs;
            for (int k = 0; k < mechVars; k++) {
                mechOffsetsNVals[k].offset = vars->offsetsNVals.mech[k].offset;
                mechOffsetsNVals[k].org = vars->offsetsNVals.mech[k].org;
                mechOffsetsNVals[k].val = vars->offsetsNVals.mech[k].val;
            }
            for (int k = 0; k < mechLegsVars; k++) {
                mechLegsOffsetsNVals[k].offset = vars->offsetsNVals.mechLegs[k].offset;
                mechLegsOffsetsNVals[k].org = vars->offsetsNVals.mechLegs[k].org;
                mechLegsOffsetsNVals[k].val = vars->offsetsNVals.mechLegs[k].val;
            }

            if (vars->primaryIndex >= 0) {
                DebugBox("SetPrimaryBackup");
                primaryAddress = vars->baseAddresses.weapons[vars->primaryIndex];
                primaryBulletAddress = vars->baseAddresses.bullets[vars->primaryIndex];
                for (int k = 0; k < weaponVars; k++) {
                    primaryOffsetsNVals[k].offset = vars->offsetsNVals.weapons[vars->primaryIndex][k].offset;
                    primaryOffsetsNVals[k].org = vars->offsetsNVals.weapons[vars->primaryIndex][k].org;
                    primaryOffsetsNVals[k].val = vars->offsetsNVals.weapons[vars->primaryIndex][k].val;
                }
                for (int k = 0; k < bulletVars; k++) {
                    primaryBulletOffsetsNVals[k].offset = vars->offsetsNVals.bullets[vars->primaryIndex][k].offset;
                    primaryBulletOffsetsNVals[k].org = vars->offsetsNVals.bullets[vars->primaryIndex][k].org;
                    primaryBulletOffsetsNVals[k].val = vars->offsetsNVals.bullets[vars->primaryIndex][k].val;
                }
            }
            if (vars->secondaryIndex >= 0) {
                secondaryAddress = vars->baseAddresses.weapons[vars->secondaryIndex];
                secondaryBulletAddress = vars->baseAddresses.bullets[vars->secondaryIndex];
                for (int k = 0; k < weaponVars; k++) {
                    secondaryOffsetsNVals[k].offset = vars->offsetsNVals.weapons[vars->secondaryIndex][k].offset;
                    secondaryOffsetsNVals[k].org = vars->offsetsNVals.weapons[vars->secondaryIndex][k].org;
                    secondaryOffsetsNVals[k].val = vars->offsetsNVals.weapons[vars->secondaryIndex][k].val;
                }

                for (int k = 0; k < bulletVars; k++) {
                    secondaryBulletOffsetsNVals[k].offset = vars->offsetsNVals.bullets[vars->secondaryIndex][k].offset;
                    secondaryBulletOffsetsNVals[k].org = vars->offsetsNVals.bullets[vars->secondaryIndex][k].org;
                    secondaryBulletOffsetsNVals[k].val = vars->offsetsNVals.bullets[vars->secondaryIndex][k].val;
                }
            }
        }

        //Get deployed resources
        uint64_t curDeployedMechAddress = fetchDeployedMechAddress;
        uint64_t curDeployedMechLegsAddress = fetchDeployedMechLegsAddress;

        uint64_t curDeployedWeaponAddresses[maxWeapons];
        uint64_t curDeployedWeaponBulletAddresses[maxWeapons];
        for (int i = 0; i < maxWeapons; i++) {
            curDeployedWeaponAddresses[i] = fetchDeployedWeaponAddress(i);
            curDeployedWeaponBulletAddresses[i] = (uint64_t)fetchDeployedWeaponAmmoTypeAddress(i);
        }
        //Reset Mech Vars and set new orgs v
        resetVars(&vars->baseAddresses.mech, vars->offsetsNVals.mech, mechVars, curDeployedMechAddress);

        //Reset Mech Legs Vars and set new orgs
        resetVars(&vars->baseAddresses.mechLegs, vars->offsetsNVals.mechLegs, mechLegsVars, curDeployedMechLegsAddress);

        //Reset weapon and associated bullet vars
        for (int i = 0; i < maxWeapons; i++) {
            resetVars(&vars->baseAddresses.weapons[i], vars->offsetsNVals.weapons[i], weaponVars, curDeployedWeaponAddresses[i]);
            if (vars->baseAddresses.weapons[i] != NULL && ammoTypeHasBullet(fetchDeployedWeaponAmmoTypeAddress(i))) {
                resetVars(&vars->baseAddresses.bullets[i], vars->offsetsNVals.bullets[i], bulletVars, curDeployedWeaponBulletAddresses[i]);
            }
        }

        vars->primaryIndex = getWeaponIndexOfWeaponGroup(PrimaryGroup);

        vars->secondaryIndex = getWeaponIndexOfWeaponGroup(SecondaryGroup);

        if (vars->shouldKeepPreviousValues) {
            DebugBox("ShouldKeepPrevValues  =Yes");
            if (vars->baseAddresses.mech == mechAddress) {
                for (int k = 0; k < mechVars; k++) {
                    vars->offsetsNVals.mech[k].offset = mechOffsetsNVals[k].offset;
                    vars->offsetsNVals.mech[k].org = mechOffsetsNVals[k].org;
                    vars->offsetsNVals.mech[k].val = mechOffsetsNVals[k].val;
                }
                if (vars->baseAddresses.mechLegs == mechLegsAddress) {
                    for (int k = 0; k < bulletVars; k++) {
                        vars->offsetsNVals.mechLegs[k].offset = mechLegsOffsetsNVals[k].offset;
                        vars->offsetsNVals.mechLegs[k].org = mechLegsOffsetsNVals[k].org;
                        vars->offsetsNVals.mechLegs[k].val = mechLegsOffsetsNVals[k].val;
                    }
                }
            }
            //Update primary and secondary vars to original values
            for (int i = 0; i < maxWeapons; i++) {
                if (vars->baseAddresses.weapons[i] == primaryAddress) {
                    for (int k = 0; k < weaponVars; k++) {
                        vars->offsetsNVals.weapons[i][k].offset = primaryOffsetsNVals[k].offset;
                        vars->offsetsNVals.weapons[i][k].org = primaryOffsetsNVals[k].org;
                        vars->offsetsNVals.weapons[i][k].val = primaryOffsetsNVals[k].val;
                    }
                    if (vars->baseAddresses.bullets[i] == primaryBulletAddress) {
                        for (int k = 0; k < bulletVars; k++) {
                            vars->offsetsNVals.bullets[i][k].offset = primaryBulletOffsetsNVals[k].offset;
                            vars->offsetsNVals.bullets[i][k].org = primaryBulletOffsetsNVals[k].org;
                            vars->offsetsNVals.bullets[i][k].val = primaryBulletOffsetsNVals[k].val;
                        }
                    }
                }
                else if (vars->baseAddresses.weapons[i] == secondaryAddress) {
                    for (int k = 0; k < weaponVars; k++) {
                        vars->offsetsNVals.weapons[i][k].offset = secondaryOffsetsNVals[k].offset;
                        vars->offsetsNVals.weapons[i][k].org = secondaryOffsetsNVals[k].org;
                        vars->offsetsNVals.weapons[i][k].val = secondaryOffsetsNVals[k].val;
                    }
                    if (vars->baseAddresses.bullets[i] = secondaryBulletAddress) {
                        for (int k = 0; k < bulletVars; k++) {
                            vars->offsetsNVals.bullets[i][k].offset = secondaryBulletOffsetsNVals[k].offset;
                            vars->offsetsNVals.bullets[i][k].org = secondaryBulletOffsetsNVals[k].org;
                            vars->offsetsNVals.bullets[i][k].val = secondaryBulletOffsetsNVals[k].val;
                        }
                    }
                }
            }
            updateResources(vars);
        }




        //Stop updating when choose district state has been entered
        if (currentState == FreelancerChooseDistrict || currentState == AfterLoadLevel) {
            vars->shouldResetResources = false;
            vars->shouldKeepPreviousValues = true;
        }
    }
    else if (shouldReset){
        vars->shouldResetResources = true;
        vars->shouldKeepPreviousValues = false;
    } 
}

//Global used by functions that need to update the savefile and memory of deployed pc
saveStruct saveUpdateData;

void resetSaveUpdateDataGlobal() {
    saveUpdateData.mechAddress = NULL;
    saveUpdateData.weaponCount = 0xFFFFFFFF;
    saveUpdateData.shouldReset = false;
    for (int i = 0; i < maxWeapons; i++) {
        saveUpdateData.socketID[i] = 0x0;
        saveUpdateData.socketGroup[i] = 0xFFFFFFFF;
        saveUpdateData.weaponAddresses[i] = NULL;
        saveUpdateData.socketsStringAddresses[i] = NULL;
    }
}

//This is called by a patch from the game's execution. Updates the variables in memory, and should be directly followed by the patch executing a save and load.
void updateDeployedMem() {
    char buffer[256];
    if (saveUpdateData.shouldReset) {
        fetchDeployedMechAddress = NULL;
        fetchDeployedWeaponCount = 0xFFFFFFFF;
        //Reset weapon slots
        for (int i = 0; i < maxWeapons; i++) {
            fetchDeployedWeaponAddress(i) = 0;
            fetchDeployedWeaponSocketStringAddress(i) = 0;
            fetchDeployedWeaponSocketID(i) = 0;
            fetchDeployedWeaponSocketGroup(i) = 0;
        }
    }

    //Update deployed dat
    if (saveUpdateData.mechAddress != NULL) {
        fetchDeployedMechAddress = saveUpdateData.mechAddress;
    }
    if (saveUpdateData.weaponCount != 0xFFFFFFFF) {
        fetchDeployedWeaponCount = saveUpdateData.weaponCount;
    }

    for (int i = 0; i < maxWeapons; i++) {
        if (saveUpdateData.weaponAddresses[i] != NULL) {
            fetchDeployedWeaponAddress(i) = saveUpdateData.weaponAddresses[i];
        }
        if (saveUpdateData.socketGroup[i] != 0xFFFFFFFF) {
            fetchDeployedWeaponSocketGroup(i) = saveUpdateData.socketGroup[i];
        }
        if (saveUpdateData.socketID[i] != 0) {
            fetchDeployedWeaponSocketID(i) = saveUpdateData.socketID[i];
        }
        if ((uint64_t)saveUpdateData.socketsStringAddresses[i] != NULL) {
            fetchDeployedWeaponSocketStringAddress(i) = (uint64_t)saveUpdateData.socketsStringAddresses[i];
        }
        ////snprintf((void*)((uint64_t)data + saveWeaponJsonOffset), 64, "%s", saveUpdateData.weaponJSONBufs[i])
        //memcpy((void*)((uint64_t)data + saveWeaponJsonOffset + i * saveWeaponDataSize), saveUpdateData.weaponJSONBufs[i], 64);
        //memcpy((void*)((uint64_t)data + saveWeaponSocketIdOffset + i * saveWeaponDataSize), &saveUpdateData.socketID[i], 4);
        //memcpy((void*)((uint64_t)data + saveWeaponSocketGroupOffset + i * saveWeaponDataSize), &saveUpdateData.socketGroup[i], 4);
        ////snprintf((void*)((uint64_t)data + saveWeaponSocketStringOffset), 64, "%s", saveUpdateData.socketStringBufs[i])
        //memcpy((void*)((uint64_t)data + saveWeaponSocketStringOffset + i * saveWeaponDataSize), saveUpdateData.socketStringBufs[i], 64);
    }
    resetSaveUpdateDataGlobal();
}

asmHook updateGameToNewPlayerResources{
    "updateGameToNewPlayerResourcesV3",
    221,
    120,
    0x6ea6a,
    13,
    NULL,
    1,
    false,
    {{125}, {138}, {176}, {189}},
    { {0xf18d0}, {0x192720}, {0x1311d0}, {0x130440} },
    {{163}},
    {{(uint64_t)updateDeployedMem}}
};

void setNewWeapon(varStruct* vars, upgradeStruct* upgradeState, int weaponIDX) {
    saveUpdateData.weaponAddresses[weaponIDX] = upgradeState->randomizedWeaponAddresses[weaponIDX];

    //Reset weapon state
    vars->shouldResetResources = true;
    vars->shouldKeepPreviousValues = true;
    saveUpdateData.shouldReset = false;
    vars->resourceMemUpdateNecessary = true;
}

int setSaveWeapons(char** mechSocketStrings, uint32_t* mechSocketsWeaponGroup, uint32_t* mechSocketsID, int socketsToHandle, int prevHandledSockets, int defaultSocketsToHandle, char** mechDefaultSocketStrings, uint64_t* mechDefaultSocketWeaponAddresses,  uint64_t primaryAddress, uint64_t secondaryAddress, bool* primaryHandled, bool* secondaryHandled, upgradeStruct* upgradeState) {
    uint64_t curSkippedSockets = 0;
    for (uint64_t i = 0; i + curSkippedSockets < socketsToHandle; i++) {
        DebugBox("s1");
        uint64_t virtuali = i + curSkippedSockets;
        if ((strcmp(mechSocketStrings[virtuali], "upper") == 0 || strcmp(mechSocketStrings[virtuali], "lower") == 0 || strcmp(mechSocketStrings[virtuali], "hull") == 0)) {
            curSkippedSockets += 1;
            i--;
            continue;
        }
        uint64_t addressToSet = 0;
        //Search for a default weapon matching the current socket
        bool defaultMatchHandled = false;
        DebugBox("s2");
        //Only use default for non-primary/secondary weapons. Eg, primary and secondary are persistent across mechs
        bool isPrimary = !*primaryHandled && (weaponGroups)(mechSocketsWeaponGroup[virtuali]) != PrimaryGroup && primaryAddress != NULL;
        bool isSecondary = !*secondaryHandled && (weaponGroups)(mechSocketsWeaponGroup[virtuali]) != SecondaryGroup && secondaryAddress != NULL;
        if(!isPrimary && !isSecondary){
            for (int k = 0; k < defaultSocketsToHandle; k++) {
                if (strcmp(mechSocketStrings[virtuali], mechDefaultSocketStrings[k]) == 0) {
                    //Check if default weapon exists and is valid
                    if (mechDefaultSocketWeaponAddresses[k] && fetchWeaponCapacity(mechDefaultSocketWeaponAddresses[k])>=0) {
                        addressToSet = mechDefaultSocketWeaponAddresses[k];
                        defaultMatchHandled = true;
                    }
                }
            }
        }
        DebugBox("s3");

        //Handle if default weapon was not set by using previous deployed weapon with the correct weapon group
        if (!defaultMatchHandled) {
            weaponGroups nonDefaultweaponGroup = (weaponGroups)(mechSocketsWeaponGroup[virtuali]);
            if (nonDefaultweaponGroup == PrimaryGroup && primaryAddress != NULL && !*primaryHandled) {
                *primaryHandled = true;
                addressToSet = primaryAddress;
            }
            else if (nonDefaultweaponGroup == SecondaryGroup && secondaryAddress != NULL && !*secondaryHandled) {
                *secondaryHandled = true;
                addressToSet = secondaryAddress;
            }
            else {
                DebugBox("Setting random Weapon on what should be a default socket");
                addressToSet = upgradeState->randomizedWeaponAddresses[i];
            }
        }
        DebugBox("s4");
        saveUpdateData.weaponAddresses[i + prevHandledSockets] = addressToSet;
        saveUpdateData.socketsStringAddresses[i + prevHandledSockets] = mechSocketStrings[virtuali];
        saveUpdateData.socketID[i + prevHandledSockets] = mechSocketsID[virtuali];
        saveUpdateData.socketGroup[i + prevHandledSockets] = mechSocketsWeaponGroup[virtuali];
        DebugBox("s5");

    }
    return socketsToHandle - curSkippedSockets;
}

//Updates the globals of the deployed struct to a new mech and reset weapons to use its assigned default weapons
//Must be called before trying to save and reload the mech to get valid values after changing the deployed mech
void setNewMech(varStruct* vars, upgradeStruct* upgradeState, uint64_t newMechAddress) {
    char buffer[256];

    //Set new deployed mech
    saveUpdateData.mechAddress = newMechAddress;
    //snprintf((char*)saveUpdateData.mechJSONBuf, 64, "%s", *(char**)(newMechAddress + resourceJsonOffset));
    //fetchDeployedMechAddress = upgradeState->randomizedMechAddress;
    // 
    uint64_t primaryAddress = NULL;
    if (vars->primaryIndex >= 0) {
        primaryAddress = vars->baseAddresses.weapons[vars->primaryIndex];
    }
    uint64_t secondaryAddress = NULL;
    if (vars->secondaryIndex >= 0) {
        secondaryAddress = vars->baseAddresses.weapons[vars->secondaryIndex];
    }



    //Reset weapon slots
    //for (int i = 0; i < maxWeapons; i++) {
    //    fetchDeployedWeaponAddress(i) = 0;
    //    fetchDeployedWeaponSocketStringAddress(i) = 0;
    //    fetchDeployedWeaponSocketID(i) = 0;
    //    fetchDeployedWeaponSocketGroup(i) = 0xFFFFFFFF;
    //}

    //DebugBox("1");
    uint64_t socketsToHandle = 0;
    int curWeapons = 0;

    uint64_t defaultSocketsToHandle = fetchMechDefaultSocketAmount(newMechAddress);
    char** defaultSocketsString = new char* [defaultSocketsToHandle];
    uint64_t* defaultSocketsWeaponAddress = new uint64_t[defaultSocketsToHandle];
    snprintf(buffer, 100, "DefaultSockets %d", fetchMechDefaultSocketAmount(newMechAddress));
    DebugBox(buffer);
    for (int i = 0; i < defaultSocketsToHandle; i++) {
        defaultSocketsString[i] = fetchMechDefaultSocketString(i, newMechAddress);
        defaultSocketsWeaponAddress[i] = (fetchDeployedMechDefaultSocketWeaponList(i, newMechAddress))[0];
    }

    uint64_t legsSockets = 0;

    if (fetchMechLegsAddress(newMechAddress)) {
        DebugBox("Legs");
        snprintf(buffer, 100, "%I64x", fetchMechLegsAddress(newMechAddress));
        DebugBox(buffer);
        snprintf(buffer, 100, "%d", fetchMechLegsSocketAmount(newMechAddress));
        DebugBox(buffer);
        legsSockets = fetchMechLegsSocketAmount(newMechAddress);
    }
    socketsToHandle += legsSockets;

    uint64_t chassisSockets = 0;

    if (fetchMechChassisAddress(newMechAddress)) {
        DebugBox("Chassis");
        snprintf(buffer, 100, "%I64x", fetchMechChassisAddress(newMechAddress));
        DebugBox(buffer);
        snprintf(buffer, 100, "%d", fetchMechChassisSocketAmount(newMechAddress));
        DebugBox(buffer);
        chassisSockets = fetchMechChassisSocketAmount(newMechAddress);
    }
    socketsToHandle += chassisSockets;

    uint64_t hullSockets = 0;
    if (fetchMechHullAddress(newMechAddress)) {
        DebugBox("Hull");
        snprintf(buffer, 100, "%I64x", fetchMechHullAddress(newMechAddress));
        DebugBox(buffer);
        snprintf(buffer, 100, "%d", fetchMechHullSocketAmount(newMechAddress));
        DebugBox(buffer);
        hullSockets = fetchMechHullSocketAmount(newMechAddress);
    }
    socketsToHandle += hullSockets;

    DebugBox("0");

    char** legsSocketsString = new char*[legsSockets];
    DebugBox("0.1");
    uint32_t* legsSocketsWeaponGroup = new uint32_t[legsSockets];
    DebugBox("0.2");
    uint32_t* legsSocketsId = new uint32_t[legsSockets];
    DebugBox("0.3");
    for (int i = 0; i < legsSockets; i++) {
        DebugBox("0.4");
        legsSocketsString[i] = fetchMechLegsSocketString(i, newMechAddress);
        legsSocketsWeaponGroup[i] = fetchMechLegsSocketWeaponGroup(i, newMechAddress);
        legsSocketsId[i] = fetchMechLegsSocketID(i, newMechAddress);
        DebugBox("0.5");

    }
    DebugBox("1");
    bool primaryHandled = false;
    bool secondaryHandled = false;
    curWeapons += setSaveWeapons(legsSocketsString, legsSocketsWeaponGroup, legsSocketsId, legsSockets, curWeapons, defaultSocketsToHandle, defaultSocketsString, defaultSocketsWeaponAddress, primaryAddress, secondaryAddress, &primaryHandled, &secondaryHandled, upgradeState);
    delete[] legsSocketsString;
    delete[] legsSocketsWeaponGroup;
    delete[] legsSocketsId;
    DebugBox("SetSaveWeapons");

    char** chassisSocketsString = new char* [chassisSockets];
    uint32_t* chassisSocketsWeaponGroup = new uint32_t[chassisSockets];
    uint32_t* chassisSocketsId = new uint32_t[chassisSockets];
    for (int i = 0; i < chassisSockets; i++) {
        chassisSocketsString[i] = fetchMechChassisSocketString(i, newMechAddress);
        chassisSocketsWeaponGroup[i] = fetchMechChassisSocketWeaponGroup(i, newMechAddress);
        chassisSocketsId[i] = fetchMechChassisSocketID(i, newMechAddress);
    }
    DebugBox("2");
    curWeapons += setSaveWeapons(chassisSocketsString, chassisSocketsWeaponGroup, chassisSocketsId, chassisSockets, curWeapons, defaultSocketsToHandle, defaultSocketsString, defaultSocketsWeaponAddress, primaryAddress, secondaryAddress, &primaryHandled, &secondaryHandled, upgradeState);
    DebugBox("SetSaveWeapons");
    delete[] chassisSocketsString;
    delete[] chassisSocketsWeaponGroup;
    delete[] chassisSocketsId;

    char** hullSocketsString = new char*[hullSockets];
    uint32_t* hullSocketsWeaponGroup = new uint32_t[hullSockets];
    uint32_t* hullSocketsId = new uint32_t[hullSockets];
    for (int i = 0; i < hullSockets; i++) {
        hullSocketsString[i] = fetchMechHullSocketString(i, newMechAddress);
        hullSocketsWeaponGroup[i] = fetchMechHullSocketWeaponGroup(i, newMechAddress);
        hullSocketsId[i] = fetchMechHullSocketID(i, newMechAddress);
    }
    DebugBox("3");
    curWeapons += setSaveWeapons(hullSocketsString, hullSocketsWeaponGroup, hullSocketsId, hullSockets, curWeapons, defaultSocketsToHandle, defaultSocketsString, defaultSocketsWeaponAddress, primaryAddress, secondaryAddress, &primaryHandled, &secondaryHandled, upgradeState);
    DebugBox("SetSaveWeapons");
    delete[] hullSocketsString;
    delete[] hullSocketsWeaponGroup;
    delete[] hullSocketsId;

    DebugBox("4");

    //Set amount of deployed weapons to number of sockets on chassis + legs discounting mech parts. 
    saveUpdateData.weaponCount = curWeapons;
    //fetchDeployedWeaponCount = socketsToHandle - skippedChassisSockets - skippedLegSockets - skippedHullSockets;
    snprintf(buffer, 100, "Deployed weapons %d", curWeapons);
    DebugBox(buffer);

    //Reset weapon state
    vars->shouldResetResources = true;
    vars->shouldKeepPreviousValues = true;
    saveUpdateData.shouldReset = true;
    vars->resourceMemUpdateNecessary = true;
}

double getMoney() {
    double* dVar2 = (double*)(moneyBase + 0xa4b8);
    double* dVar1 = (double*)(moneyBase + 0xa4c0);
    if (*dVar2 <= 0.0) {
        *dVar2 = 0.0;
    }
    double* dVar4 = (double*)(moneyBase + 0xa4c8);
    if (*dVar1 <= 0.0) {
        *dVar1 = 0.0;
    }
    if (*dVar4 <= 0.0) {
        *dVar4 = 0.0;
    }
    double* dVar3 = (double*)(moneyBase + 0xa4d0);
    if (*dVar3 <= 0.0) {
        *dVar3 = 0.0;
    }

    return *dVar1 + *dVar2 + *dVar4 + *dVar3;
}

void addMoney(double amount) {
    *(double*)(moneyBase + 0xa4b8) += amount;
}

//Returns True on valid and successful subtraction of in run money.
bool subtractMoney(double subtractAmount) {
    double* dVar2 = (double*)(moneyBase + 0xa4b8);
    double* dVar1 = (double*)(moneyBase + 0xa4c0);
    if (*dVar2 <= 0.0) {
        *dVar2 = 0.0;
    }
    double* dVar4 = (double*)(moneyBase + 0xa4c8);
    if (*dVar1 <= 0.0) {
        *dVar1 = 0.0;
    }
    if (*dVar4 <= 0.0) {
        *dVar4 = 0.0;
    }
    double* dVar3 = (double*)(moneyBase + 0xa4d0);
    if (*dVar3 <= 0.0) {
        *dVar3 = 0.0;
    }

    double currentMoney = *dVar1 + *dVar2 + *dVar4 + *dVar3;
    if (currentMoney >= subtractAmount) {
        if (*dVar1 >= subtractAmount) {
            *dVar1 -= subtractAmount;
            return true;
        }
        else {
            subtractAmount -= *dVar1;
            *dVar1 = 0;
        }

        if (*dVar2 >= subtractAmount) {
            *dVar2 -= subtractAmount;
            return true;
        }
        else {
            subtractAmount -= *dVar2;
            *dVar2 = 0;
        }

        if (*dVar3 >= subtractAmount) {
            *dVar3 -= subtractAmount;
            return true;
        }
        else {
            subtractAmount -= *dVar3;
            *dVar3 = 0;
        }

        if (*dVar4 >= subtractAmount) {
            *dVar4 -= subtractAmount;
            return true;
        }
        else {
            subtractAmount -= *dVar4;
            *dVar4 = 0;
        }
        return true;
    }
    else {
        return false;
    }
}

//Function created by combining functions brigador.exe+0x1542f0 and brigador.exe+0x16580
uint64_t getPlayerAddress() {
    //Check for uninitialized player
    if ((*(uint64_t*)keyAddress + offsetUsedToFetchPlayerAddress) != NULL) {
        long long* entry = (long long*)*(uint64_t*)(*(uint64_t*)keyAddress + offsetUsedToFetchPlayerAddress);
        //Check for uninitialized player
        if (entry == NULL || (long long**)(*entry + 0x1b8c8) == NULL || (int**)(*entry + 0x1b8f8) == NULL) {
            return 0;
        }

        long long* param_1 = *(long long**)(*entry + 0x1b8c8);

        int param_2 = 0;
        int* param_3 = *(int**)(*entry + 0x1b8f8);


        long long lVar1;
        long long lVar2;
        unsigned long long uVar3;
        unsigned long long uVar4;

        uVar3 = (unsigned long long) * param_3;
        if (*param_3 == param_2) {
            lVar1 = *param_1;
            if (uVar3 < (unsigned long long)((param_1[1] - lVar1) / 0x30)) {
                uVar4 = (unsigned long long)param_3[1];
                lVar2 = *(long long*)(lVar1 + uVar3 * 0x30);
                if (((uVar4 < (unsigned long long)(*(long long*)(lVar1 + 8 + uVar3 * 0x30) - lVar2 >> 4)) &&
                    (*(int*)(lVar2 + uVar4 * 0x10) == param_3[2])) &&
                    (*(long long*)(lVar2 + 8 + uVar4 * 0x10) != 0)) {
                    //DebugBox("Found Player Address");
                    return *(uint64_t*)(lVar2 + 8 + uVar4 * 0x10);
                }
            }
        }
    }
    //DebugBox("Could not find player address");
    return 0;
}

#define getPlayerHealthAddress ((*(uint64_t*)(getPlayerAddress()+0x2b8) + 0x78)+0x4)
float getPlayerHealth() {
    uint64_t playerAddress = getPlayerAddress();
    if (playerAddress != NULL) {
        return *(float*)getPlayerHealthAddress;
    }
    return 0;
}

//Function should result in repair amount always being added safely while keeping track of current in-game player health. 
void handlePlayerHealth(upgradeStruct* upgradeState) {
    if (fetchCurrentState == InGame && upgradeState->repairAmount != 0 && getPlayerHealth() != 0) {
        if (getPlayerHealth() + upgradeState->repairAmount >= upgradeState->maxHealth) {
            *(float*)getPlayerHealthAddress = upgradeState->maxHealth;
        }
        else {
            *(float*)getPlayerHealthAddress += upgradeState->repairAmount;
        }
        upgradeState->repairAmount = 0;
    }
}

double getUpgradeCost(buttons buttonToHandle, upgradeStruct* upgradeState) {
    return upgradeState->consumed < upgradeState->freeUpgradesPerLevel ? 0 : upgradeState->upgradesCost[buttonToHandle] * pow(5, (upgradeState->consumed- upgradeState->freeUpgradesPerLevel));
}


//Formats the strings in addedButtonStrings to be used when displaying buttons
//Should be called after any on the variables used to format the strings are altered.
void formatButtonStrings(upgradeStruct* upgradeState) {
    char buffer[256];
    for (int i = 0; i < addedButtons; i++) {
        switch ((buttons)i) {
        case M_Repair:
            snprintf(upgradeState->formattedButtonStrings[i], 
                maxButtonStringLength, 
                addedButtonStrings[i], 
                repairHealthPoints, 
                (long long)(getUpgradeCost((buttons)i, upgradeState)/1000));
            break;
        case M_PosOvercharge:
            snprintf(upgradeState->formattedButtonStrings[i],
                maxButtonStringLength,
                addedButtonStrings[i],
                overchargeMult,
                (long long)(getUpgradeCost((buttons)i, upgradeState) / 1000));
            break;
        case M_PosForwardSpeed:
            snprintf(upgradeState->formattedButtonStrings[i],
                maxButtonStringLength,
                addedButtonStrings[i],
                forwardSpeedMult,
                (long long)(getUpgradeCost((buttons)i, upgradeState) / 1000));
            break;
        case P_PosCapacity: // Primary: +Capacity
            snprintf(upgradeState->formattedButtonStrings[i],
                maxButtonStringLength,
                addedButtonStrings[i],
                pCapacityMult,
                (long long)(getUpgradeCost((buttons)i, upgradeState) / 1000));
            break;
        case P_PosFireRate: // Primary: +Fire Rate
            snprintf(upgradeState->formattedButtonStrings[i],
                maxButtonStringLength,
                addedButtonStrings[i],
                pFireRateMult,
                (long long)(getUpgradeCost((buttons)i, upgradeState) / 1000));
            break;
        case P_PosProjectilesNegAccuracy: // Primary: +Projectiles, -Accuracy
            snprintf(upgradeState->formattedButtonStrings[i],
                maxButtonStringLength,
                addedButtonStrings[i],
                pProjectiles,
                pAccuracy,
                (long long)(getUpgradeCost((buttons)i, upgradeState) / 1000));
            break;
        case P_PosPropMult: // Primary: +Structure Damage
            snprintf(upgradeState->formattedButtonStrings[i],
                maxButtonStringLength,
                addedButtonStrings[i],
                pPropMult,
                (long long)(getUpgradeCost((buttons)i, upgradeState) / 1000));
            break;
        case S_PosCapacity: // Secondary: +Capacity
            snprintf(upgradeState->formattedButtonStrings[i],
                maxButtonStringLength,
                addedButtonStrings[i],
                sCapacityMult,
                (long long)(getUpgradeCost((buttons)i, upgradeState) / 1000));
            break;
        case S_PosFireRate: // Secondary: +Fire Rate
            snprintf(upgradeState->formattedButtonStrings[i],
                maxButtonStringLength,
                addedButtonStrings[i],
                sFireRateMult,
                (long long)(getUpgradeCost((buttons)i, upgradeState) / 1000));
            break;
        case S_PosProjectilesNegAccuracy: // Secondary: +Projectiles, -Accuracy
            snprintf(upgradeState->formattedButtonStrings[i],
                maxButtonStringLength,
                addedButtonStrings[i],
                sProjectiles,
                sAccuracy,
                (long long)(getUpgradeCost((buttons)i, upgradeState) / 1000));
            break;
        case S_PosPropMult: // Secondary: +Structure Damage
            snprintf(upgradeState->formattedButtonStrings[i],
                maxButtonStringLength,
                addedButtonStrings[i],
                sPropMult,
                (long long)(getUpgradeCost((buttons)i, upgradeState) / 1000));
            break;
        case RandomMech: // Random Mech purchase
            snprintf(upgradeState->formattedButtonStrings[i],
                maxButtonStringLength,
                addedButtonStrings[i],
                text_lookup((char*)(upgradeState->randomizedMechAddress + mechNameOffset)),
                (getUpgradeCost((buttons)i, upgradeState) / (double)1000000));
            break;
        case RandomPrimary:
            snprintf(upgradeState->formattedButtonStrings[i],
                maxButtonStringLength,
                addedButtonStrings[i],
                text_lookup((char*)(upgradeState->randomizedWeaponAddresses[getWeaponIndexOfWeaponGroup(PrimaryGroup)] + weaponNameOffset)),
                (getUpgradeCost((buttons)i, upgradeState) / (double)1000000));
            break;
        case RandomSecondary:
            snprintf(upgradeState->formattedButtonStrings[i],
                maxButtonStringLength,
                addedButtonStrings[i],
                text_lookup((char*)(upgradeState->randomizedWeaponAddresses[getWeaponIndexOfWeaponGroup(SecondaryGroup)] + weaponNameOffset)),
                (getUpgradeCost((buttons)i, upgradeState) / (double)1000000));
            break;
        default:
            DebugBox("Error: Undefined button tried to be formatted. ");
            return;
        }
    }
}

//Updates the list with available upgrades and the state.
//Should be called each time a button has been used or availableUpgrades has been updated
void updateAvailableUpgrades(upgradeStruct* upgradeState) {
    formatButtonStrings(upgradeState);
    for (int i = 0; i < upgradeState->availableCount - upgradeState->removedUpgrades; i++) {
        if (getUpgradeCost(upgradeState->availableUpgrades->buttonIndex[i], upgradeState) > getMoney()) {
            for (int n = i + 1; n < upgradeState->availableCount - upgradeState->removedUpgrades; n++) {
                upgradeState->availableUpgrades->upgradeText[n - 1] = upgradeState->availableUpgrades->upgradeText[n];
                upgradeState->availableUpgrades->buttonIndex[n - 1] = upgradeState->availableUpgrades->buttonIndex[n];
            }
            i--;
            upgradeState->removedUpgrades++;
        }
    }
    writeBytesToDeployedAsm(&addButtonsChooseDistrict, (upgradeState->availableCount-upgradeState->removedUpgrades), 10, 8);
}

//Randomizes the list of upgrade buttons that should be shown
void setupUpgradeList(upgradeStruct* upgradeState, varStruct* vars, rngStruct* rng) {
    char buffer[256];
    //Randomize once
    if (upgradeState->randomizeUpgrades && fetchCurrentState == FreelancerChooseDistrict) {
        uint32_t mechOffsetList[1] = { 0xE };
        setupResourceList(&upgradeState->mechResources, fetchResourceDictAddress, mechOffsetList, 1);
        snprintf(buffer, 100, "Mech resources = %d", upgradeState->mechResources.len);
        DebugBox(buffer);

        uint32_t weaponOffsetList[3] = { 0x7, 0x8, 0x9 };
        setupResourceList(&upgradeState->weaponResources, fetchResourceDictAddress, weaponOffsetList, 3); //0x0000000800000007, 1);
        snprintf(buffer, 100, "Weapon resources = %d", upgradeState->weaponResources.len);
        DebugBox(buffer);

        //Setup always available upgrades
        for (int n = 0; n < upgradeState->alwaysAvailableCount; n++) {
            upgradeState->availableUpgrades->buttonIndex[n] = upgradeState->alwaysAvailableButtons[n];
            upgradeState->availableUpgrades->upgradeText[n] = (char*)upgradeState->formattedButtonStrings[upgradeState->alwaysAvailableButtons[n]];
        }

        //Generate unique random numbers and setup upgradeList
        for (int n = upgradeState->alwaysAvailableCount; n < upgradeState->availableCount; n++) {
            upgradeState->availableUpgrades->buttonIndex[n] = (buttons)rng->distrib(rng->gen);
            upgradeState->availableUpgrades->upgradeText[n] = (char*)upgradeState->formattedButtonStrings[upgradeState->availableUpgrades->buttonIndex[n]];
            //Ensure the number is unique
            bool unique = true;
            for (int i = n - 1; i > -1; i--) {
                if (upgradeState->availableUpgrades->buttonIndex[n] == upgradeState->availableUpgrades->buttonIndex[i]) {
                    unique = false;
                    break;
                }
            }
            if (!unique) {
                n--;
            }
        }

        DebugBox("Start randomizing all parts");

        //Set new randomized resources (mech and weapons)
        setRandomPartsAddresses(upgradeState, rng);
        DebugBox("Randomized all parts");

        snprintf(buffer, 100, "Random mech address = %I64x", upgradeState->randomizedMechAddress);
        DebugBox(buffer);
        upgradeState->upgradesCost[RandomMech] = *(double*)(upgradeState->randomizedMechAddress + mechCostOffset)/4;
        
        if (vars->primaryIndex >= 0) {
            snprintf(buffer, 100, "Random primary address = %I64x", upgradeState->randomizedWeaponAddresses[vars->primaryIndex]);
            DebugBox(buffer);
            upgradeState->upgradesCost[RandomPrimary] = *(double*)(upgradeState->randomizedWeaponAddresses[vars->primaryIndex] + weaponCostOffset)/4;
        }
        if (vars->secondaryIndex >= 0) {
            snprintf(buffer, 100, "Random secondary address = %I64x", upgradeState->randomizedWeaponAddresses[vars->secondaryIndex]);
            DebugBox(buffer);
            upgradeState->upgradesCost[RandomSecondary] = *(double*)(upgradeState->randomizedWeaponAddresses[vars->secondaryIndex] + weaponCostOffset)/4;
        }
        

        DebugBox("Set new costs according to parts");

        //Reset state
        upgradeState->randomizeUpgrades = false;
        updateAvailableUpgrades(upgradeState);
    }
    //Prepare to randomize when player next enters the choose district menu.
    else if (!upgradeState->randomizeUpgrades && fetchCurrentState != FreelancerChooseDistrict) {
        upgradeState->consumed = 0;
        upgradeState->removedUpgrades = 0;
        upgradeState->randomizeUpgrades = true;
    }
}

//Logic for upgrade / button press handling
void handlePressedButton(buttons buttonToHandle, upgradeStruct* upgradeState, varStruct* vars, rngStruct* rng) {
    char buffer[256];

    float refloat;
    if (subtractMoney(getUpgradeCost(buttonToHandle, upgradeState))) {
        upgradeState->consumed++;
        switch (buttonToHandle) {
        case M_Repair:
            upgradeState->repairAmount += repairHealthPoints;
            upgradeState->maxHealth = *(float*)&vars->offsetsNVals.mech[MaxHealthIdx].val;
            break;
        case M_PosOvercharge:
            refloat = reinterpret_cast<float&>(vars->offsetsNVals.mech[MaxOverchargeIdx].org);
            refloat *= overchargeMult-1;
            refloat += reinterpret_cast<float&>(vars->offsetsNVals.mech[MaxOverchargeIdx].val);
            vars->offsetsNVals.mech[MaxOverchargeIdx].val = reinterpret_cast<uint32_t&>(refloat);
            break;
        case M_PosForwardSpeed:
            refloat = reinterpret_cast<float&>(vars->offsetsNVals.mechLegs[MaxForwardSpeedIdx].org);
            refloat *= forwardSpeedMult-1;
            refloat += reinterpret_cast<float&>(vars->offsetsNVals.mechLegs[MaxForwardSpeedIdx].val);
            vars->offsetsNVals.mechLegs[MaxForwardSpeedIdx].val = reinterpret_cast<uint32_t&>(refloat);
            break;
        case P_PosCapacity: // Primary: +Capacity
            vars->offsetsNVals.weapons[vars->primaryIndex][CapacityIdx].val += (int32_t)((float)(int32_t)vars->offsetsNVals.weapons[vars->primaryIndex][CapacityIdx].org * (pCapacityMult-1));
            break;
        case P_PosFireRate: // Primary: +Fire Rate
            refloat = reinterpret_cast<float&>(vars->offsetsNVals.weapons[vars->primaryIndex][CooldownIdx].val);
            refloat *= 1 - pFireRateMult;
            refloat += reinterpret_cast<float&>(vars->offsetsNVals.weapons[vars->primaryIndex][CooldownIdx].val);
            vars->offsetsNVals.weapons[vars->primaryIndex][CooldownIdx].val = reinterpret_cast<uint32_t&>(refloat);
            break;
        case P_PosProjectilesNegAccuracy: // Primary: +Projectiles, -Accuracy
            vars->offsetsNVals.weapons[vars->primaryIndex][ShotCountIdx].val += pProjectiles;
            refloat = reinterpret_cast<float&>(vars->offsetsNVals.weapons[vars->primaryIndex][AccuracyIdx].val);
            refloat += pAccuracy*0.017; //+degrees
            vars->offsetsNVals.weapons[vars->primaryIndex][AccuracyIdx].val = reinterpret_cast<uint32_t&>(refloat);
            break;
        case P_PosPropMult: // Primary: +Structure Damage
            refloat = reinterpret_cast<float&>(vars->offsetsNVals.bullets[vars->primaryIndex][PropMultIdx].org);
            refloat *= pPropMult-1;
            refloat += reinterpret_cast<float&>(vars->offsetsNVals.bullets[vars->primaryIndex][PropMultIdx].val);
            vars->offsetsNVals.bullets[vars->primaryIndex][PropMultIdx].val += reinterpret_cast<uint32_t&>(refloat);
            break;
        case S_PosCapacity: // Secondary: +Capacity
            vars->offsetsNVals.weapons[vars->secondaryIndex][CapacityIdx].val += (int32_t)((float)(int32_t)vars->offsetsNVals.weapons[vars->secondaryIndex][CapacityIdx].org * (sCapacityMult-1));
            break;
        case S_PosFireRate: // Secondary: +Fire Rate
            refloat = reinterpret_cast<float&>(vars->offsetsNVals.weapons[vars->secondaryIndex][CooldownIdx].val);
            refloat *= 1-sFireRateMult;
            refloat += reinterpret_cast<float&>(vars->offsetsNVals.weapons[vars->secondaryIndex][CooldownIdx].val);
            vars->offsetsNVals.weapons[vars->secondaryIndex][CooldownIdx].val -= reinterpret_cast<uint32_t&>(refloat);
            break;
        case S_PosProjectilesNegAccuracy: // Secondary: +Projectiles, -Accuracy
            vars->offsetsNVals.weapons[vars->secondaryIndex][ShotCountIdx].val += pProjectiles;
            refloat = reinterpret_cast<float&>(vars->offsetsNVals.weapons[vars->secondaryIndex][AccuracyIdx].val);
            refloat += sAccuracy*0.017; //+degrees.
            vars->offsetsNVals.weapons[vars->secondaryIndex][AccuracyIdx].val = reinterpret_cast<uint32_t&>(refloat);
            break;
        case S_PosPropMult: // Secondary: +Structure Damage
            refloat = reinterpret_cast<float&>(vars->offsetsNVals.bullets[vars->secondaryIndex][PropMultIdx].org);
            refloat *= sPropMult-1;
            refloat += reinterpret_cast<float&>(vars->offsetsNVals.bullets[vars->secondaryIndex][PropMultIdx].val);
            vars->offsetsNVals.bullets[vars->secondaryIndex][PropMultIdx].val += reinterpret_cast<uint32_t&>(refloat);
            break;
        case RandomMech:
            DebugBox("Set");
            setNewMech(vars, upgradeState, upgradeState->randomizedMechAddress);
            DebugBox("Setted");
            break;
        case RandomPrimary:
            DebugBox("Set");
            setNewWeapon(vars, upgradeState, vars->primaryIndex);
            DebugBox("Setted");
            break;
        case RandomSecondary:
            DebugBox("Set");
            setNewWeapon(vars, upgradeState, vars->secondaryIndex);
            DebugBox("Setted");
            break;
        default:
            DebugBox("Error: Undefined button pressed. ");
            return;
        }
        upgradeState->randomizeUpgrades = true;
        setupUpgradeList(upgradeState, vars, rng);
        DebugBox("setupUpgradeList");

    }
}


//Handles a player's action on the choose district menu in freelancer by observing the state of the currently selected button
void handleChooseDistrictMenu(upgradeStruct* upgradeState, varStruct* vars, rngStruct* rng) {
    if (addButtonsChooseDistrict.isDeployed && fetchCurrentState == FreelancerChooseDistrict) {
        uint64_t chooseDistrictMenuStruct = (*(uint64_t*)(*(uint64_t*)keyAddress + stateStructOffset) + 0x128 + 0x3e * 0x88);
        uint32_t chooseDistrictMenuIndex = (*(uint32_t*)chooseDistrictMenuStruct) & 0xffff;   

        //Fetch which button is selected and should be handled
        uint64_t selectedDistrictItemAddress = *(uint64_t*)(*(uint64_t*)(*(uint64_t*)keyAddress + stateStructOffset) + 0x128 + (0x3e * 0x88) + 0x18);

        //Unknown (and therefore explicitly injected/ modded) button handling
        if (selectedDistrictItemAddress != NULL && *(uint64_t*)selectedDistrictItemAddress == NULL) {
            //Find closest original item in district button array to conclude which added
            // button is pressed and should be handled. Done by going backwards from selected 
            // button and seing how many added buttons we actually have to pass through before
            // we get to an original button.
            uint64_t closestOriginalDistrictItemAddress = selectedDistrictItemAddress;
            int distanceFromOriginalItem = 0;
            while (*(uint64_t*)closestOriginalDistrictItemAddress == NULL) {
                distanceFromOriginalItem++;
                closestOriginalDistrictItemAddress = (selectedDistrictItemAddress - (distanceFromOriginalItem * districtItemSize));
            }

            buttons buttonToHandle = upgradeState->availableUpgrades->buttonIndex[distanceFromOriginalItem - 1];

            //Handle the pressed mod button
            handlePressedButton(buttonToHandle, upgradeState, vars, rng);
            //if (vars->resourceMemUpdateNecessary) {
            //
                DebugBox("handlePressedButton");
            //}
            //Update weapon vars in memory
            updateResources(vars);
            //if (vars->resourceMemUpdateNecessary) {
            //
                DebugBox("updateResources");
            //}
            //Reset selected button to index 0 
            memset((void*)chooseDistrictMenuStruct, 0x0, 2);

            if (vars->resourceMemUpdateNecessary) {
                DebugBox("updateGameToNewPlayerResources");
                writeBytesToDeployedAsm(&updateGameToNewPlayerResources, 0x1, 0, 1);
                while (!readByteFromDeployedAsm(&updateGameToNewPlayerResources, 0)) {
                    Sleep(100);
                }
                vars->resourceMemUpdateNecessary = false;
            }
        } 
    }
    return;
}

bool shouldExit = false;

DWORD WINAPI MainThread(LPVOID param) {
    char buffer[256];
    resetSaveUpdateDataGlobal();


    uint64_t weaponBaseAddresses[maxWeapons] = { 0 };
    uint64_t bulletBaseAddresses[maxWeapons] = { 0 };

    //Setup variables struct
    varBaseAddresses deployedBaseAddresses{ 0, 0, weaponBaseAddresses, bulletBaseAddresses};

    offsetsAndVals deployedOffsetsAndVals{
        deployedMechOffsetsAndVals,
        deployedMechLegsOffsetsAndVals,
        deployedWeaponsOffsetsAndVals,
        deployedBulletsOffsetsAndVals
    };

    varStruct variablesStruct{ deployedBaseAddresses, deployedOffsetsAndVals, -1, -1, false, false, false};

    //Repair is set to always show up
    const buttons alwaysAvailableUpgradesButtons[] = { RandomMech };

    //Setup buffers for formatted strings to be shown by added buttons
    char formattedButtonStrings[addedButtons][maxButtonStringLength];

    //Setup list containing resources we wish to have access to.
    uint64_t* mechAddressList = new uint64_t[arbitraryResourceCount];
    resourceList mechList = { mechAddressList, 0, arbitraryResourceCount };
    uint64_t* weaponAddressList = new uint64_t[arbitraryResourceCount];
    resourceList weaponList = { weaponAddressList, 0, arbitraryResourceCount };

    //Setup upgrade state struct
    upgradeStruct upgradeState{
        .availableUpgrades = &upgradeList,
        .formattedButtonStrings = formattedButtonStrings,
        .upgradesCost = baseButtonCosts,
        .alwaysAvailableCount = sizeof(alwaysAvailableUpgradesButtons) / sizeof(alwaysAvailableUpgradesButtons[0]),
        .alwaysAvailableButtons = alwaysAvailableUpgradesButtons,
        .availableCount = 4,
        .freeUpgradesPerLevel = 0,
        .consumeMax = 5,
        .consumed = 0,
        .removedUpgrades = 0,
        .randomizeUpgrades = true,
        .repairAmount = 0,
        .maxHealth = 0,
        .randomizedMechAddress = 0,
        .randomizedWeaponAddresses = {0,0,0,0,0,0,0,0,0},
        .mechResources = mechList,
        .weaponResources = weaponList,
        .saveUpt = &saveUpdateData
    };

    //Setup rng
    std::random_device rd;  // a seed source for the random number engine
    std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<> distrib(0, addedButtons - 1);

    rngStruct rng{ gen, distrib };

    while (!shouldExit) {
        //Uncomment this and the unsuspend at the end of the loop if race conditions cause issues
        //_SetOtherThreadsSuspended(true);
        
        
        if (GetAsyncKeyState(VK_NUMPAD1) & 0x80000) {
            uint32_t mechOffsetList[1] = { 0xE };
            setupResourceList(&upgradeState.mechResources, fetchResourceDictAddress, mechOffsetList, 1);
            addMoney(1000000);
            snprintf(buffer, 100, "freelancerMenuState = %llx", (uint32_t)fetchFreelancerMenuState);
            DebugBox(buffer);
            snprintf(buffer, 100, "playerAddress = %#016x", getPlayerAddress());
            DebugBox(buffer);
            if (getPlayerHealth() != 0) {
                snprintf(buffer, 100, "playerHealthAddress = %#016x", getPlayerHealthAddress);
                DebugBox(buffer);
                snprintf(buffer, 100, "playerHealth = %f", *(float*)getPlayerHealthAddress);
                DebugBox(buffer);
                *(float*)getPlayerHealthAddress += 100;
            }
        }
        

        //Update playerHealth to the modded value
        handlePlayerHealth(&upgradeState);

        //Reset modded weapon vars if certain game states are observed.
        resetResources(&variablesStruct);

        //Handle logic in freelancer menu
        //handleFreelancerMenu();

        //Randomize the list of available upgrades
        setupUpgradeList(&upgradeState, &variablesStruct, &rng);

        //Handle logic in freelancer choose district menu
        handleChooseDistrictMenu(&upgradeState, &variablesStruct, &rng);

        //_SetOtherThreadsSuspended(false);

        Sleep(100);
    }

    delete[] upgradeState.mechResources.addresses;
    delete[] upgradeState.weaponResources.addresses;
    return 0;
}

void applyPatches(void) {
    _SetOtherThreadsSuspended(true);

    deployExecutableASM(&addButtonsChooseDistrict);
    uint64_t nextStringToPrintAddress = (addButtonsChooseDistrict.hookTarget + (addButtonsChooseDistrict.fileSize - addButtonsChooseDistrict.bytesToStrip) - addButtonsChooseDistrict.numberOfWritableBytes + 18);
    createUIButtonUseSetString.externalReplacementValues[0] = nextStringToPrintAddress;
    deployExecutableASM(&createUIButtonUseSetString);
    deployExecutableASM(&updateGameToNewPlayerResources);
    //DebugBox("Patches injected", "Patches injected", MB_OK);

    _SetOtherThreadsSuspended(false);
}

void freePatches(void) {
    VirtualFree((LPVOID)addButtonsChooseDistrict.hookTarget, 0, MEM_RELEASE);
    VirtualFree((LPVOID)createUIButtonUseSetString.hookTarget, 0, MEM_RELEASE);
    VirtualFree((LPVOID)updateGameToNewPlayerResources.hookTarget, 0, MEM_RELEASE);
}

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        baseModule = GetBaseModuleForProcess();
        //DebugBox("DLL injected", "DLL injected", MB_OK);
        applyPatches();
        CreateThread(0, 0, MainThread, hModule, 0, 0);
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        //VirtualProtect(executable_area, asms->numberOfBytes, old_protect, &old_protect);
        //VirtualFree(executable_area, asms->numberOfBytes, MEM_RELEASE);
        freePatches();
        shouldExit = true;
        _SetOtherThreadsSuspended(false);
        break;
    }
    return TRUE;
}

