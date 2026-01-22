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
#include "BrigadorRogue.h"

using namespace std;

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
};

//Must correspond to buttons enum
const char* addedButtonStrings[] = {
    {"Repair +%dhp $%lld k"}
    , {"Overcharge +%.1fx $%lld k"}
    , {"Forward Speed +%.1fx $%lld k"}
    , {"P: Capacity +%.1fx $%lld k"}
    , {"P: Fire Rate +%.1fx $%lld k"}
    , {"P: Projectiles +%d Accuracy -%d $%lld k"}
    , {"P: Structure Damage +%.1f%% $%lld k"}
    , {"S: Capacity +%.1fx $%lld k"}
    , {"S: Fire Rate +%.1fx $%lld k"}
    , {"S: Projectiles +%d Accuracy -%d $%lld k"}
    , {"S: Structure +%.1fx $%lld k"}
};
#define addedButtons sizeof(addedButtonStrings) / sizeof(addedButtonStrings[0])

//Cost relate to the button with the same index in addedButtonStrings
const double baseButtonCosts[addedButtons]{
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
};

//Values used to determine upgrade magnitude
#define repairHealthPoints 50
#define overchargeMult 1.1
#define forwardSpeedMult 1.1
#define pCapacityMult 1.1
#define sCapacityMult 1.1
#define pFireRateMult 1.1
#define sFireRateMult 1.1
#define pProjectiles 1
#define pAccuracy 5
#define sProjectiles 1
#define sAccuracy 5
#define pPropMult 1.1
#define sPropMult 1.1

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

//Weapons
const enum weaponVarIdx {
    CapacityIdx,
    CooldownIdx,
    ShotCountIdx,
    AccuracyIdx
};

uthruple deployedPrimaryWeaponOffsetsAndVals[] = {
    {weaponVarsOffset + weaponCapacityOffset, 0x0, 0x0} //Capacity
    ,{weaponVarsOffset + weaponCooldownOffset, 0x0, 0x0} //Cooldown
    ,{weaponVarsOffset + weaponShotCountOffset, 0x0, 0x0} //Shot Count
    ,{weaponVarsOffset + weaponAccuracyOffset, 0x0, 0x0} //Accuracy Cone Width
};
#define primaryWeaponVars sizeof(deployedPrimaryWeaponOffsetsAndVals) / sizeof(deployedPrimaryWeaponOffsetsAndVals[0])

uthruple deployedSecondaryWeaponOffsetsAndVals[] = {
    {weaponVarsOffset + weaponCapacityOffset, 0x0, 0x0} //Capacity
    ,{weaponVarsOffset + weaponCooldownOffset, 0x0, 0x0} //Cooldown
    ,{weaponVarsOffset + weaponShotCountOffset, 0x0, 0x0} //Shot Count
    ,{weaponVarsOffset + weaponAccuracyOffset, 0x0, 0x0} //Accuracy Cone Width
};
#define secondaryWeaponVars sizeof(deployedSecondaryWeaponOffsetsAndVals) / sizeof(deployedSecondaryWeaponOffsetsAndVals[0])

//Bullets
const enum bulletVars {
    PropMultIdx
};

uthruple deployedPrimaryBulletOffsetsAndVals[] = {
    {bulletPropMultOffset, 0x0, 0x0} //prop multiplier
};
#define primaryBulletVars sizeof(deployedPrimaryBulletOffsetsAndVals) / sizeof(deployedPrimaryBulletOffsetsAndVals[0])

uthruple deployedSecondaryBulletOffsetsAndVals[] = {
    {bulletPropMultOffset, 0x0, 0x0} //prop multiplier
};
#define secondaryBulletVars sizeof(deployedSecondaryBulletOffsetsAndVals) / sizeof(deployedSecondaryBulletOffsetsAndVals[0])

struct varBaseAddresses {
    uint64_t mech;
    uint64_t mechLegs;
    uint64_t primary;
    uint64_t secondary;
    uint64_t primaryBullet;
    uint64_t secondaryBullet;
};

