__Brigador Rogue__

This mod enables random upgrades between each freelancer district.


__Code__

The repo consists of two main folders.

* Brigador Injector performs the task of injecting a dll.

* Brigador Rogue creates a dll which itself injects some assembly into the brigador executable.

The code should hopefully be simple to extend to support more upgrade options. Some basic programming knowledge may make things quicker.

Pointers to the current mech, mech legs, primary weapon, secondary weapon, and the corresponding bullets are already defined. Small offsets from these defined addresses are all that's required to use additional parameters. These are easily found by a program such as cheat engine while altering values with the debug menu to isolate the desired memory locations.
