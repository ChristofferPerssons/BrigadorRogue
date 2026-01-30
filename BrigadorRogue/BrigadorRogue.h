#pragma once

uint64_t baseModule;
//During the game loop the value of Base+RootOffset is often contained within r14. 
// This value is the key to finding pointer paths. Most (if not all) paths begin with this value.
#define rootOffset 0x4fdc18
#define keyAddress (baseModule + rootOffset)


const enum states {
    MainMenu = 0x2,
    Campaign = 0x3,
    Freelancer = 0x4,
    Aquisitions = 0x5,
    Settings = 0x7,
    Credits = 0x6,
    languageSelect = 0xA,
    AfterLoadLevel = 0x8,
    InGame = 0xE,
    PauseMenu = 0x10,
    InGameSettings = 0x11,
    FreelancerChooseDistrict = 0xC,
    LoseScreen = 0x0B
};

#define stateStructOffset 0x2918

#define fetchCurrentState (states)*(uint32_t*)(*(uint64_t*)(*(uint64_t*)keyAddress + stateStructOffset) + 0x4)

//MechOffset and weapons offsets are correlated by "mechOffset + 0x18 = primaryWeaponOffset"
#define mechOffset 0x2d00

#define mechLegsOffset 0x80

#define weaponOffset(y) (0x2d18+(y<<0x5))

#define weaponMountOffset(y) 0x8+weaponOffset(y)

#define bulletOffset 0x540

#define fetchDeployedMechAddress  *(uint64_t*)(*(uint64_t*)keyAddress + mechOffset)
#define maxOverchargeOffset 0x10b8
#define maxHealthOffset 0x10c0

#define fetchDeployedMechLegsAddress  *(uint64_t*)(*(uint64_t*)(*(uint64_t*)keyAddress + mechOffset)+mechLegsOffset)
#define maxForwardSpeedOffset 0x354

#define fetchDeployedWeaponAddress(y)  *(uint64_t*)(*(uint64_t*)keyAddress + weaponOffset(y))
#define weaponVarsOffset 0x3d8
#define weaponCapacityOffset 0x20
#define weaponCooldownOffset 0x0
#define weaponShotCountOffset 0x28
#define weaponAccuracyOffset 0x10

#define fetchDeployedWeaponBulletAddress(y)  *(uint64_t*)(*(uint64_t*)(*(uint64_t*)keyAddress + weaponOffset(y))+bulletOffset)
#define bulletPropMultOffset 0x24

#define fetchDeployedWeaponMountAddress(y) (*(uint64_t*)(*(uint64_t*)keyAddress + weaponMountOffset(y)))
#define weaponGroupOffset 0x24
#define fetchDeployedWeaponMountGroup(y) *(unsigned char*)(fetchDeployedWeaponMountAddress(y)+weaponGroupOffset)

const enum weaponGroups {
    PrimaryGroup,
    SecondaryGroup,
    HornGroup,
    NoneGroup
};

const enum ammoTypes {
    None = 0x0,
    Bullet = 0x1,
    Artillery = 0x2,
    Flame = 0x4,
    Laser = 0x8,
    Cannon = 0x10,
    Smoke = 0x20,
    EMP = 0x40
};

#define ammoTypeOffset 0x48c
#define fetchDeployedWeaponAmmoTypeAddress(x)  (unsigned char*)(*(uint64_t*)(*(uint64_t*)keyAddress + weaponOffset(x))+ammoTypeOffset)

//thruple. Currently used as: offset, 4byte default value, 4byte modded value
struct uthruple {
    uint64_t offset;
    uint32_t org;
    uint32_t val;
};

#define districtItemSize 0x2110

#define fetchFreelancerSelectedMechAddress (*(uint64_t*)(*(uint64_t*)keyAddress+stateStructOffset)+0x128+0x18+(0x35*0x88))
#define fetchFreelancerSelectedPrimaryWeaponAddress (*(uint64_t*)(*(uint64_t*)keyAddress+stateStructOffset)+0x128+0x18+(0x36*0x88))
#define fetchFreelancerSelectedSecondaryWeaponAddress (*(uint64_t*)(*(uint64_t*)keyAddress+stateStructOffset)+0x128+0x18+(0x37*0x88))

const enum freelancerMenuStates {
    Pilot,
    Vehicle,
    Primary,
    Secondary,
    Special,
    Operation
};

//Both of these paths are the same but using the one that is more similar to fetchFreelancerSelectedMechAddress
//#define fetchFreelancerMenuState (freelancerMenuStates)*(uint32_t*)(0x1b18+(*(uint64_t*)(*(uint64_t*)keyAddress + stateStructOffset))+0x128)
#define fetchFreelancerMenuState (freelancerMenuStates)*(uint32_t*)(*(uint64_t*)(*(uint64_t*)keyAddress + stateStructOffset) + 0x128 + (0x33 * 0x88))
#define offsetUsedToFetchPlayerAddress 0x2ba0


#define offsetUsedToFetchDebugMechMenuParameter 0x224cd8
#define fetchDebugMechMenuParameter *(uint64*)(baseModule + offsetUsedToFetchDebugMechMenuParameterAddress)


#define moneyBase baseModule + 0x4fdea0

#define mechResourceBytes 4632
//Weapon types differ in size. Max seems to be laser weapons at 1608 bytes
#define maxWeaponResourceBytes 1608

#define maxAvailableUpgrades 32

#define maxButtonStringLength 256

#define mechResourceBytes 4632

#define maxWeaponResourceBytes 1608

#define maxWeapons 9