struct offsetsAndVals {
    uthruple* mech;
    uthruple* mechLegs;
    uthruple* primary;
    uthruple* secondary;
    uthruple* primaryBullet;
    uthruple* secondaryBullet;
};

struct varStruct {
    varBaseAddresses baseAddresses;
    offsetsAndVals offsetsNVals;
    bool shouldResetResources;
};

struct upgrades {
    char* upgradeText[maxAvailableUpgrades];
    buttons buttonIndex[maxAvailableUpgrades];
};

struct upgradeStruct {
    upgrades* availableUpgrades;
    char (*formattedButtonStrings)[maxButtonStringLength];
    const double* upgradesCost;
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

void applyPatches(void) {
    _SetOtherThreadsSuspended(true);
    deployExecutableASM(&addButtonsChooseDistrict);
    uint64_t nextStringToPrintAddress = (addButtonsChooseDistrict.hookTarget + (addButtonsChooseDistrict.fileSize - addButtonsChooseDistrict.bytesToStrip) - addButtonsChooseDistrict.numberOfWritableBytes + 18);
    createUIButtonUseSetString.externalReplacementValues[0] = nextStringToPrintAddress;
    deployExecutableASM(&createUIButtonUseSetString);
    //MessageBoxA(NULL, "Patches injected", "Patches injected", MB_OK);

    _SetOtherThreadsSuspended(false);
}

void freePatches(void) {
    VirtualFree((LPVOID)addButtonsChooseDistrict.hookTarget, 0, MEM_RELEASE);
    VirtualFree((LPVOID)createUIButtonUseSetString.hookTarget, 0, MEM_RELEASE);
}

bool ammoTypeHasBullet(unsigned char* ammoTypeAddress) {
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
    updateVars(vars->baseAddresses.mech, vars->offsetsNVals.mech, mechVars);
    updateVars(vars->baseAddresses.mechLegs, vars->offsetsNVals.mechLegs, mechLegsVars);
    updateVars(vars->baseAddresses.primary, vars->offsetsNVals.primary, primaryWeaponVars);
    updateVars(vars->baseAddresses.secondary, vars->offsetsNVals.secondary, secondaryWeaponVars);
    if (ammoTypeHasBullet(fetchDeployedPrimaryAmmoTypeAddress)) {
        updateVars(vars->baseAddresses.primaryBullet, vars->offsetsNVals.primaryBullet, primaryBulletVars);
    }
    if (ammoTypeHasBullet(fetchDeployedSecondaryAmmoTypeAddress)) {
        updateVars(vars->baseAddresses.secondaryBullet, vars->offsetsNVals.secondaryBullet, secondaryBulletVars);
    }
}

void resetVars(uint64_t* baseAddress, uthruple* offsetsNVals, uint64_t varCount, uint64_t deployedResourceAddress) {
    for (int i = 0; i < varCount; i++) {
        if (*baseAddress != NULL) {
            *(uint32_t*)(*baseAddress + offsetsNVals[i].offset) = offsetsNVals[i].org;
        }
        offsetsNVals[i].org = *(uint32_t*)(deployedResourceAddress + offsetsNVals[i].offset);
        offsetsNVals[i].val = offsetsNVals[i].org;
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
        uint64_t curDeployedMechAddress = fetchDeployedMechAddress;
        uint64_t curDeployedMechLegsAddress = fetchDeployedMechLegsAddress;
        uint64_t curDeployedPrimaryWeaponAddress = fetchDeployedPrimaryWeaponAddress;
        uint64_t curDeployedSecondaryWeaponAddress = fetchDeployedSecondaryWeaponAddress;
        uint64_t curDeployedPrimaryBulletAddress = fetchDeployedPrimaryBulletAddress;
        uint64_t curDeployedSecondaryBulletAddress = fetchDeployedSecondaryBulletAddress;

        //Should refactor but w/e
        //Reset Mech Vars and set new orgs
        resetVars(&vars->baseAddresses.mech, vars->offsetsNVals.mech, mechVars, curDeployedMechAddress);

        //Reset Mech Legs Vars and set new orgs
        resetVars(&vars->baseAddresses.mechLegs, vars->offsetsNVals.mechLegs, mechLegsVars, curDeployedMechLegsAddress);

        //Reset Primary Weapon Vars and set new orgs
        resetVars(&vars->baseAddresses.primary, vars->offsetsNVals.primary, primaryWeaponVars, curDeployedPrimaryWeaponAddress);


        //Reset Secondary Weapon Vars and set new orgs
        resetVars(&vars->baseAddresses.secondary, vars->offsetsNVals.secondary, secondaryWeaponVars, curDeployedSecondaryWeaponAddress);

        //Reset Primary Bullet Vars and set new orgs
        if (ammoTypeHasBullet(fetchDeployedPrimaryAmmoTypeAddress)) {
            resetVars(&vars->baseAddresses.primaryBullet, vars->offsetsNVals.primaryBullet, primaryBulletVars, curDeployedPrimaryBulletAddress);
        }

        //Reset Secondary Bullet Vars and set new orgs
        if (ammoTypeHasBullet(fetchDeployedSecondaryAmmoTypeAddress)) {
            resetVars(&vars->baseAddresses.secondaryBullet, vars->offsetsNVals.secondaryBullet, secondaryBulletVars, curDeployedSecondaryBulletAddress);
        }

        //Stop updating when choose district state has been entered
        if (currentState == FreelancerChooseDistrict)
            vars->shouldResetResources = false;
    }
    else if (shouldReset){
        vars->shouldResetResources = true;
    } 
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

//#define getPlayerAddressFunctionOffset 0x1542f0
//#define getPlayerAddressFunction baseModule + getPlayerAddressFunctionOffset
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
                    //MessageBoxA(NULL, "Found Player Address", "ALIVE", MB_OK);
                    return *(uint64_t*)(lVar2 + 8 + uVar4 * 0x10);
                }
            }
        }
    }
    //MessageBoxA(NULL, "Could not find player address", "ALIVE", MB_OK);
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
    return upgradeState->consumed < upgradeState->freeUpgradesPerLevel ? 0 : upgradeState->upgradesCost[buttonToHandle] * pow(5, (upgradeState->consumed-1));
}


