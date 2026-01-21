Brigador Modding



freelancer menu state = \[r15+rsi+0x128] where:

rsi = \[r14+0x2918]

r15 = \[\[r14+0x2918]+0x8]\*0x88



TODO: "Upgrade randomization", "testing of separately stored secondary to allow for building two weapons of the same type"



rcx   \[\[\[stacktop+0x2ba0]]+0c1b8c8]

r8    \[\[\[\[stacktop+0x2ba0]]+0c1b8c8]]



\[r14+0x58]+0x8



r8-0x80



\[0x4FDEA0+0xa4b8]



0x2110 is the difference between the addresses of two district structure (BIG ?). If we have a start address we can therefore use this to to see which index we are currently using.



When calling createUIButton r9 contains the string address of what you want to print or %s if it is variable. Use this to differentiate each button



At +0x1f880 rdx contains the Unicode string printed to a button.



RDI should be start of weaponaddresslist then call createUIButtonFreelancer



\[stackTop+2b28] gets currently chosen level struct?



Method: In choose district, keep track of the selected menu index. If this is above the amount of districts to choose from, we have selected a button we created which should behave differently from the rest and other rules should take effect. Should probably have any of these new events cause a player reload to keep everything up to date.



What to do now:

I want to do a trial run on if it is possible to create a miniature roguelike mode.

First, save money when pressing the deploy button such that money can be restored after run end. Second populate the district choosing menu with additional buttons that serve to upgrade weapons. Third, restore money and add the x% of the collected run money to the new total this may necessitate updating a separate variable with the amount gained from a mission.

To begin the implementation, populate the choose district with new buttons.





Money is stored as double on +0x4fdea



I believe you are able to find many of the lists of key for different types of assets by going to the "\_printStatsAfterLoadingLevel" function and tracing back the key strings address of the "loadValueFromKey" function to r14 (param1) which should be static during gameplay. The strings used in the "debug\_print" function after a loadValueFromKey call indicate the type of key that was used.



4 bytes before the start of a key list's strings the number of keys in the list is stored.



\[mechstructAdress + 0x1208] should get the price for any mech struct?





createUIButtonAquisition at +0x49c10l is the function responsible for creating the buttons to display. Replacing a call to it with nop seems to not create any side effects (except for removing the buttons).



Searching for RBP-0x50. Is RBP = stackTop?



Current mech struct pointer = \[r14+0x2d00] = \[stackTop+0x2d00] gets input from activeButtonValue

activeButtonValue = r15 at +0x65200 = \[(\[stackTop+0x2918]+(0x35\*0x88)+0x128)+0x18] = \[\[stackTop+0x2918]+0x1d68]

Generally UI selected structures (like mechs or weapons) seems to be found by looking at the value of \[(\[stackTop+0x2918]+(INDEX\*0x88)+0x128)+0x18] or \[\[stackTop+0x2918]+(INDEX\*0x88)+0x140] where stacktop can be found dynamically by looking at brigador.exe+0x4fdc18 and INDEX is on a per UI type basis.



Known UI types are: 0x35=mech, 0x39, 0x34, 0x38, 0x36, 0x37,







Selected mech's string seems to be accessed in mayBeRelatedToLoadLevel by loading \[\[\[\[param\_1+0x2d00]]+0x80]-0x10] where param\_1 has been observed to be supplied by r14 (eg stackTop). RDX seems to be supplied from \[R14 + 0x2850] in the main loop which I still believe to be "static". As such, I should always be able to extract and manipulate this value. as we now also have the code for reloading the player (in mayBeRelatedToLoadLevel look for ("reload player")) we should be able to reload the player mech whenever. Should grab r14 during initiation of game loop or during one of the supposed "stable" sections of the code since it may be used (I seem to recall that it is altered in some spots).



Path to player component key: \[\[\[\[r14+0x2d00]]+0x80]-0x10] where 0x80 is an offset which seems to be depending on what component we are after.

\[\[\[r14+0x2d00]+0x80] - 0x10]



Register behaviour in game loop:

\- RBP seems to always point to a constant value and used as a base for all other pointers(?). Of course, its a fucking base stack pointer

\- RSP seems to be RBP+0x100

\- R14 seems to be RBP+0x3E60. Should save this after hooking into the game loop and use it to alter state

