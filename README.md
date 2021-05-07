## Building the Code

```
To build this package, do the following steps:

    1. meson build
    2. ninja -C build

To clean the repository run `rm -rf build`.
```

## Hostboot dump collection interface

The host dump collection interface can be invoked in the following way

```
busctl --verbose call org.open_power.Dump.Manager
       /org/openpower/dump xyz.openbmc_project.Dump.Create
       CreateDump a{sv} 2
       "com.ibm.Dump.Create.CreateParameters.DumpType" s
       "com.ibm.Dump.Create.DumpType.Hostboot"
       "com.ibm.Dump.Create.CreateParameters.ErrorLogId" t  <ERROR LOG ID>
```

## Hardware dump collection interface

The hardware dump collection interface can be invoked in the following way

```
busctl --verbose call org.open_power.Dump.Manager
       /org/openpower/dump xyz.openbmc_project.Dump.Create
       CreateDump a{sv} 3
       "com.ibm.Dump.Create.CreateParameters.DumpType" s
       "com.ibm.Dump.Create.DumpType.Hardware"
       "com.ibm.Dump.Create.CreateParameters.ErrorLogId" t  <ERROR LOG ID>
       "com.ibm.Dump.Create.CreateParameters.FailingUnitId" t <FAILING_UNIT_ID>
```

Where

- Valid dump types are listed
  [here](https://github.com/openbmc/phosphor-dbus-interfaces/blob/master/yaml/com/ibm/Dump/Create.interface.yaml)
- ERROR LOG ID is a 32bit number.
  - An error will be logged and dump will be collected with error log id as 0,
    if an invalid number is passed.
- FAILING UNIT ID is an 8bit number indicating the id of the failing processor
  and the maximum allowed value is 32.