//Formats the strings in addedButtonStrings to be used when displaying buttons
//Should be called after any on the variables used to format the strings are altered.
void formatButtonStrings(upgradeStruct* upgradeState) {
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
       default:
           MessageBoxA(NULL, "Error: Undefined button tried to be formatted. ", "ALIVE", MB_OK);
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
void setupUpgradeList(upgradeStruct* upgradeState, rngStruct* rng) {
    //Randomize once
    if (upgradeState->randomizeUpgrades && fetchCurrentState == FreelancerChooseDistrict) {
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
        upgradeState->randomizeUpgrades = false;
        upgradeState->consumed = 0;
        upgradeState->removedUpgrades = 0;
        updateAvailableUpgrades(upgradeState);
    }
    //Prepare to randomize when player next enters the choose district menu.
    else if (!upgradeState->randomizeUpgrades && fetchCurrentState != FreelancerChooseDistrict) {
        upgradeState->randomizeUpgrades = true;
    }
}

//Logic for upgrade / button press handling
void handlePressedButton(buttons buttonToHandle, upgradeStruct* upgradeState, varStruct* vars) {
    float refloat;
    if (subtractMoney(getUpgradeCost(buttonToHandle, upgradeState))) {
        upgradeState->consumed++;
        switch (buttonToHandle) {
        case M_Repair:
            upgradeState->repairAmount += repairHealthPoints;
            upgradeState->maxHealth = *(float*)&vars->offsetsNVals.mech[MaxHealthIdx].val;
            /*
            if (upgradeState->playerHealth + repairAmount <= upgradeState->maxHealth)
                upgradeState->playerHealth += repairAmount;
            else
                upgradeState->playerHealth = upgradeState->maxHealth;
            */
            break;
        case M_PosOvercharge:
            refloat = reinterpret_cast<float&>(vars->offsetsNVals.mech[MaxOverchargeIdx].val);
            refloat *= overchargeMult;
            vars->offsetsNVals.mech[MaxOverchargeIdx].val = reinterpret_cast<uint32_t&>(refloat);
            break;
        case M_PosForwardSpeed:
            refloat = reinterpret_cast<float&>(vars->offsetsNVals.mechLegs[MaxForwardSpeedIdx].val);
            refloat *= forwardSpeedMult;
            vars->offsetsNVals.mechLegs[MaxForwardSpeedIdx].val = reinterpret_cast<uint32_t&>(refloat);
            break;
        case P_PosCapacity: // Primary: +Capacity
            vars->offsetsNVals.primary[CapacityIdx].val = (uint32_t)(vars->offsetsNVals.primary[CapacityIdx].val * pCapacityMult);
            break;
        case P_PosFireRate: // Primary: +Fire Rate
            refloat = reinterpret_cast<float&>(vars->offsetsNVals.primary[CooldownIdx].val);
            refloat *= 2-pFireRateMult;
            vars->offsetsNVals.primary[CooldownIdx].val = reinterpret_cast<uint32_t&>(refloat);
            break;
        case P_PosProjectilesNegAccuracy: // Primary: +Projectiles, -Accuracy
            vars->offsetsNVals.primary[ShotCountIdx].val = (uint32_t)((int)vars->offsetsNVals.primary[ShotCountIdx].val + pProjectiles);
            refloat = reinterpret_cast<float&>(vars->offsetsNVals.primary[AccuracyIdx].val);
            refloat += pAccuracy*0.017; //+5 degrees
            //refloat *= 1.1;
            vars->offsetsNVals.primary[AccuracyIdx].val = reinterpret_cast<uint32_t&>(refloat);
            break;
        case P_PosPropMult: // Primary: +Structure Damage
            refloat = reinterpret_cast<float&>(vars->offsetsNVals.primaryBullet[PropMultIdx].val);
            refloat *= pPropMult;
            vars->offsetsNVals.primaryBullet[PropMultIdx].val = reinterpret_cast<uint32_t&>(refloat);
            break;
        case S_PosCapacity: // Secondary: +Capacity
            vars->offsetsNVals.secondary[CapacityIdx].val = (uint32_t)(vars->offsetsNVals.secondary[CapacityIdx].val * sCapacityMult);
            break;
        case S_PosFireRate: // Secondary: +Fire Rate
            refloat = reinterpret_cast<float&>(vars->offsetsNVals.secondary[CooldownIdx].val);
            refloat *= 2 - sFireRateMult;
            vars->offsetsNVals.secondary[CooldownIdx].val = reinterpret_cast<uint32_t&>(refloat);
            break;
        case S_PosProjectilesNegAccuracy: // Secondary: +Projectiles, -Accuracy
            vars->offsetsNVals.secondary[ShotCountIdx].val = (uint32_t)((int)vars->offsetsNVals.secondary[ShotCountIdx].val + pProjectiles);
            refloat = reinterpret_cast<float&>(vars->offsetsNVals.secondary[AccuracyIdx].val);
            refloat += sAccuracy*0.017; //+5 degrees.
            //refloat *= 1.1;
            vars->offsetsNVals.secondary[AccuracyIdx].val = reinterpret_cast<uint32_t&>(refloat);
            break;
        case S_PosPropMult: // Secondary: +Structure Damage
            refloat = reinterpret_cast<float&>(vars->offsetsNVals.secondaryBullet[PropMultIdx].val);
            refloat *= sPropMult;
            vars->offsetsNVals.secondaryBullet[PropMultIdx].val = reinterpret_cast<uint32_t&>(refloat);
            break;
        default:
            MessageBoxA(NULL, "Error: Undefined button pressed. ", "ALIVE", MB_OK);
            return;
        }
        updateAvailableUpgrades(upgradeState);
    }
}


//Handles a player's action on the choose district menu in freelancer by observing the state of the currently selected button
void handleChooseDistrictMenu(upgradeStruct* upgradeState, varStruct* vars) {
    if (addButtonsChooseDistrict.isDeployed && fetchCurrentState == FreelancerChooseDistrict) {
        char buffer[256];

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
            //buttons buttonToHandle = (buttons)(distanceFromOriginalItem - 1);

            //Handle the pressed mod button
            handlePressedButton(buttonToHandle, upgradeState, vars);
            
            //Update weapon vars in memory
            updateResources(vars);

            //Reset selected button to index 0 
            memset((void*)chooseDistrictMenuStruct, 0x0, 2);
        } 
    }
    return;
}