\- R15 seems to be RBP+0x65F8



brigador.exe+0x5caf2 is when in the gameloop r13 holds the states number as is to be used







RSI = \*(RSP+0x60)

R13D = \*(RSI+0x4)

->\*RSI+0x4->r13d



\- State pointer register path when r13 is used for state(menu) switch: r14->rsi->rsi+0x4->r13

\- Adresses of ops resulting in this path: constantSinceInit(?)->brigador.exe+0x5c631->brigador.exe+0x5c66f

\- R13 is used to fetch state value at brigador.exe+0x5cd66.

\- The switch jump at brigador.exe+0x5cd86 is a possible avenue of hooking to be able to construct my own switch location if necessary or continue with the intended hook if the menu state does not match.



\[RSP+0x60]->RSI





R14 in main loop does not seem to change anywhere (hopefully) this means we have access to the pointer of most likely the game state struct





"Z:\\\\host\\\\mnt\\\\volume\_tor1\_01\\\\volume\_tor1\_01\\\\gitlab-runner\\\\builds\\\\4xJrWP85\\\\0\\\\pap\\\\sjgame1\\\\build\\\\Windows\\\\release-assertions-gogx64\\\\obj\\\\brigador\\\\brigador.pdb"

Doing Currently:

 	-Go through all calls to debug info to find function names. -DONE



01400362e0

Progress:

 	Menu:

 		-Bool for advising a level change has been found

 		-Debug tab levelselect variables have been mostly found

 		-Able to alter menu text without issue by changing the Unicode in memory

 		-Able to set game state by altering memory. Look at the setGameState function for how to do this. To find the memory adress, we should be able to use some registers that are constant throughout the game loop (R14? I think it always points to the game state struct) or a stack pointer with fixed offsets. Currently you can put a breakpoint in the setGameState function to extract the location of where parameter 3 is writtern(eg R8 is written)

 	Loading:

 		-SV file is created by save command during a mission (freelance only??). This saves loadout and level as far as I know now.

 		-cJSON is used for JSON parsing?

 	In-game:

 		-Weapon stats seem to update automatically after altering the memory which should enable for example berserker pickups

 	GameState:

 		-State to State number: MainMenu=2, Campaign=3, Freelance=4, Aquisitions=5, Settings=7, credits=6, languageSelect=A, AfterLoadLevel=8, InGame=E, PauseMenu=10, InGameSettings=11, FreelancerChooseDistrict=C, LoseScreen=0B (different from win screen??),







TODO:

 	Menu:

 		-Find a way to choose which level to load

 		-Use dll injection to load a level

 		-Change acquisition button to instantly load a level to test if changes to menu behaviour is possible

 	Explore the Debugprint function. If this is understood tracing the code should become simpler. Perhaps always get output.



 	Loadout selection:



 	Mission Selection:





Memory translation notes:

 	Cheat Engine(7FF6355A0000) = Cheat Engine(brigador.exe)

 	Ghidra(0x140000000) = Cheat Engine(brigador.exe0)

 	Ghidra(0x1400b96a0) = Cheat Engine(brigador.exe+0xb96a0)

 	Cheat Engine(7ff63567d850) = Cheat Engine(brigador.exe+dd850)

 	Cheat Engine(7ff63567d850) = Ghidra(0x1400dd850)



8ae4ff8e0 = 7ff6355a0000 + x

x = 8ae4ff8e0 - 7ff6355a0000





Definitions:

 	(?) = Is it possible/ worth it to include



What I ultimately want to do:

 	Roguelike mode for brigador. Each round you are spawned into a random map and tasked with completing the mission. After each mission you gain cash to spend on upgrades. Persistent upgrades through initial



Pre-run:

 	-A portion of the cash gained through a run is added to the liquid cash and used to



Mission select:

 	-Increased difficulty and cash gained after each completed mission

 	-(?) Each mission is able to utilize mission/spawn modifiers.





Upgrades:

 	-Mech purchase

 	-Weapon purchase

 	-Mech upgrades (increased speed, shields, health, repair, turning speed)

 	-Switch weapon slot type (turret to main for example)

 	-Weapon upgrades (fire rate, shield damage, hull damage, aoe, (?)projectile speed)