DWORD WINAPI MainThread(LPVOID param) {
    char buffer[256];

    //Setup variables struct
    varBaseAddresses deployedBaseAddresses{ 0, 0, 0, 0, 0, 0 };

    offsetsAndVals deployedOffsetsAndVals{
        deployedMechOffsetsAndVals,
        deployedMechLegsOffsetsAndVals,
        deployedPrimaryWeaponOffsetsAndVals,
        deployedSecondaryWeaponOffsetsAndVals,
        deployedPrimaryBulletOffsetsAndVals,
        deployedSecondaryBulletOffsetsAndVals
    };
    varStruct variablesStruct{ deployedBaseAddresses, deployedOffsetsAndVals , false};

    //Repair is set to always show up
    const buttons alwaysAvailableUpgradesButtons[] = { M_Repair };

    //Setup buffers for formatted strings to be shown by added buttons
    char formattedButtonStrings[addedButtons][maxButtonStringLength];

    //Setup upgrade state struct
    upgradeStruct upgradeState{
        &upgradeList,
        formattedButtonStrings,
        baseButtonCosts,
        sizeof(alwaysAvailableUpgradesButtons) / sizeof(alwaysAvailableUpgradesButtons[0]),
        alwaysAvailableUpgradesButtons,
        4,
        1,
        5,
        0,
        0,
        true,
        0,
        0
    };

    //Setup rng
    std::random_device rd;  // a seed source for the random number engine
    std::mt19937 gen(rd()); // mersenne_twister_engine seeded with rd()
    std::uniform_int_distribution<> distrib(0, addedButtons - 1);

    rngStruct rng{ gen, distrib };

    while (true) {
        //Uncomment this and the unsuspend at the end of the loop if race conditions cause issues
        //_SetOtherThreadsSuspended(true);
        
        /*
        if (GetAsyncKeyState(VK_NUMPAD1) & 0x80000) {
            addMoney(1000000);
            snprintf(buffer, 100, "freelancerMenuState = %llx", (uint32_t)fetchFreelancerMenuState);
            MessageBoxA(NULL, buffer, "ALIVE", MB_OK);
            snprintf(buffer, 100, "playerAddress = %#016x", getPlayerAddress());
            MessageBoxA(NULL, buffer, "ALIVE", MB_OK);
            if (getPlayerHealth() != 0) {
                snprintf(buffer, 100, "playerHealthAddress = %#016x", getPlayerHealthAddress);
                MessageBoxA(NULL, buffer, "ALIVE", MB_OK);
                snprintf(buffer, 100, "playerHealth = %f", *(float*)getPlayerHealthAddress);
                MessageBoxA(NULL, buffer, "ALIVE", MB_OK);
                *(float*)getPlayerHealthAddress += 100;
            }
        }
        */

        //Update playerHealth to the modded value
        handlePlayerHealth(&upgradeState);

        //Reset modded weapon vars if certain game states are observed.
        resetResources(&variablesStruct);

        //Handle logic in freelancer menu
        //handleFreelancerMenu();

        //Randomize the list of available upgrades
        setupUpgradeList(&upgradeState, &rng);

        //Handle logic in freelancer choose district menu
        handleChooseDistrictMenu(&upgradeState, &variablesStruct);

        //_SetOtherThreadsSuspended(false);

        Sleep(100);
    }
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
        //MessageBoxA(NULL, "DLL injected", "DLL injected", MB_OK);
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
        _SetOtherThreadsSuspended(false);
        break;
    }
    return TRUE;
}

